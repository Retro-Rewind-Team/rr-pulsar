using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Text;
using System.Xml.Linq;
using static Pulsar_Pack_Creator.MainWindow;

namespace Pulsar_Pack_Creator.IO {
    static class TracklistSpreadsheet {
        static readonly XNamespace spreadsheetNs = "http://schemas.openxmlformats.org/spreadsheetml/2006/main";
        static readonly XNamespace relationshipNs = "http://schemas.openxmlformats.org/officeDocument/2006/relationships";
        static readonly XNamespace packageRelationshipNs = "http://schemas.openxmlformats.org/package/2006/relationships";

        public static void Write(string path, IReadOnlyList<Cup> cups, ushort cupCount) {
            using FileStream stream = File.Create(path);
            using ZipArchive archive = new ZipArchive(stream, ZipArchiveMode.Create);

            WriteXml(archive, "[Content_Types].xml", CreateContentTypes());
            WriteXml(archive, "_rels/.rels", CreateRootRelationships());
            WriteXml(archive, "xl/workbook.xml", CreateWorkbook());
            WriteXml(archive, "xl/_rels/workbook.xml.rels", CreateWorkbookRelationships());
            WriteXml(archive, "xl/styles.xml", CreateStyles());
            WriteXml(archive, "xl/worksheets/sheet1.xml", CreateWorksheet(BuildTrackRows(cups, cupCount)));
            WriteXml(archive, "xl/worksheets/sheet2.xml", CreateWorksheet(BuildVariantRows(cups, cupCount)));
        }

        private static List<string[]> BuildTrackRows(IReadOnlyList<Cup> cups, ushort cupCount) {
            List<string[]> rows = new List<string[]>();
            for (int cupIdx = 0; cupIdx < cupCount; cupIdx++) {
                Cup cup = cups[cupIdx];
                for (int trackIdx = 0; trackIdx < 4; trackIdx++) {
                    Cup.Track track = cup.tracks[trackIdx];
                    rows.Add(new[] {
                        string.IsNullOrEmpty(track.commonName) ? track.main.trackName : track.commonName,
                        track.main.authorName,
                        track.musicCredit,
                        GetTrackSlotName(track.main.slot),
                        GetMusicSlotName(track.main.musicSlot),
                        track.main.fileName
                    });
                }
            }
            return rows;
        }

        private static List<string[]> BuildVariantRows(IReadOnlyList<Cup> cups, ushort cupCount) {
            List<string[]> rows = new List<string[]>();
            for (int cupIdx = 0; cupIdx < cupCount; cupIdx++) {
                Cup cup = cups[cupIdx];
                for (int trackIdx = 0; trackIdx < 4; trackIdx++) {
                    foreach (Cup.Track.Variant variant in cup.tracks[trackIdx].variants) {
                        string name = variant.trackName;
                        if (!string.IsNullOrEmpty(variant.versionName) && variant.versionName != Cup.defaultVersion)
                            name += $" \\c{{red3}}{variant.versionName}\\c{{off}}";

                        rows.Add(new[] {
                            name,
                            variant.authorName,
                            variant.musicCredit,
                            GetTrackSlotName(variant.slot),
                            GetMusicSlotName(variant.musicSlot),
                            variant.fileName
                        });
                    }
                }
            }
            return rows;
        }

        private static string GetTrackSlotName(byte slot) {
            int idx = Array.IndexOf(PulsarGame.MarioKartWii.idxToCourseId, slot);
            return idx >= 0 ? PulsarGame.MarioKartWii.idxToFullNames[idx] : slot.ToString("X2");
        }

        private static string GetMusicSlotName(byte slot) {
            int idx = Array.IndexOf(PulsarGame.MarioKartWii.musicIdxToCourseId, slot);
            return idx >= 0 ? PulsarGame.MarioKartWii.musicIdxToFullNames[idx] : slot.ToString("X2");
        }

