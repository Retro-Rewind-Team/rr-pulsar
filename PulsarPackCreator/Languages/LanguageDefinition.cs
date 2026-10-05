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
        public uint TrackBmgOffset { get; }
        public IReadOnlyList<string> SheetHeaders { get; }
        public bool IsEnglish => FolderName == null;

        public LanguageDefinition(string displayName, string folderName, string raceArchiveName, uint trackBmgOffset, params string[] sheetHeaders)
        {
            DisplayName = displayName;
            FolderName = folderName;
            RaceArchiveName = raceArchiveName;
            TrackBmgOffset = trackBmgOffset;
            SheetHeaders = sheetHeaders;
        }

        public override string ToString() => DisplayName;

        public static IReadOnlyList<LanguageDefinition> All { get; } = new[]
        {
            new LanguageDefinition("English", null, "Race_U.szs", 0x0000, "English", "Common/English", "Common English", "Common", "ENG", "EN"),
            new LanguageDefinition("Japanese", "JPN", "Race_J.szs", 0x1000, "Japanese", "JPN", "JA"),
            new LanguageDefinition("French", "FRA", "Race_F.szs", 0x2000, "French", "FRA", "FR"),
            new LanguageDefinition("German", "GER", "Race_G.szs", 0x3000, "German", "GER", "DE"),
            new LanguageDefinition("Dutch", "DUT", "Race_D.szs", 0x4000, "Dutch", "DUT", "NL"),
            new LanguageDefinition("Spanish (US)", "SPA(NTSC)", "Race_AS.szs", 0x5000, "Spanish (US)", "Spanish US", "Spanish(NTSC)", "Spanish (NTSC)", "SPA(NTSC)", "ES-US"),
            new LanguageDefinition("Spanish (EU)", "SPA(EU)", "Race_ES.szs", 0x6000, "Spanish (EU)", "Spanish EU", "Spanish(EU)", "SPA(EU)", "ES-EU"),
            new LanguageDefinition("Finnish", "FIN", "Race_FI.szs", 0x7000, "Finnish", "FIN", "FI"),
            new LanguageDefinition("Italian", "ITA", "Race_I.szs", 0x8000, "Italian", "ITA", "IT"),
            new LanguageDefinition("Korean", "KOR", "Race_K.szs", 0x9000, "Korean", "KOR", "KO"),
            new LanguageDefinition("Russian", "RUS", "Race_R.szs", 0xA000, "Russian", "RUS", "RU"),
            new LanguageDefinition("Turkish", "TUR", "Race_T.szs", 0xB000, "Turkish", "TUR", "TR"),
            new LanguageDefinition("Czech", "CZE", "Race_C.szs", 0xC000, "Czech", "CZE", "CS", "CZ")
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
