using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using Avalonia;
using ModularVoiceStudio.Installer.Services;

namespace ModularVoiceStudio.Installer;

class Program
{
    [STAThread]
    public static void Main(string[] args)
    {
        AppDomain.CurrentDomain.UnhandledException += (s, e) =>
        {
            try
            {
                File.WriteAllText("installer_unhandled.log", e.ExceptionObject?.ToString() ?? "Unknown exception");
            }
            catch { }
        };

        try
        {
            bool isUninstall = false;
            bool isSilent = false;
            string exeName = Path.GetFileName(Process.GetCurrentProcess().MainModule?.FileName ?? "").ToLowerInvariant();
            if (exeName.Contains("uninstall") || (args != null && args.Any(a => a.Contains("uninstall", StringComparison.OrdinalIgnoreCase))))
            {
                isUninstall = true;
            }
            if (args != null && args.Any(a => a.Equals("--silent", StringComparison.OrdinalIgnoreCase) || a.Equals("/silent", StringComparison.OrdinalIgnoreCase) || a.Equals("/S", StringComparison.OrdinalIgnoreCase)))
            {
                isSilent = true;
            }

            if (isUninstall && isSilent)
            {
                InstallerEngine.UninstallAsync().GetAwaiter().GetResult();
                return;
            }

            App.IsUninstallMode = isUninstall;
            BuildAvaloniaApp().StartWithClassicDesktopLifetime(args);
        }
        catch (Exception ex)
        {
            try
            {
                File.WriteAllText("installer_fatal.log", ex.ToString());
            }
            catch { }
        }
    }

    public static AppBuilder BuildAvaloniaApp()
        => AppBuilder.Configure<App>()
            .UsePlatformDetect()
            .WithInterFont()
            .LogToTrace();
}
