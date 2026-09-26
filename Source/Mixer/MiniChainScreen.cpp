#include "MiniChainScreen.h"

MiniChainScreen::MiniChainScreen()
{
    setMouseCursor(juce::MouseCursor::PointingHandCursor);
    startTimerHz(30);
}

MiniChainScreen::~MiniChainScreen()
{
    stopTimer();
}

void MiniChainScreen::setChannelInfo(const juce::String& name, bool isAssigned, bool hasActiveDSP)
{
    channelTitle = name;
    channelAssigned = isAssigned;
    hasDspChain = hasActiveDSP;
    repaint();
}

void MiniChainScreen::setDspNodes(const std::vector<juce::String>& nodes)
{
    dspNodes = nodes;
    hasDspChain = !nodes.empty();
    repaint();
}

void MiniChainScreen::setAudioLevel(float level)
{
    currentAudioLevel = juce::jlimit(0.0f, 1.0f, level);
}

void MiniChainScreen::setBypassed(bool bypassed)
{
    isBypassActive = bypassed;
    repaint();
}

void MiniChainScreen::timerCallback()
{
    visualPulsePhase += 0.08f;
    if (visualPulsePhase > juce::MathConstants<float>::twoPi)
        visualPulsePhase -= juce::MathConstants<float>::twoPi;

    currentAudioLevel *= 0.88f;

    if (isShowing())
        repaint();
}

void MiniChainScreen::resized()
{
    powerButtonArea = juce::Rectangle<int>(getWidth() - 24, 6, 18, 18);
}

void MiniChainScreen::mouseEnter(const juce::MouseEvent&)
{
    isHovered = true;
    repaint();
}

void MiniChainScreen::mouseExit(const juce::MouseEvent&)
{
    isHovered = false;
    repaint();
}

void MiniChainScreen::mouseDown(const juce::MouseEvent& e)
{
    if (powerButtonArea.contains(e.getPosition()) && hasDspChain)
    {
        isBypassActive = !isBypassActive;
        if (onBypassToggled)
            onBypassToggled(isBypassActive);
        repaint();
        return;
    }

    if (onOpenCanvasRequested && channelAssigned)
    {
        onOpenCanvasRequested();
    }
}

