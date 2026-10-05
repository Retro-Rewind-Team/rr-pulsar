using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Text.RegularExpressions;
using Pulsar_Pack_Creator.IO;

namespace Pulsar_Pack_Creator.Languages
{
    public sealed class LanguagePackBuilder
    {
        private static readonly Regex BmgColorCodeRegex = new Regex(@"\\c\{[^}]+\}", RegexOptions.Compiled);
        private static readonly Regex WhitespaceRegex = new Regex(@"\s+", RegexOptions.Compiled);
        private static readonly Regex LeadingColoredPrefixRegex = new Regex(@"^(?<prefix>(?:\\c\{[^}]+\}.*?\\c\{off\}\s*)+)(?<name>.*)$", RegexOptions.Compiled);
        private static readonly Regex TrailingColoredTextRegex = new Regex(@"^(.*?)(\s+)(\\c\{[^}]+\})(.*?)(\\c\{off\})\s*$", RegexOptions.Compiled);

        public async Task BuildAsync(string packRoot, IReadOnlyDictionary<TranslationTarget, TranslationTable> sheets, GameImageSheet gameImages, TrackNameTranslationSheets trackNameSheets, IEnumerable<LanguageDefinition> languages, IProgress<string> progress, CancellationToken cancellationToken)
        {
            ValidatePackRoot(packRoot);
            LanguageDefinition[] languageList = languages.ToArray();
            string tempRoot = Path.Combine(Path.GetTempPath(), "PulsarLanguageBuilder", Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(tempRoot);
            try
            {
                await WriteToolsAsync(tempRoot);
                foreach (LanguageDefinition language in languageList)
                {
                    cancellationToken.ThrowIfCancellationRequested();
                    progress?.Report($"Building {language.DisplayName}...");
                    foreach (IGrouping<string, TranslationTarget> group in TranslationTarget.Required.GroupBy(target => target.ArchiveName))
                        await BuildArchiveAsync(packRoot, language, group.Key, group, sheets, tempRoot, progress, cancellationToken);
                    await BuildRaceUiAsync(packRoot, language, gameImages, tempRoot, progress, cancellationToken);
                }
                await BuildTrackNameConfigsAsync(packRoot, trackNameSheets, languageList, tempRoot, progress, cancellationToken);
            }
            finally
            {
                try { if (Directory.Exists(tempRoot)) Directory.Delete(tempRoot, true); } catch { }
            }
        }

        private async Task BuildTrackNameConfigsAsync(string packRoot, TrackNameTranslationSheets sheets, IReadOnlyList<LanguageDefinition> languages, string tempRoot, IProgress<string> progress, CancellationToken cancellationToken)
        {
            LanguageDefinition[] translatedLanguages = languages.Where(language => !language.IsEnglish).ToArray();
            if (translatedLanguages.Length == 0) return;

            string binariesDir = Path.Combine(packRoot, "Binaries");
            string[] configFiles = Directory.Exists(binariesDir)
                ? Directory.EnumerateFiles(binariesDir, "Config*.pul", SearchOption.TopDirectoryOnly).OrderBy(path => path, StringComparer.OrdinalIgnoreCase).ToArray()
                : Array.Empty<string>();
            if (configFiles.Length == 0)
            {
                progress?.Report("  No Binaries/Config*.pul files found; track and variant name translation skipped.");
                return;
            }

            foreach (string configPath in configFiles)
            {
                cancellationToken.ThrowIfCancellationRequested();
                await TranslateConfigNamesAsync(configPath, sheets, translatedLanguages, tempRoot, progress, cancellationToken);
            }
        }

        private async Task TranslateConfigNamesAsync(string configPath, TrackNameTranslationSheets sheets, IReadOnlyList<LanguageDefinition> languages, string tempRoot, IProgress<string> progress, CancellationToken cancellationToken)
        {
            byte[] config = await File.ReadAllBytesAsync(configPath, cancellationToken);
            if (config.Length < 0x20) throw new InvalidOperationException($"{Path.GetFileName(configPath)} is too small to be a valid Config.pul.");

            PulsarGame.BinaryHeader header = PulsarGame.BytesToStruct<PulsarGame.BinaryHeader>((byte[])config.Clone());
            if (header.magic != 0x50554C53) throw new InvalidOperationException($"{Path.GetFileName(configPath)} is not a valid Pulsar config.");
            int bmgOffset = header.offsetToBMG;
            if (bmgOffset < 0 || bmgOffset + 12 > config.Length) throw new InvalidOperationException($"{Path.GetFileName(configPath)} has an invalid BMG offset.");

            int oldBmgLength = checked((int)ReadBigEndianUInt32(config, bmgOffset + 8));
            if (oldBmgLength < 0x20 || bmgOffset + oldBmgLength > config.Length) throw new InvalidOperationException($"{Path.GetFileName(configPath)} has an invalid BMG size.");

            string workDir = Path.Combine(tempRoot, "ConfigNames", Path.GetFileNameWithoutExtension(configPath));
            Directory.CreateDirectory(workDir);
            string bmgPath = Path.Combine(workDir, "bmg.bmg");
            byte[] oldBmg = new byte[oldBmgLength];
            Buffer.BlockCopy(config, bmgOffset, oldBmg, 0, oldBmgLength);
            await File.WriteAllBytesAsync(bmgPath, oldBmg, cancellationToken);
            await RunToolAsync(Path.Combine(tempRoot, "wbmgt.exe"), "decode \"bmg.bmg\" --no-header --export", workDir, cancellationToken);

            string textPath = Path.Combine(workDir, "BMG.txt");
            if (!File.Exists(textPath)) throw new InvalidOperationException($"wbmgt did not decode the BMG in {Path.GetFileName(configPath)}.");
            string[] lines = await File.ReadAllLinesAsync(textPath, cancellationToken);
            var englishNames = new List<(uint Id, string Text, bool IsVariant, bool UseVariantSheet)>();
            for (int i = 0; i < lines.Length; ++i)
            {
                if (!IOBase.TryParseBMGLine(lines[i], out uint id, out string text)) continue;
                if (IsEnglishTrackNameId(id)) englishNames.Add((id, text, false, false));
                else if (IsEnglishVariantNameId(id)) englishNames.Add((id, text, true, ((id - 0x420000u) & 0xFu) != 0));
            }

            var patchLines = new List<string> { "#BMG", string.Empty };

            foreach (LanguageDefinition language in languages)
            {
                IReadOnlyList<NameTranslation> trackNames = sheets.Tracks.GetNameTranslations(language);
                IReadOnlyList<NameTranslation> variantNames = sheets.Variants.GetNameTranslations(language);
                int translated = 0;
                int fallback = 0;
                var unmatched = new List<string>();

                foreach ((uint Id, string Text, bool IsVariant, bool UseVariantSheet) source in englishNames)
                {
                    IReadOnlyList<NameTranslation> lookup = source.UseVariantSheet ? variantNames : trackNames;
                    bool matched = TryGetNameTranslation(lookup, source.Text, out NameTranslation translation);
                    string translatedText;
                    if (!matched)
                    {
                        translatedText = source.Text;
                        ++fallback;
                        if (unmatched.Count < 6) unmatched.Add(GetDisplayLookupName(source.Text));
                    }
                    else
                    {
                        translatedText = PreserveNameFormatting(source.Text, translation.Text, translation.Prefix, source.IsVariant);
                        ++translated;
                    }

                    uint targetId = source.Id + language.TrackBmgOffset;
                    patchLines.Add($"{targetId:X} = {EscapeBmgText(translatedText)}");
                }

                string fallbackText = fallback == 0 ? string.Empty : $"; {fallback} unmatched -> English ({string.Join(", ", unmatched)}{(fallback > unmatched.Count ? ", ..." : string.Empty)})";
                progress?.Report($"  {Path.GetFileName(configPath)} / {language.DisplayName}: {translated} names matched{fallbackText}");
            }

            string patchPath = Path.Combine(workDir, "translations.txt");
            await File.WriteAllLinesAsync(patchPath, patchLines, new UTF8Encoding(false), cancellationToken);
            string translatedBmgPath = Path.Combine(workDir, "translated.bmg");
            await RunToolAsync(Path.Combine(tempRoot, "wbmgt.exe"), "patch \"bmg.bmg\" --patch-bmg=overwrite=\"translations.txt\" --dest \"translated.bmg\" -o", workDir, cancellationToken);
            byte[] translatedBmg = await File.ReadAllBytesAsync(translatedBmgPath, cancellationToken);
            ValidateBmg(translatedBmg, configPath);

            int suffixOffset = bmgOffset + oldBmgLength;
            byte[] output = new byte[bmgOffset + translatedBmg.Length + (config.Length - suffixOffset)];
            Buffer.BlockCopy(config, 0, output, 0, bmgOffset);
            Buffer.BlockCopy(translatedBmg, 0, output, bmgOffset, translatedBmg.Length);
            Buffer.BlockCopy(config, suffixOffset, output, bmgOffset + translatedBmg.Length, config.Length - suffixOffset);
            ValidateConfig(output, header, translatedBmg.Length, configPath);

            string outputPath = configPath + ".languagebuilder.tmp";
            await File.WriteAllBytesAsync(outputPath, output, cancellationToken);
            File.Move(outputPath, configPath, true);
            progress?.Report($"  Wrote translated track/variant BMGs to {configPath}");
        }

        private static void ValidateBmg(byte[] bmg, string configPath)
        {
            if (bmg.Length < 0x20 || ReadBigEndianUInt64(bmg, 0) != 0x4D455347626D6731UL)
                throw new InvalidOperationException($"wbmgt produced an invalid BMG for {Path.GetFileName(configPath)}.");

            uint declaredLength = ReadBigEndianUInt32(bmg, 8);
            if (declaredLength != bmg.Length)
                throw new InvalidOperationException($"wbmgt produced a malformed BMG for {Path.GetFileName(configPath)}.");
        }

        private static void ValidateConfig(byte[] config, PulsarGame.BinaryHeader originalHeader, int bmgLength, string configPath)
        {
            PulsarGame.BinaryHeader header = PulsarGame.BytesToStruct<PulsarGame.BinaryHeader>((byte[])config.Clone());
            if (header.magic != 0x50554C53 || header.version != originalHeader.version ||
                header.offsetToInfo != originalHeader.offsetToInfo || header.offsetToCups != originalHeader.offsetToCups ||
                header.offsetToBMG != originalHeader.offsetToBMG)
                throw new InvalidOperationException($"Translation changed the Config.pul header for {Path.GetFileName(configPath)}.");

            int fileOffset = header.offsetToBMG + bmgLength;
            if (fileOffset + 4 > config.Length || ReadBigEndianUInt32(config, fileOffset) != 0x46494C45)
                throw new InvalidOperationException($"Translation produced an invalid FILE section for {Path.GetFileName(configPath)}.");
        }

        public async Task UpdateConfigTranslationsAsync(string packRoot, TrackNameTranslationSheets trackNameSheets, IEnumerable<LanguageDefinition> languages, IProgress<string> progress, CancellationToken cancellationToken)
        {
            ValidateConfigPackRoot(packRoot);
            string tempRoot = Path.Combine(Path.GetTempPath(), "PulsarLanguageBuilder", Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(tempRoot);
            try
            {
                await WriteToolsAsync(tempRoot);
                await BuildTrackNameConfigsAsync(packRoot, trackNameSheets, languages.ToArray(), tempRoot, progress, cancellationToken);
            }
            finally
            {
                try { Directory.Delete(tempRoot, true); } catch { }
            }
        }

        private static bool IsEnglishTrackNameId(uint id) => id >= 0x20000 && id < 0x21000;

        private static bool IsEnglishVariantNameId(uint id) => id >= 0x420000 && id < 0x421000;

        private static uint ReadBigEndianUInt32(byte[] data, int offset)
        {
            return ((uint)data[offset] << 24) | ((uint)data[offset + 1] << 16) | ((uint)data[offset + 2] << 8) | data[offset + 3];
        }

        private static ulong ReadBigEndianUInt64(byte[] data, int offset)
        {
            return ((ulong)ReadBigEndianUInt32(data, offset) << 32) | ReadBigEndianUInt32(data, offset + 4);
        }

        private static bool TryGetNameTranslation(IReadOnlyList<NameTranslation> names, string english, out NameTranslation translated)
        {
            string englishName = NormalizeTrackName(english);
            string englishPrefix = NormalizeTrackPrefix(english);

            foreach (NameTranslation name in names)
            {
                if (NormalizeTrackName(name.EnglishText) == englishName && NormalizePrefixText(name.EnglishPrefix) == englishPrefix)
                {
                    translated = name;
                    return true;
                }
            }

            NameTranslation unique = null;
            foreach (NameTranslation name in names)
            {
                if (NormalizeTrackName(name.EnglishText) != englishName) continue;
                if (unique != null)
                {
                    translated = null;
                    return false;
                }
                unique = name;
            }

            translated = unique;
            return unique != null;
        }

        private static string NormalizeTrackPrefix(string text)
        {
            Match prefixMatch = LeadingColoredPrefixRegex.Match(text ?? string.Empty);
            return prefixMatch.Success ? NormalizePrefixText(prefixMatch.Groups["prefix"].Value) : string.Empty;
        }

        private static string NormalizePrefixText(string text)
        {
            return WhitespaceRegex.Replace(BmgColorCodeRegex.Replace(text ?? string.Empty, string.Empty), " ").Trim();
        }

        private static string GetDisplayLookupName(string text)
        {
            string prefix = NormalizeTrackPrefix(text);
            string name = NormalizeTrackName(text);
            return prefix.Length == 0 ? name : prefix + " " + name;
        }

        private static string NormalizeTrackName(string text)
        {
            string value = text ?? string.Empty;
            Match prefixMatch = LeadingColoredPrefixRegex.Match(value);
            if (prefixMatch.Success) value = prefixMatch.Groups["name"].Value;
            string withoutColors = BmgColorCodeRegex.Replace(value, string.Empty);
            return WhitespaceRegex.Replace(withoutColors, " ").Trim();
        }

        private static string PreserveNameFormatting(string english, string translated, string sheetPrefix, bool isVariant)
        {
            string prefix = string.Empty;
            string name = english;
            Match prefixMatch = LeadingColoredPrefixRegex.Match(english);
            if (prefixMatch.Success)
            {
                prefix = prefixMatch.Groups["prefix"].Value;
                name = prefixMatch.Groups["name"].Value;
            }
            if (!string.IsNullOrWhiteSpace(sheetPrefix)) prefix = sheetPrefix.TrimEnd() + " ";

            string value = translated;
            if (isVariant)
            {
                Match match = TrailingColoredTextRegex.Match(name);
                if (match.Success)
                {
                    string suffix = NormalizeTrackName(match.Groups[4].Value);
                    string trimmed = translated.Trim();
                    if (suffix.Length > 0 && trimmed.EndsWith(suffix, StringComparison.Ordinal))
                    {
                        int suffixIndex = trimmed.Length - suffix.Length;
                        string translatedPrefix = trimmed.Substring(0, suffixIndex).TrimEnd();
                        string coloredSuffix = match.Groups[3].Value + trimmed.Substring(suffixIndex) + match.Groups[5].Value;
                        value = translatedPrefix.Length == 0 ? coloredSuffix : translatedPrefix + " " + coloredSuffix;
                    }
                }
            }

            return prefix + value;
        }

        private static string EscapeBmgText(string text)
        {
            return (text ?? string.Empty).Replace("\r\n", "\n").Replace("\r", "\n").Replace("\n", "\\n");
        }

        public static string FindDefaultPackRoot()
        {
            foreach (string start in new[] { Directory.GetCurrentDirectory(), AppContext.BaseDirectory })
            {
                DirectoryInfo current = new DirectoryInfo(start);
                for (int i = 0; current != null && i < 10; ++i, current = current.Parent)
                {
                    foreach (string candidate in new[] { current.FullName, Path.Combine(current.FullName, "RetroRewind6"), Path.Combine(current.FullName, "Pack", "RetroRewind6") })
                    {
                        if (File.Exists(Path.Combine(candidate, "Assets", "UIAssets.szs")) && File.Exists(Path.Combine(candidate, "Assets", "RaceAssets.szs"))) return candidate;
                    }
                }
            }
            return string.Empty;
        }

        private async Task BuildArchiveAsync(string packRoot, LanguageDefinition language, string archiveName, IEnumerable<TranslationTarget> targets, IReadOnlyDictionary<TranslationTarget, TranslationTable> sheets, string tempRoot, IProgress<string> progress, CancellationToken cancellationToken)
        {
            string englishArchive = Path.Combine(packRoot, "Assets", archiveName);
            string destination = language.IsEnglish ? englishArchive : Path.Combine(packRoot, "Language", language.FolderName, "Assets", archiveName);
            string safeLanguage = new string(language.DisplayName.Select(c => char.IsLetterOrDigit(c) ? c : '_').ToArray());
            string workDir = Path.Combine(tempRoot, safeLanguage, Path.GetFileNameWithoutExtension(archiveName));
            Directory.CreateDirectory(workDir);
            // Every language starts from the current English archive. This ensures all
            // non-BMG UI/race assets stay identical to English before translated BMGs
            // are layered on top.
            File.Copy(englishArchive, Path.Combine(workDir, archiveName), true);

            await RunToolAsync(Path.Combine(tempRoot, "wszst.exe"), $"extract \"{archiveName}\"", workDir, cancellationToken);
            string extractedDir = Path.Combine(workDir, Path.GetFileNameWithoutExtension(archiveName) + ".d");
            if (!Directory.Exists(extractedDir)) throw new InvalidOperationException($"wszst did not extract {archiveName}.");

            foreach (TranslationTarget target in targets)
            {
                IReadOnlyList<KeyValuePair<string, string>> messages = sheets[target].GetMessages(language);
                string sourcePath = Path.Combine(workDir, Path.GetFileNameWithoutExtension(target.BmgRelativePath) + ".txt");
                string destinationBmg = Path.Combine(extractedDir, target.BmgRelativePath.Replace('/', Path.DirectorySeparatorChar));
                Directory.CreateDirectory(Path.GetDirectoryName(destinationBmg));
                await File.WriteAllTextAsync(sourcePath, CreateBmgText(messages), new UTF8Encoding(false), cancellationToken);
                await RunToolAsync(Path.Combine(tempRoot, "wbmgt.exe"), $"encode \"{sourcePath}\" --dest \"{destinationBmg}\" -o", workDir, cancellationToken);
                progress?.Report($"  {target.SheetNames[0]}: {messages.Count} messages");
            }

            string outputArchive = Path.Combine(workDir, "output.szs");
            await RunToolAsync(Path.Combine(tempRoot, "wszst.exe"), $"create \"{extractedDir}\" --dest \"{outputArchive}\" -o", workDir, cancellationToken);
            Directory.CreateDirectory(Path.GetDirectoryName(destination));
            File.Copy(outputArchive, destination, true);
            progress?.Report($"  Wrote {destination}");
        }

        private async Task BuildRaceUiAsync(string packRoot, LanguageDefinition language, GameImageSheet gameImages, string tempRoot, IProgress<string> progress, CancellationToken cancellationToken)
        {
            string englishArchive = Path.Combine(packRoot, "UI", "Race_U.szs");
            if (!File.Exists(englishArchive)) throw new FileNotFoundException("The selected pack does not contain UI/Race_U.szs.", englishArchive);

            string safeLanguage = new string(language.DisplayName.Select(c => char.IsLetterOrDigit(c) ? c : '_').ToArray());
            string workDir = Path.Combine(tempRoot, safeLanguage, "RaceUI");
            Directory.CreateDirectory(workDir);
            File.Copy(englishArchive, Path.Combine(workDir, "Race_U.szs"), true);
            await RunToolAsync(Path.Combine(tempRoot, "wszst.exe"), "extract \"Race_U.szs\"", workDir, cancellationToken);

            string extractedDir = Path.Combine(workDir, "Race_U.d");
            string timgDir = Path.Combine(extractedDir, "game_image", "timg");
            if (!Directory.Exists(timgDir)) throw new InvalidOperationException("Race_U.szs does not contain game_image/timg.");

            var textureHashes = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            string decodedDir = Path.Combine(workDir, "decoded");
            Directory.CreateDirectory(decodedDir);
            foreach (string tpl in Directory.EnumerateFiles(timgDir, "*.tpl"))
            {
                cancellationToken.ThrowIfCancellationRequested();
                string png = Path.Combine(decodedDir, Path.GetFileNameWithoutExtension(tpl) + ".png");
                await RunToolAsync(Path.Combine(tempRoot, "wimgt.exe"), $"decode \"{tpl}\" --dest \"{png}\" -o", workDir, cancellationToken);
                textureHashes[GetImageHash(await File.ReadAllBytesAsync(png, cancellationToken))] = Path.GetFileName(tpl);
            }

            int applied = 0;
            int ignored = 0;
            foreach (GameImageRow row in gameImages.GetRows(language))
            {
                cancellationToken.ThrowIfCancellationRequested();
                if (!textureHashes.TryGetValue(GetImageHash(row.EnglishImage), out string targetName))
                {
                    progress?.Report($"  RR: Game Image row {row.SheetRow}: English image does not match Race_U.szs; skipped");
                    continue;
                }

                string sourceImage = Path.Combine(workDir, $"game_image_{row.SheetRow}{GetImageExtension(row.SelectedImage)}");
                await File.WriteAllBytesAsync(sourceImage, row.SelectedImage, cancellationToken);
                string targetTpl = Path.Combine(timgDir, targetName);
                await RunToolAsync(Path.Combine(tempRoot, "wimgt.exe"), $"encode \"{sourceImage}\" --dest \"{targetTpl}\" --transform {GetTplTransform(targetName)} -o", workDir, cancellationToken);
                ++applied;
            }

            string outputArchive = Path.Combine(workDir, language.RaceArchiveName);
            await RunToolAsync(Path.Combine(tempRoot, "wszst.exe"), $"create \"{extractedDir}\" --dest \"{outputArchive}\" -o", workDir, cancellationToken);
            string destination = language.IsEnglish ? Path.Combine(packRoot, "UI", language.RaceArchiveName) : Path.Combine(packRoot, "Language", language.FolderName, "UI", language.RaceArchiveName);
            Directory.CreateDirectory(Path.GetDirectoryName(destination));
            File.Copy(outputArchive, destination, true);
            progress?.Report($"  RR: Game Image: applied {applied} textures" + (ignored > 0 ? $" ({ignored} sheet-only images ignored)" : string.Empty));
            progress?.Report($"  Wrote {destination}");
        }

        private static string GetTplTransform(string fileName)
        {
            if (fileName.StartsWith("tt_multi_position_", StringComparison.OrdinalIgnoreCase)) return "IA4";
            if (fileName.StartsWith("tt_position_", StringComparison.OrdinalIgnoreCase)) return "RGB5A3";
            return "IA8";
        }

        private static string GetImageExtension(byte[] image)
        {
            if (image.Length >= 8 && image[0] == 0x89 && image[1] == 0x50 && image[2] == 0x4e && image[3] == 0x47) return ".png";
            if (image.Length >= 2 && image[0] == 0xff && image[1] == 0xd8) return ".jpg";
            return ".png";
        }

        private static string GetImageHash(byte[] image)
        {
            using var stream = new MemoryStream(image, false);
            using var source = new Bitmap(stream);
            using var bitmap = new Bitmap(source.Width, source.Height, PixelFormat.Format32bppArgb);
            using (Graphics graphics = Graphics.FromImage(bitmap)) graphics.DrawImageUnscaled(source, 0, 0);
            Rectangle rectangle = new Rectangle(0, 0, bitmap.Width, bitmap.Height);
            BitmapData data = bitmap.LockBits(rectangle, ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
            try
            {
                int length = Math.Abs(data.Stride) * data.Height;
                byte[] pixels = new byte[length + 8];
                Marshal.Copy(data.Scan0, pixels, 8, length);
                BitConverter.GetBytes(bitmap.Width).CopyTo(pixels, 0);
                BitConverter.GetBytes(bitmap.Height).CopyTo(pixels, 4);
                return Convert.ToHexString(SHA256.HashData(pixels));
            }
            finally
            {
                bitmap.UnlockBits(data);
            }
        }

        private static string CreateBmgText(IEnumerable<KeyValuePair<string, string>> messages)
        {
            var builder = new StringBuilder("#BMG\n\n");
            foreach (KeyValuePair<string, string> message in messages)
            {
                string value = message.Value.Replace("\r\n", "\n").Replace("\r", "\n").Replace("\n", "\\n");
                builder.Append(message.Key).Append(" = ").Append(value).Append('\n');
            }
            return builder.ToString();
        }

        private static async Task WriteToolsAsync(string directory)
        {
            await File.WriteAllBytesAsync(Path.Combine(directory, "wszst.exe"), PulsarRes.wszst);
            await File.WriteAllBytesAsync(Path.Combine(directory, "wbmgt.exe"), PulsarRes.wbmgt);
            await File.WriteAllBytesAsync(Path.Combine(directory, "wimgt.exe"), PulsarRes.wimgt);
            await File.WriteAllBytesAsync(Path.Combine(directory, "cygwin1.dll"), PulsarRes.cygwin1);
            await File.WriteAllBytesAsync(Path.Combine(directory, "cygz.dll"), PulsarRes.cygz);
            await File.WriteAllBytesAsync(Path.Combine(directory, "cygcrypto-1.1.dll"), PulsarRes.cygcrypto_1_1);
            await File.WriteAllBytesAsync(Path.Combine(directory, "cygncursesw-10.dll"), PulsarRes.cygncursesw_10);
            await File.WriteAllBytesAsync(Path.Combine(directory, "cygpng16-16.dll"), PulsarRes.cygpng16_16);
        }

        private static async Task RunToolAsync(string executable, string arguments, string workingDirectory, CancellationToken cancellationToken)
        {
            var startInfo = new ProcessStartInfo { FileName = executable, Arguments = arguments, WorkingDirectory = workingDirectory, CreateNoWindow = true, WindowStyle = ProcessWindowStyle.Hidden, UseShellExecute = false, RedirectStandardOutput = true, RedirectStandardError = true };
            using Process process = Process.Start(startInfo) ?? throw new InvalidOperationException($"Unable to start {Path.GetFileName(executable)}.");
            string stdout = await process.StandardOutput.ReadToEndAsync();
            string stderr = await process.StandardError.ReadToEndAsync();
            await process.WaitForExitAsync(cancellationToken);
            if (process.ExitCode != 0)
            {
                string detail = string.IsNullOrWhiteSpace(stderr) ? stdout : stderr;
                throw new InvalidOperationException($"{Path.GetFileName(executable)} failed ({process.ExitCode}).\n{detail}".Trim());
            }
        }

        private static void ValidatePackRoot(string packRoot)
        {
            if (string.IsNullOrWhiteSpace(packRoot) || !Directory.Exists(packRoot)) throw new DirectoryNotFoundException("Choose the RetroRewind6 pack folder.");
            if (!File.Exists(Path.Combine(packRoot, "Assets", "UIAssets.szs")) || !File.Exists(Path.Combine(packRoot, "Assets", "RaceAssets.szs")))
                throw new DirectoryNotFoundException("The selected folder does not contain Assets/UIAssets.szs and Assets/RaceAssets.szs.");
            if (!File.Exists(Path.Combine(packRoot, "UI", "Race_U.szs")))
                throw new DirectoryNotFoundException("The selected folder does not contain UI/Race_U.szs.");
        }

        private static void ValidateConfigPackRoot(string packRoot)
        {
            if (string.IsNullOrWhiteSpace(packRoot) || !Directory.Exists(packRoot)) throw new DirectoryNotFoundException("Choose the RetroRewind6 pack folder.");
            string binariesDir = Path.Combine(packRoot, "Binaries");
            if (!Directory.Exists(binariesDir) || !Directory.EnumerateFiles(binariesDir, "Config*.pul", SearchOption.TopDirectoryOnly).Any())
                throw new DirectoryNotFoundException("The selected folder does not contain Binaries/Config*.pul files.");
        }
    }
}
