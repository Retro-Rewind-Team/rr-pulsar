using System;
using System.Collections.Generic;
using System.Linq;

namespace Pulsar_Pack_Creator.Languages
{
    public sealed class LanguageDefinition
    {
        public string DisplayName { get; }
        public string FolderName { get; }
        public string RaceArchiveName { get; }
        public IReadOnlyList<string> SheetHeaders { get; }
        public bool IsEnglish => FolderName == null;

        public LanguageDefinition(string displayName, string folderName, string raceArchiveName, params string[] sheetHeaders)
        {
            DisplayName = displayName;
            FolderName = folderName;
            RaceArchiveName = raceArchiveName;
            SheetHeaders = sheetHeaders;
        }

        public override string ToString() => DisplayName;

        public static IReadOnlyList<LanguageDefinition> All { get; } = new[]
        {
            new LanguageDefinition("English", null, "Race_U.szs", "English", "Common/English", "Common English", "Common", "ENG", "EN"),
            new LanguageDefinition("Japanese", "JPN", "Race_J.szs", "Japanese", "JPN", "JA"),
            new LanguageDefinition("French", "FRA", "Race_F.szs", "French", "FRA", "FR"),
            new LanguageDefinition("German", "GER", "Race_G.szs", "German", "GER", "DE"),
            new LanguageDefinition("Dutch", "DUT", "Race_D.szs", "Dutch", "DUT", "NL"),
            new LanguageDefinition("Spanish (US)", "SPA(NTSC)", "Race_AS.szs", "Spanish (US)", "Spanish US", "Spanish(NTSC)", "Spanish (NTSC)", "SPA(NTSC)", "ES-US"),
            new LanguageDefinition("Spanish (EU)", "SPA(EU)", "Race_ES.szs", "Spanish (EU)", "Spanish EU", "Spanish(EU)", "SPA(EU)", "ES-EU"),
            new LanguageDefinition("Finnish", "FIN", "Race_FI.szs", "Finnish", "FIN", "FI"),
            new LanguageDefinition("Italian", "ITA", "Race_I.szs", "Italian", "ITA", "IT"),
            new LanguageDefinition("Korean", "KOR", "Race_K.szs", "Korean", "KOR", "KO"),
            new LanguageDefinition("Russian", "RUS", "Race_R.szs", "Russian", "RUS", "RU"),
            new LanguageDefinition("Turkish", "TUR", "Race_T.szs", "Turkish", "TUR", "TR"),
            new LanguageDefinition("Czech", "CZE", "Race_C.szs", "Czech", "CZE", "CS", "CZ")
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
