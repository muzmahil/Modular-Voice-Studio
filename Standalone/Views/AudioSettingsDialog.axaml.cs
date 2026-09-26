using System;
using Avalonia.Controls;
using Avalonia.Interactivity;
using ModularVoiceStudio.App.Engine;

namespace ModularVoiceStudio.App.Views;

public partial class AudioSettingsDialog : Window
{
    public AudioSettingsDialog()
    {
        InitializeComponent();
        PopulateDevices();
    }

    private void PopulateDevices()
    {
        try
        {
            var inDevs = NativeBridge.GetInputDeviceList();
            CmbInput.Items.Clear();
            foreach (var d in inDevs)
                CmbInput.Items.Add(new ComboBoxItem { Content = d });
            if (CmbInput.Items.Count > 0)
                CmbInput.SelectedIndex = 0;

            var outDevs = NativeBridge.GetOutputDeviceList();
            CmbOutput.Items.Clear();
            foreach (var d in outDevs)
                CmbOutput.Items.Add(new ComboBoxItem { Content = d });
            if (CmbOutput.Items.Count > 0)
                CmbOutput.SelectedIndex = 0;
        }
        catch { }
    }

    private void OnOpenJuceSetupClicked(object? sender, RoutedEventArgs e)
    {
        try
        {
            NativeBridge.MVS_OpenAudioSettingsWindow();
        }
        catch { }
    }

    private void OnCloseClicked(object? sender, RoutedEventArgs e)
    {
        Close();
    }
}
