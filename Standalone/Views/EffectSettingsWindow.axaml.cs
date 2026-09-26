using System;
using System.Collections.Generic;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using ModularVoiceStudio.App.Controls;
using ModularVoiceStudio.App.Engine;

namespace ModularVoiceStudio.App.Views;

public partial class EffectSettingsWindow : Window
{
    private readonly int _stripIndex;
    private readonly string _typeName;
    private readonly string _displayName;
    private readonly List<EffectParamData> _parameters = new();
    private readonly Dictionary<string, (KnobControl? knob, Slider? slider, TextBlock? readout, EffectParamData param)> _paramControls = new();
    private bool _isUpdatingParam = false;

    public EffectSettingsWindow() : this(0, "Effect", "Effect")
    {
    }

    public EffectSettingsWindow(int stripIndex, string typeName, string displayName)
    {
        InitializeComponent();
        _stripIndex = stripIndex;
        _typeName = typeName;
        _displayName = string.IsNullOrWhiteSpace(displayName) ? typeName : displayName;

        TxtWindowTitle.Text = $"{_displayName.ToUpperInvariant()} - PARAMETERS";
        TxtEffectName.Text = _displayName.ToUpperInvariant();
        TxtEffectDesc.Text = $"Processor: {_typeName.ToUpperInvariant()} (Channel Strip #{_stripIndex + 1:D2})";

        Visualizer.EffectType = _typeName;

        bool isVst = _typeName.Contains("VST") || _typeName.Contains("Host");
        BtnVstGui.IsVisible = isVst;

        BuildPresets();
        LoadAndBuildParameters();
    }

    private void OnTitleBarPointerPressed(object? sender, PointerPressedEventArgs e)
    {
        if (e.GetCurrentPoint(this).Properties.IsLeftButtonPressed)
            BeginMoveDrag(e);
    }

    private void OnCloseClicked(object? sender, RoutedEventArgs e)
    {
        Close();
    }

    private void OnOpenVstGuiClicked(object? sender, RoutedEventArgs e)
    {
        NativeBridge.MVS_EffectChain_OpenStripEditor(_stripIndex);
    }

