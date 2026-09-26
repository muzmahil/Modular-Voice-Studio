using System;
using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Media;
using Avalonia.Threading;

namespace ModularVoiceStudio.App.Controls;

public partial class MiniChainScreenControl : UserControl
{
    public event EventHandler? OpenCanvasRequested;
    public event EventHandler<bool>? BypassChanged;

    private bool isBypassed = false;
    private double pulsePhase = 0;
    private readonly DispatcherTimer animTimer;

    public MiniChainScreenControl()
    {
        InitializeComponent();
        
        animTimer = new DispatcherTimer
        {
            Interval = TimeSpan.FromMilliseconds(33) // ~30 FPS
        };
        animTimer.Tick += (s, e) =>
        {
            pulsePhase += 0.1;
            if (pulsePhase > Math.PI * 2)
                pulsePhase = 0;

            if (PulseDot != null)
            {
                double x = 20 + 130 * (0.5 + 0.5 * Math.Sin(pulsePhase));
                Canvas.SetLeft(PulseDot, x);
            }
        };
        animTimer.Start();
    }

    private void OnPointerEntered(object? sender, PointerEventArgs e)
    {
        RootBorder.BorderBrush = new SolidColorBrush(Color.Parse("#00B4D8"));
    }

    private void OnPointerExited(object? sender, PointerEventArgs e)
    {
        RootBorder.BorderBrush = new SolidColorBrush(Color.Parse(isBypassed ? "#223344" : "#253E50"));
    }

    private void OnPointerPressed(object? sender, PointerPressedEventArgs e)
    {
        OpenCanvasRequested?.Invoke(this, EventArgs.Empty);
    }

    private void OnPowerClicked(object? sender, RoutedEventArgs e)
    {
        isBypassed = !isBypassed;
        if (PowerBtn != null)
        {
            PowerBtn.BorderBrush = new SolidColorBrush(Color.Parse(isBypassed ? "#E53935" : "#00E676"));
            if (PowerBtn.Content is Avalonia.Controls.Shapes.Ellipse el)
                el.Fill = new SolidColorBrush(Color.Parse(isBypassed ? "#E53935" : "#00E676"));
        }
        BypassChanged?.Invoke(this, isBypassed);
    }
}
