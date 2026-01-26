using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;


namespace Pulsar_Pack_Creator.IO
{
    class Importer : IOBase
    {
        public Importer(MainWindow window, in byte[] raw) : base(window)
        {
            cups = window.cups;
            this.raw = raw;
            regsExperts = window.regsExperts;
        }

        readonly byte[] raw;
        List<MainWindow.Cup> cups;
        public new ushort ctsCupCount { get; private set; }
        uint configVersion;
        uint cupVersion;
        public string date { get; private set; }
        public string[,,] regsExperts { get; private set; }

        public Result ImportV3()
        {
            PulsarGame.BinaryHeader header = PulsarGame.BytesToStruct<PulsarGame.BinaryHeader>(raw.ToArray());

            //Read HEADER
            parameters.modFolderName = header.modFolderName.TrimStart('/');

            Result ret;
            //INFO Reading
            ret = ReadInfo(CreateSubCat<PulsarGame.InfoHolder>(raw, header.offsetToInfo));
            if (ret != Result.Success) return ret;

            //CUPS reading
            ret = ReadCups(CreateSubCat<PulsarGame.CupsHolderV3>(raw, header.offsetToCups));
            if (ret != Result.Success) return ret;

            nint tracksOffset = header.offsetToCups
                + Marshal.OffsetOf(typeof(PulsarGame.CupsHolderV3), "totalVariantCount")
                + Marshal.SizeOf(typeof(int));
            nint trackSize = Marshal.SizeOf(typeof(PulsarGame.TrackV3));
            
            // Account for padding tracks if cup count is odd
            int totalTracks = ctsCupCount * 4;
            if (ctsCupCount % 2 != 0) totalTracks += 4;
            
            nint variantsOffset = tracksOffset + trackSize * totalTracks;
            nint variantSize = Marshal.SizeOf(typeof(PulsarGame.Variant));

            PulsarGame.TrackV3[] curCup = new PulsarGame.TrackV3[4];
            // We'll collect all variants for a cup into a single array so each track can read its portion.
            List<PulsarGame.Variant> variantsList = new List<PulsarGame.Variant>();
            for (int i = 0; i < ctsCupCount; i++)
            {
                for (int j = 0; j < 4; j++)
                {
                    curCup[j] = CreateSubCat<PulsarGame.TrackV3>(raw, (int)tracksOffset);
                    for (int k = 0; k < curCup[j].variantCount; k++)
                    {
                        // Append each variant for the current track in sequence
                        variantsList.Add(CreateSubCat<PulsarGame.Variant>(raw, (int)variantsOffset));
                        variantsOffset += variantSize;
                    }
                    tracksOffset += trackSize;
                }
                // Convert the gathered list to an array and pass it into the cup
                ReadCup(curCup, variantsList.ToArray());
                variantsList.Clear();

            }

            //BMG reading
            int bmgSize;
            ret = ReadBMG(raw.Skip(header.offsetToBMG).Take(raw.Length - header.offsetToBMG).ToArray(), out bmgSize);
            if (ret != Result.Success) return ret;

            //FILE reading
            ret = ReadFile(raw.Skip(header.offsetToBMG + bmgSize).Take(raw.Length - header.offsetToBMG).ToArray());
            if (ret != Result.Success) return ret;

            RequestBMGAction(false);
            using StreamReader bmgSR = new StreamReader("temp/BMG.txt");
            using StreamReader fileSR = new StreamReader("temp/files.txt");

            ParseBMGAndFILE(bmgSR, fileSR);

            return Result.Success;
        }

        public Result Import()
        {

            try
            {
                PulsarGame.BinaryHeader header = PulsarGame.BytesToStruct<PulsarGame.BinaryHeader>(raw.ToArray());
                //PulsarGame.InfoHolder infoHolder = CreateSubCat<PulsarGame.InfoHolder>(raw, header.offsetToInfo);
                //PulsarGame.CupsHolder cupsHolder = CreateSubCat<PulsarGame.CupsHolder>(raw, header.offsetToCups);

                //Read HEADER
                configVersion = (uint)Math.Abs(header.version);
                if (header.magic != configMagic || header.version > CONFIGVERSION) return Result.InvalidConfigFile;

                Result ret = Result.UnknownError;
                if (configVersion == 1) ret = ImportV1();
                else if (configVersion == 2) ret = ImportV2();
                else ret = ImportV3();

                return ret;
            }

            catch (Exception ex)
            {
                error = ex.ToString();
                return Result.UnknownError;
            }
            finally
            {
                File.Delete("temp/bmg.bmg");
                File.Delete("temp/bmg.txt");
                File.Delete("temp/Config.pul");
                File.Delete("temp/files.txt");
            }
        }