    private void BuildPresets()
    {
        PanelPresets.Children.Clear();
        string normType = _typeName.ToLowerInvariant();

        if (normType.Contains("pitch"))
        {
            AddPresetBtn("Octave Down (-12 st)", () => { SetParamValue("pitch", -12.0f); SetParamValue("fine", 0.0f); SetParamValue("mix", 100.0f); });
            AddPresetBtn("Deep Male Voice (-5 st)", () => { SetParamValue("pitch", -5.0f); SetParamValue("fine", 0.0f); SetParamValue("mix", 100.0f); });
            AddPresetBtn("Neutral / Clean (0 st)", () => { SetParamValue("pitch", 0.0f); SetParamValue("fine", 0.0f); SetParamValue("mix", 100.0f); });
            AddPresetBtn("Anime / Chipmunk (+7 st)", () => { SetParamValue("pitch", 7.0f); SetParamValue("fine", 0.0f); SetParamValue("mix", 100.0f); });
            AddPresetBtn("Octave Up (+12 st)", () => { SetParamValue("pitch", 12.0f); SetParamValue("fine", 0.0f); SetParamValue("mix", 100.0f); });
            AddPresetBtn("Micro Detune Chorus (+15 ct)", () => { SetParamValue("pitch", 0.0f); SetParamValue("fine", 15.0f); SetParamValue("mix", 50.0f); });
        }
        else if (normType.Contains("reverb"))
        {
            AddPresetBtn("Subtle Vocal Room", () => {
                SetParamValue("preDelay", 15.0f);
                SetParamValue("roomSize", 30.0f);
                SetParamValue("damping", 60.0f);
                SetParamValue("width", 80.0f);
                SetParamValue("lowCut", 120.0f);
                SetParamValue("highCut", 10000.0f);
                SetParamValue("mix", 25.0f);
            });
            AddPresetBtn("Studio Plate Reverb", () => {
                SetParamValue("preDelay", 25.0f);
                SetParamValue("roomSize", 60.0f);
                SetParamValue("damping", 35.0f);
                SetParamValue("width", 100.0f);
                SetParamValue("lowCut", 100.0f);
                SetParamValue("highCut", 14000.0f);
                SetParamValue("mix", 35.0f);
            });
            AddPresetBtn("Lush Vocal Concert Hall", () => {
                SetParamValue("preDelay", 40.0f);
                SetParamValue("roomSize", 85.0f);
                SetParamValue("damping", 25.0f);
                SetParamValue("width", 100.0f);
                SetParamValue("lowCut", 80.0f);
                SetParamValue("highCut", 16000.0f);
                SetParamValue("mix", 45.0f);
            });
            AddPresetBtn("Cathedral Space (Ambient)", () => {
                SetParamValue("preDelay", 60.0f);
                SetParamValue("roomSize", 98.0f);
                SetParamValue("damping", 15.0f);
                SetParamValue("width", 100.0f);
                SetParamValue("lowCut", 60.0f);
                SetParamValue("highCut", 18000.0f);
                SetParamValue("mix", 60.0f);
            });
        }
        else if (normType.Contains("delay"))
        {
            AddPresetBtn("Slapback (80ms)", () => { SetParamValue("time", 80.0f); SetParamValue("feedback", 15.0f); SetParamValue("mix", 30.0f); });
            AddPresetBtn("1/8 Note Stereo (250ms)", () => { SetParamValue("time", 250.0f); SetParamValue("feedback", 35.0f); SetParamValue("mix", 35.0f); });
            AddPresetBtn("1/4 Note Ping-Pong (500ms)", () => { SetParamValue("time", 500.0f); SetParamValue("feedback", 45.0f); SetParamValue("mix", 40.0f); });
            AddPresetBtn("Long Ambient Echo (800ms)", () => { SetParamValue("time", 800.0f); SetParamValue("feedback", 60.0f); SetParamValue("mix", 50.0f); });
        }
        else if (normType.Contains("robot"))
        {
            AddPresetBtn("Deep Cyborg (60Hz)", () => { SetParamValue("freq", 60.0f); SetParamValue("crush", 20.0f); SetParamValue("mix", 75.0f); });
            AddPresetBtn("Dalek (120Hz)", () => { SetParamValue("freq", 120.0f); SetParamValue("crush", 0.0f); SetParamValue("mix", 85.0f); });
            AddPresetBtn("Cyber Droid (240Hz)", () => { SetParamValue("freq", 240.0f); SetParamValue("crush", 35.0f); SetParamValue("mix", 80.0f); });
            AddPresetBtn("Alien Synth (440Hz)", () => { SetParamValue("freq", 440.0f); SetParamValue("crush", 50.0f); SetParamValue("mix", 90.0f); });
        }
        else if (normType.Contains("double"))
        {
            AddPresetBtn("Tight Double", () => { SetParamValue("spread", 40.0f); SetParamValue("detune", 10.0f); SetParamValue("delayTime", 15.0f); });
            AddPresetBtn("Wide Studio Doubler", () => { SetParamValue("spread", 85.0f); SetParamValue("detune", 20.0f); SetParamValue("delayTime", 28.0f); });
            AddPresetBtn("Thick Vocal Chorus", () => { SetParamValue("spread", 100.0f); SetParamValue("detune", 35.0f); SetParamValue("delayTime", 40.0f); });
        }
        else if (normType.Contains("saturat"))
        {
            AddPresetBtn("Warm Tube Preamp", () => { SetParamValue("drive", 20.0f); SetParamValue("warmth", 50.0f); SetParamValue("tone", 9000.0f); });
            AddPresetBtn("Analog Tape Saturation", () => { SetParamValue("drive", 45.0f); SetParamValue("warmth", 65.0f); SetParamValue("tone", 7000.0f); });
            AddPresetBtn("Heavy Drive Crunch", () => { SetParamValue("drive", 80.0f); SetParamValue("warmth", 85.0f); SetParamValue("tone", 5000.0f); });
        }
        else
        {
            PanelPresetsSection.IsVisible = false;
        }
    }

    private void AddPresetBtn(string text, Action apply)
    {
        var btn = new Button
        {
            Content = text,
            Classes = { "preset-pill" }
        };
        btn.Click += (s, e) => apply();
        PanelPresets.Children.Add(btn);
    }

