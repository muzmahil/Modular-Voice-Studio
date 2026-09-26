using System;
using System.IO;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Platform.Storage;
using ModularVoiceStudio.Installer.Services;

namespace ModularVoiceStudio.Installer;

public partial class MainWindow : Window
{
    private readonly InstallerEngine _engine = new();

    public MainWindow()
    {
        InitializeComponent();

        try
        {
            TxtVst3Path.Text = WindowsRegistryHelper.GetDefaultVst3Path();
            TxtVst2Path.Text = WindowsRegistryHelper.GetDefaultVst2Path();

            ChkAgreeLicense.IsCheckedChanged += (s, e) =>
            {
                BtnLicenseNext.IsEnabled = ChkAgreeLicense.IsChecked == true;
            };

            ChkVst3.IsCheckedChanged += (s, e) => ValidateComponents();
            ChkVst2.IsCheckedChanged += (s, e) => ValidateComponents();

            LoadLicenseText();
            UpdateLocalization();
        }
        catch (Exception ex)
        {
            try { File.WriteAllText("installer_init_error.log", ex.ToString()); } catch { }
        }
    }

    protected override void OnClosed(EventArgs e)
    {
        base.OnClosed(e);
        if (Avalonia.Application.Current?.ApplicationLifetime is Avalonia.Controls.ApplicationLifetimes.IClassicDesktopStyleApplicationLifetime desktop)
        {
            desktop.Shutdown();
        }
    }

    private void LoadLicenseText()
    {
        try
        {
            // 1. Try Avalonia AssetLoader
            var uri = new Uri("avares://ModularVoiceStudio_Setup/Assets/LICENSE.txt");
            if (Avalonia.Platform.AssetLoader.Exists(uri))
            {
                using var stream = Avalonia.Platform.AssetLoader.Open(uri);
                using var reader = new StreamReader(stream);
                string text = reader.ReadToEnd();
                if (!string.IsNullOrWhiteSpace(text))
                {
                    TxtLicenseContent.Text = text;
                    return;
                }
            }
        }
        catch { }

        try
        {
            // 2. Try Embedded Resource from Assembly
            var assembly = System.Reflection.Assembly.GetExecutingAssembly();
            foreach (var resName in assembly.GetManifestResourceNames())
            {
                if (resName.EndsWith("LICENSE.txt", StringComparison.OrdinalIgnoreCase) || resName.EndsWith("LICENSE", StringComparison.OrdinalIgnoreCase))
                {
                    using var stream = assembly.GetManifestResourceStream(resName);
                    if (stream != null)
                    {
                        using var reader = new StreamReader(stream);
                        string text = reader.ReadToEnd();
                        if (!string.IsNullOrWhiteSpace(text))
                        {
                            TxtLicenseContent.Text = text;
                            return;
                        }
                    }
                }
            }
        }
        catch { }

        try
        {
            // 3. Try Local File Paths
            string baseDir = AppDomain.CurrentDomain.BaseDirectory;
            string[] searchPaths = new[]
            {
                Path.Combine(baseDir, "Assets", "LICENSE.txt"),
                Path.Combine(baseDir, "..", "LICENSE"),
                Path.Combine(baseDir, "..", "..", "..", "..", "LICENSE"),
                Path.Combine(baseDir, "LICENSE")
            };

            foreach (var sp in searchPaths)
            {
                if (File.Exists(sp))
                {
                    string text = File.ReadAllText(sp);
                    if (!string.IsNullOrWhiteSpace(text))
                    {
                        TxtLicenseContent.Text = text;
                        return;
                    }
                }
            }
        }
        catch { }

        // 4. Default In-Memory Fallback
        TxtLicenseContent.Text =
@"GNU GENERAL PUBLIC LICENSE
Version 3, 29 June 2007

Copyright (C) 2026 Furkan ""rootcf"" Çentek <https://github.com/rootcf>

Everyone is permitted to copy and distribute verbatim copies
of this license document, but changing it is not allowed.

Preamble

The GNU General Public License is a free, copyleft license for
software and other kinds of works.

The licenses for most software and other practical works are designed
to take away your freedom to share and change the works. By contrast,
the GNU General Public License is intended to guarantee your freedom to
share and change all versions of a program--to make sure it remains free
software for all its users. We, the Free Software Foundation, use the
GNU General Public License for most of our software; it applies also to
any other work released this way by its authors. You can apply it to
your programs, too.

TERMS AND CONDITIONS

0. Definitions.
""This License"" refers to version 3 of the GNU General Public License.
""The Program"" refers to any copyrightable work licensed under this License.
Each licensee is addressed as ""you"". ""Licensees"" and ""recipients"" may be individuals or organizations.

1. Source Code.
The ""source code"" for a work means the preferred form of the work for making modifications to it. ""Object code"" means any non-source form of a work.

2. Basic Permissions.
All rights granted under this License are granted for the term of copyright on the Program, and are irrevocable provided the stated conditions are met. This License explicitly affirms your unlimited permission to run the unmodified Program.

3. Protecting Users' Legal Rights From Anti-Circumvention Law.
No covered work shall be deemed part of an effective technological measure under any applicable law fulfilling obligations under article 11 of the WIPO copyright treaty.

4. Conveying Verbatim Copies.
You may convey verbatim copies of the Program's source code as you receive it, in any medium, provided that you conspicuously and appropriately publish on each copy an appropriate copyright notice.

5. Conveying Modified Source Versions.
You may convey a work based on the Program, or the modifications to produce it from the Program, in the form of source code under the terms of section 4.

6. Conveying Non-Source Forms.
You may convey a covered work in object code form under the terms of sections 4 and 5, provided that you also convey the machine-readable Corresponding Source.

For full license terms and conditions, visit: https://www.gnu.org/licenses/gpl-3.0.html";
    }