        private static XDocument CreateWorksheet(List<string[]> rows) {
            XElement sheetData = new XElement(spreadsheetNs + "sheetData");
            sheetData.Add(CreateRow(1, new[] { "NAME", "AUTHOR(S)", "MUSIC AUTHOR(S)", "TRACK SLOT", "MUSIC SLOT", "" }, true));
            for (int i = 0; i < rows.Count; i++)
                sheetData.Add(CreateRow(i + 2, rows[i], false));

            return new XDocument(
                new XElement(spreadsheetNs + "worksheet",
                    new XElement(spreadsheetNs + "sheetViews",
                        new XElement(spreadsheetNs + "sheetView", new XAttribute("workbookViewId", 0))),
                    new XElement(spreadsheetNs + "sheetFormatPr",
                        new XAttribute("customHeight", 1),
                        new XAttribute("defaultColWidth", "12.63"),
                        new XAttribute("defaultRowHeight", "15.75")),
                    new XElement(spreadsheetNs + "cols",
                        Column(1, 1, "4.63"),
                        Column(2, 2, "53.0"),
                        Column(3, 4, "52.38"),
                        Column(5, 5, "24.13"),
                        Column(6, 6, "23.13"),
                        Column(7, 7, "22.25")),
                    sheetData));
        }

        private static XElement CreateRow(int rowNumber, string[] values, bool header) {
            XElement row = new XElement(spreadsheetNs + "row", new XAttribute("r", rowNumber));
            row.Add(Cell($"A{rowNumber}", "", header ? 1 : 3));
            for (int i = 0; i < 6; i++) {
                int column = i + 2;
                int style = header ? 2 : (column == 7 ? 4 : 5);
                row.Add(Cell($"{(char)('A' + column - 1)}{rowNumber}", values[i] ?? "", style));
            }
            return row;
        }

        private static XElement Cell(string reference, string value, int style) {
            return new XElement(spreadsheetNs + "c",
                new XAttribute("r", reference),
                new XAttribute("s", style),
                new XAttribute("t", "inlineStr"),
                new XElement(spreadsheetNs + "is",
                    new XElement(spreadsheetNs + "t",
                        new XAttribute(XNamespace.Xml + "space", "preserve"), value)));
        }

        private static XElement Column(int min, int max, string width) {
            return new XElement(spreadsheetNs + "col",
                new XAttribute("min", min),
                new XAttribute("max", max),
                new XAttribute("width", width),
                new XAttribute("customWidth", 1));
        }

        private static XDocument CreateWorkbook() {
            return new XDocument(
                new XElement(spreadsheetNs + "workbook",
                    new XAttribute(XNamespace.Xmlns + "r", relationshipNs),
                    new XElement(spreadsheetNs + "sheets",
                        new XElement(spreadsheetNs + "sheet", new XAttribute("name", "Tracklist"), new XAttribute("sheetId", 1), new XAttribute(relationshipNs + "id", "rId1")),
                        new XElement(spreadsheetNs + "sheet", new XAttribute("name", "Variants"), new XAttribute("sheetId", 2), new XAttribute(relationshipNs + "id", "rId2")))));
        }

        private static XDocument CreateStyles() {
            XElement border = new XElement(spreadsheetNs + "border",
                BorderSide("left"), BorderSide("right"), BorderSide("top"), BorderSide("bottom"));

            return new XDocument(
                new XElement(spreadsheetNs + "styleSheet",
                    new XElement(spreadsheetNs + "fonts", new XAttribute("count", 4),
                        Font(false, "FF000000"),
                        Font(false, "FFFFFFFF"),
                        Font(true, "FFFFFFFF"),
                        Font(true, "FF000000")),
                    new XElement(spreadsheetNs + "fills", new XAttribute("count", 3),
                        new XElement(spreadsheetNs + "fill", new XElement(spreadsheetNs + "patternFill", new XAttribute("patternType", "none"))),
                        new XElement(spreadsheetNs + "fill", new XElement(spreadsheetNs + "patternFill", new XAttribute("patternType", "gray125"))),
                        new XElement(spreadsheetNs + "fill", new XElement(spreadsheetNs + "patternFill", new XAttribute("patternType", "solid"), new XElement(spreadsheetNs + "fgColor", new XAttribute("rgb", "FF000000")), new XElement(spreadsheetNs + "bgColor", new XAttribute("rgb", "FF000000"))))),
                    new XElement(spreadsheetNs + "borders", new XAttribute("count", 2), new XElement(spreadsheetNs + "border"), border),
                    new XElement(spreadsheetNs + "cellStyleXfs", new XAttribute("count", 1), new XElement(spreadsheetNs + "xf", new XAttribute("numFmtId", 0), new XAttribute("fontId", 0), new XAttribute("fillId", 0), new XAttribute("borderId", 0))),
                    new XElement(spreadsheetNs + "cellXfs", new XAttribute("count", 6),
                        Xf(0, 0, 0, false),
                        Xf(1, 2, 1, false),
                        Xf(2, 2, 1, true),
                        Xf(2, 2, 1, true),
                        Xf(2, 2, 1, true),
                        Xf(3, 0, 1, true)),
                    new XElement(spreadsheetNs + "cellStyles", new XAttribute("count", 1), new XElement(spreadsheetNs + "cellStyle", new XAttribute("name", "Normal"), new XAttribute("xfId", 0), new XAttribute("builtinId", 0)))));
        }

