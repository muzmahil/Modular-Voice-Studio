using System;
using Avalonia.Controls;
using Avalonia.Media;

namespace ModularVoiceStudio.App.Controls;

public partial class ChannelStripControl : UserControl
{
    public event EventHandler? OpenCanvasRequested;
    public event EventHandler<bool>? BypassChanged;

    private int channelIndex;
    private readonly Border[] meterSegments;

    public ChannelStripControl()
    {
        InitializeComponent();

        meterSegments = new Border[]
        {
            Seg0, Seg1, Seg2, Seg3, Seg4, Seg5, Seg6, Seg7,
            Seg8, Seg9, Seg10, Seg11, Seg12, Seg13, Seg14, Seg15
        };

        MiniScreen.OpenCanvasRequested += (s, e) => OpenCanvasRequested?.Invoke(this, EventArgs.Empty);
        MiniScreen.BypassChanged += (s, e) => BypassChanged?.Invoke(this, e);

        SldGain.PropertyChanged += (s, e) =>
        {
            if (e.Property.Name == nameof(Slider.Value))
            {
                double val = SldGain.Value;
                TxtGainReadout.Text = $"GAIN: {(val > 0 ? "+" : "")}{val:F1} dB";
            }
        };

        SldFader.PropertyChanged += (s, e) =>
        {
            if (e.Property.Name == nameof(Slider.Value))
            {
                double val = SldFader.Value;
                TxtFaderReadout.Text = val <= -59.5 ? "-inf dB" : $"{(val > 0 ? "+" : "")}{val:F1} dB";
            }
        };

        BtnMute.IsCheckedChanged += (s, e) =>
        {
            bool isMuted = BtnMute.IsChecked == true;
            BtnMute.Foreground = new SolidColorBrush(Color.Parse(isMuted ? "#FF3366" : "#94A3B8"));
            BtnMute.Background = new SolidColorBrush(Color.Parse(isMuted ? "#3D1420" : "#1C2430"));
        };

        BtnSolo.IsCheckedChanged += (s, e) =>
        {
            bool isSolo = BtnSolo.IsChecked == true;
            BtnSolo.Foreground = new SolidColorBrush(Color.Parse(isSolo ? "#FFB300" : "#94A3B8"));
            BtnSolo.Background = new SolidColorBrush(Color.Parse(isSolo ? "#3D3010" : "#1C2430"));
        };
    }

    public void SetupChannel(int index, string title, string defaultDevice)
    {
        channelIndex = index;
        TxtChannelTitle.Text = title;
        CmbSource.Items.Clear();
        CmbSource.Items.Add(defaultDevice);
        CmbSource.SelectedIndex = 0;

        if (index == 0)
        {
            TxtChannelTitle.Foreground = new SolidColorBrush(Color.Parse("#00E5FF"));
            BtnB1.IsChecked = true;
        }
        else if (index == 1)
        {
            TxtChannelTitle.Foreground = new SolidColorBrush(Color.Parse("#FF9F1C"));
            BtnB1.IsChecked = false;
        }
        else
        {
            TxtChannelTitle.Foreground = new SolidColorBrush(Color.Parse("#94A3B8"));
            BtnB1.IsChecked = false;
        }
    }

    public void UpdateMeterLevel(float level)
    {
        int litCount = (int)Math.Clamp(level * meterSegments.Length, 0, meterSegments.Length);
        
        for (int i = 0; i < meterSegments.Length; i++)
        {
            bool isLit = i < litCount;
            if (i >= 13) // Red
                meterSegments[i].Background = new SolidColorBrush(Color.Parse(isLit ? "#FF3366" : "#33FF3366"));
            else if (i >= 9) // Amber
                meterSegments[i].Background = new SolidColorBrush(Color.Parse(isLit ? "#FFB300" : "#33FFB300"));
            else // Green
                meterSegments[i].Background = new SolidColorBrush(Color.Parse(isLit ? "#00E676" : "#3300E676"));
        }
    }
}
