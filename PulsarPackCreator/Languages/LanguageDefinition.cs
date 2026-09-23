using System;
using System.Collections.Generic;
using System.Linq;

namespace Pulsar_Pack_Creator.Languages
{
    public sealed class LanguageDefinition
    {
        public string DisplayName { get; }
        public string FolderName { get; }
        public IReadOnlyList<string> SheetHeaders { get; }
        public bool IsEnglish => FolderName == null;

        public LanguageDefinition(string displayName, string folderName, params string[] sheetHeaders)
        {
            DisplayName = displayName;
            FolderName = folderName;
            SheetHeaders = sheetHeaders;
        }

        public override string ToString() => DisplayName;

        public static IReadOnlyList<LanguageDefinition> All { get; } = new[]
        {
            new LanguageDefinition("English", null, "English", "Common/English", "Common English", "Common", "ENG", "EN"),
            new LanguageDefinition("Japanese", "JPN", "Japanese", "JPN", "JA"),
            new LanguageDefinition("French", "FRA", "French", "FRA", "FR"),
            new LanguageDefinition("German", "GER", "German", "GER", "DE"),
            new LanguageDefinition("Dutch", "DUT", "Dutch", "DUT", "NL"),
            new LanguageDefinition("Spanish (US)", "SPA(NTSC)", "Spanish (US)", "Spanish US", "Spanish (NTSC)", "SPA(NTSC)", "ES-US"),
            new LanguageDefinition("Spanish (EU)", "SPA(EU)", "Spanish (EU)", "Spanish EU", "SPA(EU)", "ES-EU"),
            new LanguageDefinition("Finnish", "FIN", "Finnish", "FIN", "FI"),
            new LanguageDefinition("Italian", "ITA", "Italian", "ITA", "IT"),
            new LanguageDefinition("Korean", "KOR", "Korean", "KOR", "KO"),
            new LanguageDefinition("Russian", "RUS", "Russian", "RUS", "RU"),
            new LanguageDefinition("Turkish", "TUR", "Turkish", "TUR", "TR"),
            new LanguageDefinition("Czech", "CZE", "Czech", "CZE", "CS", "CZ")
        };

        public static bool HeaderMatches(string value, IEnumerable<string> aliases)
        {
            string normalized = NormalizeHeader(value);
            return aliases.Any(alias => NormalizeHeader(alias) == normalized);
        }

        private static string NormalizeHeader(string value)
        {
            if (value == null) return string.Empty;
            return new string(value.Where(char.IsLetterOrDigit).Select(char.ToLowerInvariant).ToArray());
        }
    }
}
