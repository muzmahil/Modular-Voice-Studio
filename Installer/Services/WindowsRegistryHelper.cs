using System;
using System.IO;
using System.Runtime.Versioning;
using Microsoft.Win32;

namespace ModularVoiceStudio.Installer.Services;

[SupportedOSPlatform("windows")]
public static class WindowsRegistryHelper
{
    public static string GetDefaultVst2Path()
    {
        try
        {
            // Check 64-bit VST2 registry location
            using var vstKey = Registry.LocalMachine.OpenSubKey(@"Software\VST");
            if (vstKey != null)
            {
                var val = vstKey.GetValue("VSTPluginsPath") as string;
                if (!string.IsNullOrWhiteSpace(val) && Directory.Exists(val))
                    return val;
            }

            using var cuKey = Registry.CurrentUser.OpenSubKey(@"Software\VST");
            if (cuKey != null)
            {
                var val = cuKey.GetValue("VSTPluginsPath") as string;
                if (!string.IsNullOrWhiteSpace(val) && Directory.Exists(val))
                    return val;
            }

            // Check standard Program Files\VSTPlugins or Steinberg\VstPlugins
            string progFiles = Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles);
            string stdPath1 = Path.Combine(progFiles, "VSTPlugins");
            if (Directory.Exists(stdPath1)) return stdPath1;

            string stdPath2 = Path.Combine(progFiles, "Steinberg", "VstPlugins");
            if (Directory.Exists(stdPath2)) return stdPath2;

            return stdPath1;
        }
        catch
        {
            return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ProgramFiles), "VSTPlugins");
        }
    }

    public static string GetDefaultVst3Path()
    {
        string commonProgFiles = Environment.GetFolderPath(Environment.SpecialFolder.CommonProgramFiles);
        return Path.Combine(commonProgFiles, "VST3");
    }

    public static void RegisterUninstall(string? vst3InstalledPath, string? vst2InstalledPath, string uninstallerExe, string version)
    {
        try
        {
            using var uninstKey = Registry.CurrentUser.CreateSubKey(@"Software\Microsoft\Windows\CurrentVersion\Uninstall\ModularVoiceStudio");
            if (uninstKey != null)
            {
                uninstKey.SetValue("DisplayName", "Modular Voice Studio");
                uninstKey.SetValue("DisplayVersion", version);
                uninstKey.SetValue("Publisher", "Furkan \"rootcf\" Çentek");
                uninstKey.SetValue("DisplayIcon", uninstallerExe + ",0");
                uninstKey.SetValue("UninstallString", $"\"{uninstallerExe}\"");
                uninstKey.SetValue("QuietUninstallString", $"\"{uninstallerExe}\" --silent");
                uninstKey.SetValue("NoModify", 1, RegistryValueKind.DWord);
                uninstKey.SetValue("NoRepair", 1, RegistryValueKind.DWord);

                if (!string.IsNullOrWhiteSpace(vst3InstalledPath))
                    uninstKey.SetValue("VST3Path", vst3InstalledPath);
                if (!string.IsNullOrWhiteSpace(vst2InstalledPath))
                    uninstKey.SetValue("VST2Path", vst2InstalledPath);
            }
        }
        catch { }
    }

    public static (string? vst3Path, string? vst2Path) GetInstalledPaths()
    {
        try
        {
            using var uninstKey = Registry.CurrentUser.OpenSubKey(@"Software\Microsoft\Windows\CurrentVersion\Uninstall\ModularVoiceStudio");
            if (uninstKey != null)
            {
                string? v3 = uninstKey.GetValue("VST3Path") as string;
                string? v2 = uninstKey.GetValue("VST2Path") as string;
                return (v3, v2);
            }
        }
        catch { }
        return (null, null);
    }

    public static void RemoveUninstallRegistry()
    {
        try
        {
            using var uninst = Registry.CurrentUser.OpenSubKey(@"Software\Microsoft\Windows\CurrentVersion\Uninstall", true);
            if (uninst != null)
            {
                try { uninst.DeleteSubKeyTree("ModularVoiceStudio", false); } catch { }
            }
        }
        catch { }
    }
}
