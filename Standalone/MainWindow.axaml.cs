using System;
using System.IO;
using System.Linq;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Shapes;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Media;
using Avalonia.Platform;
using Avalonia.Platform.Storage;
using Avalonia.Controls.Primitives;
using Avalonia.Threading;
using ModularVoiceStudio.App.Engine;

namespace ModularVoiceStudio.App;

public partial class MainWindow : Window
{
    private readonly DispatcherTimer meterTimer;
    private TrayIcon? trayIcon;
    private float smoothedInMeter = 0f;
    private float peakHoldIn = 0f;
    private int peakHoldInCounter = 0;

    private float smoothedVbMeter = 0f;
    private float peakHoldVb = 0f;
    private int peakHoldVbCounter = 0;

    private float smoothedMonMeter = 0f;
    private float peakHoldMon = 0f;
    private int peakHoldMonCounter = 0;

    private bool isUpdatingControls = false;

    public MainWindow()
    {
        InitializeComponent();

        // 1. Load persistent settings & init C++ Audio Engine
        SettingsManager.Load();
        try
        {
            NativeBridge.MVS_Init(48000, 256);

            string audioState = SettingsManager.GetAudioDevicesXmlFilePath();
            if (File.Exists(audioState))
                NativeBridge.MVS_LoadAudioDeviceState(audioState);

            string dspState = SettingsManager.GetDspStateFilePath();
            if (File.Exists(dspState))
                NativeBridge.MVS_LoadStateFromFile(dspState);
        }
        catch (Exception ex)
        {
            Console.WriteLine("Native init error: " + ex.Message);
        }

        // 2. Populate Audio Devices
        PopulateAudioDevices();

        // 3. Apply saved parameters to UI & Engine
        ApplySavedSettings();

        // 4. Setup System Tray Icon (Minimize to Tray feature)
        SetupTrayIcon();

        // 5. Hook window focus to refresh devices if changed in Settings window
        Activated += (_, _) =>
        {
            try { PopulateAudioDevices(); } catch { }
        };

        // 6. Start 60 FPS DispatcherTimer for Live LED VU Meters
        meterTimer = new DispatcherTimer(TimeSpan.FromMilliseconds(16), DispatcherPriority.Render, OnMeterTimerTick);
        meterTimer.Start();
    }

