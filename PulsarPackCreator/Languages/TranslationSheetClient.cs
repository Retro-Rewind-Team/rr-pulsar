using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.Http;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;

namespace Pulsar_Pack_Creator.Languages
{
    public sealed class TranslationSheetClient
    {
        private static readonly Regex SheetIdRegex = new Regex(@"/spreadsheets/(?:u/\d+/)?d/([A-Za-z0-9_-]+)", RegexOptions.Compiled | RegexOptions.IgnoreCase);
        private readonly HttpClient client;

        public TranslationSheetClient(HttpClient client)
        {
            this.client = client;
        }

        public async Task<Dictionary<TranslationTarget, TranslationTable>> LoadRequiredSheetsAsync(string sheetUrlOrId, CancellationToken cancellationToken)
        {
            string spreadsheetId = GetSpreadsheetId(sheetUrlOrId);
            var result = new Dictionary<TranslationTarget, TranslationTable>();
            foreach (TranslationTarget target in TranslationTarget.Required)
                result[target] = await DownloadFirstMatchingTabAsync(spreadsheetId, target.SheetNames, cancellationToken);
            return result;
        }

        private async Task<TranslationTable> DownloadFirstMatchingTabAsync(string spreadsheetId, IReadOnlyList<string> sheetNames, CancellationToken cancellationToken)
        {
            Exception lastError = null;
            foreach (string sheetName in sheetNames)
            {
                try
                {
                    string url = $"https://docs.google.com/spreadsheets/d/{spreadsheetId}/gviz/tq?tqx=out:csv&sheet={Uri.EscapeDataString(sheetName)}";
                    using HttpResponseMessage response = await client.GetAsync(url, cancellationToken);
                    string content = await response.Content.ReadAsStringAsync(cancellationToken);
                    if (!response.IsSuccessStatusCode)
                    {
                        lastError = new InvalidOperationException($"Google Sheets returned {(int)response.StatusCode} for '{sheetName}'.");
                        continue;
                    }
                    if (content.TrimStart().StartsWith("<", StringComparison.Ordinal))
                        throw new InvalidOperationException("Google returned a sign-in page. The spreadsheet must be viewable by anyone with the link.");

                    TranslationTable table = TranslationTable.ParseCsv(sheetName, content);
                    if (table.Rows.Count == 0)
                    {
                        lastError = new InvalidOperationException($"'{sheetName}' was empty.");
                        continue;
                    }
                    return table;
                }
                catch (Exception ex) when (ex is not OperationCanceledException)
                {
                    lastError = ex;
                }
            }
            throw new InvalidOperationException($"Could not read the required sheet '{sheetNames[0]}'.", lastError);
        }

        public static string GetSpreadsheetId(string sheetUrlOrId)
        {
            string input = sheetUrlOrId?.Trim();
            if (string.IsNullOrWhiteSpace(input)) throw new ArgumentException("Enter a Google Sheets URL or spreadsheet ID.");
            Match match = SheetIdRegex.Match(input);
            if (match.Success) return match.Groups[1].Value;
            if (Regex.IsMatch(input, @"^[A-Za-z0-9_-]{20,}$")) return input;
            throw new ArgumentException("That does not look like a valid Google Sheets URL or spreadsheet ID.");
        }
    }

    public sealed class TranslationTable
    {
        public string SheetName { get; }
        public IReadOnlyList<IReadOnlyList<string>> Rows { get; }

        private TranslationTable(string sheetName, IReadOnlyList<IReadOnlyList<string>> rows)
        {
            SheetName = sheetName;
            Rows = rows;
        }

