#include "CanvasMinimap.h"
#include "NodeCanvas.h"
#include "../PluginProcessor.h"
#include "../UITheme.h"

CanvasMinimap::CanvasMinimap (NodeCanvas& canvas, PluginProcessor& proc)
    : nodeCanvas (canvas), processor (proc)
{
    setAlwaysOnTop (true);
}

juce::Rectangle<float> CanvasMinimap::getMapArea() const
{
    return getLocalBounds().toFloat().reduced (6.0f).withTrimmedTop (14.0f);
}

juce::Point<float> CanvasMinimap::mapLogicalToMinimap (juce::Point<float> logicalPos) const
{
    auto map = getMapArea();
    float nx = juce::jlimit (0.0f, 1.0f, logicalPos.x / 4000.0f);
    float ny = juce::jlimit (0.0f, 1.0f, logicalPos.y / 4000.0f);
    return { map.getX() + nx * map.getWidth(), map.getY() + ny * map.getHeight() };
}

juce::Point<float> CanvasMinimap::mapMinimapToLogical (juce::Point<float> minimapPos) const
{
    auto map = getMapArea();
    float nx = juce::jlimit (0.0f, 1.0f, (minimapPos.x - map.getX()) / map.getWidth());
    float ny = juce::jlimit (0.0f, 1.0f, (minimapPos.y - map.getY()) / map.getHeight());
    return { nx * 4000.0f, ny * 4000.0f };
}

void CanvasMinimap::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    const float corner = 8.0f;

    // 1. Frameless Soft Apple Dark Glass (Border removed)
    g.setColour (juce::Colour (0xd0151518));
    g.fillRoundedRectangle (bounds, corner);

    // Header label: "NAVIGATOR"
    g.setColour (UITheme::textTertiary.withAlpha (0.6f));
    g.setFont (UITheme::getFont (8.5f));
    g.drawText ("NAVIGATOR", juce::Rectangle<float> (bounds.getX() + 8.0f, bounds.getY() + 3.0f, 100.0f, 12.0f), juce::Justification::centredLeft);

    auto map = getMapArea();

    // Subtle frameless map background
    g.setColour (juce::Colour (0x80101012));
    g.fillRoundedRectangle (map, 4.0f);

    // 2. Draw patch cables in miniature
    auto& graph = processor.getGraph();
    g.setColour (UITheme::appleBlue.withAlpha (0.45f));
    for (auto& c : graph.getConnections())
    {
        auto* src = graph.getNodeForId (c.source.nodeID);
        auto* dst = graph.getNodeForId (c.destination.nodeID);
        if (src && dst)
        {
            float sx = (float) (int) src->properties["x"] + 75.0f;
            float sy = (float) (int) src->properties["y"] + 70.0f;
            float dx = (float) (int) dst->properties["x"] + 75.0f;
            float dy = (float) (int) dst->properties["y"] + 5.0f;

            auto p1 = mapLogicalToMinimap ({ sx, sy });
            auto p2 = mapLogicalToMinimap ({ dx, dy });
            g.drawLine (p1.x, p1.y, p2.x, p2.y, 1.0f);
        }
    }

    // 3. Draw nodes in miniature
    for (auto* node : graph.getNodes())
    {
        float nx = (float) (int) node->properties["x"];
        float ny = (float) (int) node->properties["y"];
        auto pt = mapLogicalToMinimap ({ nx, ny });

        bool isInput = node->nodeID == processor.getAudioInputNodeID();
        bool isOutput = node->nodeID == processor.getAudioOutputNodeID();
        bool isSelected = nodeCanvas.getSelectedNodeID() == node->nodeID;

        juce::Rectangle<float> miniNode (pt.x, pt.y, 7.0f, 4.0f);

        if (isSelected)
            g.setColour (UITheme::appleBlue);
        else if (isInput || isOutput)
            g.setColour (juce::Colour (0xff636366));
        else
            g.setColour (juce::Colour (0xff3f3f46));

        g.fillRoundedRectangle (miniNode, 1.0f);
    }

    // 4. Draw Camera Viewport Frame
    auto visible = nodeCanvas.getVisibleCanvasArea();
    auto pTopLeft = mapLogicalToMinimap (visible.getTopLeft());
    auto pBottomRight = mapLogicalToMinimap (visible.getBottomRight());

    juce::Rectangle<float> camRect (pTopLeft, pBottomRight);
    camRect = camRect.constrainedWithin (map);

    // Glowing camera viewport fill
    g.setColour (UITheme::appleBlue.withAlpha (0.12f));
    g.fillRoundedRectangle (camRect, 2.0f);

    // Camera viewport stroke
    g.setColour (UITheme::appleBlue.withAlpha (0.85f));
    g.drawRoundedRectangle (camRect, 2.0f, 1.2f);
}

void CanvasMinimap::handleMouseNav (const juce::MouseEvent& e)
{
    auto map = getMapArea();
    if (! map.contains (e.position))
        return;

    auto logicalTarget = mapMinimapToLogical (e.position);
    nodeCanvas.panToLogicalCenter (logicalTarget);
    repaint();
}

void CanvasMinimap::mouseDown (const juce::MouseEvent& e)
{
    handleMouseNav (e);
}

void CanvasMinimap::mouseDrag (const juce::MouseEvent& e)
{
    handleMouseNav (e);
}
