using System;
using System.ComponentModel;
using System.Globalization;
using System.IO;
using System.Runtime.InteropServices;
using System.Windows;

namespace Pulsar_Pack_Creator
{
    public partial class CrashWindow : Window
    {
        private char region = 'P';
        public CrashWindow()
        {
            InitializeComponent();
        }

        public void Load(string fileName)
        {
            byte[] raw = File.ReadAllBytes(fileName);
            int magic = raw[3] | (raw[2] << 8) | (raw[1] << 16) | (raw[0] << 24);
            if (magic != 0x50554c44)
            {
                MsgWindow.Show("Invalid Crash File.");
                return;
            }
            Show();
            ImportCrash(raw);
        }
        protected override void OnClosing(CancelEventArgs e)
        {
            e.Cancel = true;
            Hide();
        }

        public void ImportCrash(byte[] raw)
        {
            byte[] rawFileBytes = PadCrashData(raw, Marshal.SizeOf<PulsarGame.ExceptionFile>());
            PulsarGame.ExceptionFile rawFile = PulsarGame.BytesToStruct<PulsarGame.ExceptionFile>(rawFileBytes);
            region = ((char)(byte)rawFile.region);

            PulsarGame.OSError error = (PulsarGame.OSError)rawFile.error;
            string errorString;
            if (error == PulsarGame.OSError.OSERROR_DSI) errorString = "DSI";
            else if (error == PulsarGame.OSError.OSERROR_ISI) errorString = "ISI";
            else if (error == PulsarGame.OSError.OSERROR_FLOATING_POINT || error == PulsarGame.OSError.OSERROR_FPE) errorString = "Float";
            else errorString = $"{error}";

            string crash = $"Error: {errorString}\nRegion: {region}";
            crash += BuildCrashMetadata(rawFile);
            crash += $"SRR0: 0x{rawFile.srr0.gpr:X8} {CheckSymbols(rawFile.srr0.gpr)}\n" +
                $"SRR1: 0x{rawFile.srr1.gpr:X8}\nMSR:  0x{rawFile.msr.gpr:X8}\nCR:   0x{rawFile.cr.gpr:X8}\nLR:   0x{rawFile.lr.gpr:X8} {CheckSymbols(rawFile.lr.gpr)}";
            crash = crash + "\n\nGPRs";
            for (int i = 0; i < 8; i++)
            {
                crash = crash + "\n";
                for (int j = 0; j < 4; j++)
                {
                    int idx = i + j * 8;
                    crash = crash + $"R{idx:D2}: 0x{rawFile.gprs[idx].gpr:X8} ";
                }

            }

            crash = crash + $"\n\nFPRs FSCR: 0x{rawFile.fpscr.fpr:X8}";
            for (int i = 0; i < 8; i++)
            {
                crash = crash + "\n";
                for (int j = 0; j < 4; j++)
                {
                    int idx = i + j * 8;
                    crash = crash + $"F{idx:D2}: {rawFile.fprs[idx].fpr.ToString("E2").PadLeft(10)} ";
                }
            }
            crash = crash + "\n\nStack Frame";
            for (int i = 0; i < 10; i++)
            {
                crash = crash + "\n";
                crash = crash + $"SP: 0x{rawFile.frames[i].sp:X8} LR: 0x{rawFile.frames[i].lr:X8} {CheckSymbols(rawFile.frames[i].lr)}";
            }
            CrashText.Text = crash;
        }

        private static byte[] PadCrashData(byte[] raw, int requiredSize)
        {
            if (raw.Length >= requiredSize) return (byte[])raw.Clone();
            byte[] padded = new byte[requiredSize];
            Buffer.BlockCopy(raw, 0, padded, 0, raw.Length);
            return padded;
        }