        private Result ReadInfo(PulsarGame.InfoHolder raw)
        {
            uint magic = raw.header.magic;
            uint infoVersion = raw.header.version;
            if (magic != infoMagic || infoVersion != INFOVERSION) return Result.BadInfo;

            PulsarGame.Info info = raw.info;
            parameters.prob100cc = info.prob100cc;
            parameters.prob150cc = info.prob150cc;
            parameters.probMirror = 100 - (parameters.prob100cc + parameters.prob150cc);
            parameters.wiimmfiRegion = info.wiimmfiRegion;
            parameters.trackBlocking = info.trackBlocking;
            parameters.hasTTTrophies = info.hasTTTrophies == 1 ? true : false;
            parameters.has200cc = info.has200cc == 1 ? true : false;
            parameters.hasUMTs = info.hasUMTs == 1 ? true : false;
            parameters.hasFeather = info.hasFeather == 1 ? true : false;
            parameters.hasMegaTC = info.hasMegaTC == 1 ? true : false;
            int timer = info.chooseNextTrackTimer;
            parameters.chooseNextTrackTimer = (byte)(timer == 0 ? 10 : timer);
            return Result.Success;
        }

        private Result ReadCups(PulsarGame.CupsHolderV3 raw)
        {
            uint magic = raw.header.magic;
            cupVersion = raw.header.version;
            if (magic != cupsMagic || cupVersion > CUPSVERSION) return Result.BadCups;
            cups.Clear();
            //PulsarGame.Cups rawCups = raw.cups;
            ctsCupCount = raw.ctsCupCount;
            parameters.regsMode = raw.regsMode;
            return Result.Success;
        }
        private Result ReadCups(PulsarGame.CupsHolderV2 raw)
        {
            uint magic = raw.header.magic;
            cupVersion = raw.header.version;
            if (magic != cupsMagic || cupVersion > CUPSVERSION) return Result.BadCups;
            cups.Clear();
            PulsarGame.Cups rawCups = raw.cups;
            ctsCupCount = rawCups.ctsCupCount;
            parameters.regsMode = rawCups.regsMode;
            return Result.Success;
        }

        private Result ReadCups(PulsarGame.CupsHolderV1 raw)
        {
            uint magic = raw.header.magic;
            cupVersion = raw.header.version;
            if (magic != cupsMagic || cupVersion > CUPSVERSION) return Result.BadCups;
            cups.Clear();
            PulsarGame.CupsV1 rawCups = raw.cups;
            ctsCupCount = rawCups.ctsCupCount;
            parameters.regsMode = rawCups.regsMode;
            return Result.Success;
        }
        private void ReadCup(PulsarGame.TrackV3[] tracks, PulsarGame.Variant[] variants)
        {
            cups.Add(new MainWindow.Cup(tracks, variants));
        }
        private void ReadCup(PulsarGame.CupV2 raw)
        {
            cups.Add(new MainWindow.Cup(raw));
        }
        private void ReadCup(PulsarGame.CupV1 raw)
        {
            cups.Add(new MainWindow.Cup(raw));
        }

        private Result ReadBMG(byte[] raw, out int bmgSize)
        {
            bmgSize = 0;
            using (BigEndianReader bin = new BigEndianReader(new MemoryStream(raw)))
            {

                ulong magic = bin.ReadUInt64();
                if (magic != bmgMagic) return Result.BadBMG;
                bmgSize = bin.ReadInt32();
                bin.BaseStream.Position -= 12;
                using (BigEndianWriter bmg = new BigEndianWriter(File.Create("temp/bmg.bmg")))
                {
                    bmg.Write(bin.ReadBytes(bmgSize));
                }

                return Result.Success;
            }
        }

