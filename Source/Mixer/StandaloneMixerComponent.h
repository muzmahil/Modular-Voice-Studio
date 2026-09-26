#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "ChannelStripComponent.h"
#include "CalmTheme.h"
#include "../PluginProcessor.h"

class StandaloneMixerComponent : public juce::Component, 
                                 public juce::ChangeListener,
                                 private juce::Timer
{
public:
    explicit StandaloneMixerComponent (PluginProcessor& proc);
    ~StandaloneMixerComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void updateAudioMeters();
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void refreshOutputDeviceLists();

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;

    std::function<void(int channelIndex)> onOpenModularCanvas;
    std::function<void()> onOpenSettings;

private:
    void timerCallback() override;

    PluginProcessor& processor;
    CalmConsoleLookAndFeel calmLnF;

    // Minimal Sleek TopBar & Window Controls
    juce::ComponentDragger windowDragger;
    juce::TextButton menuBtn    { "MENU ▾" };
    juce::TextButton btnMin     { "—" };
    juce::TextButton btnClose   { "✕" };

    // Single Voice Channel Strip (MIC 1)
    std::unique_ptr<ChannelStripComponent> micStrip;

    // VB-CABLE Virtual Mic Output Controls
    juce::Label vbCableHeader;
    juce::ComboBox vbCableDeviceBox;
    juce::Slider vbCableFader;
    juce::Label vbCableDbLabel;
    juce::TextButton vbCableMuteBtn { "MUTE" };
    juce::Label vbCableStatusLabel;
    float vbCablePeak = 0.0f;

    // Headphone Monitor (A1) Output Controls
    juce::Label monitorHeader;
    juce::ComboBox monitorDeviceBox;
    juce::Slider monitorFader;
    juce::Label monitorDbLabel;
    juce::TextButton monitorMuteBtn { "MUTE" };
    juce::ToggleButton monitorEnableBtn { "MONITOR VOICE (LOCAL HEADPHONES)" };
    float monitorPeak = 0.0f;

    // Quick Action Buttons
    juce::TextButton openCanvasBtn { "OPEN MODULAR DSP CANVAS  >" };
    juce::TextButton resetDspBtn { "RESET DSP CHAIN" };
    juce::TextButton audioSettingsBtn { "AUDIO DEVICE SETTINGS" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StandaloneMixerComponent)
};
