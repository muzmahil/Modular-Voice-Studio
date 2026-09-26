using System;
using System.Collections.Generic;
using System.Linq;

namespace ModularVoiceStudio.Installer.Services;

public class LanguageItem
{
    public string Code { get; set; } = "en";
    public string Name { get; set; } = "English";
    public string NativeName { get; set; } = "English";
    public Dictionary<string, string> Strings { get; set; } = new(StringComparer.OrdinalIgnoreCase);

    public override string ToString() => string.IsNullOrWhiteSpace(NativeName) || NativeName == Name
        ? Name
        : $"{NativeName} ({Name})";
}

public static class LocalizationManager
{
    private static readonly Dictionary<string, LanguageItem> _languages = new(StringComparer.OrdinalIgnoreCase);
    public static LanguageItem CurrentLanguage { get; private set; } = null!;

    static LocalizationManager()
    {
        RegisterBuiltInLanguages();
        SetLanguage("en");
    }

    public static IReadOnlyList<LanguageItem> AvailableLanguages => _languages.Values.ToList();

    public static void SetLanguage(string code)
    {
        if (_languages.TryGetValue(code, out var lang))
        {
            CurrentLanguage = lang;
        }
        else if (_languages.TryGetValue("en", out var enLang))
        {
            CurrentLanguage = enLang;
        }
        else if (_languages.Count > 0)
        {
            CurrentLanguage = _languages.Values.First();
        }
    }

    public static string Get(string key, string fallback = "")
    {
        if (CurrentLanguage?.Strings != null && CurrentLanguage.Strings.TryGetValue(key, out var val))
        {
            return val;
        }

        if (_languages.TryGetValue("en", out var enLang) && enLang.Strings.TryGetValue(key, out var enVal))
        {
            return enVal;
        }

        return fallback;
    }

