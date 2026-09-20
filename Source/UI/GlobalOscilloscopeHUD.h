#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_dsp/juce_dsp.h>
#include "../Localization.h"
#include <vector>

class PluginProcessor;

//==============================================================================
/** Detailed CRT Oscilloscope & Waveform Comparison Component shown inside the popup window. */
class OscilloscopeDetailView : public juce::Component, public juce::Timer, public LocalizationManager::Listener
{
public:
    OscilloscopeDetailView (PluginProcessor& processor);
    ~OscilloscopeDetailView() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;
    void localizationChanged() override;

private:
    PluginProcessor& processor;
    std::vector<float> inputWave;
    std::vector<float> outputWave;

    enum ViewMode { CompareDual, InputOnly, OutputOnly, ImpactDelta, SpectrumFFT };
    ViewMode currentMode = CompareDual;
    bool isFrozen = false;
    float verticalScale = 1.0f;

    juce::TextButton modeCompareBtn  { "Dual Compare" };
    juce::TextButton modeInputBtn    { "Input Only" };
    juce::TextButton modeOutputBtn   { "Output Only" };
    juce::TextButton modeDeltaBtn    { "Delta / Impact" };
    juce::TextButton modeSpectrumBtn { "FFT Spectrum" };
    juce::TextButton freezeBtn       { "Freeze" };
    juce::ComboBox   scaleCombo;

    juce::dsp::FFT fftEngine { 9 }; // 512 points
    juce::dsp::WindowingFunction<float> window { 512, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> fftBufferIn;
    std::vector<float> fftBufferOut;
    std::vector<float> spectrumIn;
    std::vector<float> spectrumOut;

    void computeFFT();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OscilloscopeDetailView)
};

//==============================================================================
/** Independent Floating Window for Detailed Audio Analysis & Comparison */
class OscilloscopeWindow : public juce::DocumentWindow
{
public:
    OscilloscopeWindow (PluginProcessor& processor);
    ~OscilloscopeWindow() override = default;

    void closeButtonPressed() override;

private:
    std::unique_ptr<OscilloscopeDetailView> detailView;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OscilloscopeWindow)
};

//==============================================================================
/** Compact HUD Waveform Monitor docked in the bottom-right of the canvas. */
class GlobalOscilloscopeHUD : public juce::Component,
                              public juce::SettableTooltipClient,
                              public juce::Timer,
                              public LocalizationManager::Listener
{
public:
    GlobalOscilloscopeHUD (PluginProcessor& processor);
    ~GlobalOscilloscopeHUD() override;

    void paint (juce::Graphics& g) override;
    void timerCallback() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void localizationChanged() override;

    void openDetailedWindow();

private:
    PluginProcessor& processor;
    std::vector<float> inputWave;
    std::vector<float> outputWave;
    std::unique_ptr<OscilloscopeWindow> activeWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlobalOscilloscopeHUD)
};