    private void SetParamValue(string paramId, float value)
    {
        _isUpdatingParam = true;
        try
        {
            if (_paramControls.TryGetValue(paramId, out var tuple))
            {
                if (tuple.knob != null) tuple.knob.Value = value;
                if (tuple.slider != null) tuple.slider.Value = value;
                if (tuple.readout != null) tuple.readout.Text = FormatParamValue(value, tuple.param.Label);
                NativeBridge.SetStripParameterByIndex(_stripIndex, tuple.param.Index, value);
            }
            NativeBridge.SetStripParameter(_stripIndex, paramId, value);
            UpdateVisualizerParams();
        }
        finally
        {
            _isUpdatingParam = false;
        }
    }

    private void LoadAndBuildParameters()
    {
        PanelKnobs.Children.Clear();
        ContainerParams.Children.Clear();
        _paramControls.Clear();
        _parameters.Clear();

        var paramsList = NativeBridge.GetStripParameters(_stripIndex);
        if (paramsList.Count == 0)
        {
            paramsList = GetDefaultParametersForType(_typeName, _stripIndex);
        }

        _parameters.AddRange(paramsList);

        if (_parameters.Count == 0)
        {
            ContainerParams.Children.Add(new TextBlock
            {
                Text = "No adjustable parameters exposed by this module.",
                FontSize = 10,
                Foreground = new SolidColorBrush(Color.Parse("#888888")),
                HorizontalAlignment = HorizontalAlignment.Center,
                Margin = new Thickness(0, 30)
            });
            return;
        }

        // Build Rotary Knobs for Primary continuous parameters
        foreach (var p in _parameters)
        {
            bool isBoolean = Math.Abs(p.MaxValue - p.MinValue) <= 1.01 && p.MinValue == 0.0 && p.MaxValue == 1.0 && p.Label == "";
            if (!isBoolean)
            {
                var knobContainer = BuildKnobCard(p);
                PanelKnobs.Children.Add(knobContainer);
            }

            var sliderCard = BuildSliderRow(p);
            ContainerParams.Children.Add(sliderCard);
        }

        UpdateVisualizerParams();
    }

