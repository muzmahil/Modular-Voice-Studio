#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "MiniChainScreen.h"
#include "CalmTheme.h"

class ChannelStripComponent : public juce::Component, private juce::Timer
{
public:
    ChannelStripComponent(int channelIndex, const juce::String& defaultName, bool isPreAssigned, bool hasDefaultDSP);
    ~ChannelStripComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void setAudioPeak(float leftPeak, float rightPeak);
    void setChannelAssigned(bool assigned, const juce::String& name = {});
    void setDspNodes(const std::vector<juce::String>& nodes);
    void refreshSourceList();

    int getChannelIndex() const { return channelIndex; }
    float getFaderVolume() const { return static_cast<float>(faderSlider.getValue()); }
    bool isMuted() const { return muteButton.getToggleState(); }

    std::function<void(int channelIndex)> onOpenCanvasRequested;
    std::function<void(int channelIndex, bool bypassed)> onBypassChanged;
    std::function<void(int channelIndex)> onAssignRequested;
    std::function<void(int channelIndex, float faderDb)> onFaderChanged;
    std::function<void(int channelIndex, bool muted)> onMuteChanged;
    std::function<void(int channelIndex, const juce::String& deviceName)> onSourceSelected;
    std::function<void()> onOpenSettingsRequested;

private:
    void timerCallback() override;

    int channelIndex;
    juce::String channelName;
    bool isAssigned { true };
    bool hasDSP { true };

    CalmConsoleLookAndFeel calmLnF;

    MiniChainScreen miniChainScreen;
    juce::ComboBox sourceSelector;

    juce::Slider faderSlider;
    juce::Label dbValueLabel;

    juce::TextButton muteButton { "MUTE" };
    juce::TextButton assignButton { "+ ASSIGN CHANNEL" };

    struct DeviceEntry {
        bool isInput { true };
        juce::String deviceName;
    };
    std::map<int, DeviceEntry> deviceMap;

    float leftPeakLevel { 0.0f };
    float rightPeakLevel { 0.0f };
    float smoothedMeterLevel { 0.0f };
    float peakHoldLevel { 0.0f };
    int peakHoldTimer { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChannelStripComponent)
};