        private static XElement Font(bool bold, string color) {
            XElement font = new XElement(spreadsheetNs + "font",
                new XElement(spreadsheetNs + "sz", new XAttribute("val", 10)),
                new XElement(spreadsheetNs + "color", new XAttribute("rgb", color)),
                new XElement(spreadsheetNs + "name", new XAttribute("val", "Arial")));
            if (bold)
                font.AddFirst(new XElement(spreadsheetNs + "b"));
            return font;
        }

        private static XElement Xf(int fontId, int fillId, int borderId, bool centered) {
            XElement xf = new XElement(spreadsheetNs + "xf",
                new XAttribute("numFmtId", 0),
                new XAttribute("fontId", fontId),
                new XAttribute("fillId", fillId),
                new XAttribute("borderId", borderId),
                new XAttribute("xfId", 0),
                new XAttribute("applyFont", 1),
                new XAttribute("applyFill", 1),
                new XAttribute("applyBorder", 1),
                new XAttribute("applyAlignment", 1));
            xf.Add(new XElement(spreadsheetNs + "alignment",
                centered ? new XAttribute("horizontal", "center") : null,
                new XAttribute("vertical", "bottom")));
            return xf;
        }

        private static XElement BorderSide(string name) {
            return new XElement(spreadsheetNs + name,
                new XAttribute("style", "thin"),
                new XElement(spreadsheetNs + "color", new XAttribute("rgb", "FF000000")));
        }

        private static XDocument CreateContentTypes() {
            XNamespace ns = "http://schemas.openxmlformats.org/package/2006/content-types";
            return new XDocument(new XElement(ns + "Types",
                new XElement(ns + "Default", new XAttribute("Extension", "rels"), new XAttribute("ContentType", "application/vnd.openxmlformats-package.relationships+xml")),
                new XElement(ns + "Default", new XAttribute("Extension", "xml"), new XAttribute("ContentType", "application/xml")),
                new XElement(ns + "Override", new XAttribute("PartName", "/xl/workbook.xml"), new XAttribute("ContentType", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml")),
                new XElement(ns + "Override", new XAttribute("PartName", "/xl/worksheets/sheet1.xml"), new XAttribute("ContentType", "application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml")),
                new XElement(ns + "Override", new XAttribute("PartName", "/xl/worksheets/sheet2.xml"), new XAttribute("ContentType", "application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml")),
                new XElement(ns + "Override", new XAttribute("PartName", "/xl/styles.xml"), new XAttribute("ContentType", "application/vnd.openxmlformats-officedocument.spreadsheetml.styles+xml"))));
        }

        private static XDocument CreateRootRelationships() {
            return new XDocument(new XElement(packageRelationshipNs + "Relationships",
                new XElement(packageRelationshipNs + "Relationship", new XAttribute("Id", "rId1"), new XAttribute("Type", "http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument"), new XAttribute("Target", "xl/workbook.xml"))));
        }

        private static XDocument CreateWorkbookRelationships() {
            return new XDocument(new XElement(packageRelationshipNs + "Relationships",
                new XElement(packageRelationshipNs + "Relationship", new XAttribute("Id", "rId1"), new XAttribute("Type", "http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet"), new XAttribute("Target", "worksheets/sheet1.xml")),
                new XElement(packageRelationshipNs + "Relationship", new XAttribute("Id", "rId2"), new XAttribute("Type", "http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet"), new XAttribute("Target", "worksheets/sheet2.xml")),
                new XElement(packageRelationshipNs + "Relationship", new XAttribute("Id", "rId3"), new XAttribute("Type", "http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles"), new XAttribute("Target", "styles.xml"))));
        }

        private static void WriteXml(ZipArchive archive, string path, XDocument document) {
            ZipArchiveEntry entry = archive.CreateEntry(path, CompressionLevel.Optimal);
            using StreamWriter writer = new StreamWriter(entry.Open(), new UTF8Encoding(false));
            document.Save(writer, SaveOptions.DisableFormatting);
        }
    }
}