    private void PopulateAudioDevices()
    {
        isUpdatingControls = true;
        try
        {
            // Input Devices
            CmbInputDevice.Items.Clear();
            var inDevs = NativeBridge.GetInputDeviceList();
            int selectedInIdx = 0;
            for (int i = 0; i < inDevs.Length; i++)
            {
                var item = new ComboBoxItem { Content = "IN: " + inDevs[i], Tag = inDevs[i] };
                CmbInputDevice.Items.Add(item);
                if (!string.IsNullOrEmpty(SettingsManager.Current.SelectedInputDevice) &&
                    inDevs[i].Equals(SettingsManager.Current.SelectedInputDevice, StringComparison.OrdinalIgnoreCase))
                {
                    selectedInIdx = i;
                }
            }
            if (CmbInputDevice.Items.Count > 0)
                CmbInputDevice.SelectedIndex = selectedInIdx;

            // Output Devices
            var outDevs = NativeBridge.GetOutputDeviceList();

            // 1. VB-Cable Virtual Mic Filter
            CmbVBCableDevice.Items.Clear();
            int selectedCableIdx = 0;
            int cableCount = 0;
            for (int i = 0; i < outDevs.Length; i++)
            {
                string dev = outDevs[i];
                bool isVirtual = dev.Contains("CABLE", StringComparison.OrdinalIgnoreCase)
                              || dev.Contains("Virtual", StringComparison.OrdinalIgnoreCase)
                              || dev.Contains("VB-Audio", StringComparison.OrdinalIgnoreCase)
                              || dev.Contains("VoiceMeeter", StringComparison.OrdinalIgnoreCase);

                if (isVirtual)
                {
                    var item = new ComboBoxItem { Content = dev, Tag = dev };
                    CmbVBCableDevice.Items.Add(item);
                    if (dev.Contains("CABLE Input", StringComparison.OrdinalIgnoreCase) ||
                        (!string.IsNullOrEmpty(SettingsManager.Current.SelectedVBCableDevice) &&
                         dev.Equals(SettingsManager.Current.SelectedVBCableDevice, StringComparison.OrdinalIgnoreCase)))
                    {
                        selectedCableIdx = cableCount;
                    }
                    cableCount++;
                }
            }
            if (CmbVBCableDevice.Items.Count == 0)
            {
                CmbVBCableDevice.Items.Add(new ComboBoxItem { Content = "CABLE Input (VB-Audio Virtual Cable)", Tag = "CABLE Input" });
            }
            CmbVBCableDevice.SelectedIndex = selectedCableIdx;

            // 2. Headphone Monitor (A1) Filter (Physical Outputs only, Auto-Select Headphones)
            CmbMonitorDevice.Items.Clear();
            int selectedMonIdx = 0;
            int monCount = 0;
            for (int i = 0; i < outDevs.Length; i++)
            {
                string dev = outDevs[i];
                bool isVirtual = dev.Contains("CABLE", StringComparison.OrdinalIgnoreCase)
                              || dev.Contains("Virtual", StringComparison.OrdinalIgnoreCase)
                              || dev.Contains("VB-Audio", StringComparison.OrdinalIgnoreCase)
                              || dev.Contains("VoiceMeeter", StringComparison.OrdinalIgnoreCase);

                if (!isVirtual)
                {
                    var item = new ComboBoxItem { Content = dev, Tag = dev };
                    CmbMonitorDevice.Items.Add(item);
                    bool isHeadphone = dev.Contains("Headphone", StringComparison.OrdinalIgnoreCase)
                                    || dev.Contains("Kulaklık", StringComparison.OrdinalIgnoreCase)
                                    || dev.Contains("Headset", StringComparison.OrdinalIgnoreCase)
                                    || dev.Contains("Earphone", StringComparison.OrdinalIgnoreCase);

                    if (isHeadphone || (string.IsNullOrEmpty(SettingsManager.Current.SelectedMonitorDevice) && selectedMonIdx == 0)
                        || (!string.IsNullOrEmpty(SettingsManager.Current.SelectedMonitorDevice) &&
                            dev.Equals(SettingsManager.Current.SelectedMonitorDevice, StringComparison.OrdinalIgnoreCase)))
                    {
                        selectedMonIdx = monCount;
                    }
                    monCount++;
                }
            }
            if (CmbMonitorDevice.Items.Count == 0)
            {
                for (int i = 0; i < outDevs.Length; i++)
                    CmbMonitorDevice.Items.Add(new ComboBoxItem { Content = outDevs[i], Tag = outDevs[i] });
            }
            CmbMonitorDevice.SelectedIndex = selectedMonIdx;
        }
        finally
        {
            isUpdatingControls = false;
        }
    }

    private void ApplySavedSettings()
    {
        isUpdatingControls = true;
        try
        {
            // Mic Gain & Mute
            SldMicGain.Value = SettingsManager.Current.MicGainDb;
            TxtMicDb.Text = FormatDb(SettingsManager.Current.MicGainDb);
            BtnMicMute.IsChecked = SettingsManager.Current.MicMuted;
            UpdateMicMuteVisuals(SettingsManager.Current.MicMuted);
            NativeBridge.MVS_SetMicFaderGain(DbToLinear(SettingsManager.Current.MicGainDb));
            NativeBridge.MVS_SetMicMute(SettingsManager.Current.MicMuted ? 1 : 0);

            // VB-Cable
            SldVBCableGain.Value = SettingsManager.Current.VBCableGainDb;
            TxtVBCableDb.Text = FormatDb(SettingsManager.Current.VBCableGainDb);
            BtnVBCableMute.IsChecked = SettingsManager.Current.VBCableMuted;
            UpdateVBCableMuteVisuals(SettingsManager.Current.VBCableMuted);
            NativeBridge.MVS_SetVBCableActive(SettingsManager.Current.VBCableMuted ? 0 : 1);

            // Monitor
            SldMonitorGain.Value = SettingsManager.Current.MonitorGainDb;
            TxtMonitorDb.Text = FormatDb(SettingsManager.Current.MonitorGainDb);
            ChkMonitorVoice.IsChecked = SettingsManager.Current.MonitorActive;
            UpdateMonitorVisuals(SettingsManager.Current.MonitorActive);
            NativeBridge.MVS_SetMonitorActive(SettingsManager.Current.MonitorActive ? 1 : 0);

            // DSP Chain Bypass
            bool isBypassed = SettingsManager.Current.DspBypassed;
            BtnDspBypass.IsChecked = isBypassed;
            UpdateDspBypassVisuals(isBypassed);
            NativeBridge.MVS_SetMicBypass(isBypassed ? 1 : 0);

            // Active devices to engine
            if (CmbInputDevice.SelectedItem is ComboBoxItem inItem && inItem.Tag is string inTag)
                NativeBridge.MVS_SetInputDevice(inTag);

            if (CmbVBCableDevice.SelectedItem is ComboBoxItem vbItem && vbItem.Tag is string vbTag)
                NativeBridge.MVS_SetVBCableOutputDevice(vbTag);

            if (CmbMonitorDevice.SelectedItem is ComboBoxItem monItem && monItem.Tag is string monTag)
                NativeBridge.MVS_SetMonitorOutputDevice(monTag);

            // Update DSP screen text
            TxtDspNodes.Text = NativeBridge.GetActiveDSPNodesString();
        }
        finally
        {
            isUpdatingControls = false;
        }
    }