    private void ValidateComponents()
    {
        bool anySelected = ChkVst3.IsChecked == true || ChkVst2.IsChecked == true;
        BtnComponentsNext.IsEnabled = anySelected;
        TxtWarningNoSelection.IsVisible = !anySelected;
    }

    private void UpdateLocalization()
    {
        try
        {
            var cur = LocalizationManager.CurrentLanguage;
            if (cur == null) return;

            TxtLangToggle.Text = cur.Code.ToUpperInvariant();
            TxtHeaderSubtitle.Text = LocalizationManager.Get("AppSubtitle", "SETUP");

            // Intro
            TxtIntroTitle.Text = LocalizationManager.Get("IntroTitle");
            TxtIntroSubtitle.Text = LocalizationManager.Get("IntroSubtitle");
            TxtFeat1.Text = LocalizationManager.Get("Feat1");
            TxtFeat2.Text = LocalizationManager.Get("Feat2");
            TxtFeat3.Text = LocalizationManager.Get("Feat3");
            TxtFeat4.Text = LocalizationManager.Get("Feat4");
            TxtIntroDesc.Text = LocalizationManager.Get("IntroDesc");
            BtnIntroCancel.Content = LocalizationManager.Get("Cancel");
            BtnIntroNext.Content = LocalizationManager.Get("Next");

            // License
            TxtLicenseTitle.Text = LocalizationManager.Get("LicenseTitle");
            TxtLicenseSubtitle.Text = LocalizationManager.Get("LicenseSubtitle");
            ChkAgreeLicense.Content = LocalizationManager.Get("AgreeLicense");
            BtnLicenseCancel.Content = LocalizationManager.Get("Cancel");
            BtnLicenseBack.Content = LocalizationManager.Get("Back");
            BtnLicenseNext.Content = LocalizationManager.Get("AgreeNext");

            // Components
            TxtComponentsTitle.Text = LocalizationManager.Get("ComponentsTitle");
            TxtComponentsSubtitle.Text = LocalizationManager.Get("ComponentsSubtitle");
            TxtVst3Checkbox.Text = LocalizationManager.Get("Vst3Checkbox");
            TxtVst3Desc.Text = LocalizationManager.Get("Vst3Desc");
            TxtVst2Checkbox.Text = LocalizationManager.Get("Vst2Checkbox");
            TxtVst2Desc.Text = LocalizationManager.Get("Vst2Desc");
            TxtWarningNoSelection.Text = LocalizationManager.Get("SelectAtLeastOne");
            BtnComponentsCancel.Content = LocalizationManager.Get("Cancel");
            BtnComponentsBack.Content = LocalizationManager.Get("Back");
            BtnComponentsNext.Content = LocalizationManager.Get("Next");

            // Paths
            TxtPathsTitle.Text = LocalizationManager.Get("PathsTitle");
            TxtPathsSubtitle.Text = LocalizationManager.Get("PathsSubtitle");
            TxtVst3PathLabel.Text = LocalizationManager.Get("Vst3PathLabel");
            TxtVst2PathLabel.Text = LocalizationManager.Get("Vst2PathLabel");
            BtnBrowseVst3.Content = LocalizationManager.Get("Browse");
            BtnBrowseVst2.Content = LocalizationManager.Get("Browse");
            BtnPathsCancel.Content = LocalizationManager.Get("Cancel");
            BtnPathsBack.Content = LocalizationManager.Get("Back");
            BtnStartInstall.Content = LocalizationManager.Get("StartInstall");

            // Progress
            TxtProgressTitle.Text = LocalizationManager.Get("ProgressTitle");
            TxtStatus.Text = LocalizationManager.Get("StatusExtracting");
            TxtInstallingBadge.Text = LocalizationManager.Get("InstallingBadge");

            // Finish
            TxtFinishTitle.Text = LocalizationManager.Get("FinishTitle");
            TxtFinishDesc.Text = LocalizationManager.Get("FinishDesc");
            TxtDawTip.Text = LocalizationManager.Get("FinishDawTip");
            BtnFinish.Content = LocalizationManager.Get("Finish");
        }
        catch { }
    }

