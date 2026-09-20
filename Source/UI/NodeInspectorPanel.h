#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Localization.h"

class NodeCanvas;
class PluginProcessor;

/**
    Right sidebar panel that automatically inspects the currently selected canvas node.
    Provides instant two-way parameter controls (sliders/toggles), bypass button,
    and direct actions without requiring floating windows.
*/
class NodeInspectorPanel : public juce::Component,
                           public LocalizationManager::Listener
{
public:
    NodeInspectorPanel (PluginProcessor& processor, NodeCanvas& canvas);
    ~NodeInspectorPanel() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void localizationChanged() override;

    void inspectNode (juce::AudioProcessorGraph::NodeID nodeID);
    void clearInspection();

    std::function<void()> onCollapseRequested;

private:
    PluginProcessor& processor;
    NodeCanvas& nodeCanvas;

    juce::AudioProcessorGraph::NodeID currentNodeID;

    // Header controls
    juce::TextButton collapseBtn { juce::CharPointer_UTF8 ("\xe2\x80\xba") }; // '›'
    juce::TextButton powerBtn;
    juce::TextButton openWindowBtn { "Open UI" };
    juce::TextButton deleteBtn { "Delete" };

    // Parameter sliders list
    struct ParamRow : public juce::Component
    {
        ParamRow (juce::AudioProcessorParameter* param);
        void paint (juce::Graphics& g) override;
        void resized() override;

        juce::AudioProcessorParameter* parameter;
        juce::Slider slider;
        juce::Label nameLabel;
        juce::Label valueLabel;
    };

    juce::Viewport paramViewport;
    juce::Component paramContainer;
    juce::OwnedArray<ParamRow> paramRows;

    void rebuildParams();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NodeInspectorPanel)
};