    private void UpdateDspBypassVisuals(bool isBypassed)
    {
        if (isBypassed)
        {
            TxtDspBypass.Text = "BYPASS";
            TxtDspBypass.Foreground = new SolidColorBrush(Color.Parse("#FDE68A"));
            LedDspBypass.Fill = new SolidColorBrush(Color.Parse("#D97706"));
            BtnDspBypass.Background = new SolidColorBrush(Color.Parse("#5C3A1F"));
            BtnDspBypass.BorderBrush = new SolidColorBrush(Color.Parse("#D97706"));
        }
        else
        {
            TxtDspBypass.Text = "ACTIVE";
            TxtDspBypass.Foreground = new SolidColorBrush(Color.Parse("#6EE7B7"));
            LedDspBypass.Fill = new SolidColorBrush(Color.Parse("#10B981"));
            BtnDspBypass.Background = new SolidColorBrush(Color.Parse("#14331E"));
            BtnDspBypass.BorderBrush = new SolidColorBrush(Color.Parse("#059669"));
        }
    }

    private void UpdateMicMuteVisuals(bool isMuted)
    {
        if (isMuted)
        {
            LedMicMute.Fill = new SolidColorBrush(Color.Parse("#EF4444"));
            TxtMicMute.Foreground = new SolidColorBrush(Color.Parse("#FCA5A5"));
            TxtMicMute.Text = "MUTED";
            BtnMicMute.Background = new SolidColorBrush(Color.Parse("#381313"));
            BtnMicMute.BorderBrush = new SolidColorBrush(Color.Parse("#EF4444"));
        }
        else
        {
            LedMicMute.Fill = new SolidColorBrush(Color.Parse("#334155"));
            TxtMicMute.Foreground = new SolidColorBrush(Color.Parse("#94A3B8"));
            TxtMicMute.Text = "MUTE MIC";
            BtnMicMute.Background = new SolidColorBrush(Color.Parse("#242424"));
            BtnMicMute.BorderBrush = new SolidColorBrush(Color.Parse("#3D3D3D"));
        }
    }

    private void UpdateVBCableMuteVisuals(bool isMuted)
    {
        if (isMuted)
        {
            LedVBCableMute.Fill = new SolidColorBrush(Color.Parse("#EF4444"));
            TxtVBCableMute.Foreground = new SolidColorBrush(Color.Parse("#FCA5A5"));
            TxtVBCableMute.Text = "MUTED";
            BtnVBCableMute.Background = new SolidColorBrush(Color.Parse("#381313"));
            BtnVBCableMute.BorderBrush = new SolidColorBrush(Color.Parse("#EF4444"));
        }
        else
        {
            LedVBCableMute.Fill = new SolidColorBrush(Color.Parse("#334155"));
            TxtVBCableMute.Foreground = new SolidColorBrush(Color.Parse("#94A3B8"));
            TxtVBCableMute.Text = "MUTE";
            BtnVBCableMute.Background = new SolidColorBrush(Color.Parse("#242424"));
            BtnVBCableMute.BorderBrush = new SolidColorBrush(Color.Parse("#3D3D3D"));
        }
    }