    private static void RegisterBuiltInLanguages()
    {
        // 1. English (Default)
        var en = new LanguageItem
        {
            Code = "en",
            Name = "English",
            NativeName = "English",
            Strings = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
            {
                ["AppTitle"] = "Modular Voice Studio",
                ["AppSubtitle"] = "SETUP",
                ["IntroTitle"] = "Modular Voice Studio v1.5.0",
                ["IntroSubtitle"] = "Next-Generation Modular Vocal DSP Suite & 3D Spatial Audio Engine",
                ["Feat1"] = "• 64-bit ultra-low latency modular DSP canvas & audio routing",
                ["Feat2"] = "• RNNoise deep learning AI neural background noise suppression",
                ["Feat3"] = "• Precision dynamic de-esser & 7-band surgical parametric EQ",
                ["Feat4"] = "• Interactive 3D binaural spatial acoustic realm simulation",
                ["IntroDesc"] = "Click NEXT to proceed with installation.",
                ["Cancel"] = "CANCEL",
                ["Next"] = "NEXT >",
                ["Back"] = "< BACK",
                ["LicenseTitle"] = "License Agreement (GNU GPLv3)",
                ["LicenseSubtitle"] = "Please review the license terms before proceeding.",
                ["AgreeLicense"] = "I have read and agree to the terms of the license agreement.",
                ["AgreeNext"] = "I AGREE >",

                // Components
                ["ComponentsTitle"] = "Select Plugin Formats",
                ["ComponentsSubtitle"] = "Choose the plugin formats you want to install on your computer.",
                ["Vst3Checkbox"] = "VST3 Plugin (Recommended / Modern DAWs)",
                ["Vst3Desc"] = "Industry standard for FL Studio, Ableton Live, Reaper, Cubase, Studio One, OBS Studio.",
                ["Vst2Checkbox"] = "VST2 Plugin (Legacy / 64-bit VST 2.4)",
                ["Vst2Desc"] = "Support for older hosts and DAWs requiring 64-bit VST2 (.dll).",
                ["SelectAtLeastOne"] = "Please select at least one plugin format to proceed.",

                // Paths
                ["PathsTitle"] = "Installation Directories",
                ["PathsSubtitle"] = "Specify the destination directories for the selected plugin formats.",
                ["Vst3PathLabel"] = "VST3 Plugin Location (64-bit):",
                ["Vst2PathLabel"] = "VST2 Plugin Location (64-bit):",
                ["Browse"] = "Browse...",
                ["BrowseDialogTitle"] = "Select Plugin Directory",
                ["StartInstall"] = "INSTALL NOW",

                // Progress
                ["ProgressTitle"] = "Installing Modular Voice Studio...",
                ["StatusPreparing"] = "Preparing destination folders...",
                ["StatusExtracting"] = "Extracting plugin files...",
                ["StatusVst3"] = "Installing VST3 plugin bundle...",
                ["StatusVst2"] = "Installing VST2 dynamic library...",
                ["StatusRegistering"] = "Registering system installation...",
                ["InstallingBadge"] = "INSTALLING",

                // Finish
                ["FinishTitle"] = "Installation Completed Successfully!",
                ["FinishDesc"] = "Modular Voice Studio has been installed and is ready for use in your DAW.",
                ["FinishVst3Installed"] = "• VST3 Plugin installed to: ",
                ["FinishVst2Installed"] = "• VST2 Plugin installed to: ",
                ["FinishDawTip"] = "Rescan plugins in your DAW (FL Studio, Ableton, Reaper, Cubase, etc.) to start using Modular Voice Studio.",
                ["Finish"] = "FINISH",

                // Uninstall
                ["UninstallTitle"] = "Uninstall Modular Voice Studio",
                ["UninstallDesc"] = "Are you sure you want to completely remove Modular Voice Studio from your computer?",
                ["UninstallProgressTitle"] = "Uninstalling Modular Voice Studio...",
                ["UninstallProgressDesc"] = "Removing plugin files and registry entries...",
                ["UninstallSuccessTitle"] = "Uninstallation Complete",
                ["UninstallSuccessDesc"] = "Modular Voice Studio was successfully removed from your computer.",
                ["UninstallButton"] = "UNINSTALL",
                ["Close"] = "CLOSE"
            }
        };

        // 2. Turkish
        var tr = new LanguageItem
        {
            Code = "tr",
            Name = "Turkish",
            NativeName = "Türkçe",
            Strings = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase)
            {
                ["AppTitle"] = "Modular Voice Studio",
                ["AppSubtitle"] = "KURULUM",
                ["IntroTitle"] = "Modular Voice Studio v1.5.0",
                ["IntroSubtitle"] = "Yeni Nesil Modüler Vokal DSP Paketi ve 3D Uzamsal Ses Motoru",
                ["Feat1"] = "• 64-bit ultra düşük gecikmeli modüler DSP tuvali ve sinyal yönlendirme",
                ["Feat2"] = "• Derin öğrenme RNNoise yapay zeka arka plan gürültü engelleme",
                ["Feat3"] = "• Cerrahi dinamik de-esser ve 7-bant parametrik ekolayzır",
                ["Feat4"] = "• Etkileşimli 3D çift kulaklı (binaural) uzamsal oda akustiği",
                ["IntroDesc"] = "Kuruluma devam etmek için İLERİ butonuna tıklayınız.",
                ["Cancel"] = "İPTAL",
                ["Next"] = "İLERİ >",
                ["Back"] = "< GERİ",
                ["LicenseTitle"] = "Lisans Sözleşmesi (GNU GPLv3)",
                ["LicenseSubtitle"] = "Lütfen kuruluma devam etmeden önce lisans bildirisini okuyunuz.",
                ["AgreeLicense"] = "Lisans sözleşmesindeki şartları okudum ve kabul ediyorum.",
                ["AgreeNext"] = "KABUL EDİYORUM >",

                // Components
                ["ComponentsTitle"] = "Eklenti Formatlarını Seçin",
                ["ComponentsSubtitle"] = "Bilgisayarınıza yüklemek istediğiniz eklenti formatlarını belirleyin.",
                ["Vst3Checkbox"] = "VST3 Eklentisi (Önerilen / Modern DAW'lar)",
                ["Vst3Desc"] = "FL Studio, Ableton Live, Reaper, Cubase, Studio One, OBS Studio için standart.",
                ["Vst2Checkbox"] = "VST2 Eklentisi (Eski / 64-bit VST 2.4)",
                ["Vst2Desc"] = "64-bit VST2 (.dll) gerektiren programlar için destek.",
                ["SelectAtLeastOne"] = "Devam etmek için lütfen en az bir eklenti formatı seçiniz.",

                // Paths
                ["PathsTitle"] = "Kurulum Dizinleri",
                ["PathsSubtitle"] = "Seçilen eklenti formatları için hedef klasörleri belirleyin.",
                ["Vst3PathLabel"] = "VST3 Eklenti Konumu (64-bit):",
                ["Vst2PathLabel"] = "VST2 Eklenti Konumu (64-bit):",
                ["Browse"] = "Gözat...",
                ["BrowseDialogTitle"] = "Eklenti Dizinini Seçin",
                ["StartInstall"] = "ŞİMDİ KUR",

                // Progress
                ["ProgressTitle"] = "Modular Voice Studio Yükleniyor...",
                ["StatusPreparing"] = "Hedef klasörler hazırlanıyor...",
                ["StatusExtracting"] = "Eklenti dosyaları ayıklanıyor...",
                ["StatusVst3"] = "VST3 eklenti paketi kopyalanıyor...",
                ["StatusVst2"] = "VST2 dinamik kütüphanesi kopyalanıyor...",
                ["StatusRegistering"] = "Sistem kurulum kaydı yapılıyor...",
                ["InstallingBadge"] = "YÜKLENİYOR",

                // Finish
                ["FinishTitle"] = "Kurulum Başarıyla Tamamlandı!",
                ["FinishDesc"] = "Modular Voice Studio yüklendi ve DAW programlarınızda kullanıma hazır.",
                ["FinishVst3Installed"] = "• VST3 Eklentisi şuraya kuruldu: ",
                ["FinishVst2Installed"] = "• VST2 Eklentisi şuraya kuruldu: ",
                ["FinishDawTip"] = "Modular Voice Studio'yu kullanmak için DAW programınızda (FL Studio, Ableton, Reaper vb.) eklenti taraması yapınız.",
                ["Finish"] = "BİTİR",

                // Uninstall
                ["UninstallTitle"] = "Modular Voice Studio Kaldırma",
                ["UninstallDesc"] = "Modular Voice Studio uygulamasını bilgisayarınızdan tamamen kaldırmak istediğinize emin misiniz?",
                ["UninstallProgressTitle"] = "Modular Voice Studio Kaldırılıyor...",
                ["UninstallProgressDesc"] = "Eklenti dosyaları ve kayıt defteri temizleniyor...",
                ["UninstallSuccessTitle"] = "Kaldırma Tamamlandı",
                ["UninstallSuccessDesc"] = "Modular Voice Studio bilgisayarınızdan başarıyla kaldırıldı.",
                ["UninstallButton"] = "KALDIR",
                ["Close"] = "KAPAT"
            }
        };

        _languages["en"] = en;
        _languages["tr"] = tr;
    }
}
