using System;
using System.Collections.Generic;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Controls.Shapes;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Platform.Storage;
using Avalonia.Threading;
using ModularVoiceStudio.App.Engine;

namespace ModularVoiceStudio.App.Views;

public partial class EffectChainWindow : Window
{
    private readonly DispatcherTimer _meterTimer;
    private readonly List<MixerStripControl> _activeStrips = new();
    private List<VstPluginInfo> _installedPlugins = new();

    public EffectChainWindow()
    {
        InitializeComponent();

        _meterTimer = new DispatcherTimer
        {
            Interval = TimeSpan.FromMilliseconds(33) // ~30 FPS
        };
        _meterTimer.Tick += OnMeterTimerTick;

        Loaded += (s, e) =>
        {
            PopulateInstalledVstMenu();
            RebuildMixerStrips();
            _meterTimer.Start();
        };

        Unloaded += (s, e) =>
        {
            _meterTimer.Stop();
        };
    }

    private void OnTitleBarPointerPressed(object? sender, PointerPressedEventArgs e)
    {
        if (e.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
            BeginMoveDrag(e);
    }

    private void OnMinimizeClicked(object? sender, RoutedEventArgs e)
    {
        WindowState = WindowState.Minimized;
    }

    private void OnCloseClicked(object? sender, RoutedEventArgs e)
    {
        Hide();
    }

    private void OnScanVSTClicked(object? sender, RoutedEventArgs e)
    {
        PopulateInstalledVstMenu();
    }

    public void PopulateInstalledVstMenu()
    {
        try
        {
            _installedPlugins = NativeBridge.ScanInstalledVST3Plugins();
            if (MenuInstalledVst != null) MenuInstalledVst.ItemsSource = null;
            if (MenuInstalledVstEmpty != null) MenuInstalledVstEmpty.ItemsSource = null;

            List<Control> CreateItems()
            {
                var items = new List<Control>();
                if (_installedPlugins.Count == 0)
                {
                    items.Add(new MenuItem { Header = "No VST3 plugins found (Click Scan to search)", IsEnabled = false });
                }
                else
                {
                    foreach (var plugin in _installedPlugins)
                    {
                        var mi = new MenuItem
                        {
                            Header = $"{plugin.Name} ({plugin.Vendor})",
                            Tag = plugin.Path
                        };
                        mi.Click += (s, e) =>
                        {
                            if (s is MenuItem clicked && clicked.Tag is string path)
                            {
                                int idx = NativeBridge.AddEffectStrip("VST3 Host", path);
                                if (idx >= 0) RebuildMixerStrips();
                            }
                        };
                        items.Add(mi);
                    }
                }
                return items;
            }

            if (MenuInstalledVst != null)
                MenuInstalledVst.ItemsSource = CreateItems();
            if (MenuInstalledVstEmpty != null)
                MenuInstalledVstEmpty.ItemsSource = CreateItems();
        }
        catch { }
    }

    private void OnInsertEffectClicked(object? sender, RoutedEventArgs e)
    {
        if (sender is MenuItem mi && mi.Tag is string typeName)
        {
            int idx = NativeBridge.AddEffectStrip(typeName);
            if (idx >= 0)
            {
                RebuildMixerStrips();
            }
        }
    }

    private async void OnInsertVSTPluginClicked(object? sender, RoutedEventArgs e)
    {
        try
        {
            var topLevel = GetTopLevel(this);
            if (topLevel == null) return;

            var files = await topLevel.StorageProvider.OpenFilePickerAsync(new FilePickerOpenOptions
            {
                Title = "Select VST3 Plugin File",
                AllowMultiple = false,
                FileTypeFilter = new List<FilePickerFileType>
                {
                    new("VST3 Audio Plugins (*.vst3)") { Patterns = new[] { "*.vst3" } }
                }
            });

            if (files.Count > 0)
            {
                string path = files[0].Path.LocalPath;
                int idx = NativeBridge.AddEffectStrip("VST3 Host", path);
                if (idx >= 0)
                {
                    RebuildMixerStrips();
                }
            }
        }
        catch { }
    }

    private void OnClearChainClicked(object? sender, RoutedEventArgs e)
    {
        NativeBridge.ClearEffectChain();
        RebuildMixerStrips();
    }

    public void RebuildMixerStrips()
    {
        ContainerStrips.Children.Clear();
        _activeStrips.Clear();

        int count = NativeBridge.GetEffectChainCount();
        TxtStripCount.Text = $"ACTIVE STRIPS: {count}";

        if (count == 0)
        {
            PanelEmptyState.IsVisible = true;
            ScrollStrips.IsVisible = false;
            return;
        }

        PanelEmptyState.IsVisible = false;
        ScrollStrips.IsVisible = true;

        for (int i = 0; i < count; i++)
        {
            var data = NativeBridge.GetEffectStripData(i);
            if (data == null) continue;

            var stripCtrl = new MixerStripControl(data, this);
            _activeStrips.Add(stripCtrl);
            ContainerStrips.Children.Add(stripCtrl.RootBorder);
        }
    }

    private void OnMeterTimerTick(object? sender, EventArgs e)
    {
        int count = NativeBridge.GetEffectChainCount();
        for (int i = 0; i < count && i < _activeStrips.Count; i++)
        {
            var data = NativeBridge.GetEffectStripData(i);
            if (data != null)
            {
                _activeStrips[i].UpdateMeter(data.MeterPeakL, data.MeterPeakR);
            }
        }
    }
}

/// <summary>
/// Individual Vertical Channel Strip Control for Effect Chain Mixer Console
/// </summary>
public class MixerStripControl
{
    private readonly EffectChainWindow _parent;
    private int _index;
    private readonly string _typeName;
    private readonly string _displayName;