    private void UpdateMonitorVisuals(bool isMon)
    {
        if (isMon)
        {
            LedMonState.Fill = new SolidColorBrush(Color.Parse("#00F0FF"));
            TxtMonState.Foreground = new SolidColorBrush(Color.Parse("#67E8F9"));
            TxtMonState.Text = "MON ON";
            ChkMonitorVoice.Background = new SolidColorBrush(Color.Parse("#0F2936"));
            ChkMonitorVoice.BorderBrush = new SolidColorBrush(Color.Parse("#00F0FF"));
        }
        else
        {
            LedMonState.Fill = new SolidColorBrush(Color.Parse("#334155"));
            TxtMonState.Foreground = new SolidColorBrush(Color.Parse("#94A3B8"));
            TxtMonState.Text = "MON";
            ChkMonitorVoice.Background = new SolidColorBrush(Color.Parse("#242424"));
            ChkMonitorVoice.BorderBrush = new SolidColorBrush(Color.Parse("#3D3D3D"));
        }
    }

    private void OnDspBypassChanged(object? sender, RoutedEventArgs e)
    {
        if (isUpdatingControls) return;
        bool isBypassed = BtnDspBypass.IsChecked == true;
        UpdateDspBypassVisuals(isBypassed);
        NativeBridge.MVS_SetMicBypass(isBypassed ? 1 : 0);
        SettingsManager.Current.DspBypassed = isBypassed;
        SettingsManager.Save();
    }

    // =========================================================================
    // Control Event Handlers
    // =========================================================================