    private static List<EffectParamData> GetDefaultParametersForType(string typeName, int stripIndex)
    {
        var list = new List<EffectParamData>();
        string t = typeName.ToLowerInvariant();

        if (t.Contains("reverb"))
        {
            list.Add(new EffectParamData { Index = 0, StripIndex = stripIndex, ParamId = "preDelay", Name = "Pre-Delay", Value = 20f, MinValue = 0f, MaxValue = 200f, DefaultValue = 20f, Label = "ms" });
            list.Add(new EffectParamData { Index = 1, StripIndex = stripIndex, ParamId = "roomSize", Name = "Room Size / Decay", Value = 65f, MinValue = 0f, MaxValue = 100f, DefaultValue = 65f, Label = "%" });
            list.Add(new EffectParamData { Index = 2, StripIndex = stripIndex, ParamId = "damping", Name = "High Damping", Value = 40f, MinValue = 0f, MaxValue = 100f, DefaultValue = 40f, Label = "%" });
            list.Add(new EffectParamData { Index = 3, StripIndex = stripIndex, ParamId = "width", Name = "Stereo Width", Value = 100f, MinValue = 0f, MaxValue = 100f, DefaultValue = 100f, Label = "%" });
            list.Add(new EffectParamData { Index = 4, StripIndex = stripIndex, ParamId = "lowCut", Name = "Low Cut (HPF)", Value = 100f, MinValue = 20f, MaxValue = 1000f, DefaultValue = 100f, Label = "Hz" });
            list.Add(new EffectParamData { Index = 5, StripIndex = stripIndex, ParamId = "highCut", Name = "High Cut (LPF)", Value = 12000f, MinValue = 1000f, MaxValue = 20000f, DefaultValue = 12000f, Label = "Hz" });
            list.Add(new EffectParamData { Index = 6, StripIndex = stripIndex, ParamId = "earlyReflect", Name = "Early Reflections", Value = 60f, MinValue = 0f, MaxValue = 100f, DefaultValue = 60f, Label = "%" });
            list.Add(new EffectParamData { Index = 7, StripIndex = stripIndex, ParamId = "tailLevel", Name = "Reverb Tail", Value = 80f, MinValue = 0f, MaxValue = 100f, DefaultValue = 80f, Label = "%" });
            list.Add(new EffectParamData { Index = 8, StripIndex = stripIndex, ParamId = "mix", Name = "Dry/Wet Mix", Value = 35f, MinValue = 0f, MaxValue = 100f, DefaultValue = 35f, Label = "%" });
        }
        else if (t.Contains("pitch"))
        {
            list.Add(new EffectParamData { Index = 0, StripIndex = stripIndex, ParamId = "pitch", Name = "Pitch Shift", Value = 0f, MinValue = -24f, MaxValue = 24f, DefaultValue = 0f, Label = "st" });
            list.Add(new EffectParamData { Index = 1, StripIndex = stripIndex, ParamId = "fine", Name = "Fine Tune", Value = 0f, MinValue = -100f, MaxValue = 100f, DefaultValue = 0f, Label = "ct" });
            list.Add(new EffectParamData { Index = 2, StripIndex = stripIndex, ParamId = "formant", Name = "Formant / Gender", Value = 0f, MinValue = -12f, MaxValue = 12f, DefaultValue = 0f, Label = "st" });
            list.Add(new EffectParamData { Index = 3, StripIndex = stripIndex, ParamId = "grain", Name = "Grain Window", Value = 35f, MinValue = 10f, MaxValue = 80f, DefaultValue = 35f, Label = "ms" });
            list.Add(new EffectParamData { Index = 4, StripIndex = stripIndex, ParamId = "mix", Name = "Dry/Wet Mix", Value = 100f, MinValue = 0f, MaxValue = 100f, DefaultValue = 100f, Label = "%" });
        }
        else if (t.Contains("delay"))
        {
            list.Add(new EffectParamData { Index = 0, StripIndex = stripIndex, ParamId = "time", Name = "Delay Time", Value = 280f, MinValue = 10f, MaxValue = 1000f, DefaultValue = 280f, Label = "ms" });
            list.Add(new EffectParamData { Index = 1, StripIndex = stripIndex, ParamId = "feedback", Name = "Feedback", Value = 35f, MinValue = 0f, MaxValue = 90f, DefaultValue = 35f, Label = "%" });
            list.Add(new EffectParamData { Index = 2, StripIndex = stripIndex, ParamId = "mix", Name = "Dry/Wet Mix", Value = 30f, MinValue = 0f, MaxValue = 100f, DefaultValue = 30f, Label = "%" });
        }
        else if (t.Contains("robot"))
        {
            list.Add(new EffectParamData { Index = 0, StripIndex = stripIndex, ParamId = "freq", Name = "Modulation Pitch", Value = 120f, MinValue = 30f, MaxValue = 800f, DefaultValue = 120f, Label = "Hz" });
            list.Add(new EffectParamData { Index = 1, StripIndex = stripIndex, ParamId = "crush", Name = "Digital Grit (Crush)", Value = 0f, MinValue = 0f, MaxValue = 100f, DefaultValue = 0f, Label = "%" });
            list.Add(new EffectParamData { Index = 2, StripIndex = stripIndex, ParamId = "mix", Name = "Dry/Wet Mix", Value = 60f, MinValue = 0f, MaxValue = 100f, DefaultValue = 60f, Label = "%" });
        }
        else if (t.Contains("double"))
        {
            list.Add(new EffectParamData { Index = 0, StripIndex = stripIndex, ParamId = "spread", Name = "Stereo Spread", Value = 75f, MinValue = 0f, MaxValue = 100f, DefaultValue = 75f, Label = "%" });
            list.Add(new EffectParamData { Index = 1, StripIndex = stripIndex, ParamId = "detune", Name = "Detune Depth", Value = 15f, MinValue = 0f, MaxValue = 50f, DefaultValue = 15f, Label = "ct" });
            list.Add(new EffectParamData { Index = 2, StripIndex = stripIndex, ParamId = "delayTime", Name = "Delay Offset", Value = 20f, MinValue = 5f, MaxValue = 50f, DefaultValue = 20f, Label = "ms" });
            list.Add(new EffectParamData { Index = 3, StripIndex = stripIndex, ParamId = "doubleLevel", Name = "Double Level", Value = 80f, MinValue = 0f, MaxValue = 100f, DefaultValue = 80f, Label = "%" });
        }
        else if (t.Contains("saturat"))
        {
            list.Add(new EffectParamData { Index = 0, StripIndex = stripIndex, ParamId = "drive", Name = "Drive Level", Value = 25f, MinValue = 0f, MaxValue = 100f, DefaultValue = 25f, Label = "%" });
            list.Add(new EffectParamData { Index = 1, StripIndex = stripIndex, ParamId = "warmth", Name = "Tube Warmth", Value = 50f, MinValue = 0f, MaxValue = 100f, DefaultValue = 50f, Label = "%" });
            list.Add(new EffectParamData { Index = 2, StripIndex = stripIndex, ParamId = "tone", Name = "Tone Cutoff", Value = 8000f, MinValue = 1000f, MaxValue = 20000f, DefaultValue = 8000f, Label = "Hz" });
            list.Add(new EffectParamData { Index = 3, StripIndex = stripIndex, ParamId = "mix", Name = "Dry/Wet Mix", Value = 100f, MinValue = 0f, MaxValue = 100f, DefaultValue = 100f, Label = "%" });
        }

        return list;
    }

