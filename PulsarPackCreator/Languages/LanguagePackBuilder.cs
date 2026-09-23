using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;

namespace Pulsar_Pack_Creator.Languages
{
    public sealed class LanguagePackBuilder
    {
        public async Task BuildAsync(string packRoot, IReadOnlyDictionary<TranslationTarget, TranslationTable> sheets, IEnumerable<LanguageDefinition> languages, IProgress<string> progress, CancellationToken cancellationToken)
        {
            ValidatePackRoot(packRoot);
            string tempRoot = Path.Combine(Path.GetTempPath(), "PulsarLanguageBuilder", Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(tempRoot);
            try
            {
                await WriteToolsAsync(tempRoot);
                foreach (LanguageDefinition language in languages)
                {
                    cancellationToken.ThrowIfCancellationRequested();
                    progress?.Report($"Building {language.DisplayName}...");
                    foreach (IGrouping<string, TranslationTarget> group in TranslationTarget.Required.GroupBy(target => target.ArchiveName))
                        await BuildArchiveAsync(packRoot, language, group.Key, group, sheets, tempRoot, progress, cancellationToken);
                }
            }
            finally
            {
                try { if (Directory.Exists(tempRoot)) Directory.Delete(tempRoot, true); } catch { }
            }
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
        }
    }
}
