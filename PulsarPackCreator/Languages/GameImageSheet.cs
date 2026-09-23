using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Xml.Linq;

namespace Pulsar_Pack_Creator.Languages
{
    public sealed class GameImageSheet
    {
        private const string SheetName = "RR: Game Image";
        private readonly Dictionary<(int Row, int Column), byte[]> images;

        private GameImageSheet(Dictionary<(int Row, int Column), byte[]> images)
        {
            this.images = images;
        }

        public IEnumerable<GameImageRow> GetRows(LanguageDefinition language)
        {
            int languageIndex = LanguageDefinition.All.ToList().IndexOf(language);
            if (languageIndex < 0) throw new InvalidOperationException($"Unknown language '{language.DisplayName}'.");

            const int englishColumn = 1;
            int languageColumn = languageIndex + 1;
            foreach (int row in images.Keys.Where(cell => cell.Column == englishColumn).Select(cell => cell.Row).Distinct().OrderBy(row => row))
            {
                byte[] englishImage = images[(row, englishColumn)];
                images.TryGetValue((row, languageColumn), out byte[] selectedImage);
                yield return new GameImageRow(row + 1, englishImage, selectedImage ?? englishImage);
            }
        }

        public static GameImageSheet FromXlsx(byte[] xlsx)
        {
            using var stream = new MemoryStream(xlsx, false);
            using var archive = new ZipArchive(stream, ZipArchiveMode.Read, false);
            string worksheetPath = FindWorksheetPath(archive, SheetName);
            XDocument worksheet = LoadXml(archive, worksheetPath);
            XNamespace relNs = "http://schemas.openxmlformats.org/officeDocument/2006/relationships";
            XElement drawing = worksheet.Descendants().FirstOrDefault(element => element.Name.LocalName == "drawing");
            string drawingRelId = drawing?.Attribute(relNs + "id")?.Value;
            if (drawingRelId == null) throw new InvalidOperationException($"'{SheetName}' does not contain embedded images.");

            string drawingPath = ResolveRelationship(archive, worksheetPath, drawingRelId);
            XDocument drawingXml = LoadXml(archive, drawingPath);
            var result = new Dictionary<(int Row, int Column), byte[]>();
            XNamespace drawingNs = "http://schemas.openxmlformats.org/drawingml/2006/spreadsheetDrawing";
            XNamespace artNs = "http://schemas.openxmlformats.org/drawingml/2006/main";

            foreach (XElement anchor in drawingXml.Root.Elements())
            {
                XElement from = anchor.Element(drawingNs + "from");
                XElement blip = anchor.Descendants(artNs + "blip").FirstOrDefault();
                string imageRelId = blip?.Attribute(relNs + "embed")?.Value;
                if (from == null || imageRelId == null) continue;
                if (!int.TryParse(from.Element(drawingNs + "row")?.Value, out int row)) continue;
                if (!int.TryParse(from.Element(drawingNs + "col")?.Value, out int column)) continue;

                string imagePath = ResolveRelationship(archive, drawingPath, imageRelId);
                ZipArchiveEntry imageEntry = archive.GetEntry(imagePath);
                if (imageEntry == null) continue;
                using Stream imageStream = imageEntry.Open();
                using var imageBytes = new MemoryStream();
                imageStream.CopyTo(imageBytes);
                result[(row, column)] = imageBytes.ToArray();
            }

            if (result.Count == 0) throw new InvalidOperationException($"No embedded images could be read from '{SheetName}'.");
            return new GameImageSheet(result);
        }

        private static string FindWorksheetPath(ZipArchive archive, string sheetName)
        {
            XDocument workbook = LoadXml(archive, "xl/workbook.xml");
            XNamespace relNs = "http://schemas.openxmlformats.org/officeDocument/2006/relationships";
            List<XElement> sheets = workbook.Descendants().Where(element => element.Name.LocalName == "sheet").ToList();
            XElement sheet = sheets.FirstOrDefault(element => string.Equals((string)element.Attribute("name"), sheetName, StringComparison.OrdinalIgnoreCase));
            if (sheet == null)
            {
                string normalizedName = NormalizeSheetName(sheetName);
                sheet = sheets.FirstOrDefault(element => NormalizeSheetName((string)element.Attribute("name")) == normalizedName);
            }
            string relId = sheet?.Attribute(relNs + "id")?.Value;
            if (relId == null)
            {
                string available = string.Join(", ", sheets.Select(element => (string)element.Attribute("name")).Where(name => !string.IsNullOrWhiteSpace(name)));
                throw new InvalidOperationException($"The XLSX export does not contain '{sheetName}'. Exported sheets: {available}");
            }
            return ResolveRelationship(archive, "xl/workbook.xml", relId);
        }

        private static string NormalizeSheetName(string value)
        {
            if (value == null) return string.Empty;
            return new string(value.Where(char.IsLetterOrDigit).Select(char.ToLowerInvariant).ToArray());
        }

        private static string ResolveRelationship(ZipArchive archive, string sourcePath, string relationshipId)
        {
            string directory = Path.GetDirectoryName(sourcePath)?.Replace('\\', '/') ?? string.Empty;
            string fileName = Path.GetFileName(sourcePath);
            string relsPath = string.IsNullOrEmpty(directory) ? $"_rels/{fileName}.rels" : $"{directory}/_rels/{fileName}.rels";
            XDocument rels = LoadXml(archive, relsPath);
            XElement relation = rels.Root.Elements().FirstOrDefault(element => (string)element.Attribute("Id") == relationshipId);
            string target = (string)relation?.Attribute("Target");
            if (target == null) throw new InvalidOperationException($"Broken XLSX relationship '{relationshipId}'.");
            return NormalizePartPath(directory, target);
        }

        private static string NormalizePartPath(string directory, string target)
        {
            if (target.StartsWith("/", StringComparison.Ordinal)) target = target.TrimStart('/');
            else if (!string.IsNullOrEmpty(directory)) target = directory + "/" + target;
            var parts = new List<string>();
            foreach (string part in target.Split('/'))
            {
                if (part == "..") { if (parts.Count > 0) parts.RemoveAt(parts.Count - 1); }
                else if (part != "." && part.Length > 0) parts.Add(part);
            }
            return string.Join("/", parts);
        }

        private static XDocument LoadXml(ZipArchive archive, string path)
        {
            ZipArchiveEntry entry = archive.GetEntry(path) ?? throw new InvalidOperationException($"The XLSX export is missing '{path}'.");
            using Stream stream = entry.Open();
            return XDocument.Load(stream);
        }
    }

    public sealed class GameImageRow
    {
        public int SheetRow { get; }
        public byte[] EnglishImage { get; }
        public byte[] SelectedImage { get; }

        public GameImageRow(int sheetRow, byte[] englishImage, byte[] selectedImage)
        {
            SheetRow = sheetRow;
            EnglishImage = englishImage;
            SelectedImage = selectedImage;
        }
    }
}
