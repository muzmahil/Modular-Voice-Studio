#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace CalmTheme
{
    // Knox-Inspired Neutral Studio Slate Grey Palette
    inline const juce::Colour bgApp         { 0xFF282C35 }; // Main Studio Slate Grey background
    inline const juce::Colour bgTopbar      { 0xFF1E2128 }; // Topbar background
    inline const juce::Colour bgCard        { 0xFF333842 }; // Channel strip card
    inline const juce::Colour bgCardElevated{ 0xFF3D4350 }; // Raised buttons / header chips
    inline const juce::Colour bgSunken      { 0xFF1C1F26 }; // Recessed meter slots / fader tracks
    inline const juce::Colour borderSubtle  { 0xFF454C5A }; // Clean 1px neutral border
    inline const juce::Colour borderStrong  { 0xFF586274 }; // High-contrast border

    // Accents
    inline const juce::Colour cyanSoft      { 0xFF00B4D8 }; // Crisp Studio Cyan (Mic 1)
    inline const juce::Colour amberSoft     { 0xFFF59E0B }; // Warm Amber (Desktop / System)
    inline const juce::Colour sageGreen     { 0xFF10B981 }; // Emerald Green (Signal / A1)
    inline const juce::Colour coralRed      { 0xFFF43F5E }; // Coral Red (Mute / Close)
    inline const juce::Colour purpleSoft    { 0xFFA855F7 }; // AI Node accent

    // Typography
    inline const juce::Colour textPrimary   { 0xFFFFFFFF }; // Pure white text
    inline const juce::Colour textSecondary { 0xFFCBD5E1 }; // Light silver text
    inline const juce::Colour textMuted     { 0xFF94A3B8 }; // Slate gray text
}

namespace MeterMath
{
    inline float dbToLevel (float db)
    {
        if (db <= -60.0f) return 0.0f;
        if (db >= 18.0f)  return 1.0f;
        if (db < -24.0f)  return juce::jmap (db, -60.0f, -24.0f, 0.0f, 0.30f);
        if (db < -12.0f)  return juce::jmap (db, -24.0f, -12.0f, 0.30f, 0.52f);
        if (db < 0.0f)    return juce::jmap (db, -12.0f,   0.0f, 0.52f, 0.76f);
        return juce::jmap (db, 0.0f, 18.0f, 0.76f, 1.0f);
    }

    inline float levelToDb (float level)
    {
        if (level <= 0.0f) return -60.0f;
        if (level >= 1.0f) return 18.0f;
        if (level < 0.30f) return juce::jmap (level, 0.0f, 0.30f, -60.0f, -24.0f);
        if (level < 0.52f) return juce::jmap (level, 0.30f, 0.52f, -24.0f, -12.0f);
        if (level < 0.76f) return juce::jmap (level, 0.52f, 0.76f, -12.0f, 0.0f);
        return juce::jmap (level, 0.76f, 1.0f, 0.0f, 18.0f);
    }
}

class CalmConsoleLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CalmConsoleLookAndFeel()
    {
        setColour(juce::ComboBox::backgroundColourId, CalmTheme::bgCardElevated);
        setColour(juce::ComboBox::outlineColourId, CalmTheme::borderSubtle);
        setColour(juce::ComboBox::textColourId, CalmTheme::textPrimary);
        setColour(juce::ComboBox::arrowColourId, CalmTheme::cyanSoft);
        
        setColour(juce::PopupMenu::backgroundColourId, CalmTheme::bgTopbar);
        setColour(juce::PopupMenu::textColourId, CalmTheme::textPrimary);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, CalmTheme::cyanSoft);
        setColour(juce::PopupMenu::highlightedTextColourId, CalmTheme::bgApp);
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override
    {
        juce::ignoreUnused(slider);
        auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                             static_cast<float>(width), static_cast<float>(height)).reduced(2.0f);
        auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        auto center = bounds.getCentre();

        // Recessed outer bezel
        g.setColour(CalmTheme::bgSunken);
        g.fillEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f);
        g.setColour(CalmTheme::borderSubtle);
        g.drawEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f, 1.0f);

        // Center zero angle (12 o'clock)
        float zeroAngle = (rotaryStartAngle + rotaryEndAngle) * 0.5f;
        float currentAngle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);
        float trackThickness = 2.5f;
        float arcRadius = radius - 3.5f;

        // Background Track Ring
        juce::Path bgPath;
        bgPath.addCentredArc(center.x, center.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(CalmTheme::bgCardElevated);
        g.strokePath(bgPath, juce::PathStrokeType(trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Active Arc Fill (Bipolar from center 12 o'clock)
        if (std::abs(currentAngle - zeroAngle) > 0.02f)
        {
            float a1 = juce::jmin(zeroAngle, currentAngle);
            float a2 = juce::jmax(zeroAngle, currentAngle);
            juce::Path activePath;
            activePath.addCentredArc(center.x, center.y, arcRadius, arcRadius, 0.0f, a1, a2, true);
            
            juce::Colour arcCol = (currentAngle >= zeroAngle) ? CalmTheme::cyanSoft : CalmTheme::amberSoft;
            g.setColour(arcCol);
            g.strokePath(activePath, juce::PathStrokeType(trackThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Center 0dB tick mark
        float tickLen = 3.0f;
        g.setColour(CalmTheme::textMuted);
        g.drawLine(center.x, center.y - arcRadius + 1.0f, center.x, center.y - arcRadius - tickLen, 1.0f);

        // Inner Knob Core
        auto knobRadius = radius - 7.0f;
        auto knobArea = juce::Rectangle<float>(center.x - knobRadius, center.y - knobRadius, knobRadius * 2.0f, knobRadius * 2.0f);
        
        // Solid Studio Disc with Chamfer
        g.setColour(CalmTheme::bgCard);
        g.fillEllipse(knobArea);
        g.setColour(CalmTheme::borderStrong);
        g.drawEllipse(knobArea, 1.2f);

        // Pointer Line
        float pCos = std::cos(currentAngle - juce::MathConstants<float>::halfPi);
        float pSin = std::sin(currentAngle - juce::MathConstants<float>::halfPi);
        float innerDist = knobRadius * 0.25f;
        float outerDist = knobRadius * 0.85f;
        
        juce::Colour pointerCol = (std::abs(currentAngle - zeroAngle) < 0.03f) ? CalmTheme::textSecondary :
                                  (currentAngle > zeroAngle ? CalmTheme::cyanSoft : CalmTheme::amberSoft);
        g.setColour(pointerCol);
        g.drawLine(center.x + innerDist * pCos, center.y + innerDist * pSin,
                   center.x + outerDist * pCos, center.y + outerDist * pSin, 2.0f);
    }

    int getSliderThumbRadius (juce::Slider&) override
    {
        return 15;
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        if (style == juce::Slider::LinearVertical)
        {
            float trackW = 4.0f;
            float trackX = x + (width - trackW) * 0.5f;
            float trackY = static_cast<float>(y) + 6.0f;
            float trackH = static_cast<float>(height) - 12.0f;

            // Recessed Minimal Track
            g.setColour(CalmTheme::bgSunken);
            g.fillRoundedRectangle(trackX, trackY, trackW, trackH, 2.0f);
            g.setColour(CalmTheme::borderSubtle);
            g.drawRoundedRectangle(trackX, trackY, trackW, trackH, 2.0f, 1.0f);

            // Center 0 dB marker notch on fader track
            float zeroPos = static_cast<float>(slider.getPositionOfValue(0.0));
            g.setColour(CalmTheme::borderStrong);
            g.drawLine(trackX - 3.0f, zeroPos, trackX + trackW + 3.0f, zeroPos, 1.0f);

            // Active Track Fill
            float bottom = trackY + trackH;
            float fillH = bottom - sliderPos;
            if (fillH > 0.0f)
            {
                g.setColour(CalmTheme::cyanSoft.withAlpha(0.6f));
                g.fillRoundedRectangle(trackX, sliderPos, trackW, fillH, 2.0f);
            }

            // Ergonomic Smooth Capsule Fader Handle
            float capW = 24.0f;
            float capH = 30.0f;
            float capX = x + (width - capW) * 0.5f;
            float capY = sliderPos - capH * 0.5f;

            juce::Rectangle<float> capRect(capX, capY, capW, capH);

            // Drop shadow
            g.setColour(juce::Colour(0x60000000));
            g.fillRoundedRectangle(capRect.translated(0, 2.0f), 4.0f);

            // Cap Body (Studio Slate Grey)
            g.setColour(CalmTheme::bgCardElevated);
            g.fillRoundedRectangle(capRect, 4.0f);
            g.setColour(CalmTheme::borderStrong);
            g.drawRoundedRectangle(capRect, 4.0f, 1.0f);

            // Grip ribs
            g.setColour(CalmTheme::borderSubtle);
            g.drawLine(capX + 4.0f, capY + 6.0f, capX + capW - 4.0f, capY + 6.0f, 1.0f);
            g.drawLine(capX + 4.0f, capY + capH - 6.0f, capX + capW - 4.0f, capY + capH - 6.0f, 1.0f);

            // Center Indicator Line
            g.setColour(CalmTheme::cyanSoft);
            g.drawLine(capX + 3.0f, capY + capH * 0.5f, capX + capW - 3.0f, capY + capH * 0.5f, 2.0f);
        }
        else
        {
            juce::LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        }
    }
};