    private void OnInputDeviceChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (isUpdatingControls) return;
        if (CmbInputDevice.SelectedItem is ComboBoxItem item && item.Tag is string devName)
        {
            NativeBridge.MVS_SetInputDevice(devName);
            SettingsManager.Current.SelectedInputDevice = devName;
            SettingsManager.Save();
        }
    }

    private void OnVBCableDeviceChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (isUpdatingControls) return;
        if (CmbVBCableDevice.SelectedItem is ComboBoxItem item && item.Tag is string devName)
        {
            NativeBridge.MVS_SetVBCableOutputDevice(devName);
            SettingsManager.Current.SelectedVBCableDevice = devName;
            SettingsManager.Save();
        }
    }

    private void OnMonitorDeviceChanged(object? sender, SelectionChangedEventArgs e)
    {
        if (isUpdatingControls) return;
        if (CmbMonitorDevice.SelectedItem is ComboBoxItem item && item.Tag is string devName)
        {
            NativeBridge.MVS_SetMonitorOutputDevice(devName);
            SettingsManager.Current.SelectedMonitorDevice = devName;
            SettingsManager.Save();
        }
    }

    private void OnMicGainChanged(object? sender, Avalonia.Controls.Primitives.RangeBaseValueChangedEventArgs e)
    {
        if (isUpdatingControls) return;
        double db = SldMicGain.Value;
        TxtMicDb.Text = FormatDb(db);
        NativeBridge.MVS_SetMicFaderGain(DbToLinear(db));
        SettingsManager.Current.MicGainDb = db;
        SettingsManager.Save();
    }

    private void OnMicMuteChanged(object? sender, RoutedEventArgs e)
    {
        if (isUpdatingControls) return;
        bool isMuted = BtnMicMute.IsChecked == true;
        UpdateMicMuteVisuals(isMuted);
        NativeBridge.MVS_SetMicMute(isMuted ? 1 : 0);
        SettingsManager.Current.MicMuted = isMuted;
        SettingsManager.Save();
    }

    private void OnVBCableGainChanged(object? sender, Avalonia.Controls.Primitives.RangeBaseValueChangedEventArgs e)
    {
        if (isUpdatingControls) return;
        double db = SldVBCableGain.Value;
        TxtVBCableDb.Text = FormatDb(db);
        NativeBridge.MVS_SetVBCableFaderGain(DbToLinear(db));
        SettingsManager.Current.VBCableGainDb = db;
        SettingsManager.Save();
    }

    private void OnVBCableMuteChanged(object? sender, RoutedEventArgs e)
    {
        if (isUpdatingControls) return;
        bool isMuted = BtnVBCableMute.IsChecked == true;
        UpdateVBCableMuteVisuals(isMuted);
        NativeBridge.MVS_SetVBCableActive(isMuted ? 0 : 1);
        SettingsManager.Current.VBCableMuted = isMuted;
        SettingsManager.Save();
    }

    private void OnMonitorGainChanged(object? sender, Avalonia.Controls.Primitives.RangeBaseValueChangedEventArgs e)
    {
        if (isUpdatingControls) return;
        double db = SldMonitorGain.Value;
        TxtMonitorDb.Text = FormatDb(db);
        NativeBridge.MVS_SetMonitorFaderGain(DbToLinear(db));
        SettingsManager.Current.MonitorGainDb = db;
        SettingsManager.Save();
    }

    private void OnMonitorVoiceCheckedChanged(object? sender, RoutedEventArgs e)
    {
        if (isUpdatingControls) return;
        bool isMon = ChkMonitorVoice.IsChecked == true;
        UpdateMonitorVisuals(isMon);
        NativeBridge.MVS_SetMonitorActive(isMon ? 1 : 0);
        SettingsManager.Current.MonitorActive = isMon;
        SettingsManager.Save();
    }

    private void OnMicGainDoubleTapped(object? sender, RoutedEventArgs e)
    {
        SldMicGain.Value = 0.0;
    }

    private void OnMicDbPointerPressed(object? sender, PointerPressedEventArgs e)
    {
        if (e.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
        {
            SldMicGain.Value = 0.0;
        }
    }

    private void OnVBCableGainDoubleTapped(object? sender, RoutedEventArgs e)
    {
        SldVBCableGain.Value = 0.0;
    }

    private void OnVBCableDbPointerPressed(object? sender, PointerPressedEventArgs e)
    {
        if (e.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
        {
            SldVBCableGain.Value = 0.0;
        }
    }

    private void OnMonitorGainDoubleTapped(object? sender, RoutedEventArgs e)
    {
        SldMonitorGain.Value = 0.0;
    }

    private void OnMonitorDbPointerPressed(object? sender, PointerPressedEventArgs e)
    {
        if (e.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
        {
            SldMonitorGain.Value = 0.0;
        }
    }

    // =========================================================================
    // EFFECT CHAIN CONSOLE WINDOW MANAGEMENT
    // =========================================================================
    private Views.EffectChainWindow? _effectChainWindow;

    private void OnOpenEffectChainClicked(object? sender, RoutedEventArgs e)
    {
        if (_effectChainWindow == null)
        {
            _effectChainWindow = new Views.EffectChainWindow();
            _effectChainWindow.Closed += (s, ev) => _effectChainWindow = null;
        }
        _effectChainWindow.RebuildMixerStrips();
        _effectChainWindow.Show();
        _effectChainWindow.Activate();
    }

    private void OnResetAllFadersClicked(object? sender, RoutedEventArgs e)
    {
        SldMicGain.Value = 0.0;
        SldVBCableGain.Value = 0.0;
        SldMonitorGain.Value = 0.0;
    }

    private void OnOpenCanvasClicked(object? sender, RoutedEventArgs e)
    {
        NativeBridge.MVS_OpenModularCanvasWindow();
    }

    private void OnResetDspClicked(object? sender, RoutedEventArgs e)
    {
        NativeBridge.MVS_ResetDSPChain();
        TxtDspNodes.Text = "Direct Clean Input -> Output";
        string dspState = SettingsManager.GetDspStateFilePath();
        NativeBridge.MVS_SaveStateToFile(dspState);
    }

    private void OnAudioSettingsClicked(object? sender, RoutedEventArgs e)
    {
        try
        {
            var dlg = new Views.AudioSettingsDialog();
            dlg.ShowDialog(this);
        }
        catch
        {
            NativeBridge.MVS_OpenAudioSettingsWindow();
        }
    }

    private void OnDspScreenPointerPressed(object? sender, PointerPressedEventArgs e)
    {
        NativeBridge.MVS_OpenModularCanvasWindow();
    }

    private void OnVBCableGuideClicked(object? sender, RoutedEventArgs e)
    {
        try
        {
            var dlg = new Views.VBCableGuideDialog();
            dlg.ShowDialog(this);
        }
        catch { }
    }

    private void OnAboutClicked(object? sender, RoutedEventArgs e)
    {
        try
        {
            var dlg = new Views.AboutDialog();
            dlg.ShowDialog(this);
        }
        catch { }
    }

    private void OnTitleBarPointerPressed(object? sender, PointerPressedEventArgs e)
    {
        if (e.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
            BeginMoveDrag(e);
    }

    private void SetupTrayIcon()
    {
        try
        {
            WindowIcon? windowIcon = null;
            try
            {
                var iconUri = new Uri("avares://ModularVoiceStudio/Assets/icon.ico");
                windowIcon = new WindowIcon(AssetLoader.Open(iconUri));
            }
            catch
            {
                try
                {
                    var iconUri = new Uri("avares://ModularVoiceStudio/Assets/icon.png");
                    windowIcon = new WindowIcon(AssetLoader.Open(iconUri));
                }
                catch { }
            }
            if (windowIcon != null)
                Icon = windowIcon;

            trayIcon = new TrayIcon
            {
                Icon = windowIcon,
                ToolTipText = "Modular Voice Studio",
                IsVisible = true
            };

            var menu = new NativeMenu();

            var openItem = new NativeMenuItem("Show Modular Voice Studio");
            openItem.Click += (_, _) => ShowFromTray();

            var canvasItem = new NativeMenuItem("Open Modular DSP Canvas");
            canvasItem.Click += (_, _) => NativeBridge.MVS_OpenModularCanvasWindow();

            var settingsItem = new NativeMenuItem("Audio Hardware Settings");
            settingsItem.Click += (_, _) => NativeBridge.MVS_OpenAudioSettingsWindow();

            var sep = new NativeMenuItemSeparator();

            var exitItem = new NativeMenuItem("Exit Application");
            exitItem.Click += (_, _) =>
            {
                if (trayIcon != null)
                {
                    trayIcon.IsVisible = false;
                    trayIcon.Dispose();
                    trayIcon = null;
                }
                OnCloseClicked(null, new RoutedEventArgs());
            };

            menu.Items.Add(openItem);
            menu.Items.Add(canvasItem);
            menu.Items.Add(settingsItem);
            menu.Items.Add(sep);
            menu.Items.Add(exitItem);

            trayIcon.Menu = menu;
            trayIcon.Clicked += (_, _) => ShowFromTray();
        }
        catch (Exception ex)
        {
            Console.WriteLine("TrayIcon error: " + ex.Message);
        }
    }

    public void ShowFromTray()
    {
        Show();
        WindowState = WindowState.Normal;
        Activate();
    }

    private void OnMinimizeClicked(object? sender, RoutedEventArgs e)
    {
        Hide();
    }

    private void OnMinimizeToTrayClicked(object? sender, RoutedEventArgs e)
    {
        Hide();
    }

    private void OnCloseClicked(object? sender, RoutedEventArgs e)
    {
        meterTimer.Stop();
        if (trayIcon != null)
        {
            trayIcon.IsVisible = false;
            trayIcon.Dispose();
            trayIcon = null;
        }

        try
        {
            string audioState = SettingsManager.GetAudioDevicesXmlFilePath();
            NativeBridge.MVS_SaveAudioDeviceState(audioState);

            string dspState = SettingsManager.GetDspStateFilePath();
            NativeBridge.MVS_SaveStateToFile(dspState);

            SettingsManager.Save();
            NativeBridge.MVS_Shutdown();
        }
        catch { }
        Close();
    }

    // =========================================================================
    // Live LED Stereo VU Meters Rendering with Peak Hold & Clip Detection
    // =========================================================================

    private void OnMeterTimerTick(object? sender, EventArgs e)
    {
        if (!IsVisible) return;

        try
        {
            NativeBridge.MVS_GetLiveLevels(out float inLvl, out float outLvl, out float vbLvl, out float monLvl);

            // 1. Mic Meter with Ballistics & Peak
            float inDb = inLvl > 0.00001f ? 20f * MathF.Log10(inLvl) : -60f;
            float targetInNorm = Math.Clamp((inDb + 60f) / 78f, 0f, 1f);
            UpdateBallistics(targetInNorm, ref smoothedInMeter, ref peakHoldIn, ref peakHoldInCounter);
            RenderAdvancedMeter(CanvasMicMeter, smoothedInMeter, peakHoldIn, LedMicClip);

            // 2. VB-Cable Output Meter with Ballistics & Peak
            float vbDb = vbLvl > 0.00001f ? 20f * MathF.Log10(vbLvl) : -60f;
            float targetVb = Math.Clamp((vbDb + 60f) / 66f, 0f, 1f);
            UpdateBallistics(targetVb, ref smoothedVbMeter, ref peakHoldVb, ref peakHoldVbCounter);
            RenderAdvancedMeter(CanvasVBCableMeter, smoothedVbMeter, peakHoldVb, LedVBCableClip);

            // 3. Monitor Output Meter with Ballistics & Peak
            float monDb = monLvl > 0.00001f ? 20f * MathF.Log10(monLvl) : -60f;
            float targetMon = Math.Clamp((monDb + 60f) / 66f, 0f, 1f);
            UpdateBallistics(targetMon, ref smoothedMonMeter, ref peakHoldMon, ref peakHoldMonCounter);
            RenderAdvancedMeter(CanvasMonitorMeter, smoothedMonMeter, peakHoldMon, LedMonClip);
        }
        catch { }
    }

    private static void UpdateBallistics(float target, ref float smoothed, ref float peakHold, ref int counter)
    {
        if (target > smoothed)
            smoothed = target;
        else
            smoothed = smoothed * 0.85f + target * 0.15f;

        if (smoothed > peakHold)
        {
            peakHold = smoothed;
            counter = 35;
        }
        else if (counter > 0)
        {
            counter--;
        }
        else
        {
            peakHold = Math.Max(0f, peakHold - 0.02f);
        }
    }

    private static void RenderAdvancedMeter(Canvas canvas, float level, float peakHold, Border? clipLed)
    {
        canvas.Children.Clear();
        double w = canvas.Bounds.Width;
        double h = canvas.Bounds.Height;
        if (w <= 0 || h <= 0) return;

        // Multi-segment acrylic meter (24 segments)
        int numSegments = 24;
        double segSpacing = 1.5;
        double segW = (w - (numSegments - 1) * segSpacing) / numSegments;
        int litCount = (int)Math.Round(level * numSegments);
        int peakIdx = (int)Math.Round(peakHold * numSegments) - 1;

        for (int s = 0; s < numSegments; s++)
        {
            double segX = s * (segW + segSpacing);
            // Segments 0-14: Clean Emerald Green (< -10 dBFS)
            // Segments 15-19: Warning Amber Gold (-10 to -3 dBFS)
            // Segments 20-23: Peak Danger Red (> -3 dBFS)
            Color activeCol = s >= 20 ? Color.Parse("#EF4444") :
                              s >= 15 ? Color.Parse("#F59E0B") :
                                        Color.Parse("#10B981");

            Color offCol = s >= 20 ? Color.Parse("#2B1214") :
                           s >= 15 ? Color.Parse("#2B2010") :
                                     Color.Parse("#12241C");

            bool isLit = s < litCount;
            bool isPeak = (s == peakIdx && peakHold > 0.04f);

            var rect = new Rectangle
            {
                Width = Math.Max(1, segW),
                Height = Math.Max(1, h - 2),
                RadiusX = 1,
                RadiusY = 1,
                Fill = new SolidColorBrush(isPeak ? Color.Parse("#FFFFFF") : (isLit ? activeCol : offCol))
            };
            Canvas.SetLeft(rect, segX);
            Canvas.SetTop(rect, 1);
            canvas.Children.Add(rect);
        }

        // Clip / Overload LED feedback
        if (clipLed != null)
        {
            bool isClipping = level >= 0.94f || peakHold >= 0.96f;
            clipLed.Background = new SolidColorBrush(isClipping ? Color.Parse("#EF4444") : Color.Parse("#330A0E"));
            clipLed.BorderBrush = new SolidColorBrush(isClipping ? Color.Parse("#FCA5A5") : Color.Parse("#551118"));
        }
    }

    private static float DbToLinear(double db) => db <= -59.5 ? 0f : MathF.Pow(10f, (float)db / 20f);

    private static string FormatDb(double db) => db <= -59.5 ? "-inf dB" : (db > 0 ? "+" : "") + db.ToString("F1") + " dB";
}
