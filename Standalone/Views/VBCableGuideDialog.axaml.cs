using System;
using System.Diagnostics;
using Avalonia.Controls;
using Avalonia.Interactivity;

namespace ModularVoiceStudio.App.Views;

public partial class VBCableGuideDialog : Window
{
    public VBCableGuideDialog()
    {
        InitializeComponent();
    }

    private void OnOpenWebsiteClicked(object? sender, RoutedEventArgs e)
    {
        try
        {
            Process.Start(new ProcessStartInfo
            {
                FileName = "https://vb-audio.com/Cable/",
                UseShellExecute = true
            });
        }
        catch { }
    }

    private void OnCloseClicked(object? sender, RoutedEventArgs e)
    {
        Close();
    }
}