    private Border BuildKnobCard(EffectParamData p)
    {
        var card = new Border
        {
            Width = 94,
            Height = 106,
            Background = new SolidColorBrush(Color.Parse("#202020")),
            BorderBrush = new SolidColorBrush(Color.Parse("#333333")),
            BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(3),
            Margin = new Thickness(3)
        };

        var knob = new KnobControl
        {
            Minimum = p.MinValue,
            Maximum = p.MaxValue,
            DefaultValue = p.DefaultValue,
            Value = Math.Clamp(p.Value, p.MinValue, p.MaxValue),
            Unit = p.Label,
            KnobLabel = p.Name.Length > 12 ? p.Name[..12] : p.Name,
            Width = 88,
            Height = 100,
            HorizontalAlignment = HorizontalAlignment.Center,
            VerticalAlignment = VerticalAlignment.Center
        };

        knob.ValueChanged += (s, val) =>
        {
            if (_isUpdatingParam) return;
            _isUpdatingParam = true;
            try
            {
                float fVal = (float)val;
                NativeBridge.SetStripParameterByIndex(_stripIndex, p.Index, fVal);
                NativeBridge.SetStripParameter(_stripIndex, p.ParamId, fVal);
                if (_paramControls.TryGetValue(p.ParamId, out var tuple))
                {
                    if (tuple.slider != null) tuple.slider.Value = fVal;
                    if (tuple.readout != null) tuple.readout.Text = FormatParamValue(fVal, p.Label);
                }
                UpdateVisualizerParams();
            }
            finally
            {
                _isUpdatingParam = false;
            }
        };

        card.Child = knob;

        if (_paramControls.TryGetValue(p.ParamId, out var existing))
            _paramControls[p.ParamId] = (knob, existing.slider, existing.readout, p);
        else
            _paramControls[p.ParamId] = (knob, null, null, p);

        return card;
    }

