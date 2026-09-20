#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

class NodeCanvas;
class PluginProcessor;

/**
    Floating HUD Canvas Minimap (Navigator) located in the bottom-right corner of the canvas.
    Displays a bird's-eye view of all nodes and patch cables across the 4000x4000 logical canvas,
    with an interactive camera viewport frame that can be dragged to pan the canvas.
*/
class CanvasMinimap : public juce::Component
{
public:
    CanvasMinimap (NodeCanvas& canvas, PluginProcessor& processor);
    ~CanvasMinimap() override = default;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;

private:
    NodeCanvas& nodeCanvas;
    PluginProcessor& processor;

    juce::Rectangle<float> getMapArea() const;
    juce::Point<float> mapLogicalToMinimap (juce::Point<float> logicalPos) const;
    juce::Point<float> mapMinimapToLogical (juce::Point<float> minimapPos) const;

    void handleMouseNav (const juce::MouseEvent& e);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CanvasMinimap)
};
