using System;
using System.Collections.Generic;
using System.Globalization;
using System.Text.RegularExpressions;

namespace Pulsar_Pack_Creator
{
    internal static class CrashMetadataResolver
    {
        private static readonly Lazy<Dictionary<int, string>> sectionNames = new(() => ParseEnumResource("IdentifiersHpp", "SectionId"));
        private static readonly Lazy<Dictionary<int, string>> pageNames = new(() =>
        {
            Dictionary<int, string> names = ParseEnumResource("IdentifiersHpp", "PageId");
            foreach (KeyValuePair<int, string> entry in ParseEnumResource("PulsarUiHpp", "PulPageId"))
            {
                names[entry.Key] = entry.Value;
            }
            return names;
        });
        private static readonly Lazy<Dictionary<int, string>> contextNames = new(() => ParseEnumResource("PulsarSystemHpp", "Context"));
        private static readonly Lazy<Dictionary<int, string>> context2Names = new(() => ParseEnumResource("PulsarSystemHpp", "Context2"));

        public static string GetSectionName(int id)
        {
            return sectionNames.Value.TryGetValue(id, out string name) ? name : string.Empty;
        }

        public static string GetPageName(int id)
        {
            return pageNames.Value.TryGetValue(id, out string name) ? name : string.Empty;
        }

        public static string GetEnabledContexts(uint context, uint context2)
        {
            List<string> enabled = new();
            AppendEnabledContexts(enabled, contextNames.Value, context);
            AppendEnabledContexts(enabled, context2Names.Value, context2);
            return enabled.Count == 0 ? "None" : string.Join(", ", enabled);
        }

        private static void AppendEnabledContexts(List<string> enabled, Dictionary<int, string> names, uint value)
        {
            foreach (KeyValuePair<int, string> entry in names)
            {
                if (entry.Key < 0 || entry.Key >= 32) continue;
                if ((value & (1u << entry.Key)) != 0) enabled.Add(entry.Value);
            }
        }

        private static Dictionary<int, string> ParseEnumResource(string resourceKey, string enumName)
        {
            string source = PulsarRes.ResourceManager.GetString(resourceKey, CultureInfo.InvariantCulture) ?? string.Empty;
            return ParseEnum(source, enumName);
        }

        private static Dictionary<int, string> ParseEnum(string source, string enumName)
        {
            Dictionary<int, string> values = new();
            if (string.IsNullOrWhiteSpace(source)) return values;

            Match match = Regex.Match(
                source,
                $@"enum\s+{Regex.Escape(enumName)}\s*\{{(?<body>.*?)\}};",
                RegexOptions.Singleline);
            if (!match.Success) return values;

            string body = Regex.Replace(match.Groups["body"].Value, @"//.*", string.Empty);
            string[] entries = body.Split(',');
            int nextValue = 0;

            foreach (string rawEntry in entries)
            {
                string entry = rawEntry.Trim();
                if (string.IsNullOrEmpty(entry)) continue;

                string[] parts = entry.Split(new[] { '=' }, 2);
                string name = parts[0].Trim();
                if (string.IsNullOrEmpty(name)) continue;

                int value = nextValue;
                if (parts.Length == 2 && !TryParseInteger(parts[1].Trim(), out value)) continue;

                values[value] = name;
                nextValue = value + 1;
            }

            return values;
        }

        private static bool TryParseInteger(string value, out int parsed)
        {
            value = value.Trim();
            if (value.StartsWith("0x", StringComparison.OrdinalIgnoreCase))
            {
                return int.TryParse(value.Substring(2), NumberStyles.HexNumber, CultureInfo.InvariantCulture, out parsed);
            }
            if (value.StartsWith("-0x", StringComparison.OrdinalIgnoreCase))
            {
                bool success = int.TryParse(value.Substring(3), NumberStyles.HexNumber, CultureInfo.InvariantCulture, out int hexValue);
                parsed = -hexValue;
                return success;
            }
            return int.TryParse(value, NumberStyles.Integer, CultureInfo.InvariantCulture, out parsed);
        }
    }
}
