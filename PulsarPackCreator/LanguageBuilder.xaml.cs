using Pulsar_Pack_Creator.Languages;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Net.Http;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;

namespace Pulsar_Pack_Creator
{
    public partial class LanguageBuilderWindow : Window
    {
        private readonly HttpClient httpClient = new HttpClient();
        private readonly LanguagePackBuilder builder = new LanguagePackBuilder();
        private Dictionary<TranslationTarget, TranslationTable> cachedSheets;
        private GameImageSheet cachedGameImages;
        private TrackNameTranslationSheets cachedTrackNameSheets;
        private string cachedSheetId;
        private string cachedGameImageSheetId;
        private string cachedTrackNameSheetId;
        private CancellationTokenSource cancellation;

        public LanguageBuilderWindow()
        {
            InitializeComponent();
            LanguageBox.ItemsSource = LanguageDefinition.All;
            LanguageBox.SelectedIndex = 0;
            PackFolder.Text = LanguagePackBuilder.FindDefaultPackRoot();
        }

        private void OnBrowsePackClick(object sender, RoutedEventArgs e)
        {
            using var dialog = new System.Windows.Forms.FolderBrowserDialog
            {
                Description = "Select the RetroRewind6 folder",
                UseDescriptionForTitle = true,
                SelectedPath = PackFolder.Text
            };
            if (dialog.ShowDialog() == System.Windows.Forms.DialogResult.OK) PackFolder.Text = dialog.SelectedPath;
        }

        private async void OnValidateClick(object sender, RoutedEventArgs e)
        {
            await RunAsync(async token =>
            {
                await LoadSheetsAsync(token, true);
                await LoadGameImagesAsync(token, true);
                await LoadTrackNameSheetsAsync(token, true);
                AppendLog("Required RR translation tabs, track names, variants, and game images loaded successfully.");
            });
        }

        private async void OnBuildSelectedClick(object sender, RoutedEventArgs e)
        {
            if (LanguageBox.SelectedItem is not LanguageDefinition selected) return;
            await RunAsync(async token =>
            {
                IReadOnlyDictionary<TranslationTarget, TranslationTable> sheets = await LoadSheetsAsync(token, false);
                GameImageSheet gameImages = await LoadGameImagesAsync(token, false);
                TrackNameTranslationSheets trackNameSheets = await LoadTrackNameSheetsAsync(token, false);
                await builder.BuildAsync(PackFolder.Text.Trim(), sheets, gameImages, trackNameSheets, new[] { selected }, new Progress<string>(AppendLog), token);
                AppendLog($"Finished {selected.DisplayName}.");
            });
        }

        private async void OnBuildAllClick(object sender, RoutedEventArgs e)
        {
            await RunAsync(async token =>
            {
                IReadOnlyDictionary<TranslationTarget, TranslationTable> sheets = await LoadSheetsAsync(token, false);
                GameImageSheet gameImages = await LoadGameImagesAsync(token, false);
                TrackNameTranslationSheets trackNameSheets = await LoadTrackNameSheetsAsync(token, false);
                await builder.BuildAsync(PackFolder.Text.Trim(), sheets, gameImages, trackNameSheets, LanguageDefinition.All, new Progress<string>(AppendLog), token);
                AppendLog("Finished all languages.");
            });
        }

        private async void OnUpdateConfigClick(object sender, RoutedEventArgs e)
        {
            await RunAsync(async token =>
            {
                TrackNameTranslationSheets trackNameSheets = await LoadTrackNameSheetsAsync(token, false);
                await builder.UpdateConfigTranslationsAsync(PackFolder.Text.Trim(), trackNameSheets, LanguageDefinition.All, new Progress<string>(AppendLog), token);
                AppendLog("Finished config translations.");
            });
        }

        private async Task<IReadOnlyDictionary<TranslationTarget, TranslationTable>> LoadSheetsAsync(CancellationToken token, bool forceRefresh)
        {
            string id = TranslationSheetClient.GetSpreadsheetId(SheetUrl.Text);
            if (!forceRefresh && cachedSheets != null && cachedSheetId == id) return cachedSheets;
            AppendLog("Downloading translation sheets...");
            cachedSheets = await new TranslationSheetClient(httpClient).LoadRequiredSheetsAsync(id, token);
            cachedSheetId = id;
            return cachedSheets;
        }

        private async Task<GameImageSheet> LoadGameImagesAsync(CancellationToken token, bool forceRefresh)
        {
            string id = TranslationSheetClient.GetSpreadsheetId(SheetUrl.Text);
            if (!forceRefresh && cachedGameImages != null && cachedGameImageSheetId == id) return cachedGameImages;
            AppendLog("Downloading RR: Game Image...");
            cachedGameImages = await new TranslationSheetClient(httpClient).LoadGameImagesAsync(id, token);
            cachedGameImageSheetId = id;
            return cachedGameImages;
        }

        private async Task<TrackNameTranslationSheets> LoadTrackNameSheetsAsync(CancellationToken token, bool forceRefresh)
        {
            string id = TranslationSheetClient.GetSpreadsheetId(SheetUrl.Text);
            if (!forceRefresh && cachedTrackNameSheets != null && cachedTrackNameSheetId == id) return cachedTrackNameSheets;
            AppendLog("Downloading RR: Track Name and RR: Variant Name...");
            cachedTrackNameSheets = await new TranslationSheetClient(httpClient).LoadTrackNameSheetsAsync(id, token);
            cachedTrackNameSheetId = id;
            return cachedTrackNameSheets;
        }

        private async Task RunAsync(Func<CancellationToken, Task> action)
        {
            SetBusy(true);
            cancellation?.Dispose();
            cancellation = new CancellationTokenSource();
            try
            {
                await action(cancellation.Token);
                Status.Text = "Done.";
            }
            catch (OperationCanceledException)
            {
                Status.Text = "Cancelled.";
            }
            catch (Exception ex)
            {
                Status.Text = "Build failed.";
                AppendLog(ex.Message);
                MessageBox.Show(this, ex.Message, "RR Language Builder", MessageBoxButton.OK, MessageBoxImage.Error);
            }
            finally
            {
                SetBusy(false);
            }
        }

        private void SetBusy(bool busy)
        {
            ValidateButton.IsEnabled = !busy;
            UpdateConfigButton.IsEnabled = !busy;
            BuildSelectedButton.IsEnabled = !busy;
            BuildAllButton.IsEnabled = !busy;
            SheetUrl.IsEnabled = !busy;
            PackFolder.IsEnabled = !busy;
            LanguageBox.IsEnabled = !busy;
            if (busy) Status.Text = "Working...";
        }

        private void OnWindowClosing(object sender, CancelEventArgs e)
        {
            e.Cancel = true;
            Hide();
        }

        private void AppendLog(string text)
        {
            if (!Dispatcher.CheckAccess())
            {
                Dispatcher.Invoke(() => AppendLog(text));
                return;
            }
            Log.Text += (Log.Text.Length == 0 ? string.Empty : Environment.NewLine) + text;
        }
    }
}