        private Result ReadFile(byte[] raw)
        {
            using (BigEndianReader bin = new BigEndianReader(new MemoryStream(raw)))
            {
                uint magic = bin.ReadUInt32();
                if (magic != fileMagic) return Result.BadFile;
                bin.BaseStream.Position -= 4;
                using (BigEndianWriter file = new BigEndianWriter(File.Create("temp/files.txt")))
                {
                    file.Write(bin.ReadBytes((int)bin.BaseStream.Length));
                }
            }
            return Result.Success;
        }


        private void ParseBMGAndFILE(StreamReader bmgSR, StreamReader fileSR)
        {
            bmgSR.ReadLine(); //#BMG
            string curLine = bmgSR.ReadLine();

            while (curLine != null)
            {
                if (curLine != "")
                {
                    uint bmgId = 0;
                    bool ret = false;
                    string[] parts = curLine.Split('=');
                    if (parts.Length >= 2)
                    {
                        string idPart = parts[0].Trim();
                        ret = uint.TryParse(idPart, NumberStyles.HexNumber, CultureInfo.InvariantCulture, out bmgId);
                    }

                    if (ret)
                    {
                        if (bmgId == 0x2847) date = curLine.Split(' ')[curLine.Split(' ').Length - 1];
                        else if (bmgId >= 0x10000 && bmgId < 0x600000)
                        {
                            string content = "";
                            try
                            {
                                // Re-join parts in case content contained '='
                                content = string.Join("=", parts.Skip(1)).TrimStart(' ');
                            }
                            catch
                            {
                                ret = false;
                            }
                            if (ret)
                            {
                                const uint VARIANT_TRACKS_BASE = 0x420000u;
                                const uint VARIANT_AUTHORS_BASE = 0x520000u;
                                uint type = 0;
                                uint trackIndex = 0; // The track index (0-based across all cups)
                                int variantIdxParsed = 0; // 0 == base

                                if (bmgId >= VARIANT_TRACKS_BASE && bmgId < VARIANT_AUTHORS_BASE)
                                {
                                    // Variant track block (0x420000..0x51FFFF)
                                    uint raw = bmgId - VARIANT_TRACKS_BASE;
                                    trackIndex = (raw >> 4); // track index in upper bits
                                    variantIdxParsed = (int)(raw & 0xF);
                                    type = (uint)BMGIds.BMG_TRACKS;
                                }
                                else if (bmgId >= VARIANT_AUTHORS_BASE && bmgId < VARIANT_AUTHORS_BASE + 0x100000u)
                                {
                                    // Variant authors block (0x520000..0x61FFFF)
                                    uint raw = bmgId - VARIANT_AUTHORS_BASE;
                                    trackIndex = (raw >> 4);
                                    variantIdxParsed = (int)(raw & 0xF);
                                    type = (uint)BMGIds.BMG_AUTHORS;
                                }
                                else
                                {
                                    uint rawType = bmgId & 0xFFFF0000; // original high word block
                                    uint rest = bmgId & 0xFFFF;
                                    // Normalize type block to base group 0x20000 (tracks) or 0x30000 (authors)
                                    uint typeGroup = (rawType >> 16) & 0xFFFF;
                                    if (typeGroup == 1)
                                    {
                                        type = (uint)BMGIds.BMG_CUPS;
                                        trackIndex = rest; // For cups, this is the cup index
                                    }
                                    else if (typeGroup == 2)
                                    {
                                        // BMG_TRACKS base range (0x20000 + trackIdx) - common track name
                                        // The entire lower 16 bits is the track index, no variant component
                                        type = (uint)BMGIds.BMG_TRACKS;
                                        trackIndex = rest; // Full 16 bits is the track index
                                        variantIdxParsed = -1; // common name
                                    }
                                    else if (typeGroup == 3)
                                    {
                                        // BMG_AUTHORS base range (0x30000 + trackIdx) - main track author
                                        // The entire lower 16 bits is the track index, no variant component
                                        type = (uint)BMGIds.BMG_AUTHORS;
                                        trackIndex = rest; // Full 16 bits is the track index
                                        variantIdxParsed = 0; // main track author
                                    }
                                    else
                                    {
                                        // For other ranges (0x4XXXX, etc. that didn't match variant blocks),
                                        // try to normalize as before (legacy handling)
                                        uint normalizedTypeGroup = (typeGroup % 2 == 0) ? 2u : 3u;
                                        type = normalizedTypeGroup << 16;
                                        // In legacy format, bits 12-15 of rest hold variant index
                                        uint lowIdx = (rest & 0xF000) >> 12;
                                        trackIndex = rest & 0xFFF; // Lower 12 bits is track index
                                        if (lowIdx >= 8)
                                            variantIdxParsed = -1; // use common/base name
                                        else if (lowIdx == 0)
                                        {
                                            if (type == (uint)BMGIds.BMG_AUTHORS)
                                                variantIdxParsed = 0;
                                            else
                                                variantIdxParsed = -1;
                                        }
                                        else
                                            variantIdxParsed = (int)lowIdx; // old mapping 1..7
                                    }
                                }

                                int cupIdx = (int)trackIndex / 4;
                                if (cupIdx < ctsCupCount)
                                {
                                    int trackIdx = (int)trackIndex % 4;
                                    MainWindow.Cup.Track track = cups[cupIdx].tracks[trackIdx];
                                    switch (type)
                                    {
                                        case (uint)BMGIds.BMG_CUPS:
                                            if ((int)trackIndex < ctsCupCount) cups[(int)trackIndex].name = content;
                                            break;
                                        case (uint)BMGIds.BMG_TRACKS:
                                        case (uint)BMGIds.BMG_AUTHORS:
                                            {
                                                int parsedVariantIdx = variantIdxParsed; // -1 means common/base name

                                                MainWindow.Cup.Track.Variant variant;
	                                                if (parsedVariantIdx == -1)
	                                                {
	                                                    // Base BMG_TRACKS block (0x20000 + trackIdx): import as the common (BMG_TRACKS) name.
	                                                    // Always set the track.commonName from this base entry. Only overwrite the
	                                                    // main track name when there are no variants configured for the track.
	                                                    if (type == (uint)BMGIds.BMG_TRACKS)
	                                                    {
	                                                        bool hadVariantMarker;
	                                                        string sanitizedContent = StripCommonNameVisualMarker(content, out hadVariantMarker);
	                                                        // Use the base entry as the common name unconditionally
	                                                        track.commonName = sanitizedContent;
	                                                        // Preserve per-variant/main names when variants exist; if there are no variants,
	                                                        // populate the main track name from the common name for compatibility.
	                                                        if (track.variants.Count == 0)
	                                                            track.main.trackName = sanitizedContent;
	                                                    }
	                                                    break;
	                                                }
                                                else if (parsedVariantIdx == 0)
                                                {
                                                    // main track
                                                    variant = track.main;
                                                }
                                                else
                                                {
                                                    // variant mapping: parsedVariantIdx 1 maps to track.variants[0]
                                                    int vIndex = parsedVariantIdx - 1;
                                                    if (vIndex >= 0 && vIndex < track.variants.Count)
                                                        variant = track.variants[vIndex];
                                                    else
                                                        variant = track.main;
                                                }
                                                if (type == (uint)BMGIds.BMG_AUTHORS)
                                                {
                                                    variant.authorName = content;
                                                    break;
                                                }
                                                else
                                                {
                                                    if (content.Contains("\\c{red3}"))
                                                    {
                                                        string[] split = content.Split("\\c{red3}");
                                                        variant.trackName = split[0].Trim();
                                                        variant.versionName = split[1].Split("\\c{off}")[0];
                                                    }
                                                    else variant.trackName = content.Trim();
                                                }
                                            }
                                            break;
                                    }
                                }

                            }
                        }
                    }
                }
                curLine = bmgSR.ReadLine();
            }
            fileSR.ReadLine();
            curLine = fileSR.ReadLine(); //FILE
            while (curLine != null)
            {
                if (curLine != "")
                {
                    if (curLine.Contains("?"))
                    {
                        string[] split = curLine.Split("?");
                        if (uint.TryParse(split[0], NumberStyles.HexNumber, null, out uint idx))
                        {
                            if (split.Length > 1 && split[1] != "") cups[(int)idx].iconName = split[1];
                        }
                    }
                    else if (curLine.Contains("*"))
                    {
                        string[] split = curLine.Split("=");
                        if (uint.TryParse(split[0], NumberStyles.HexNumber, null, out uint id))
                        {
                            int cupIdx = (int)id / 4;
                            if (cupIdx < 8)
                            {
                                int trackIdx = (int)id % 4;
                                string[] names = split[1].Split("*");
                                if (names.Length > 0 && names.Length <= 4)
                                {
                                    for (int i = 0; i < names.Length; i++)
                                    {
                                        regsExperts[cupIdx, trackIdx, i] = names[i];
                                    }
                                }
                            }
                        }
                    }
                    else if (curLine.Contains("|"))
                    {
                        string[] split = curLine.Split("=");
                        if (uint.TryParse(split[0], NumberStyles.HexNumber, null, out uint id))
                        {
                            int cupIdx = (int)(id & 0xFFF) / 4;
                            int variantIdx = (int)(id & 0xf000) >> 12;
                            if (cupIdx < ctsCupCount)
                            {
                                int trackIdx = (int)(id & 0xFFF) % 4;
                                string[] names = split[1].Split("|");
                                if (names.Length > 0)
                                {
                                    if (variantIdx == 0)
                                    {
                                        cups[cupIdx].tracks[trackIdx].main.fileName = names[0];
                                        for (int i = 1; i < names.Length; i++)
                                        {
                                            cups[cupIdx].tracks[trackIdx].expertFileNames[i - 1] = names[i];
                                        }
                                        
                                        string trackFile = names[0];
                                        bool hasValidTrackFile = !string.IsNullOrWhiteSpace(trackFile) && trackFile != "File";
                                        bool hasExplicit200 = names.Length >= 3 && !string.IsNullOrWhiteSpace(names[2]);
                                        if (hasValidTrackFile && !hasExplicit200)
                                        {
                                            cups[cupIdx].tracks[trackIdx].expertFileNames[1] = trackFile; // 200cc index
                                        }
                                    }
                                    else if (variantIdx <= 7) 
                                    {
                                        int variantIndex = variantIdx - 1;
                                        if (variantIndex < cups[cupIdx].tracks[trackIdx].variants.Count)
                                        {
                                            cups[cupIdx].tracks[trackIdx].variants[variantIndex].fileName = names[0];
                                            // Read expert file names for variants
                                            for (int i = 1; i < names.Length && i <= 4; i++)
                                            {
                                                cups[cupIdx].tracks[trackIdx].variants[variantIndex].expertFileNames[i - 1] = names[i];
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                curLine = fileSR.ReadLine();
            }
        }

        private static string StripCommonNameVisualMarker(string content, out bool hadVariantMarker)
        {
            hadVariantMarker = false;

            if (string.IsNullOrEmpty(content))
                return content;

            string trimmed = content.TrimEnd();

            // Legacy marker
            if (trimmed.EndsWith(" ->", StringComparison.Ordinal))
            {
                hadVariantMarker = true;
                return trimmed.Substring(0, trimmed.Length - 3).TrimEnd();
            }

            // Plain marker
            if (trimmed.EndsWith(" *", StringComparison.Ordinal))
            {
                hadVariantMarker = true;
                return trimmed.Substring(0, trimmed.Length - 2).TrimEnd();
            }

            // Colored marker (e.g. " ... \\c{red1}*")
            if (trimmed.EndsWith("*", StringComparison.Ordinal))
            {
                int spaceIndex = trimmed.LastIndexOf(' ');
                if (spaceIndex >= 0 && spaceIndex < trimmed.Length - 1)
                {
                    string suffix = trimmed.Substring(spaceIndex + 1);
                    if (suffix.EndsWith("*", StringComparison.Ordinal))
                    {
                        string escapeSequence = suffix.Substring(0, suffix.Length - 1);
                        if (IsEscapeSequence(escapeSequence))
                        {
                            hadVariantMarker = true;
                            return trimmed.Substring(0, spaceIndex).TrimEnd();
                        }
                    }
                }
            }

            return trimmed;
        }

        private static bool IsEscapeSequence(string value)
        {
            if (string.IsNullOrEmpty(value) || value[0] != '\\')
                return false;

            int openBraceIndex = value.IndexOf('{');
            if (openBraceIndex <= 1)
                return false;

            for (int i = 1; i < openBraceIndex; i++)
            {
                if (!char.IsLetter(value[i]))
                    return false;
            }

            int closeBraceIndex = value.IndexOf('}', openBraceIndex + 1);
            return closeBraceIndex == value.Length - 1;
        }

        public Result ImportV2()
        {
            PulsarGame.BinaryHeader header = PulsarGame.BytesToStruct<PulsarGame.BinaryHeader>(raw.ToArray());

            //Read HEADER
            parameters.modFolderName = header.modFolderName.TrimStart('/');

            Result ret;
            //INFO Reading
            ret = ReadInfo(CreateSubCat<PulsarGame.InfoHolder>(raw, header.offsetToInfo));
            if (ret != Result.Success) return ret;

            //CUPS reading
            ret = ReadCups(CreateSubCat<PulsarGame.CupsHolderV2>(raw, header.offsetToCups));
            if (ret != Result.Success) return ret;

            nint offset = header.offsetToCups + Marshal.OffsetOf(typeof(PulsarGame.CupsHolderV2), "cups") + Marshal.OffsetOf(typeof(PulsarGame.Cups), "cupsArray");
            nint size = Marshal.SizeOf(typeof(PulsarGame.CupV2));


            for (int i = 0; i < ctsCupCount; i++)
            {
                PulsarGame.CupV2 cup = CreateSubCat<PulsarGame.CupV2>(raw, (int)offset);
                ReadCup(cup);
                offset += size;
            }

            //BMG reading
            int bmgSize;
            ret = ReadBMG(raw.Skip(header.offsetToBMG).Take(raw.Length - header.offsetToBMG).ToArray(), out bmgSize);
            if (ret != Result.Success) return ret;

            //FILE reading
            ret = ReadFile(raw.Skip(header.offsetToBMG + bmgSize).Take(raw.Length - header.offsetToBMG).ToArray());
            if (ret != Result.Success) return ret;

            RequestBMGAction(false);
            using StreamReader bmgSR = new StreamReader("temp/BMG.txt");
            using StreamReader fileSR = new StreamReader("temp/files.txt");

            ParseBMGAndFILE(bmgSR, fileSR);

            return Result.Success;
        }
        public Result ImportV1()
        {
            PulsarGame.BinaryHeader header = PulsarGame.BytesToStruct<PulsarGame.BinaryHeader>(raw.ToArray());

            //Read HEADER
            parameters.modFolderName = header.modFolderName.TrimStart('/');

            Result ret;
            //INFO Reading
            ret = ReadInfo(CreateSubCat<PulsarGame.InfoHolder>(raw, header.offsetToInfo));
            if (ret != Result.Success) return ret;

            //CUPS reading
            ret = ReadCups(CreateSubCat<PulsarGame.CupsHolderV1>(raw, header.offsetToCups));
            if (ret != Result.Success) return ret;

            nint offset = header.offsetToCups + Marshal.OffsetOf(typeof(PulsarGame.CupsHolderV1), "cups") + Marshal.OffsetOf(typeof(PulsarGame.CupsV1), "cupsArray");
            nint size = Marshal.SizeOf(typeof(PulsarGame.CupV1));


            for (int i = 0; i < ctsCupCount; i++)
            {
                PulsarGame.CupV1 cup = CreateSubCat<PulsarGame.CupV1>(raw, (int)offset);
                ReadCup(cup);
                offset += size;
            }

            //BMG reading
            int bmgSize;
            ret = ReadBMG(raw.Skip(header.offsetToBMG).Take(raw.Length - header.offsetToBMG).ToArray(), out bmgSize);
            if (ret != Result.Success) return ret;

            //FILE reading
            ret = ReadFile(raw.Skip(header.offsetToBMG + bmgSize).Take(raw.Length - header.offsetToBMG).ToArray());
            if (ret != Result.Success) return ret;

            RequestBMGAction(false);
            using StreamReader bmgSR = new StreamReader("temp/BMG.txt");
            using StreamReader fileSR = new StreamReader("temp/files.txt");

            ParseBMGAndFILE(bmgSR, fileSR);

            return Result.Success;
        }

    }



}
