using Avalonia.Controls;
using Avalonia.Interactivity;

namespace ModularVoiceStudio.App.Views;

public partial class AboutDialog : Window
{
    public AboutDialog()
    {
        InitializeComponent();
    }

    private void OnCloseClicked(object? sender, RoutedEventArgs e)
    {
        Close();
    }
}