        private static string BuildCrashMetadata(PulsarGame.ExceptionFile rawFile)
        {
            if (rawFile.extra.version < 2) return "\n";

            string section = FormatNamedId(rawFile.extra.sectionId, CrashMetadataResolver.GetSectionName(rawFile.extra.sectionId));
            string page = FormatNamedId(rawFile.extra.pageId, CrashMetadataResolver.GetPageName(rawFile.extra.pageId));
            string lastTrackSzs = string.IsNullOrWhiteSpace(rawFile.extra.lastTrackSzs) ? "Unknown" : rawFile.extra.lastTrackSzs;
            string contexts = CrashMetadataResolver.GetEnabledContexts(rawFile.extra.context, rawFile.extra.context2);
            bool looseOverridesEnabled = (rawFile.extra.flags & 1u) != 0;
            bool customCharacterEnabled = (rawFile.extra.flags & 2u) != 0;
            bool hasPatchFiles = rawFile.extra.looseOverrideFileCount > 0;
            string myStuff = GetMyStuffState(rawFile.extra.version, rawFile.extra.myStuffState);

            return $"\n\nSection: {section}\nPage: {page}\nLast Track SZS: {lastTrackSzs}\nContexts: {contexts}\nCustom Character Enabled: {customCharacterEnabled}\nMy Stuff: {myStuff}\nPatches Enabled: {looseOverridesEnabled}\nPatches Folder Has Files: {hasPatchFiles} ({rawFile.extra.looseOverrideFileCount})\n\n";
        }

        private static string GetMyStuffState(uint version, uint myStuffState)
        {
            if (version < 3) return "Unknown";
            if (myStuffState == 1) return "Enabled";
            if (myStuffState == 2) return "Music Only";
            return "Disabled";
        }

        private static string FormatNamedId(int id, string name)
        {
            if (id < 0) return "Unknown";
            return string.IsNullOrWhiteSpace(name) ? $"0x{id:X}" : $"{name} (0x{id:X})";
        }

        private void OnDropFile(object sender, DragEventArgs e)
        {
            if (e.Data.GetDataPresent(DataFormats.FileDrop))
            {
                string[] files = (string[])e.Data.GetData(DataFormats.FileDrop);
                IO.BigEndianReader bin = new IO.BigEndianReader(File.Open(files[0], FileMode.Open));
                int magic = bin.ReadInt32();
                bin.BaseStream.Position -= 4;
                if (magic == 0x50554c44)
                {
                    bin.Close();
                    Load(files[0]);
                }
            }
        }

        private string CheckSymbols(uint address)
        {
            try
            {
                char[] delims = new[] { '\r', '\n' };
                string[] lines = PulsarRes.MAP.Split(delims, StringSplitOptions.RemoveEmptyEntries);
                string[] splices = PulsarRes.versions.Replace("-8", " 8").Replace(":", "").Split(delims, StringSplitOptions.RemoveEmptyEntries);

                int idx = Array.IndexOf(splices, $"[{region}]");
                if (idx == -1) throw new Exception();
                if (region != 'P')
                {
                    idx++;
                    while (true)
                    {
                        if (splices[idx] == "#") throw new Exception();
                        string[] curSplice = splices[idx].Split(' ');
                        int isNegative = curSplice[2].Contains('-') ? -1 : 1;
                        uint palAddress = (uint)((int)address - isNegative * int.Parse(curSplice[2].Replace("-0x", "").Replace("+0x", ""), NumberStyles.HexNumber));
                        if (uint.Parse(curSplice[0], NumberStyles.HexNumber) <= palAddress &&
                        palAddress < uint.Parse(curSplice[1], NumberStyles.HexNumber))
                        {
                            address = palAddress;
                            break;
                        }
                        idx++;
                    }
                }

                for (int i = 0; i < lines.Length - 1; i++)
                {
                    string[] curLine = lines[i].Split(' ');
                    string[] nextLine = lines[i + 1].Split(' ');
                    if (uint.Parse(curLine[0], NumberStyles.HexNumber) <= address &&
                        address < uint.Parse(nextLine[0], NumberStyles.HexNumber))
                    {
                        return region == 'P' ? curLine[1] : $"{curLine[1]}(0x{address:X8})";
                    }
                }
                throw new Exception();
            }
            catch
            {
                return "";
            }
        }
    }
}
