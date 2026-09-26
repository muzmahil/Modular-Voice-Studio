using System;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;

namespace ModularVoiceStudio.Installer;

public partial class App : Application
{
    public static bool IsUninstallMode { get; set; }

    public override void Initialize()
    {
        AvaloniaXamlLoader.Load(this);
    }

    public override void OnFrameworkInitializationCompleted()
    {
        if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
        {
            if (IsUninstallMode)
            {
                desktop.ShutdownMode = ShutdownMode.OnMainWindowClose;
                var uninst = new UninstallWindow();
                desktop.MainWindow = uninst;
                uninst.Show();
            }
            else
            {
                desktop.ShutdownMode = ShutdownMode.OnExplicitShutdown;
                var langDialog = new LanguageDialog();
                langDialog.Show();
            }
        }

        base.OnFrameworkInitializationCompleted();
    }
}
