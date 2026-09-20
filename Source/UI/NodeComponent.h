#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>

class NodeCanvas;

/** One draggable box on the canvas, representing a single graph node. */
class NodeComponent : public juce::Component
{
public:
    NodeComponent (NodeCanvas& owner, juce::AudioProcessorGraph::NodeID nodeId, juce::String label,
                    bool hasInputPin, bool hasOutputPin, bool isSystemNode);
    ~NodeComponent() override;

    void paint (juce::Graphics&) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void setNodeColor(juce::Colour newColor) { nodeColor = newColor; repaint(); }
    bool isBypassed = false; // Bypass durumunu tutacak
    juce::AudioProcessorGraph::NodeID getNodeID() const { return nodeID; }
    bool isSystem() const { return systemNode; }
    bool isContainerNode() const;
    bool isSplitterNode() const;
    bool isJoinerNode() const;

    int getNumPins (bool isInput) const;
    juce::String getPinName (bool isInput, int pinIndex) const;
    juce::Colour getPinColour (bool isInput, int pinIndex) const;
    int pinIndexToChannel (bool isInput, int pinIndex) const;
    int channelToPinIndex (bool isInput, int channel) const;

    /** Opens or brings to front this node's plugin editor window (singleton). */
    void openEditorWindow();

    /** Centre point (in canvas coordinates) of a given input/output pin. */
    juce::Point<float> getPinPosition (bool isInput, int channelOrPinIndex) const;

    /** This node's bounds plus enough margin to cover cables/pins, for targeted repaints. */
    juce::Rectangle<int> getRepaintBounds() const;

    static constexpr int pinHitRadius = 22;
    static constexpr int headerHeight = 24;

    juce::Rectangle<float> getMixDialBounds() const;

private:
    NodeCanvas& canvas;
    juce::AudioProcessorGraph::NodeID nodeID;
    juce::String displayName;
    juce::ComponentDragger dragger;
    bool showInputPin, showOutputPin, systemNode;
    juce::Colour nodeColor = juce::Colour(0xff9e9e9e);
    int hitTestPin (juce::Point<int> localPos, bool& isInput) const;

    bool isDraggingMix = false;
    float dragStartMix = 1.0f;
    juce::Point<int> dragStartPos;
    bool rightClickCandidate = false;
};