    private Border BuildSliderRow(EffectParamData p)
    {
        var row = new Border
        {
            Background = new SolidColorBrush(Color.Parse("#202020")),
            BorderBrush = new SolidColorBrush(Color.Parse("#333333")),
            BorderThickness = new Thickness(1),
            CornerRadius = new CornerRadius(3),
            Padding = new Thickness(10, 6)
        };

        var grid = new Grid
        {
            ColumnDefinitions = new ColumnDefinitions("140,*,Auto,65"),
            Height = 24
        };

        var txtName = new TextBlock
        {
            Text = p.Name.ToUpperInvariant(),
            FontSize = 8.5,
            FontWeight = FontWeight.Bold,
            Foreground = new SolidColorBrush(Color.Parse("#CCCCCC")),
            VerticalAlignment = VerticalAlignment.Center
        };
        Grid.SetColumn(txtName, 0);

        var slider = new Slider
        {
            Minimum = p.MinValue,
            Maximum = p.MaxValue,
            Value = Math.Clamp(p.Value, p.MinValue, p.MaxValue),
            Height = 22,
            VerticalAlignment = VerticalAlignment.Center,
            Margin = new Thickness(6, 0)
        };
        Grid.SetColumn(slider, 1);

        var btnReset = new Button
        {
            Content = "0.0",
            Classes = { "studio-btn" },
            Padding = new Thickness(6, 2),
            FontSize = 7.5,
            Height = 20,
            Margin = new Thickness(4, 0)
        };
        Grid.SetColumn(btnReset, 2);

        var txtVal = new TextBlock
        {
            Text = FormatParamValue(p.Value, p.Label),
            FontSize = 8.5,
            FontWeight = FontWeight.Bold,
            Foreground = new SolidColorBrush(Color.Parse("#38BDF8")),
            HorizontalAlignment = HorizontalAlignment.Right,
            VerticalAlignment = VerticalAlignment.Center
        };
        Grid.SetColumn(txtVal, 3);

        slider.ValueChanged += (s, e) =>
        {
            if (_isUpdatingParam) return;
            _isUpdatingParam = true;
            try
            {
                float val = (float)slider.Value;
                NativeBridge.SetStripParameterByIndex(_stripIndex, p.Index, val);
                NativeBridge.SetStripParameter(_stripIndex, p.ParamId, val);
                txtVal.Text = FormatParamValue(val, p.Label);
                if (_paramControls.TryGetValue(p.ParamId, out var tuple) && tuple.knob != null)
                {
                    tuple.knob.Value = val;
                }
                UpdateVisualizerParams();
            }
            finally
            {
                _isUpdatingParam = false;
            }
        };

        btnReset.Click += (s, e) =>
        {
            _isUpdatingParam = true;
            try
            {
                slider.Value = p.DefaultValue;
                NativeBridge.SetStripParameterByIndex(_stripIndex, p.Index, p.DefaultValue);
                NativeBridge.SetStripParameter(_stripIndex, p.ParamId, p.DefaultValue);
                txtVal.Text = FormatParamValue(p.DefaultValue, p.Label);
                if (_paramControls.TryGetValue(p.ParamId, out var tuple) && tuple.knob != null)
                {
                    tuple.knob.Value = p.DefaultValue;
                }
                UpdateVisualizerParams();
            }
            finally
            {
                _isUpdatingParam = false;
            }
        };

        grid.Children.Add(txtName);
        grid.Children.Add(slider);
        grid.Children.Add(btnReset);
        grid.Children.Add(txtVal);
        row.Child = grid;

        if (_paramControls.TryGetValue(p.ParamId, out var existing))
            _paramControls[p.ParamId] = (existing.knob, slider, txtVal, p);
        else
            _paramControls[p.ParamId] = (null, slider, txtVal, p);

        return row;
    }

    private void UpdateVisualizerParams()
    {
        if (_parameters.Count > 0)
        {
            Visualizer.Param1 = GetParamValByIndex(0);
            Visualizer.Param2 = GetParamValByIndex(1);
            Visualizer.Param3 = GetParamValByIndex(2);
        }
    }

    private double GetParamValByIndex(int idx)
    {
        if (idx >= 0 && idx < _parameters.Count)
        {
            string id = _parameters[idx].ParamId;
            if (_paramControls.TryGetValue(id, out var tuple))
            {
                if (tuple.knob != null) return tuple.knob.Value;
                if (tuple.slider != null) return tuple.slider.Value;
            }
            return _parameters[idx].Value;
        }
        return 0.0;
    }

    private static string FormatParamValue(float val, string label)
    {
        string unit = string.IsNullOrWhiteSpace(label) ? "" : " " + label;
        if (Math.Abs(val - Math.Round(val)) < 0.001)
            return $"{(int)val}{unit}";
        return $"{val:0.0}{unit}";
    }

    private void OnResetAllClicked(object? sender, RoutedEventArgs e)
    {
        _isUpdatingParam = true;
        try
        {
            foreach (var p in _parameters)
            {
                if (_paramControls.TryGetValue(p.ParamId, out var tuple))
                {
                    if (tuple.knob != null) tuple.knob.Value = p.DefaultValue;
                    if (tuple.slider != null) tuple.slider.Value = p.DefaultValue;
                    if (tuple.readout != null) tuple.readout.Text = FormatParamValue(p.DefaultValue, p.Label);
                }
                NativeBridge.SetStripParameterByIndex(_stripIndex, p.Index, p.DefaultValue);
            }
            UpdateVisualizerParams();
        }
        finally
        {
            _isUpdatingParam = false;
        }
    }
}
