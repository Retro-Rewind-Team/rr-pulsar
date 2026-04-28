#requires -version 5.1
<#
.SYNOPSIS
  Build (make install -j8), launch Dolphin with a Riivolution JSON, then record results to crash.txt.

.DESCRIPTION
  1) Runs "make install -j8" from the repository root (unless -SkipBuild). Stops on build failure
     unless you pass -ContinueOnBuildFailure.
  1b) When DolphinUser and ModFolderName are set, copies build/Code.pul to
     <DolphinUser>\\Load\\Riivolution\\<ModFolderName>\\Binaries\\Code.pul so the same Dolphin
     you launch loads the new patch (avoids RIIVO pointing at a different tree than make install).
     Use -SkipCodePulDeploy or SkipCodePulDeploy=true in the paths file to turn this off.
  2) Starts Dolphin with the given Riivolution .json (Dolphin 5+ CLI: pass a JSON with "version" and
     "riivolution" sections, like the upstream gist example; extension is usually .json).
  3) Watches until Dolphin exits OR Crash.pul appears (Pulsar; see -CrashPulPath / -DolphinUser and -ModFolderName).
     Crash.pul counts only if its last-write time is on/after the Dolphin session start (ignores stale files).
  4) Deletes the report file (OutFile) at startup, then appends one run report (make, logs, etc.).
  5) Stops the Dolphin process when the watch loop ends (normal exit, crash file, or error).
  6) Merges User/Config/Logger.ini (unless -SkipLoggerIni) so WriteToFile and log types OSREPORT + OSREPORT_HLE
     are enabled — matching upstream Dolphin (Logger system: Options/Logs in Logger.ini; WriteToFile defaults false).
     After the run, copies matching lines from new bytes in User/Logs/*.log into the report.

  Pulsar writes: /shared2/Pulsar/<GetModFolder>/Crash.pul on the Wii image. In Dolphin, that is typically
  <Dolphin user folder>\Wii\shared2\Pulsar\<ModFolderName>\Crash.pul

  Requires: `make` on PATH (e.g. Git for Windows/MSYS2) if not using -SkipBuild.

  Optional file Test-DolphinScenario.paths.txt in this script's folder: key=value lines
  (see Test-DolphinScenario.paths.example.txt). Command-line parameters override the file.
#>
[CmdletBinding()]
param(
  [string]$ConfigFile = "",
  [string]$RepoRoot = "",
  [string]$DolphinExe = "",
  [string]$RiivolutionJson = "",
  [string]$DolphinUser = "",
  [string]$ModFolderName = "",
  [string]$CrashPulPath = "",
  [string]$OutFile = "",
  [switch]$SkipBuild,
  [switch]$ContinueOnBuildFailure,
  [switch]$SkipCodePulDeploy,
  [switch]$SkipLoggerIni,
  [int]$PollMs = 500
)

$ErrorActionPreference = "Stop"
# Capture for config merge (child functions would otherwise see their own $PSBoundParameters)
$__scenarioBound = $PSBoundParameters

$__scriptRoot = $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($__scriptRoot) -and $MyInvocation.MyCommand.Path) {
  $__scriptRoot = Split-Path -Parent -LiteralPath $MyInvocation.MyCommand.Path
}
if ([string]::IsNullOrWhiteSpace($__scriptRoot)) {
  throw "Could not determine script directory (PSScriptRoot). Run with: powershell -File .\Test-DolphinScenario.ps1"
}

# Value after '=' may be unquoted (spaces allowed) or wrapped in matching " or '.
function ConvertFrom-PathsConfigValue([string]$rawAfterEquals) {
  if ($null -eq $rawAfterEquals) { return "" }
  $s = $rawAfterEquals.Trim()
  if ($s.Length -eq 0) { return "" }
  if ($s.Length -ge 2) {
    $a = $s[0]
    $b = $s[$s.Length - 1]
    if (($a -eq [char]34 -or $a -eq [char]39) -and $a -eq $b) { return $s.Substring(1, $s.Length - 2) }
  }
  return $s
}

function Read-PathConfigFile([string]$Path) {
  $h = [ordered]@{}
  if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $h }
  $lines = Get-Content -LiteralPath $Path -ErrorAction Stop
  foreach ($raw in $lines) {
    $line = $raw.Trim()
    if ($line.Length -eq 0) { continue }
    if ($line[0] -eq '#' -or $line[0] -eq ';') { continue }
    $eq = $line.IndexOf('=')
    if ($eq -lt 1) { continue }
    $k = $line.Substring(0, $eq).Trim()
    $v = ConvertFrom-PathsConfigValue -rawAfterEquals $line.Substring($eq + 1)
    if ($k.Length -gt 0) { $h[$k] = $v }
  }
  return $h
}

function Test-ConfigBoolTrue([string]$s) {
  if ([string]::IsNullOrWhiteSpace($s)) { return $false }
  $t = $s.Trim().ToLowerInvariant()
  return $t -eq '1' -or $t -eq 'true' -or $t -eq 'yes' -or $t -eq 'on'
}

function Get-ConfigValueFromMap {
  param([hashtable]$FileMap, [string]$Name)
  if ($null -eq $FileMap) { return $null }
  foreach ($key in $FileMap.Keys) { if ($key -ieq $Name) { return $FileMap[$key] } }
  return $null
}

function Get-MergedStringParam {
  param(
    [string]$Name,
    [string]$BoundValue,
    [hashtable]$FileMap,
    [string]$Default,
    $ScriptBound
  )
  if ($null -ne $ScriptBound -and $ScriptBound.ContainsKey($Name)) { return $BoundValue }
  $fv = Get-ConfigValueFromMap -FileMap $FileMap -Name $Name
  if ($null -ne $fv) { return [string]$fv }
  if (-not [string]::IsNullOrWhiteSpace($BoundValue)) { return $BoundValue }
  return $Default
}

$defaultConfigPath = Join-Path $__scriptRoot "Test-DolphinScenario.paths.txt"
if ([string]::IsNullOrWhiteSpace($ConfigFile)) { $ConfigFile = $defaultConfigPath }
$pathFile = $ConfigFile

$fileMap = @{}
if (Test-Path -LiteralPath $pathFile -PathType Leaf) {
  $ordered = Read-PathConfigFile -Path $pathFile
  foreach ($e in $ordered.GetEnumerator()) { $fileMap[$e.Key] = $e.Value }
}

$defaultRepo = (Resolve-Path (Join-Path $__scriptRoot "..")).Path
$scriptBound = $__scenarioBound
$RepoRoot = [string](Get-MergedStringParam -Name "RepoRoot" -BoundValue $RepoRoot -FileMap $fileMap -Default $defaultRepo -ScriptBound $scriptBound)
if ([string]::IsNullOrWhiteSpace($RepoRoot)) { $RepoRoot = $defaultRepo }
$RepoRoot = [System.IO.Path]::GetFullPath($RepoRoot)

$DolphinExe = [string](Get-MergedStringParam -Name "DolphinExe" -BoundValue $DolphinExe -FileMap $fileMap -Default "" -ScriptBound $scriptBound)
$RiivolutionJson = [string](Get-MergedStringParam -Name "RiivolutionJson" -BoundValue $RiivolutionJson -FileMap $fileMap -Default "" -ScriptBound $scriptBound)
$DolphinUser = [string](Get-MergedStringParam -Name "DolphinUser" -BoundValue $DolphinUser -FileMap $fileMap -Default "" -ScriptBound $scriptBound)
$ModFolderName = [string](Get-MergedStringParam -Name "ModFolderName" -BoundValue $ModFolderName -FileMap $fileMap -Default "" -ScriptBound $scriptBound)
$CrashPulPath = [string](Get-MergedStringParam -Name "CrashPulPath" -BoundValue $CrashPulPath -FileMap $fileMap -Default "" -ScriptBound $scriptBound)
$OutFile = [string](Get-MergedStringParam -Name "OutFile" -BoundValue $OutFile -FileMap $fileMap -Default "" -ScriptBound $scriptBound)

$skipVal = Get-ConfigValueFromMap -FileMap $fileMap -Name "SkipBuild"
$contVal = Get-ConfigValueFromMap -FileMap $fileMap -Name "ContinueOnBuildFailure"
if (-not $scriptBound.ContainsKey("SkipBuild") -and $null -ne $skipVal) { $SkipBuild = (Test-ConfigBoolTrue $skipVal) }
if (-not $scriptBound.ContainsKey("ContinueOnBuildFailure") -and $null -ne $contVal) { $ContinueOnBuildFailure = (Test-ConfigBoolTrue $contVal) }

if (-not $scriptBound.ContainsKey("PollMs")) {
  $pPoll = Get-ConfigValueFromMap -FileMap $fileMap -Name "PollMs"
  if ($null -ne $pPoll -and "$pPoll" -ne "") { try { $PollMs = [int]$pPoll } catch { $PollMs = 500 } }
}
if ($PollMs -le 0) { $PollMs = 500 }

$skipCodePulDeploy = $SkipCodePulDeploy
$scdPul = Get-ConfigValueFromMap -FileMap $fileMap -Name "SkipCodePulDeploy"
if (-not $scriptBound.ContainsKey("SkipCodePulDeploy") -and $null -ne $scdPul) { $skipCodePulDeploy = (Test-ConfigBoolTrue $scdPul) }

$skipLoggerIni = $SkipLoggerIni
$sliIni = Get-ConfigValueFromMap -FileMap $fileMap -Name "SkipLoggerIni"
if (-not $scriptBound.ContainsKey("SkipLoggerIni") -and $null -ne $sliIni) { $skipLoggerIni = (Test-ConfigBoolTrue $sliIni) }

if ([string]::IsNullOrWhiteSpace($DolphinExe) -or [string]::IsNullOrWhiteSpace($RiivolutionJson)) {
  throw "DolphinExe and RiivolutionJson are required. Set them in '$pathFile' (see Test-DolphinScenario.paths.example.txt) or pass -DolphinExe and -RiivolutionJson."
}

function Resolve-ExistingFile([string]$p) {
  if (-not (Test-Path -LiteralPath $p -PathType Leaf)) { throw "File not found: $p" }
  return (Get-Item -LiteralPath $p).FullName
}

function Get-AbsolutePath {
  param([string]$Path, [string]$Base)
  if ([string]::IsNullOrWhiteSpace($Path)) { return $null }
  if ([System.IO.Path]::IsPathRooted($Path)) { return [System.IO.Path]::GetFullPath($Path) }
  return [System.IO.Path]::GetFullPath((Join-Path -Path $Base -ChildPath $Path))
}

# Returns full path to Crash.pul only if the file was written/updated on or after $NotBeforeUtc
# (stale files from a previous run are ignored). Mtime uses 1s tolerance for coarse filesystem timestamps.
function Test-CrashPulAtPath {
  param(
    [string]$WatchedPath,
    [DateTime]$NotBeforeUtc
  )
  if ([string]::IsNullOrWhiteSpace($WatchedPath)) { return $null }
  $cand = $null
  if (Test-Path -LiteralPath $WatchedPath -PathType Leaf) { $cand = (Get-Item -LiteralPath $WatchedPath).FullName }
  else {
    $par = Split-Path -Parent $WatchedPath
    $name = [System.IO.Path]::GetFileName($WatchedPath)
    if (-not (Test-Path -LiteralPath $par -PathType Container)) { return $null }
    $hit = Get-ChildItem -LiteralPath $par -File -ErrorAction SilentlyContinue | Where-Object { $_.Name -ieq $name }
    if ($hit) { $cand = $hit[0].FullName }
    if (-not $cand) {
      $hit2 = Get-ChildItem -LiteralPath $par -File -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '^[Cc]rash\.pul$' }
      if ($hit2) { $cand = $hit2[0].FullName }
    }
  }
  if (-not $cand) { return $null }
  $fi = Get-Item -LiteralPath $cand
  $minUtc = $NotBeforeUtc.AddSeconds(-1)
  if ($fi.LastWriteTimeUtc -lt $minUtc) { return $null }
  return $cand
}

function Get-LatestDolphinLogPath([string]$userDir) {
  if ([string]::IsNullOrWhiteSpace($userDir)) { return $null }
  $logDir = Join-Path $userDir "Logs"
  if (-not (Test-Path -LiteralPath $logDir -PathType Container)) { return $null }
  $f = Get-ChildItem -LiteralPath $logDir -File -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending | Select-Object -First 1
  if ($f) { return $f.FullName }
  return $null
}

# Dolphin keeps Logs\dolphin.log open for writing; OpenRead() fails. Use read + share and optional retries.
function New-FileStreamReadShared([string]$LiteralPath) {
  return [System.IO.File]::Open(
    $LiteralPath,
    [System.IO.FileMode]::Open,
    [System.IO.FileAccess]::Read,
    [System.IO.FileShare]::ReadWrite
  )
}

function Read-TextTail {
  param([string]$Path, [int]$MaxBytes = 2097152, [int]$RetryCount = 6, [int]$RetryDelayMs = 300)
  if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $null }
  for ($r = 0; $r -lt $RetryCount; $r++) {
    $fs = $null
    try {
      $fs = New-FileStreamReadShared -LiteralPath $Path
      $len = $fs.Length
      $read = [Math]::Min($MaxBytes, $len)
      if ($read -le 0) { return "" }
      $fs.Position = $len - $read
      $buf = New-Object byte[] $read
      [void]$fs.Read($buf, 0, $read)
      return [System.Text.Encoding]::UTF8.GetString($buf)
    } catch {
      if ($r -ge $RetryCount - 1) { throw }
      Start-Sleep -Milliseconds $RetryDelayMs
    } finally {
      if ($null -ne $fs) { $fs.Dispose() }
    }
  }
  return $null
}

# New bytes in dolphin.log from session start; cap size so a huge log cannot blow memory.
function Read-TextFromOffsetUtf8 {
  param([string]$Path, [long]$StartOffset, [int]$MaxBytes = 4194304, [int]$RetryCount = 6, [int]$RetryDelayMs = 300)
  if ([string]::IsNullOrWhiteSpace($Path) -or -not (Test-Path -LiteralPath $Path -PathType Leaf)) { return $null }
  for ($r = 0; $r -lt $RetryCount; $r++) {
    $fs = $null
    try {
      $fs = New-FileStreamReadShared -LiteralPath $Path
      $tlen = $fs.Length
      if ($tlen -le 0) { return "" }
      if ($StartOffset -ge $tlen) { $StartOffset = 0 }
      $remain = $tlen - $StartOffset
      $read = if ($remain -lt $MaxBytes) { $remain } else { $MaxBytes }
      if ($read -le 0) { return "" }
      $fs.Position = $StartOffset
      $buf = New-Object byte[] $read
      [void]$fs.Read($buf, 0, $read)
      return [System.Text.Encoding]::UTF8.GetString($buf)
    } catch {
      if ($r -ge $RetryCount - 1) { throw }
      Start-Sleep -Milliseconds $RetryDelayMs
    } finally {
      if ($null -ne $fs) { $fs.Dispose() }
    }
  }
  return $null
}

# Upstream format (LogManager::LogWithFullPath): "time path:line Lv[Type]: msg"  e.g. 12:34:567 HLE\HLE_OS.cpp:84 N[OSREPORT_HLE]: text
function Select-OsReportLogLinesFromText {
  param([string]$Raw)
  if ($null -eq $Raw) { return $null }
  if ($Raw -eq "") { return "" }
  $lines = [regex]::Split($Raw, '\r\n|\n|\r')
  # LogManager::LogWithFullPath: "{timestamp} {file}:{line} {LvChar}[{ShortName}]: {msg}" — ShortName is OSREPORT or OSREPORT_HLE
  $pat = '\[OSREPORT(?:_HLE)?\]:|(?i)\bOSREPORT_HLE\b|\bOSREPORT\b|HLE_OS\.cpp|EXI_DeviceIPL\.cpp'
  $out = $lines | Where-Object { $_ -match $pat }
  if ($null -eq $out -or @($out).Count -eq 0) { return $null }
  return ($out -join [Environment]::NewLine)
}

# Dolphin stores Logger settings in User/Config/Logger.ini (Config::System::Logger). WriteToFile defaults to false upstream.
function Read-DolphinLoggerIniOrdered {
  param([string]$LiteralPath)
  $sections = [ordered]@{}
  $cur = $null
  if (-not (Test-Path -LiteralPath $LiteralPath -PathType Leaf)) { return $sections }
  foreach ($raw in Get-Content -LiteralPath $LiteralPath -ErrorAction Stop) {
    $line = $raw.TrimEnd()
    if ($line -match '^\s*\[\s*([^\]]+?)\s*\]\s*$') {
      $cur = $matches[1].Trim()
      if (-not $sections.Contains($cur)) { $sections[$cur] = [ordered]@{} }
      continue
    }
    if ($null -eq $cur) { continue }
    $trim = $line.Trim()
    if ($trim -match '^\s*#' -or $trim -eq '') { continue }
    $eq = $line.IndexOf('=')
    if ($eq -lt 1) { continue }
    $k = $line.Substring(0, $eq).Trim()
    $v = $line.Substring($eq + 1).Trim()
    $sections[$cur][$k] = $v
  }
  return $sections
}

function ConvertTo-DolphinLoggerIniText {
  param($Sections)
  $sb = [System.Text.StringBuilder]::new()
  [void]$sb.AppendLine('# Test-DolphinScenario.ps1: ensures file logging and OS::Report types for crash.txt capture.')
  $priority = @('Options', 'Logs')
  $done = @{}
  foreach ($secName in $priority) {
    if (-not $Sections.Contains($secName)) { continue }
    $done[$secName] = $true
    [void]$sb.AppendLine("[$secName]")
    $tab = $Sections[$secName]
    foreach ($k in $tab.Keys) { [void]$sb.AppendLine("$k = $($tab[$k])") }
    [void]$sb.AppendLine('')
  }
  foreach ($secName in $Sections.Keys) {
    if ($done.ContainsKey($secName)) { continue }
    [void]$sb.AppendLine("[$secName]")
    $tab = $Sections[$secName]
    foreach ($k in $tab.Keys) { [void]$sb.AppendLine("$k = $($tab[$k])") }
    [void]$sb.AppendLine('')
  }
  return $sb.ToString().TrimEnd() + "`r`n"
}

function Merge-DolphinLoggerIniOsReport {
  param([string]$UserDir)
  if ([string]::IsNullOrWhiteSpace($UserDir)) { return 'Logger.ini: skipped (no DolphinUser).' }
  $cfgDir = Join-Path $UserDir "Config"
  $iniPath = Join-Path $cfgDir "Logger.ini"
  if (-not (Test-Path -LiteralPath $cfgDir -PathType Container)) {
    New-Item -ItemType Directory -Path $cfgDir -Force -ErrorAction Stop | Out-Null
  }
  $sections = Read-DolphinLoggerIniOrdered -LiteralPath $iniPath
  if (-not $sections.Contains('Options')) { $sections['Options'] = [ordered]@{} }
  if (-not $sections.Contains('Logs')) { $sections['Logs'] = [ordered]@{} }
  $sections['Options']['WriteToFile'] = 'True'
  $sections['Logs']['OSREPORT'] = 'True'
  $sections['Logs']['OSREPORT_HLE'] = 'True'
  $newText = ConvertTo-DolphinLoggerIniText -Sections $sections
  $oldText = $null
  if (Test-Path -LiteralPath $iniPath -PathType Leaf) {
    try { $oldText = [System.IO.File]::ReadAllText($iniPath) } catch { $oldText = $null }
  }
  if ($oldText -ceq $newText) {
    return "Logger.ini: OK (already had WriteToFile + OSREPORT + OSREPORT_HLE): $iniPath"
  }
  $enc = New-Object System.Text.UTF8Encoding $false
  [System.IO.File]::WriteAllText($iniPath, $newText, $enc)
  return "Logger.ini: updated $iniPath (WriteToFile=True, OSREPORT=True, OSREPORT_HLE=True)."
}

function Get-DolphinLogsDirSnapshot {
  param([string]$UserDir)
  $h = @{}
  if ([string]::IsNullOrWhiteSpace($UserDir)) { return $h }
  $ld = Join-Path $UserDir "Logs"
  if (-not (Test-Path -LiteralPath $ld -PathType Container)) { return $h }
  Get-ChildItem -LiteralPath $ld -File -ErrorAction SilentlyContinue | Where-Object { $_.Extension -ieq '.log' } | ForEach-Object { $h[$_.FullName] = [long]$_.Length }
  $h
}

# Pulsar ExceptionFile (Debug.hpp): big-endian (Wii). v3 on-disk size = 1096 bytes.
function Get-BigEndianUInt32 {
  param([byte[]]$Bytes, [int]$Off)
  if ($null -eq $Bytes -or $Off -lt 0 -or $Off + 3 -ge $Bytes.Length) { return $null }
  # Windows PowerShell 5.1: -shl on raw [byte] is wrong (high shifts zero out); cast each byte to uint32 first.
  $a = [uint32]$Bytes[$Off]
  $b0 = [uint32]$Bytes[$Off + 1]
  $c0 = [uint32]$Bytes[$Off + 2]
  $d0 = [uint32]$Bytes[$Off + 3]
  return [uint32](($a -shl 24) -bor ($b0 -shl 16) -bor ($c0 -shl 8) -bor $d0)
}
function Get-BigEndianInt32 {
  param([byte[]]$Bytes, [int]$Off)
  $u = Get-BigEndianUInt32 -Bytes $Bytes -Off $Off
  if ($null -eq $u) { return $null }
  # Reinterpret 4 bytes as signed int32; avoid [int]$u and ($u-0x100000000) which can overflow in PS.
  return [System.BitConverter]::ToInt32([System.BitConverter]::GetBytes([uint32]$u), 0)
}
function Get-BigEndianDouble {
  param([byte[]]$Bytes, [int]$Off)
  if ($null -eq $Bytes -or $Off -lt 0 -or $Off + 7 -ge $Bytes.Length) { return $null }
  $le = [byte[]]::new(8)
  for ($i = 0; $i -lt 8; $i++) { $le[$i] = $Bytes[$Off + 7 - $i] }
  return [System.BitConverter]::ToDouble($le, 0)
}
function Get-BigEndianDoubleAsHex {
  param([byte[]]$Bytes, [int]$Off)
  if ($null -eq $Bytes -or $Off -lt 0 -or $Off + 7 -ge $Bytes.Length) { return $null }
  $h = [System.Text.StringBuilder]::new(16)
  for ($i = 0; $i -lt 8; $i++) { [void]$h.Append(('{0:X2}' -f $Bytes[$Off + $i])) }
  return $h.ToString()
}
# Human-readable double for crash dumps (NaN / Inf instead of long scientific for edge cases)
function Format-CrashPulFloat {
  param([object]$D)
  if ($null -eq $D) { return "n/a" }
  $d = [double]$D
  if ([double]::IsNaN($d)) { return "NaN" }
  if ([double]::IsPositiveInfinity($d)) { return "+Inf" }
  if ([double]::IsNegativeInfinity($d)) { return "-Inf" }
  return $d.ToString("G9", [System.Globalization.CultureInfo]::InvariantCulture)
}
function Get-CrashPulParsedText {
  param([string]$LiteralPath)
  if ([string]::IsNullOrWhiteSpace($LiteralPath) -or -not (Test-Path -LiteralPath $LiteralPath -PathType Leaf)) { return $null }
  try {
    $b = [System.IO.File]::ReadAllBytes($LiteralPath)
  } catch { return "Could not read file: $($_.Exception.Message)" }
  if ($b.Length -lt 24) { return "File too small to be a Pulsar exception dump ($($b.Length) bytes)." }
  $magic = Get-BigEndianUInt32 -Bytes $b -Off 0
  # On-disk is big-endian: read ASCII from file bytes 0..3 (do not use BitConverter — it is little-endian on Windows)
  $magicStr = if ($b.Length -ge 4) { [System.Text.Encoding]::ASCII.GetString($b, 0, 4) } else { '????' }
  if ($magic -ne 0x50554C44) { return "Unrecognized magic (0x$('{0:X8}' -f $magic) / $magicStr); expected PULD (0x50554C44) at offset 0. Size=$($b.Length) bytes." }
  $ver = Get-BigEndianUInt32 -Bytes $b -Off 8
  if ($b.Length -ne 1096) {
    return "Unexpected Crash.pul size: $($b.Length) bytes (expected 1096 for Pulsar exception file v3 / EXCEPTION_FILE_VERSION=3). Magic=$magicStr, version@8=$ver - layout may not match; dump not parsed."
  }
  if ($ver -ne 3) { return "Unexpected exception file version: $ver (this parser expects 3). Size=1096." }
  # Do not use '0x{0:X8}' -f (if ...) here: inside a scriptblock, PowerShell misparses and treats `if` as a command at invoke.
  $regStr = { param([int]$o) $t = (Get-BigEndianUInt32 -Bytes $b -Off $o); if ($null -eq $t) { return "0x00000000" }; return "0x$(([uint32]$t).ToString('X8'))" }
  $osErrU = Get-BigEndianUInt32 -Bytes $b -Off 12
  $osErrName = switch ($osErrU) {
    2 { "OSERROR_DSI (2)"; break }
    3 { "OSERROR_ISI (3)"; break }
    7 { "OSERROR_FLOATING_POINT (7)"; break }
    8 { "OSERROR_FPE (8)"; break }
    default { "unknown ($osErrU)"; break }
  }
  $regionU = Get-BigEndianUInt32 -Bytes $b -Off 4
  $rbytes = [byte[]]($b[4], $b[5], $b[6], $b[7])
  $regId = -join ( $rbytes | ForEach-Object { if ($_ -ge 32 -and $_ -le 126) { [char]$_ } else { '?' } } )
  $srr0 = & $regStr 20
  $srr1 = & $regStr 28
  $msr = & $regStr 36
  $crGpr = & $regStr 44
  $lr = & $regStr 52
  # gprs[1] base 64, gpr u32 at +4
  $r1 = & $regStr 68
  $exBase = 1000
  $eVer = Get-BigEndianUInt32 -Bytes $b -Off $exBase
  $eSec = Get-BigEndianInt32 -Bytes $b -Off ($exBase + 4)
  $ePage = Get-BigEndianInt32 -Bytes $b -Off ($exBase + 8)
  $eCtx = Get-BigEndianUInt32 -Bytes $b -Off ($exBase + 12)
  $eCtx2 = Get-BigEndianUInt32 -Bytes $b -Off ($exBase + 16)
  $eFl = Get-BigEndianUInt32 -Bytes $b -Off ($exBase + 20)
  $eLoose = Get-BigEndianUInt32 -Bytes $b -Off ($exBase + 24)
  $eMystuff = Get-BigEndianUInt32 -Bytes $b -Off ($exBase + 28)
  # lastTrackSzs[64]: often snprintf + NUL, but the tail is sometimes uninitialized; range-slice can break IndexOf(0). Stop at first NUL, else first non-printable byte.
  $tOff = $exBase + 32
  $tSb = [System.Text.StringBuilder]::new()
  for ($ti = 0; $ti -lt 64; $ti++) {
    $bb = $b[$tOff + $ti]
    if ($bb -eq 0) { break }
    if ($bb -lt 0x20 -or $bb -gt 0x7E) { break }
    [void]$tSb.Append([char]$bb)
  }
  $lastTrack = if ($tSb.Length -gt 0) { $tSb.ToString() } else { "(empty)" }
  $fl = @()
  if ($eFl -band 1) { $fl += "LOOSE_ARCHIVE_OVERRIDES" }
  if ($eFl -band 2) { $fl += "CUSTOM_CHARACTER" }
  if ($fl.Count -eq 0) { $flTxt = "none" } else { $flTxt = $fl -join ", " }
  $msTxt = switch ($eMystuff) { 0 { "disabled" } 1 { "enabled" } 2 { "music_only" } default { "($eMystuff)" } }
  $st = [System.Text.StringBuilder]::new()
  $rule = "--------------------------------------------------------------------------------"
  [void]$st.AppendLine("Pulsar ExceptionFile (Debug.hpp)  |  v$ver  |  1096 bytes  |  big-endian (Wii)")
  [void]$st.AppendLine($rule)
  [void]$st.AppendLine("  Game / disc id (4CC)  $regId   (0x$('{0:X8}' -f $regionU))   magic: $magicStr")
  [void]$st.AppendLine("  Exception (OS)        $osErrName")
  [void]$st.AppendLine("  PC (srr0)            $srr0")
  [void]$st.AppendLine("  LR                    $lr")
  [void]$st.AppendLine("  r1 (stack pointer)   $r1")
  [void]$st.AppendLine("  srr1                  $srr1   msr  $msr   cr  $crGpr")
  [void]$st.AppendLine("")
  [void]$st.AppendLine("  CrashExtra")
  [void]$st.AppendLine("    struct version   $eVer   sectionId  $eSec   pageId  $ePage")
  [void]$st.AppendLine("    system context   0x$('{0:X8}' -f $eCtx)   context2  0x$('{0:X8}' -f $eCtx2)")
  [void]$st.AppendLine("    flags            0x$('{0:X8}' -f $eFl)  ($flTxt)")
  [void]$st.AppendLine("    looseOverrideFiles  $eLoose   myStuff  $msTxt   last track .szs  $lastTrack")
  [void]$st.AppendLine("")
  [void]$st.AppendLine("  GPRs (integers r0-31)   u32 in memory order")
  [void]$st.AppendLine("  " + $rule)
  for ($gr = 0; $gr -lt 8; $gr++) {
    $c = @()
    for ($k = 0; $k -lt 4; $k++) {
      $ri = 4 * $gr + $k
      $cell = ("r{0,2}  {1}" -f $ri, (& $regStr (60 + 8 * $ri)))
      $c += ,$cell.PadRight(22)
    }
    [void]$st.AppendLine( ("  {0} {1} {2} {3}" -f $c[0], $c[1], $c[2], $c[3]) )
  }
  $fprBase = 312
  $fprSize = 16
  [void]$st.AppendLine("")
  [void]$st.AppendLine("  FPRs (doubles f0-31)   64-bit hex = big-endian on disk;  value: finite / NaN / ±Inf as decoded")
  [void]$st.AppendLine("  " + $rule)
  for ($fr = 0; $fr -lt 16; $fr++) {
    $fi0 = 2 * $fr
    $fi1 = 2 * $fr + 1
    $dOff0 = $fprBase + ($fi0 * $fprSize) + 8
    $dOff1 = $fprBase + ($fi1 * $fprSize) + 8
    $h0 = Get-BigEndianDoubleAsHex -Bytes $b -Off $dOff0
    $h1 = Get-BigEndianDoubleAsHex -Bytes $b -Off $dOff1
    if ($null -eq $h0) { $h0 = "????????????????" }
    if ($null -eq $h1) { $h1 = "????????????????" }
    $v0 = Get-BigEndianDouble -Bytes $b -Off $dOff0
    $v1 = Get-BigEndianDouble -Bytes $b -Off $dOff1
    $f0s = Format-CrashPulFloat -D $v0
    $f1s = Format-CrashPulFloat -D $v1
    $left = ("f{0,2}  0x{1}  {2,-12}" -f $fi0, $h0, $f0s)
    $right = ("f{0,2}  0x{1}  {2,-12}" -f $fi1, $h1, $f1s)
    [void]$st.AppendLine( ("  {0}     |  {1}" -f $left.PadRight(48), $right) )
  }
  $fpsOff = 824
  $fpscHex = Get-BigEndianDoubleAsHex -Bytes $b -Off ($fpsOff + 8)
  if ($null -eq $fpscHex) { $fpscHex = "????????????????" }
  $fpscVal = Get-BigEndianDouble -Bytes $b -Off ($fpsOff + 8)
  $fpscF = Format-CrashPulFloat -D $fpscVal
  [void]$st.AppendLine("")
  [void]$st.AppendLine("  FPSCR  (field stored in a double slot)   raw: 0x$fpscHex   decoded:  $fpscF")
  $frameBase = 840
  [void]$st.AppendLine("")
  [void]$st.AppendLine("  Stack walk  StackFrame[0..9]  (unwind chain; sp/lr)")
  [void]$st.AppendLine("  " + $rule)
  $hdr = ("  {0,-4}  {1,-12}  {2,-12}" -f "frm", "SP (chain)", "LR (return)")
  $sep = ("  {0,-4}  {1,-12}  {2,-12}" -f "----", "------------", "------------")
  [void]$st.AppendLine($hdr)
  [void]$st.AppendLine($sep)
  for ($fj = 0; $fj -lt 10; $fj++) {
    $fbo = $frameBase + 16 * $fj
    $fspJ = (Get-BigEndianUInt32 -Bytes $b -Off ($fbo + 4))
    $flrJ = (Get-BigEndianUInt32 -Bytes $b -Off ($fbo + 12))
    $fspS = if ($null -ne $fspJ) { "0x$(([uint32]$fspJ).ToString('X8'))" } else { "n/a" }
    $flrS = if ($null -ne $flrJ) { "0x$(([uint32]$flrJ).ToString('X8'))" } else { "n/a" }
    $lineF = ("  {0,-4}  {1,-12}  {2,-12}" -f $fj, $fspS, $flrS)
    [void]$st.AppendLine($lineF)
  }
  return $st.ToString()
}

function Get-DolphinLogNewTextMerged {
  param([hashtable]$SnapshotBefore, [string]$UserDir)
  if ([string]::IsNullOrWhiteSpace($UserDir)) { return $null, $null }
  $ld = Join-Path $UserDir "Logs"
  if (-not (Test-Path -LiteralPath $ld -PathType Container)) { return $null, $null }
  $candidates = [System.Collections.Generic.List[string]]::new()
  Get-ChildItem -LiteralPath $ld -File -ErrorAction SilentlyContinue | Where-Object { $_.Extension -ieq '.log' } | ForEach-Object { $candidates.Add($_.FullName) }
  if ($candidates.Count -eq 0) { return $null, $null }
  $sb = [System.Text.StringBuilder]::new()
  $any = $false
  foreach ($p in ($candidates | Sort-Object)) {
    $off = 0L
    if ($null -ne $SnapshotBefore -and $SnapshotBefore.ContainsKey($p)) { $off = [long]$SnapshotBefore[$p] }
    $chunk = $null
    try { $chunk = Read-TextFromOffsetUtf8 -Path $p -StartOffset $off } catch { $chunk = $null }
    if ($null -eq $chunk) { continue }
    if ($chunk -eq [string]::Empty) { continue }
    $any = $true
    [void]$sb.AppendLine("<<< $p (new bytes from offset $off) >>>")
    [void]$sb.AppendLine($chunk)
    [void]$sb.AppendLine("")
  }
  if (-not $any) { return $null, $null }
  return $sb.ToString(), ($candidates -join [Environment]::NewLine)
}

$DolphinExe = Resolve-ExistingFile $DolphinExe
$RiivolutionJson = Resolve-ExistingFile $RiivolutionJson
if (-not $OutFile) { $OutFile = Join-Path $RepoRoot "crash.txt" }
$OutFile = [System.IO.Path]::GetFullPath($OutFile)
if (Test-Path -LiteralPath $OutFile -PathType Leaf) { Remove-Item -LiteralPath $OutFile -Force -ErrorAction Stop }

if ($DolphinUser) {
  if (-not (Test-Path -LiteralPath $DolphinUser -PathType Container)) { throw "Dolphin user directory not found: $DolphinUser" }
  $DolphinUser = (Resolve-Path -LiteralPath $DolphinUser).Path
}

# --- Build ---
$makeText = [System.Text.StringBuilder]::new()
$makeExit = 0
if (-not $SkipBuild) {
  Push-Location -LiteralPath $RepoRoot
  try {
    & make install -j8 2>&1 | ForEach-Object { [void]$makeText.AppendLine([string]$_) }
    $makeExit = $LASTEXITCODE
    if ($null -eq $makeExit) { $makeExit = 0 }
  } finally {
    Pop-Location
  }
} else {
  [void]$makeText.AppendLine("(make skipped)")
}

if ($makeExit -ne 0 -and -not $ContinueOnBuildFailure) {
  $errBody = "make install -j8 failed (exit $makeExit).`r`n`r`n" + $makeText.ToString()
  Add-Content -LiteralPath $OutFile -Value "==== make failure $(Get-Date -Format o) ==== `r`n$errBody`r`n" -Encoding utf8
  throw $errBody
}

# --- Deploy Code.pul to this Dolphin's Riivolution Binaries (same --user the test launches) ---
$codePulDeployLog = $null
$codePulSrc = Join-Path $RepoRoot (Join-Path "build" "Code.pul")
if ($skipCodePulDeploy) {
  $codePulDeployLog = "Code.pul deploy: skipped (SkipCodePulDeploy)."
} elseif (-not (Test-Path -LiteralPath $codePulSrc -PathType Leaf)) {
  $codePulDeployLog = "Code.pul deploy: skipped (no $codePulSrc); build first."
} elseif ([string]::IsNullOrWhiteSpace($DolphinUser) -or [string]::IsNullOrWhiteSpace($ModFolderName)) {
  $codePulDeployLog = "Code.pul deploy: skipped (set DolphinUser and ModFolderName in paths to deploy to User\Load\Riivolution\<mod>\Binaries\Code.pul)."
} else {
  $dUser = $DolphinUser.TrimEnd([char]'\', [char]'/')
  $riivoBin = [System.IO.Path]::GetFullPath((Join-Path $dUser (Join-Path "Load" (Join-Path "Riivolution" (Join-Path $ModFolderName "Binaries")))))
  $destPul = Join-Path $riivoBin "Code.pul"
  try {
    if (-not (Test-Path -LiteralPath $riivoBin -PathType Container)) { New-Item -ItemType Directory -Path $riivoBin -Force -ErrorAction Stop | Out-Null }
    Copy-Item -LiteralPath $codePulSrc -Destination $destPul -Force -ErrorAction Stop
    $codePulDeployLog = "Code.pul deploy: OK -> $destPul"
  } catch {
    $codePulDeployLog = "Code.pul deploy: FAILED -> $destPul : $($_.Exception.Message)"
  }
}

# --- Resolve Crash.pul watch path ---
$watchCrashPul = (Get-AbsolutePath -Path $CrashPulPath -Base $RepoRoot)
if ([string]::IsNullOrWhiteSpace($watchCrashPul) -and $DolphinUser -and -not [string]::IsNullOrWhiteSpace($ModFolderName)) {
  $d = $DolphinUser.TrimEnd('\', '/')
  $watchCrashPul = Join-Path $d (Join-Path "Wii" (Join-Path "shared2" (Join-Path "Pulsar" (Join-Path $ModFolderName "Crash.pul"))))
}

# --- Launch Dolphin: pass JSON (and optional --user) as documented for Riivolution CLI in Dolphin 5+ ---
# Use separate argv entries for --user and paths so values with spaces are passed correctly.
$argList = [System.Collections.Generic.List[string]]::new()
if ($DolphinUser) {
  $argList.Add('--user')
  $argList.Add($DolphinUser)
}
$argList.Add($RiivolutionJson)
$argLine = ($argList | ForEach-Object {
  $a = $_
  if ($a -match '\s') { '"' + ($a -replace '"', '""') + '"' } else { $a }
}) -join ' '

$loggerIniLog = $null
if ($DolphinUser -and -not $skipLoggerIni) {
  try { $loggerIniLog = Merge-DolphinLoggerIniOsReport -UserDir $DolphinUser }
  catch { $loggerIniLog = "Logger.ini merge FAILED: $($_.Exception.Message)" }
} elseif ($skipLoggerIni) {
  $loggerIniLog = 'Logger.ini: skipped (SkipLoggerIni). Enable Write to file + OSREPORT types manually if needed.'
} else {
  $loggerIniLog = 'Logger.ini: skipped (no DolphinUser).'
}

# All stock Dolphin log types (including OS::Report) go to User\Logs\dolphin.log; capture every *.log size before start.
$dolphinLogSnap = @{}
if ($DolphinUser) { $dolphinLogSnap = Get-DolphinLogsDirSnapshot -UserDir $DolphinUser }

$proc = Start-Process -FilePath $DolphinExe -ArgumentList $argList -PassThru
if ($null -eq $proc) { throw "Failed to start Dolphin." }
# Only count Crash.pul that is written/updated at or after this time (excludes previous-run leftovers).
$dolphinSessionStartUtc = [DateTime]::UtcNow

$reason = "unknown"
$crashPulHost = $null
try {
  while ($true) {
    $found = Test-CrashPulAtPath -WatchedPath $watchCrashPul -NotBeforeUtc $dolphinSessionStartUtc
    if ($found) {
      $reason = "crash_pul"
      $crashPulHost = $found
      Start-Sleep -Milliseconds 200
      break
    }
    if ($proc.HasExited) {
      $reason = "dolphin_exited"
      break
    }
    try {
      $p = [System.Diagnostics.Process]::GetProcessById($proc.Id)
      $null = $p
    } catch {
      $reason = "dolphin_exited"
      break
    }
    Start-Sleep -Milliseconds $PollMs
  }
} finally {
  if ($reason -eq "unknown" -and $proc.HasExited) { $reason = "dolphin_exited" }
  try {
    if ($null -ne $proc) { Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue }
  } catch { }
  try {
    if ($null -ne $proc) { $null = $proc.WaitForExit(15000) }
  } catch { }
  try { if ($null -ne $proc) { $proc.Refresh() } } catch { }
}

# --- Log snapshot: merge new bytes from all User\Logs\*.log (upstream: single dolphin.log) + full tail of newest log ---
$logAfterPath = if ($DolphinUser) { Get-LatestDolphinLogPath $DolphinUser } else { $null }
$logText = $null
if ($logAfterPath) {
  try { $logText = Read-TextTail -Path $logAfterPath } catch { $logText = $null }
}

$osReportRaw = $null
$osReportText = $null
$osReportSource = $null
$osReportReadError = $null
$osReportMergedList = $null
if ($DolphinUser) {
  try {
    $merged, $osReportMergedList = Get-DolphinLogNewTextMerged -SnapshotBefore $dolphinLogSnap -UserDir $DolphinUser
    $osReportRaw = $merged
    if ($osReportRaw) { $osReportText = Select-OsReportLogLinesFromText -Raw $osReportRaw; $osReportSource = 'merged new bytes from Logs\*.log (see file list; upstream Dolphin uses one dolphin.log for all log types including OS::Report)' }
  } catch { $osReportReadError = $_.Exception.Message }
}
if ($null -eq $osReportText) {
  try {
    if ([string]::IsNullOrWhiteSpace($logText) -and $logAfterPath) { $logText = Read-TextTail -Path $logAfterPath }
    if (-not [string]::IsNullOrWhiteSpace($logText)) {
      $x = Select-OsReportLogLinesFromText -Raw $logText
      if ($null -ne $x) { $osReportText = $x; if (-not $osReportSource) { $osReportSource = "filtered tail: $logAfterPath" } }
    }
  } catch { if (-not $osReportReadError) { $osReportReadError = $_.Exception.Message } }
}

# --- Assemble report ---
$sb = [System.Text.StringBuilder]::new()
$ts = Get-Date -Format o
[void]$sb.AppendLine("==== Dolphin scenario run $ts ====")
[void]$sb.AppendLine("Paths config: $pathFile (exists: $(Test-Path -LiteralPath $pathFile -PathType Leaf))")
[void]$sb.AppendLine("Reason: $reason")
[void]$sb.AppendLine("RepoRoot: $RepoRoot")
[void]$sb.AppendLine("make exit: $makeExit (0 = success if make ran)")
[void]$sb.AppendLine("Dolphin: $DolphinExe")
[void]$sb.AppendLine("Code.pul: $(if ($codePulDeployLog) { $codePulDeployLog } else { '(deploy log n/a)' })")
[void]$sb.AppendLine("Logger.ini: $(if ($loggerIniLog) { $loggerIniLog } else { '(n/a)' })")
[void]$sb.AppendLine("Argument list: " + $argLine)
[void]$sb.AppendLine("Dolphin process Id: $($proc.Id)")
[void]$sb.AppendLine("Dolphin session start (UTC, Crash.pul mtime must be >= this minus 1s): $dolphinSessionStartUtc")
$ex = $null
if ($proc.HasExited) {
  try { $ex = $proc.ExitCode } catch { $ex = "n/a" }
} else { $ex = "ended by test (forced close)" }
[void]$sb.AppendLine("Dolphin exit code: $ex")
[void]$sb.AppendLine("Watched Crash.pul path: $(if ($watchCrashPul) { $watchCrashPul } else { '(not configured)' })")
[void]$sb.AppendLine("Crash.pul detected: $(if ($crashPulHost) { 'YES' } else { 'NO' })")
if ($crashPulHost) {
  [void]$sb.AppendLine("Crash.pul full path: $crashPulHost")
  try {
    $fi = Get-Item -LiteralPath $crashPulHost
    [void]$sb.AppendLine("Crash.pul size bytes: $($fi.Length) last write UTC: $($fi.LastWriteTimeUtc) (must be on/after session start)")
  } catch {}
}
[void]$sb.AppendLine("")
[void]$sb.AppendLine("---- make output ----")
[void]$sb.AppendLine($makeText.ToString())
[void]$sb.AppendLine("---- OS::Report (Dolphin: log types OSREPORT / OSREPORT_HLE, same file as all logs) ----")
[void]$sb.AppendLine('Dolphin writes these lines to User/Logs/dolphin.log (and other *.log) when file logging is on. This script merges User/Config/Logger.ini before launch unless you use -SkipLoggerIni (see Logger.ini line above). Manual alternative: open Log Configuration from the View menu and enable Write to file plus OSREPORT and OSREPORT_HLE.')
if ($osReportMergedList) { [void]$sb.AppendLine("Log files merged for this session: $osReportMergedList") }
$osSourceNote = if ($osReportSource) { $osReportSource } elseif ($DolphinUser) { "(no new bytes in Logs, or no matching lines; enable settings above; lines must contain [OSREPORT] or [OSREPORT_HLE] in the log text)" } else { "(Dolphin user folder unknown; set DolphinUser in paths file)" }
[void]$sb.AppendLine("Source: $osSourceNote")
if ($osReportReadError) { [void]$sb.AppendLine("Log read error: $osReportReadError") }
[void]$sb.AppendLine($(if ($null -ne $osReportText) { $osReportText } else { '(no OSREPORT / OSREPORT_HLE lines in new log data this session)' }))
[void]$sb.AppendLine("---- latest Dolphin user log tail: $(if ($logAfterPath) { $logAfterPath } else { '(none; pass -DolphinUser to point at your portable/user folder or standard Documents\Dolphin Emulator)' }) ----")
[void]$sb.AppendLine($(if ($null -ne $logText) { $logText } else { '' }))

$crashPulToParse = $null
if ($crashPulHost) { $crashPulToParse = $crashPulHost } elseif ($watchCrashPul -and (Test-Path -LiteralPath $watchCrashPul -PathType Leaf)) { $crashPulToParse = $watchCrashPul }
$crashPulParseTxt = $null
if ($null -ne $crashPulToParse) { $crashPulParseTxt = Get-CrashPulParsedText -LiteralPath $crashPulToParse }
[void]$sb.AppendLine("---- Crash.pul (parsed; see header inside block for layout) ----")
[void]$sb.AppendLine($(if ($null -ne $crashPulParseTxt) { $crashPulParseTxt } elseif ($null -ne $watchCrashPul) { "(no Crash.pul at: $watchCrashPul or not this session)" } else { "(Crash.pul path not configured; set CrashPulPath or ModFolderName+DolphinUser)" }))

[void]$sb.AppendLine("---- end ----")
[void]$sb.AppendLine("")

Add-Content -LiteralPath $OutFile -Value $sb.ToString() -Encoding utf8
Write-Output "Wrote report to: $OutFile (reason: $reason)"
if ($makeExit -ne 0) { exit $makeExit }
if ($reason -eq "crash_pul") { exit 2 }
exit 0