        public IReadOnlyList<KeyValuePair<string, string>> GetMessages(LanguageDefinition language)
        {
            int headerRow = -1;
            int englishColumn = -1;
            int languageColumn = -1;
            for (int rowIndex = 0; rowIndex < Math.Min(Rows.Count, 20); ++rowIndex)
            {
                IReadOnlyList<string> row = Rows[rowIndex];
                int english = FindColumn(row, LanguageDefinition.All[0].SheetHeaders);
                int selected = language.IsEnglish ? english : FindColumn(row, language.SheetHeaders);
                if (english >= 0 && selected >= 0)
                {
                    headerRow = rowIndex;
                    englishColumn = english;
                    languageColumn = selected;
                    break;
                }
            }
            if (headerRow < 0)
                throw new InvalidOperationException($"'{SheetName}' does not contain both an English column and a '{language.DisplayName}' column.");

            int idColumn = FindIdColumn(Rows[headerRow], englishColumn);
            var messages = new List<KeyValuePair<string, string>>();
            for (int rowIndex = headerRow + 1; rowIndex < Rows.Count; ++rowIndex)
            {
                IReadOnlyList<string> row = Rows[rowIndex];
                string id = GetCell(row, idColumn).Trim();
                if (string.IsNullOrWhiteSpace(id) || id.StartsWith("#", StringComparison.Ordinal)) continue;
                if (id.EndsWith("=", StringComparison.Ordinal)) id = id.Substring(0, id.Length - 1).TrimEnd();

                string english = GetCell(row, englishColumn);
                string selected = GetCell(row, languageColumn);
                string text = string.IsNullOrWhiteSpace(selected) ? english : selected;
                if (string.IsNullOrWhiteSpace(text)) continue;
                messages.Add(new KeyValuePair<string, string>(id, text));
            }
            if (messages.Count == 0) throw new InvalidOperationException($"'{SheetName}' did not contain any translatable messages.");
            return messages;
        }

        public static TranslationTable ParseCsv(string sheetName, string csv)
        {
            var rows = new List<IReadOnlyList<string>>();
            var row = new List<string>();
            var field = new StringBuilder();
            bool quoted = false;
            for (int i = 0; i < csv.Length; ++i)
            {
                char c = csv[i];
                if (quoted)
                {
                    if (c == '"')
                    {
                        if (i + 1 < csv.Length && csv[i + 1] == '"') { field.Append('"'); ++i; }
                        else quoted = false;
                    }
                    else field.Append(c);
                    continue;
                }
                if (c == '"') quoted = true;
                else if (c == ',') { row.Add(field.ToString()); field.Clear(); }
                else if (c == '\r' || c == '\n')
                {
                    if (c == '\r' && i + 1 < csv.Length && csv[i + 1] == '\n') ++i;
                    row.Add(field.ToString()); field.Clear();
                    if (row.Any(value => !string.IsNullOrEmpty(value))) rows.Add(row.ToArray());
                    row.Clear();
                }
                else field.Append(c);
            }
            if (field.Length > 0 || row.Count > 0)
            {
                row.Add(field.ToString());
                if (row.Any(value => !string.IsNullOrEmpty(value))) rows.Add(row.ToArray());
            }
            return new TranslationTable(sheetName, rows);
        }

        private static int FindColumn(IReadOnlyList<string> row, IEnumerable<string> aliases)
        {
            for (int i = 0; i < row.Count; ++i)
                if (LanguageDefinition.HeaderMatches(row[i], aliases)) return i;
            return -1;
        }

        private static int FindIdColumn(IReadOnlyList<string> header, int englishColumn)
        {
            string[] aliases = { "ID", "BMG", "BMG ID", "Message ID", "Key" };
            int found = FindColumn(header, aliases);
            if (found >= 0) return found;
            for (int i = 0; i < englishColumn; ++i)
                if (!string.IsNullOrWhiteSpace(header[i])) return i;
            return 0;
        }

        private static string GetCell(IReadOnlyList<string> row, int index) => index >= 0 && index < row.Count ? row[index] ?? string.Empty : string.Empty;
    }

    public sealed class TranslationTarget
    {
        public string ArchiveName { get; }
        public string BmgRelativePath { get; }
        public IReadOnlyList<string> SheetNames { get; }

        private TranslationTarget(string archiveName, string bmgRelativePath, params string[] sheetNames)
        {
            ArchiveName = archiveName;
            BmgRelativePath = bmgRelativePath;
            SheetNames = sheetNames;
        }

        public static IReadOnlyList<TranslationTarget> Required { get; } = new[]
        {
            new TranslationTarget("UIAssets.szs", "message/menu.bmg", "RR: UIAssets | Menu", "RR:UIAssets | Menu"),
            new TranslationTarget("UIAssets.szs", "message/common.bmg", "RR: UIAssets | Common", "RR:UIAssets | Common"),
            new TranslationTarget("RaceAssets.szs", "message/common.bmg", "RR: RaceAssets | Common", "RR:RaceAssets | Common"),
            new TranslationTarget("RaceAssets.szs", "message/race.bmg", "RR: RaceAssets | Race", "RR:RaceAssets | Race")
        };
    }
}
