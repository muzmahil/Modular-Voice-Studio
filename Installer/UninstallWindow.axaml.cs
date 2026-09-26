using System;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using ModularVoiceStudio.Installer.Services;

namespace ModularVoiceStudio.Installer;

public partial class UninstallWindow : Window
{
    public UninstallWindow()
    {
        InitializeComponent();
        UpdateLocalization();
    }

    private void UpdateLocalization()
    {
        TxtUninstallTitle.Text = LocalizationManager.Get("UninstallTitle");
        TxtUninstallDesc.Text = LocalizationManager.Get("UninstallDesc");
        BtnCancel.Content = LocalizationManager.Get("Cancel");
        BtnUninstall.Content = LocalizationManager.Get("UninstallButton");

        TxtProgressTitle.Text = LocalizationManager.Get("UninstallProgressTitle");
        TxtProgressDesc.Text = LocalizationManager.Get("UninstallProgressDesc");

        TxtSuccessTitle.Text = LocalizationManager.Get("UninstallSuccessTitle");
        TxtSuccessDesc.Text = LocalizationManager.Get("UninstallSuccessDesc");
        BtnClose.Content = LocalizationManager.Get("Close");
    }

    private void OnTopBarPointerPressed(object? sender, PointerPressedEventArgs e)
    {
        if (e.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
        {
            BeginMoveDrag(e);
        }
    }

    private void OnCloseClick(object? sender, RoutedEventArgs e)
    {
        Close();
    }

    private async void OnStartUninstallClick(object? sender, RoutedEventArgs e)
    {
        ConfirmView.IsVisible = false;
        ProgressView.IsVisible = true;

        var progress = new Progress<(double, string)>(update =>
        {
            TxtProgressDesc.Text = update.Item2;
        });

        await InstallerEngine.UninstallAsync(progress);

        ProgressView.IsVisible = false;
        FinishView.IsVisible = true;
    }
}