    public Border RootBorder { get; }

    private Canvas _canvasMeterL = null!;
    private Canvas _canvasMeterR = null!;
    private Slider _sldFader = null!;
    private TextBlock _txtDb = null!;
    private Slider _sldPan = null!;
    private TextBlock _txtPan = null!;
    private ToggleButton _btnBypass = null!;
    private ToggleButton _btnMute = null!;
    private ToggleButton _btnSolo = null!;

    public MixerStripControl(EffectStripData data, EffectChainWindow parent)
    {
        _parent = parent;
        _index = data.Index;
        _typeName = data.TypeName;
        _displayName = string.IsNullOrWhiteSpace(data.DisplayName) ? data.TypeName : data.DisplayName;

        RootBorder = BuildUI(data);
    }

    private Border BuildUI(EffectStripData data)
    {
        var border = new Border
        {
            Width = 136,
            Background = new SolidColorBrush(Color.Parse("#202020")),
            BorderBrush = new SolidColorBrush(Color.Parse("#333333")),
            BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(4),
            Padding = new Thickness(7, 6),
            Margin = new Thickness(0, 0, 4, 0)
        };

        var mainGrid = new Grid
        {
            RowDefinitions = new RowDefinitions("Auto,Auto,Auto,Auto,*,Auto,Auto,Auto")
        };

        // 1. Header: [01 - TYPE] + Delete [X] Button
        var headerGrid = new Grid
        {
            ColumnDefinitions = new ColumnDefinitions("*,Auto"),
            Height = 20,
            Margin = new Thickness(0, 0, 0, 3)
        };

        var badgeBorder = new Border
        {
            Background = new SolidColorBrush(Color.Parse("#262626")),
            BorderBrush = new SolidColorBrush(Color.Parse("#383838")),
            BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(2),
            Padding = new Thickness(4, 1),
            HorizontalAlignment = HorizontalAlignment.Left,
            VerticalAlignment = VerticalAlignment.Center
        };
        var txtBadge = new TextBlock
        {
            Text = $"{_index + 1:D2} • {_typeName.ToUpperInvariant()}",
            FontSize = 7.0,
            FontWeight = FontWeight.Black,
            Foreground = new SolidColorBrush(Color.Parse("#38BDF8")),
            TextTrimming = TextTrimming.CharacterEllipsis,
            MaxWidth = 90
        };
        badgeBorder.Child = txtBadge;
        Grid.SetColumn(badgeBorder, 0);

        var btnDelete = new Button
        {
            Content = new TextBlock
            {
                Text = "✕",
                FontSize = 8.5,
                FontWeight = FontWeight.Bold,
                Foreground = new SolidColorBrush(Color.Parse("#EF4444")),
                HorizontalAlignment = HorizontalAlignment.Center,
                VerticalAlignment = VerticalAlignment.Center
            },
            Width = 18,
            Height = 18,
            Background = new SolidColorBrush(Color.Parse("#2A1818")),
            BorderBrush = new SolidColorBrush(Color.Parse("#4A2222")),
            BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(2),
            HorizontalAlignment = HorizontalAlignment.Right,
            VerticalAlignment = VerticalAlignment.Center
        };
        btnDelete.Click += (s, e) =>
        {
            NativeBridge.RemoveEffectStrip(_index);
            _parent.RebuildMixerStrips();
        };
        Grid.SetColumn(btnDelete, 1);

        headerGrid.Children.Add(badgeBorder);
        headerGrid.Children.Add(btnDelete);
        Grid.SetRow(headerGrid, 0);
        mainGrid.Children.Add(headerGrid);

        // 2. Name Display (Double-click to open settings)
        var nameBorder = new Border
        {
            Background = new SolidColorBrush(Color.Parse("#141414")),
            BorderBrush = new SolidColorBrush(Color.Parse("#2B2B2B")),
            BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(2),
            Padding = new Thickness(4, 3),
            Margin = new Thickness(0, 0, 0, 3),
            Cursor = new Cursor(StandardCursorType.Hand)
        };
        var txtName = new TextBlock
        {
            Text = _displayName.ToUpperInvariant(),
            FontSize = 8.5,
            FontWeight = FontWeight.Black,
            Foreground = new SolidColorBrush(Color.Parse("#EEEEEE")),
            TextTrimming = TextTrimming.CharacterEllipsis,
            HorizontalAlignment = HorizontalAlignment.Center
        };
        nameBorder.Child = txtName;
        nameBorder.PointerPressed += (s, e) =>
        {
            if (e.ClickCount >= 2) OpenSettingsWindow();
        };
        Grid.SetRow(nameBorder, 1);
        mainGrid.Children.Add(nameBorder);

        // 2.5 Action Buttons Grid: [ EDIT ] + Optional [ GUI ]
        var actionGrid = new Grid
        {
            Margin = new Thickness(0, 0, 0, 4)
        };

        var btnEdit = new Button
        {
            Height = 22,
            HorizontalAlignment = HorizontalAlignment.Stretch,
            Background = new SolidColorBrush(Color.Parse("#1E3A5F")),
            BorderBrush = new SolidColorBrush(Color.Parse("#2563EB")),
            BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(2),
            Content = new TextBlock
            {
                Text = "EDIT",
                FontSize = 7.5,
                FontWeight = FontWeight.ExtraBold,
                Foreground = new SolidColorBrush(Color.Parse("#38BDF8")),
                HorizontalAlignment = HorizontalAlignment.Center,
                VerticalAlignment = VerticalAlignment.Center
            }
        };
        btnEdit.Click += (s, e) => OpenSettingsWindow();

        bool isVst = _typeName.Contains("VST") || _typeName.Contains("Host");
        if (isVst)
        {
            actionGrid.ColumnDefinitions = new ColumnDefinitions("*,3,*");
            Grid.SetColumn(btnEdit, 0);
            actionGrid.Children.Add(btnEdit);

            var btnVstGui = new Button
            {
                Height = 22,
                HorizontalAlignment = HorizontalAlignment.Stretch,
                Background = new SolidColorBrush(Color.Parse("#262626")),
                BorderBrush = new SolidColorBrush(Color.Parse("#444444")),
                BorderThickness = new Thickness(1),
                CornerRadius = new CornerRadius(2),
                Content = new TextBlock
                {
                    Text = "GUI",
                    FontSize = 7.5,
                    FontWeight = FontWeight.ExtraBold,
                    Foreground = new SolidColorBrush(Color.Parse("#FFFFFF")),
                    HorizontalAlignment = HorizontalAlignment.Center,
                    VerticalAlignment = VerticalAlignment.Center
                }
            };
            btnVstGui.Click += (s, e) => NativeBridge.MVS_EffectChain_OpenStripEditor(_index);
            Grid.SetColumn(btnVstGui, 2);
            actionGrid.Children.Add(btnVstGui);
        }
        else
        {
            actionGrid.Children.Add(btnEdit);
        }

        Grid.SetRow(actionGrid, 2);
        mainGrid.Children.Add(actionGrid);

        // 3. Bypass / Active Toggle with Illuminated LED
        _btnBypass = new ToggleButton
        {
            Classes = { "strip-bypass" },
            Height = 20,
            Margin = new Thickness(0, 0, 0, 4),
            HorizontalAlignment = HorizontalAlignment.Stretch,
            IsChecked = data.IsBypassed,
            Content = BuildLedContent(data.IsBypassed ? "BYPASS" : "ACTIVE", !data.IsBypassed, "#10B981")
        };
        _btnBypass.IsCheckedChanged += (s, e) =>
        {
            bool bypassed = _btnBypass.IsChecked == true;
            NativeBridge.MVS_EffectChain_SetStripBypass(_index, bypassed ? 1 : 0);
            _btnBypass.Content = BuildLedContent(bypassed ? "BYPASS" : "ACTIVE", !bypassed, "#10B981");
        };
        Grid.SetRow(_btnBypass, 3);
        mainGrid.Children.Add(_btnBypass);

        // 4. Center Area: Scale Ticks (Left), Vertical Fader (Center), Stereo VU Meter (Right)
        var centerGrid = new Grid
        {
            ColumnDefinitions = new ColumnDefinitions("20,*,16"),
            Margin = new Thickness(0, 2, 0, 3)
        };

        // Calibrated Scale Ticks
        var ticksGrid = new Grid
        {
            RowDefinitions = new RowDefinitions("Auto,*,Auto,*,Auto,*,Auto,*,Auto"),
            Margin = new Thickness(0, 4, 2, 4)
        };
        AddTick(ticksGrid, 0, "+6", "#EF4444");
        AddTick(ticksGrid, 2, "0", "#CCCCCC", true);
        AddTick(ticksGrid, 4, "-6", "#888888");
        AddTick(ticksGrid, 6, "-18", "#666666");
        AddTick(ticksGrid, 8, "-inf", "#444444");
        Grid.SetColumn(ticksGrid, 0);

        // Fader Well
        var faderWell = new Border
        {
            Background = new SolidColorBrush(Color.Parse("#141414")),
            BorderBrush = new SolidColorBrush(Color.Parse("#2B2B2B")),
            BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(3),
            Padding = new Thickness(0, 2)
        };
        var faderInner = new Grid();
        // 0 dB unity gain reference line
        var refGrid = new Grid
        {
            RowDefinitions = new RowDefinitions("10*,1,90*")
        };
        var refLine = new Rectangle
        {
            Height = 1,
            Fill = new SolidColorBrush(Color.Parse("#4B5563")),
            Opacity = 0.4,
            Margin = new Thickness(2, 0)
        };
        Grid.SetRow(refLine, 1);
        refGrid.Children.Add(refLine);
        faderInner.Children.Add(refGrid);

        float gainDb = (float)(20.0 * Math.Log10(Math.Max(0.0001, data.GainLinear)));
        _sldFader = new Slider
        {
            Orientation = Orientation.Vertical,
            Minimum = -60,
            Maximum = 6,
            Value = Math.Clamp(gainDb, -60, 6),
            HorizontalAlignment = HorizontalAlignment.Center,
            VerticalAlignment = VerticalAlignment.Stretch
        };
        _sldFader.DoubleTapped += (s, e) => _sldFader.Value = 0.0;
        _sldFader.ValueChanged += (s, e) =>
        {
            float db = (float)_sldFader.Value;
            float lin = db <= -59.5f ? 0.0f : (float)Math.Pow(10.0, db / 20.0);
            NativeBridge.MVS_EffectChain_SetStripGain(_index, lin);
            _txtDb.Text = db <= -59.5f ? "-inf dB" : $"{db:+0.0;-0.0;0.0} dB";
        };
        faderInner.Children.Add(_sldFader);
        faderWell.Child = faderInner;
        Grid.SetColumn(faderWell, 1);

        // Stereo Meter
        var meterBorder = new Border
        {
            Background = new SolidColorBrush(Color.Parse("#141414")),
            BorderBrush = new SolidColorBrush(Color.Parse("#2B2B2B")),
            BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(2),
            Padding = new Thickness(1),
            Margin = new Thickness(2, 0, 0, 0)
        };
        var meterGrid = new Grid
        {
            ColumnDefinitions = new ColumnDefinitions("*,*")
        };
        _canvasMeterL = new Canvas { ClipToBounds = true };
        _canvasMeterR = new Canvas { ClipToBounds = true, Margin = new Thickness(1, 0, 0, 0) };
        Grid.SetColumn(_canvasMeterL, 0);
        Grid.SetColumn(_canvasMeterR, 1);
        meterGrid.Children.Add(_canvasMeterL);
        meterGrid.Children.Add(_canvasMeterR);
        meterBorder.Child = meterGrid;
        Grid.SetColumn(meterBorder, 2);

        centerGrid.Children.Add(ticksGrid);
        centerGrid.Children.Add(faderWell);
        centerGrid.Children.Add(meterBorder);
        Grid.SetRow(centerGrid, 4);
        mainGrid.Children.Add(centerGrid);

        // 5. dB Readout Box
        var dbBorder = new Border
        {
            Background = new SolidColorBrush(Color.Parse("#141414")),
            BorderBrush = new SolidColorBrush(Color.Parse("#2E2E2E")),
            BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(3),
            Height = 20,
            Margin = new Thickness(0, 0, 0, 3),
            Cursor = new Cursor(StandardCursorType.Hand)
        };
        _txtDb = new TextBlock
        {
            Text = gainDb <= -59.5f ? "-inf dB" : $"{gainDb:+0.0;-0.0;0.0} dB",
            FontSize = 8.0,
            FontWeight = FontWeight.Bold,
            Foreground = new SolidColorBrush(Color.Parse("#38BDF8")),
            HorizontalAlignment = HorizontalAlignment.Center,
            VerticalAlignment = VerticalAlignment.Center
        };
        dbBorder.PointerPressed += (s, e) => _sldFader.Value = 0.0;
        dbBorder.DoubleTapped += (s, e) => _sldFader.Value = 0.0;
        dbBorder.Child = _txtDb;
        Grid.SetRow(dbBorder, 5);
        mainGrid.Children.Add(dbBorder);

        // 6. Pan Control
        var panGrid = new Grid
        {
            ColumnDefinitions = new ColumnDefinitions("*,28"),
            Height = 20,
            Margin = new Thickness(0, 0, 0, 4)
        };
        _sldPan = new Slider
        {
            Minimum = -100,
            Maximum = 100,
            Value = data.Pan * 100.0f,
            Height = 20,
            VerticalAlignment = VerticalAlignment.Center
        };
        _sldPan.DoubleTapped += (s, e) => _sldPan.Value = 0.0;
        _sldPan.ValueChanged += (s, e) =>
        {
            float panVal = (float)(_sldPan.Value / 100.0);
            NativeBridge.MVS_EffectChain_SetStripPan(_index, panVal);
            int pInt = (int)_sldPan.Value;
            if (pInt == 0) _txtPan.Text = "C";
            else if (pInt < 0) _txtPan.Text = $"{Math.Abs(pInt)}L";
            else _txtPan.Text = $"{pInt}R";
        };
        Grid.SetColumn(_sldPan, 0);

        var panBox = new Border
        {
            Background = new SolidColorBrush(Color.Parse("#141414")),
            BorderBrush = new SolidColorBrush(Color.Parse("#2E2E2E")),
            BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(2),
            Height = 18,
            Margin = new Thickness(2, 0, 0, 0),
            Cursor = new Cursor(StandardCursorType.Hand)
        };
        panBox.PointerPressed += (s, e) => _sldPan.Value = 0.0;
        _txtPan = new TextBlock
        {
            Text = data.Pan == 0 ? "C" : (data.Pan < 0 ? $"{(int)(Math.Abs(data.Pan) * 100)}L" : $"{(int)(data.Pan * 100)}R"),
            FontSize = 7.5,
            FontWeight = FontWeight.Bold,
            Foreground = new SolidColorBrush(Color.Parse("#888888")),
            HorizontalAlignment = HorizontalAlignment.Center,
            VerticalAlignment = VerticalAlignment.Center
        };
        panBox.Child = _txtPan;
        Grid.SetColumn(panBox, 1);

        panGrid.Children.Add(_sldPan);
        panGrid.Children.Add(panBox);
        Grid.SetRow(panGrid, 6);
        mainGrid.Children.Add(panGrid);

        // 7. Solo & Mute Buttons Row with Illuminated LED indicators
        var btnGrid = new Grid
        {
            ColumnDefinitions = new ColumnDefinitions("*,3,*"),
            Height = 24
        };

        _btnSolo = new ToggleButton
        {
            Classes = { "strip-solo" },
            IsChecked = data.IsSolo,
            Content = BuildLedContent("S", data.IsSolo, "#F59E0B")
        };
        _btnSolo.IsCheckedChanged += (s, e) =>
        {
            bool isSolo = _btnSolo.IsChecked == true;
            NativeBridge.MVS_EffectChain_SetStripSolo(_index, isSolo ? 1 : 0);
            _btnSolo.Content = BuildLedContent("S", isSolo, "#F59E0B");
        };
        Grid.SetColumn(_btnSolo, 0);

        _btnMute = new ToggleButton
        {
            Classes = { "strip-mute" },
            IsChecked = data.IsMuted,
            Content = BuildLedContent("M", data.IsMuted, "#EF4444")
        };
        _btnMute.IsCheckedChanged += (s, e) =>
        {
            bool isMuted = _btnMute.IsChecked == true;
            NativeBridge.MVS_EffectChain_SetStripMute(_index, isMuted ? 1 : 0);
            _btnMute.Content = BuildLedContent("M", isMuted, "#EF4444");
        };
        Grid.SetColumn(_btnMute, 2);

        btnGrid.Children.Add(_btnSolo);
        btnGrid.Children.Add(_btnMute);
        Grid.SetRow(btnGrid, 7);
        mainGrid.Children.Add(btnGrid);

        border.Child = mainGrid;
        return border;
    }

