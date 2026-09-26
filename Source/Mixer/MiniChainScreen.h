#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "CalmTheme.h"

class MiniChainScreen : public juce::Component, private juce::Timer
{
public:
    MiniChainScreen();
    ~MiniChainScreen() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseEnter(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;
    void mouseDown(const juce::MouseEvent& e) override;

    void setChannelInfo(const juce::String& name, bool isAssigned, bool hasActiveDSP);
    void setDspNodes(const std::vector<juce::String>& nodes);
    void setAudioLevel(float level);
    void setBypassed(bool bypassed);
    bool isBypassed() const { return isBypassActive; }

    std::function<void()> onOpenCanvasRequested;
    std::function<void(bool)> onBypassToggled;

private:
    void timerCallback() override;

    juce::String channelTitle { "MIC 1" };
    bool channelAssigned { true };
    bool hasDspChain { true };
    bool isBypassActive { false };
    bool isHovered { false };
    std::vector<juce::String> dspNodes;

    float currentAudioLevel { 0.0f };
    float visualPulsePhase { 0.0f };

    juce::Rectangle<int> powerButtonArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MiniChainScreen)
};