void MiniChainScreen::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    
    // Sunken Screen Slot
    g.setColour(CalmTheme::bgSunken);
    g.fillRoundedRectangle(bounds, 7.0f);

    // Subtle Border
    juce::Colour borderCol = isHovered && channelAssigned 
        ? CalmTheme::cyanSoft 
        : (hasDspChain && !isBypassActive ? CalmTheme::borderSubtle : CalmTheme::borderSubtle.withAlpha(0.6f));

    g.setColour(borderCol);
    g.drawRoundedRectangle(bounds, 7.0f, isHovered ? 1.5f : 1.0f);

    if (!channelAssigned)
    {
        g.setColour(CalmTheme::textMuted);
        g.setFont(juce::FontOptions(15.0f).withStyle("Bold"));
        g.drawText("+", bounds.removeFromTop(bounds.getHeight() * 0.55f), juce::Justification::centred, false);
        
        g.setFont(juce::FontOptions(9.5f));
        g.drawText("EMPTY SLOT", bounds, juce::Justification::centred, false);
        return;
    }

    // Title
    g.setFont(juce::FontOptions(8.5f).withStyle("Bold"));
    g.setColour(isBypassActive ? CalmTheme::textMuted : CalmTheme::cyanSoft);
    g.drawText("DSP CHAIN", bounds.reduced(8, 6).removeFromTop(12), juce::Justification::topLeft, false);

    // Power / Bypass Button
    if (hasDspChain)
    {
        auto pb = powerButtonArea.toFloat();
        juce::Colour pCol = isBypassActive ? CalmTheme::coralRed : CalmTheme::sageGreen;
        g.setColour(pCol.withAlpha(0.18f));
        g.fillEllipse(pb);
        g.setColour(pCol);
        g.drawEllipse(pb, 1.2f);
        
        // Power tick & ring
        g.drawLine(pb.getCentreX(), pb.getY() + 3.5f, pb.getCentreX(), pb.getY() + 8.5f, 1.2f);
        juce::Path arc;
        arc.addCentredArc(pb.getCentreX(), pb.getCentreY(), pb.getWidth() * 0.32f, pb.getHeight() * 0.32f, 0.0f, 0.7f, juce::MathConstants<float>::twoPi - 0.7f, true);
        g.strokePath(arc, juce::PathStrokeType(1.2f));
    }

    // Schematic nodes
    auto flowArea = bounds.reduced(8, 16);
    float midY = flowArea.getCentreY() + 3.0f;
    float startX = flowArea.getX() + 8.0f;
    float endX = flowArea.getRight() - 8.0f;

    juce::Colour cableColor = isBypassActive ? CalmTheme::borderSubtle : CalmTheme::cyanSoft.withAlpha(0.45f);
    g.setColour(cableColor);
    g.drawLine(startX, midY, endX, midY, 1.8f);

    // Animated signal pulse
    if (!isBypassActive && currentAudioLevel > 0.01f)
    {
        float pulseX = startX + (endX - startX) * (0.5f + 0.5f * std::sin(visualPulsePhase));
        g.setColour(CalmTheme::sageGreen.withAlpha(juce::jlimit(0.3f, 0.95f, currentAudioLevel * 2.0f)));
        g.fillEllipse(pulseX - 3.0f, midY - 3.0f, 6.0f, 6.0f);
    }

    // Dynamic Node List Assembly
    struct NodeItem { float x; juce::String txt; juce::Colour c; };
    std::vector<NodeItem> items;

    // Helper to abbreviate and color code modules
    auto getModulePill = [](const juce::String& name) -> std::pair<juce::String, juce::Colour>
    {
        juce::String lower = name.toLowerCase();
        if (lower.contains("gain")) return { "GAIN", CalmTheme::cyanSoft };
        if (lower.contains("gate")) return { "GATE", CalmTheme::amberSoft };
        if (lower.contains("rnnoise") || lower.contains("denoise") || lower.contains("noise") || lower.contains("neural")) return { "AI", CalmTheme::purpleSoft };
        if (lower.contains("comp")) return { "COMP", CalmTheme::amberSoft };
        if (lower.contains("limit")) return { "LIM", CalmTheme::amberSoft };
        if (lower.contains("eq") || lower.contains("equal")) return { "EQ", CalmTheme::cyanSoft };
        if (lower.contains("de-ess") || lower.contains("deess") || lower.contains("de ess")) return { "DS", CalmTheme::cyanSoft };
        if (lower.contains("reverb")) return { "REV", CalmTheme::cyanSoft };
        if (lower.contains("delay")) return { "DLY", CalmTheme::cyanSoft };
        if (lower.contains("pitch") || lower.contains("autotune")) return { "PTCH", CalmTheme::purpleSoft };
        if (lower.contains("spatial") || lower.contains("3d")) return { "3D", CalmTheme::purpleSoft };

        juce::String shortTxt = name.substring(0, 4).toUpperCase();
        return { shortTxt.isEmpty() ? "DSP" : shortTxt, CalmTheme::cyanSoft };
    };

    items.push_back({ startX, "IN", CalmTheme::cyanSoft });

    int numMiddle = static_cast<int>(dspNodes.size());
    if (numMiddle > 0 && numMiddle <= 4)
    {
        float step = (endX - startX) / static_cast<float>(numMiddle + 1);
        for (int i = 0; i < numMiddle; ++i)
        {
            auto info = getModulePill(dspNodes[static_cast<size_t>(i)]);
            items.push_back({ startX + (i + 1) * step, info.first, info.second });
        }
    }
    else if (numMiddle > 4)
    {
        float step = (endX - startX) / 4.0f;
        auto info0 = getModulePill(dspNodes[0]);
        auto info1 = getModulePill(dspNodes[1]);
        items.push_back({ startX + 1.0f * step, info0.first, info0.second });
        items.push_back({ startX + 2.0f * step, info1.first, info1.second });
        items.push_back({ startX + 3.0f * step, "+" + juce::String(numMiddle - 2), CalmTheme::purpleSoft });
    }

    items.push_back({ endX, "OUT", CalmTheme::sageGreen });

    float pillW = 26.0f;
    float pillH = 15.0f;

    for (const auto& item : items)
    {
        juce::Rectangle<float> nRect(item.x - pillW * 0.5f, midY - pillH * 0.5f, pillW, pillH);
        g.setColour(CalmTheme::bgCardElevated);
        g.fillRoundedRectangle(nRect, 3.5f);

        g.setColour(isBypassActive ? CalmTheme::borderSubtle : item.c);
        g.drawRoundedRectangle(nRect, 3.5f, 1.0f);

        g.setFont(juce::FontOptions(7.5f).withStyle("Bold"));
        g.setColour(isBypassActive ? CalmTheme::textMuted : CalmTheme::textPrimary);
        g.drawText(item.txt, nRect, juce::Justification::centred, false);
    }

    // Hover Indicator
    if (isHovered)
    {
        auto hintArea = bounds.removeFromBottom(15).reduced(4, 1);
        g.setColour(CalmTheme::bgTopbar.withAlpha(0.95f));
        g.fillRoundedRectangle(hintArea, 3.0f);

        g.setColour(CalmTheme::cyanSoft);
        g.setFont(juce::FontOptions(8.0f).withStyle("Bold"));
        g.drawText("CLICK TO OPEN CANVAS", hintArea, juce::Justification::centred, false);
    }
}