    private static StackPanel BuildLedContent(string text, bool isActive, string ledColorHex)
    {
        var sp = new StackPanel
        {
            Orientation = Orientation.Horizontal,
            Spacing = 4,
            HorizontalAlignment = HorizontalAlignment.Center,
            VerticalAlignment = VerticalAlignment.Center
        };

        var led = new Ellipse
        {
            Width = 5,
            Height = 5,
            Fill = new SolidColorBrush(Color.Parse(isActive ? ledColorHex : "#334155")),
            VerticalAlignment = VerticalAlignment.Center
        };

        var tb = new TextBlock
        {
            Text = text,
            FontSize = 7.5,
            FontWeight = FontWeight.ExtraBold,
            Foreground = new SolidColorBrush(Color.Parse(isActive ? "#FFFFFF" : "#64748B")),
            VerticalAlignment = VerticalAlignment.Center
        };

        sp.Children.Add(led);
        sp.Children.Add(tb);
        return sp;
    }

    private static void AddTick(Grid g, int row, string text, string hexColor, bool isBold = false)
    {
        var tb = new TextBlock
        {
            Text = text,
            FontSize = 6.0,
            FontWeight = isBold ? FontWeight.ExtraBold : FontWeight.Normal,
            Foreground = new SolidColorBrush(Color.Parse(hexColor)),
            HorizontalAlignment = HorizontalAlignment.Right
        };
        Grid.SetRow(tb, row);
        g.Children.Add(tb);
    }

