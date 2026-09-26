using System;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using ModularVoiceStudio.Installer.Services;

namespace ModularVoiceStudio.Installer;

public partial class LanguageDialog : Window
{
    public LanguageDialog()
    {
        InitializeComponent();

        CmbLanguages.ItemsSource = LocalizationManager.AvailableLanguages;
        CmbLanguages.SelectedItem = LocalizationManager.CurrentLanguage;
    }

    private void OnTopBarPointerPressed(object? sender, PointerPressedEventArgs e)
    {
        if (e.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
        {
            BeginMoveDrag(e);
        }
    }

    private void OnCancelClick(object? sender, RoutedEventArgs e)
    {
        Close();
        if (Avalonia.Application.Current?.ApplicationLifetime is Avalonia.Controls.ApplicationLifetimes.IClassicDesktopStyleApplicationLifetime desktop)
        {
            desktop.Shutdown();
        }
    }

    private void OnOkClick(object? sender, RoutedEventArgs e)
    {
        if (CmbLanguages.SelectedItem is LanguageItem selected)
        {
            LocalizationManager.SetLanguage(selected.Code);
        }

        var mainWin = new MainWindow();
        if (Avalonia.Application.Current?.ApplicationLifetime is Avalonia.Controls.ApplicationLifetimes.IClassicDesktopStyleApplicationLifetime desktop)
        {
            desktop.MainWindow = mainWin;
        }

        mainWin.Show();
        Close();
    }
}