    private void OnLangToggleClick(object? sender, RoutedEventArgs e)
    {
        var langs = LocalizationManager.AvailableLanguages;
        if (langs.Count == 0) return;

        int idx = -1;
        for (int i = 0; i < langs.Count; i++)
        {
            if (langs[i].Code.Equals(LocalizationManager.CurrentLanguage.Code, StringComparison.OrdinalIgnoreCase))
            {
                idx = i;
                break;
            }
        }

        int nextIdx = (idx + 1) % langs.Count;
        LocalizationManager.SetLanguage(langs[nextIdx].Code);
        UpdateLocalization();
    }

    private void OnTopBarPointerPressed(object? sender, PointerPressedEventArgs e)
    {
        if (e.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
        {
            BeginMoveDrag(e);
        }
    }

    private void OnMinimizeClick(object? sender, RoutedEventArgs e)
    {
        WindowState = WindowState.Minimized;
    }

    private void OnCloseClick(object? sender, RoutedEventArgs e)
    {
        Close();
    }

    private void OnCancelClick(object? sender, RoutedEventArgs e)
    {
        Close();
    }

    private void HideAllPages()
    {
        IntroPage.IsVisible = false;
        LicensePage.IsVisible = false;
        ComponentsPage.IsVisible = false;
        PathsPage.IsVisible = false;
        ProgressPage.IsVisible = false;
        FinishPage.IsVisible = false;
    }

    private void OnGoToIntroClick(object? sender, RoutedEventArgs e)
    {
        HideAllPages();
        IntroPage.IsVisible = true;
    }

    private void OnGoToLicenseClick(object? sender, RoutedEventArgs e)
    {
        HideAllPages();
        LicensePage.IsVisible = true;
    }

    private void OnGoToComponentsClick(object? sender, RoutedEventArgs e)
    {
        HideAllPages();
        ComponentsPage.IsVisible = true;
        ValidateComponents();
    }

    private void OnGoToPathsClick(object? sender, RoutedEventArgs e)
    {
        HideAllPages();
        PathsPage.IsVisible = true;

        Vst3PathCard.IsVisible = ChkVst3.IsChecked == true;
        Vst2PathCard.IsVisible = ChkVst2.IsChecked == true;
    }

    private async void OnBrowseVst3Click(object? sender, RoutedEventArgs e)
    {
        try
        {
            var folders = await StorageProvider.OpenFolderPickerAsync(new FolderPickerOpenOptions
            {
                Title = LocalizationManager.Get("BrowseDialogTitle"),
                AllowMultiple = false
            });

            if (folders.Count > 0)
            {
                TxtVst3Path.Text = folders[0].Path.LocalPath;
            }
        }
        catch { }
    }

    private async void OnBrowseVst2Click(object? sender, RoutedEventArgs e)
    {
        try
        {
            var folders = await StorageProvider.OpenFolderPickerAsync(new FolderPickerOpenOptions
            {
                Title = LocalizationManager.Get("BrowseDialogTitle"),
                AllowMultiple = false
            });

            if (folders.Count > 0)
            {
                TxtVst2Path.Text = folders[0].Path.LocalPath;
            }
        }
        catch { }
    }

    private async void OnStartInstallClick(object? sender, RoutedEventArgs e)
    {
        HideAllPages();
        ProgressPage.IsVisible = true;

        bool installVst3 = ChkVst3.IsChecked == true;
        string vst3Path = TxtVst3Path.Text ?? "";
        bool installVst2 = ChkVst2.IsChecked == true;
        string vst2Path = TxtVst2Path.Text ?? "";

        var progress = new Progress<(double progress, string status)>(update =>
        {
            double p = Math.Clamp(update.progress, 0.0, 1.0);
            TxtPercent.Text = $"%{(int)(p * 100)}";
            TxtStatus.Text = update.status;
            ProgressBarFill.Width = p * 630.0;
        });

        await _engine.InstallAsync(installVst3, vst3Path, installVst2, vst2Path, progress);

        // Update Summary on Finish Page
        TxtSummaryVst3.IsVisible = installVst3;
        TxtSummaryVst3.Text = LocalizationManager.Get("FinishVst3Installed") + vst3Path;

        TxtSummaryVst2.IsVisible = installVst2;
        TxtSummaryVst2.Text = LocalizationManager.Get("FinishVst2Installed") + vst2Path;

        HideAllPages();
        FinishPage.IsVisible = true;
    }

    private void OnFinishClick(object? sender, RoutedEventArgs e)
    {
        Close();
    }
}