    public void UpdateMeter(float peakL, float peakR)
    {
        DrawMeterBar(_canvasMeterL, peakL);
        DrawMeterBar(_canvasMeterR, peakR);
    }

    private static void DrawMeterBar(Canvas canvas, float peakLinear)
    {
        canvas.Children.Clear();
        double w = canvas.Bounds.Width;
        double h = canvas.Bounds.Height;
        if (w <= 0 || h <= 0) return;

        double db = 20.0 * Math.Log10(Math.Max(0.00001, peakLinear));
        double norm = Math.Clamp((db + 60.0) / 60.0, 0.0, 1.0);
        double fillH = norm * h;

        var brush = norm > 0.92
            ? new SolidColorBrush(Color.Parse("#EF4444")) // Red peak
            : (norm > 0.75
                ? new SolidColorBrush(Color.Parse("#F59E0B")) // Yellow warning
                : new SolidColorBrush(Color.Parse("#10B981"))); // Green normal

        var rect = new Rectangle
        {
            Width = w,
            Height = fillH,
            Fill = brush,
            RadiusX = 1,
            RadiusY = 1
        };
        Canvas.SetLeft(rect, 0);
        Canvas.SetTop(rect, h - fillH);
        canvas.Children.Add(rect);
    }

    private void OpenSettingsWindow()
    {
        var settingsWin = new EffectSettingsWindow(_index, _typeName, _displayName);
        settingsWin.Show(_parent);
    }
}
