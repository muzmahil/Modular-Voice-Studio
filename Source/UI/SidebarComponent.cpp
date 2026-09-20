#include "SidebarComponent.h"
#include "NodeCanvas.h"
#include "../UITheme.h"
#include "../Graph/PresetManager.h"
#include "../Localization.h"

// --- Draw Vector DSP Icons ---
void LibraryItem::drawModuleIcon (juce::Graphics& g, const juce::String& type, juce::Rectangle<float> area, juce::Colour color)
{
    g.setColour (color);
    auto b = area.reduced (1.0f);
    float cx = b.getCentreX();
    float cy = b.getCentreY();
    float w = b.getWidth();
    float h = b.getHeight();

    juce::Path p;

    if (type.equalsIgnoreCase ("Gain"))
    {
        // Stylized "A" / Amplitude icon
        p.startNewSubPath (b.getX() + w * 0.15f, b.getBottom());
        p.lineTo (cx, b.getY() + 1.0f);
        p.lineTo (b.getRight() - w * 0.15f, b.getBottom());
        p.startNewSubPath (b.getX() + w * 0.28f, cy + h * 0.12f);
        p.lineTo (b.getRight() - w * 0.28f, cy + h * 0.12f);
        g.strokePath (p, juce::PathStrokeType (1.5f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
    }
    else if (type.equalsIgnoreCase ("Compressor"))
    {
        // Three vertical fader sliders with slider knobs (|||)
        float x1 = b.getX() + w * 0.20f;
        float x2 = cx;
        float x3 = b.getRight() - w * 0.20f;
        g.drawVerticalLine ((int) x1, b.getY() + 1.0f, b.getBottom() - 1.0f);
        g.drawVerticalLine ((int) x2, b.getY() + 1.0f, b.getBottom() - 1.0f);
        g.drawVerticalLine ((int) x3, b.getY() + 1.0f, b.getBottom() - 1.0f);

        // Slider knobs
        g.fillRect (x1 - 2.5f, b.getY() + h * 0.55f, 5.0f, 3.5f);
        g.fillRect (x2 - 2.5f, b.getY() + h * 0.25f, 5.0f, 3.5f);
        g.fillRect (x3 - 2.5f, b.getY() + h * 0.65f, 5.0f, 3.5f);
    }
    else if (type.equalsIgnoreCase ("Limiter"))
    {
        // Ceiling threshold line + brickwall flattening curve
        g.drawHorizontalLine ((int) (b.getY() + 2.0f), b.getX(), b.getRight());
        p.startNewSubPath (b.getX(), b.getBottom() - 1.0f);
        p.quadraticTo (b.getX() + w * 0.45f, b.getY() + 4.0f, b.getRight(), b.getY() + 4.0f);
        g.strokePath (p, juce::PathStrokeType (1.5f));
    }
    else if (type.equalsIgnoreCase ("Gate"))
    {
        // Gate threshold and downward cutoff line
        g.drawVerticalLine ((int) (b.getX() + 3.0f), b.getY(), b.getBottom());
        g.drawHorizontalLine ((int) b.getBottom(), b.getX() + 3.0f, b.getRight());
        g.fillEllipse (b.getX() + 1.0f, b.getY() + 2.0f, 4.0f, 4.0f);
        g.drawVerticalLine ((int) (b.getX() + 9.0f), b.getY() + 3.0f, b.getBottom());
    }
    else if (type.equalsIgnoreCase ("AGC"))
    {
        // Auto Gain Control: triangle / gauge symbol with needle
        p.startNewSubPath (b.getX() + 2.0f, b.getBottom());
        p.lineTo (cx, b.getY() + 1.0f);
        p.lineTo (b.getRight() - 2.0f, b.getBottom());
        p.startNewSubPath (cx, b.getY() + 5.0f);
        p.lineTo (cx, b.getBottom());
        g.strokePath (p, juce::PathStrokeType (1.4f));
    }
    else if (type.equalsIgnoreCase ("Dynamic EQ"))
    {
        // Sine / bell EQ curve
        p.startNewSubPath (b.getX(), cy + 1.0f);
        p.cubicTo (b.getX() + w * 0.25f, cy - h * 0.45f,
                   cx,                  cy - h * 0.45f,
                   cx + w * 0.15f,      cy);
        p.cubicTo (cx + w * 0.3f,       cy + h * 0.40f,
                   b.getRight(),        cy + h * 0.40f,
                   b.getRight(),        cy);
        g.strokePath (p, juce::PathStrokeType (1.5f));
    }
    else if (type.equalsIgnoreCase ("De-Esser"))
    {
        // Sibilance frequency split hash / waveform
        float x1 = b.getX() + w * 0.25f;
        float x2 = cx;
        float x3 = b.getRight() - w * 0.25f;
        g.drawVerticalLine ((int) x1, b.getY() + 2.0f, b.getBottom() - 2.0f);
        g.drawVerticalLine ((int) x2, b.getY(), b.getBottom());
        g.drawVerticalLine ((int) x3, b.getY() + 2.0f, b.getBottom() - 2.0f);
        g.drawHorizontalLine ((int) cy, b.getX() + 1.0f, b.getRight() - 1.0f);
    }
    else if (type.equalsIgnoreCase ("Noise Suppression"))
    {
        // Spectral noise reduction bars (5 vertical bars)
        float barWidth = 2.0f;
        float spacing = (w - (5.0f * barWidth)) / 4.0f;
        float heights[] = { 0.4f, 0.75f, 1.0f, 0.65f, 0.35f };
        for (int i = 0; i < 5; ++i)
        {
            float bx = b.getX() + (float) i * (barWidth + spacing);
            float bh = h * heights[i];
            g.fillRect (bx, cy - bh * 0.5f, barWidth, bh);
        }
    }
    else if (type.equalsIgnoreCase ("De-Plosive"))
    {
        // Pop filter & microphone shield curve
        p.addCentredArc (cx + 2.0f, cy, w * 0.38f, h * 0.38f, 0.0f, -0.7f, 0.7f, true);
        g.strokePath (p, juce::PathStrokeType (1.5f));
        g.drawVerticalLine ((int) (cx - 3.0f), cy - h * 0.38f, cy + h * 0.38f);
        g.fillEllipse (cx - 5.0f, cy - 2.0f, 4.0f, 4.0f);
    }
    else if (type.equalsIgnoreCase ("De-Click"))
    {
        // 8-pointed transient burst / click sparkle
        g.drawLine (cx, b.getY(), cx, b.getBottom(), 1.4f);
        g.drawLine (b.getX(), cy, b.getRight(), cy, 1.4f);
        float d = w * 0.28f;
        g.drawLine (cx - d, cy - d, cx + d, cy + d, 1.2f);
        g.drawLine (cx - d, cy + d, cx + d, cy - d, 1.2f);
    }
    else if (type.equalsIgnoreCase ("AEC"))
    {
        // Acoustic Echo Cancellation: microphone with acoustic pickup
        p.addRoundedRectangle (cx - 2.5f, b.getY() + 1.0f, 5.0f, h * 0.55f, 2.5f);
        g.strokePath (p, juce::PathStrokeType (1.2f));
        p.clear();
        p.addCentredArc (cx, b.getY() + h * 0.38f, w * 0.35f, h * 0.35f, 0.0f, 1.0f, juce::MathConstants<float>::pi - 1.0f, false);
        g.strokePath (p, juce::PathStrokeType (1.2f));
        g.drawVerticalLine ((int) cx, b.getY() + h * 0.65f, b.getBottom());
    }
    else if (type.equalsIgnoreCase ("De-reverb") || type.equalsIgnoreCase ("Dereverb"))
    {
        // Audio impulse + decaying reverberation tail
        float x1 = b.getX() + 1.0f;
        g.drawVerticalLine ((int) x1, b.getY() + 1.0f, b.getBottom() - 1.0f);
        g.drawVerticalLine ((int) (x1 + 4.0f), b.getY() + 3.0f, b.getBottom() - 3.0f);
        g.drawVerticalLine ((int) (x1 + 8.0f), b.getY() + 5.0f, b.getBottom() - 5.0f);
        g.drawVerticalLine ((int) (x1 + 12.0f), b.getY() + 7.0f, b.getBottom() - 7.0f);
    }
    else if (type.equalsIgnoreCase ("Phase Rotator"))
    {
        // 360 degree circle with phasor dot
        g.drawEllipse (b.reduced (1.0f), 1.4f);
        g.fillEllipse (cx - 1.5f, cy - 1.5f, 3.0f, 3.0f);
        g.fillEllipse (b.getX() + w * 0.22f, b.getY() + h * 0.25f, 3.5f, 3.5f);
    }
    else if (type.equalsIgnoreCase ("Saturation"))
    {
        // Dual warm analog harmonic waves
        p.startNewSubPath (b.getX(), cy - 2.5f);
        p.cubicTo (b.getX() + w * 0.3f, cy - 7.0f, cx, cy + 2.0f, b.getRight(), cy - 2.5f);
        p.startNewSubPath (b.getX(), cy + 3.5f);
        p.cubicTo (b.getX() + w * 0.3f, cy - 1.0f, cx, cy + 8.0f, b.getRight(), cy + 3.5f);
        g.strokePath (p, juce::PathStrokeType (1.4f));
    }
    else if (type.equalsIgnoreCase ("Aural Exciter"))
    {
        // Harmonic burst: vertical bars with high frequency excitation
        float x1 = b.getX() + 2.0f;
        float x2 = b.getRight() - 2.0f;
        g.drawVerticalLine ((int) x1, b.getY() + 1.0f, b.getBottom() - 1.0f);
        g.drawVerticalLine ((int) x2, b.getY() + 1.0f, b.getBottom() - 1.0f);
        g.drawVerticalLine ((int) cx, b.getY() + 3.0f, b.getBottom() - 3.0f);
        g.drawHorizontalLine ((int) cy, x1 + 1.0f, x2 - 1.0f);
    }
    else if (type.equalsIgnoreCase ("Spectral Clarity"))
    {
        // Dynamic resonance notch curve
        p.startNewSubPath (b.getX(), cy - 2.0f);
        p.lineTo (cx - 4.0f, cy - 2.0f);
        p.quadraticTo (cx, cy + 6.0f, cx + 4.0f, cy - 2.0f);
        p.lineTo (b.getRight(), cy - 2.0f);
        g.strokePath (p, juce::PathStrokeType (1.5f));
    }
    else if (type.equalsIgnoreCase ("De-Breath"))
    {
        // Acoustic breath wave + cancellation slash
        p.startNewSubPath (b.getX() + 2.0f, cy + 3.0f);
        p.quadraticTo (cx, cy - 6.0f, b.getRight() - 2.0f, cy + 3.0f);
        g.strokePath (p, juce::PathStrokeType (1.4f));
        g.drawLine (b.getX() + 3.0f, b.getBottom() - 2.0f, b.getRight() - 3.0f, b.getY() + 2.0f, 1.2f);
    }
    else if (type.equalsIgnoreCase ("Upward Compressor"))
    {
        // Dual upward lift / downward clamp arrows
        g.drawVerticalLine ((int) cx, b.getY() + 2.0f, b.getBottom() - 2.0f);
        p.startNewSubPath (cx - 3.0f, b.getY() + 5.0f);
        p.lineTo (cx, b.getY() + 2.0f);
        p.lineTo (cx + 3.0f, b.getY() + 5.0f);
        p.startNewSubPath (cx - 3.0f, b.getBottom() - 5.0f);
        p.lineTo (cx, b.getBottom() - 2.0f);
        p.lineTo (cx + 3.0f, b.getBottom() - 5.0f);
        g.strokePath (p, juce::PathStrokeType (1.4f));
    }
    else if (type.equalsIgnoreCase ("Crossover Splitter") || type.equalsIgnoreCase ("Frequency Splitter"))
    {
        // 3-way crossover split branches (1 IN -> 3 OUT diverging)
        p.startNewSubPath (b.getX() + 1.5f, cy);
        p.lineTo (cx - 2.0f, cy);
        p.lineTo (b.getRight() - 3.0f, b.getY() + 2.5f);
        p.startNewSubPath (cx - 2.0f, cy);
        p.lineTo (b.getRight() - 3.0f, cy);
        p.startNewSubPath (cx - 2.0f, cy);
        p.lineTo (b.getRight() - 3.0f, b.getBottom() - 2.5f);
        g.strokePath (p, juce::PathStrokeType (1.4f));

        // 3 output endpoint dots (LOW, MID, HIGH)
        g.fillEllipse (b.getRight() - 4.5f, b.getY() + 1.0f, 3.0f, 3.0f);
        g.fillEllipse (b.getRight() - 4.5f, cy - 1.5f, 3.0f, 3.0f);
        g.fillEllipse (b.getRight() - 4.5f, b.getBottom() - 4.0f, 3.0f, 3.0f);
    }
    else if (type.equalsIgnoreCase ("Crossover Joiner") || type.equalsIgnoreCase ("Frequency Joiner"))
    {
        // 3-way crossover join branches (3 IN -> 1 OUT converging)
        p.startNewSubPath (b.getX() + 3.0f, b.getY() + 2.5f);
        p.lineTo (cx + 2.0f, cy);
        p.lineTo (b.getRight() - 1.5f, cy);
        p.startNewSubPath (b.getX() + 3.0f, cy);
        p.lineTo (cx + 2.0f, cy);
        p.startNewSubPath (b.getX() + 3.0f, b.getBottom() - 2.5f);
        p.lineTo (cx + 2.0f, cy);
        g.strokePath (p, juce::PathStrokeType (1.4f));

        // 3 input endpoint dots (LOW, MID, HIGH)
        g.fillEllipse (b.getX() + 1.5f, b.getY() + 1.0f, 3.0f, 3.0f);
        g.fillEllipse (b.getX() + 1.5f, cy - 1.5f, 3.0f, 3.0f);
        g.fillEllipse (b.getX() + 1.5f, b.getBottom() - 4.0f, 3.0f, 3.0f);
    }
    else if (type.equalsIgnoreCase ("3-Band EQ") || type.equalsIgnoreCase ("Three-Band EQ"))
    {
        // 3 Vertical EQ Slider Faders (Low, Mid, High)
        float colW = w / 3.0f;
        float xLow  = b.getX() + colW * 0.5f;
        float xMid  = b.getX() + colW * 1.5f;
        float xHigh = b.getX() + colW * 2.5f;

        // Fader vertical track slots
        g.drawVerticalLine ((int) xLow,  b.getY() + 2.0f, b.getBottom() - 2.0f);
        g.drawVerticalLine ((int) xMid,  b.getY() + 2.0f, b.getBottom() - 2.0f);
        g.drawVerticalLine ((int) xHigh, b.getY() + 2.0f, b.getBottom() - 2.0f);

        // Fader handles at musical positions (Low = raised, Mid = lowered, High = raised)
        g.fillRect (xLow - 2.5f,  cy - 4.0f, 5.0f, 2.5f);
        g.fillRect (xMid - 2.5f,  cy + 2.0f, 5.0f, 2.5f);
        g.fillRect (xHigh - 2.5f, cy - 6.0f, 5.0f, 2.5f);
    }
    else if (type.equalsIgnoreCase ("Spatial 3D") || type.equalsIgnoreCase ("3D Spatializer") || type.equalsIgnoreCase ("Spatial Realm"))
    {
        // 3D Binaural Sphere & Orbit icon: Listener head at center + tilted 3D orbit ring + orbiting emitter orb
        // Center Listener Head
        g.drawEllipse (cx - 2.5f, cy - 2.5f, 5.0f, 5.0f, 1.2f);
        g.drawLine (cx, cy - 2.5f, cx, cy - 4.5f, 1.2f); // Nose

        // Tilted 3D Orbit Ellipse
        p.addEllipse (cx - w * 0.42f, cy - h * 0.22f, w * 0.84f, h * 0.44f);
        g.strokePath (p, juce::PathStrokeType (1.2f));

        // Orbiting Emitter Orb (top right of orbit)
        float orbX = cx + w * 0.32f;
        float orbY = cy - h * 0.16f;
        g.fillEllipse (orbX - 2.0f, orbY - 2.0f, 4.0f, 4.0f);

        // Radiating 3D Wavefront arc
        p.clear();
        p.addCentredArc (orbX, orbY, 6.0f, 6.0f, 0.0f, -0.8f, 1.8f, false);
        g.strokePath (p, juce::PathStrokeType (1.0f));
    }
    else if (type.equalsIgnoreCase ("Phantom Sub"))
    {
        // Sub-harmonic fundamental reconstructor: high fundamental dot + deep sub-bass wave
        g.fillEllipse (cx - 2.0f, b.getY() + 1.0f, 4.0f, 4.0f);
        p.startNewSubPath (b.getX(), cy + 2.0f);
        p.cubicTo (b.getX() + w * 0.25f, cy + 9.0f,
                   cx - w * 0.1f, cy - 6.0f,
                   cx + w * 0.15f, cy + 5.0f);
        p.cubicTo (cx + w * 0.35f, cy + 9.0f,
                   b.getRight() - w * 0.15f, cy + 1.0f,
                   b.getRight(), cy + 4.0f);
        g.strokePath (p, juce::PathStrokeType (1.5f));
    }
    else if (type.equalsIgnoreCase ("Proximity"))
    {
        // Acoustic distance & dynamic body: mic capsule + radiating proximity distance arcs
        p.addRoundedRectangle (cx - 2.5f, cy - 4.0f, 5.0f, 8.0f, 2.5f);
        g.strokePath (p, juce::PathStrokeType (1.3f));
        p.clear();
        p.addCentredArc (cx, cy, w * 0.32f, h * 0.32f, 0.0f, 0.4f, 2.7f, false);
        p.addCentredArc (cx, cy, w * 0.46f, h * 0.46f, 0.0f, 0.5f, 2.6f, false);
        g.strokePath (p, juce::PathStrokeType (1.2f));
    }
    else if (type.equalsIgnoreCase ("Broadcast Morph"))
    {
        // Broadcast tower & microphone character cloner: broadcast tower + transmission arcs
        p.startNewSubPath (cx - 4.0f, b.getBottom());
        p.lineTo (cx, b.getY() + 4.0f);
        p.lineTo (cx + 4.0f, b.getBottom());
        p.startNewSubPath (cx - 2.5f, cy + 2.0f);
        p.lineTo (cx + 2.5f, cy + 2.0f);
        g.strokePath (p, juce::PathStrokeType (1.3f));
        g.fillEllipse (cx - 2.0f, b.getY() + 2.0f, 4.0f, 4.0f);
        p.clear();
        p.addCentredArc (cx, b.getY() + 4.0f, w * 0.38f, h * 0.38f, 0.0f, -1.2f, 1.2f, false);
        g.strokePath (p, juce::PathStrokeType (1.2f));
    }
    else if (type.equalsIgnoreCase ("Parametric EQ"))
    {
        // 7-Band Pro Parametric EQ Curve with multi-band node dots
        p.startNewSubPath (b.getX(), cy + 4.0f);
        p.cubicTo (b.getX() + w * 0.22f, cy - 7.0f,
                   cx - 1.0f, cy + 6.0f,
                   cx + 3.0f, cy - 5.0f);
        p.cubicTo (b.getRight() - w * 0.25f, cy - 8.0f,
                   b.getRight() - 2.0f, cy + 3.0f,
                   b.getRight(), cy + 3.0f);
        g.strokePath (p, juce::PathStrokeType (1.5f));
        g.fillEllipse (b.getX() + w * 0.20f - 1.5f, cy - 7.0f - 1.5f, 3.0f, 3.0f);
        g.fillEllipse (cx + 3.0f - 1.5f, cy - 5.0f - 1.5f, 3.0f, 3.0f);
    }
    else if (type.equalsIgnoreCase ("Vocal Doubler"))
    {
        // Stereo Double Tracking: Dual panned Left & Right vocal waveforms
        p.startNewSubPath (b.getX(), cy - 3.0f);
        p.cubicTo (b.getX() + w * 0.3f, cy - 8.0f, cx, cy + 2.0f, b.getRight() - 3.0f, cy - 3.0f);
        p.startNewSubPath (b.getX() + 3.0f, cy + 3.0f);
        p.cubicTo (b.getX() + w * 0.3f + 3.0f, cy - 2.0f, cx + 3.0f, cy + 8.0f, b.getRight(), cy + 3.0f);
        g.strokePath (p, juce::PathStrokeType (1.3f));
    }
    else if (type.equalsIgnoreCase ("Patch"))
    {
        // Modular patch / preset icon: patch plug
        g.drawRoundedRectangle (b.reduced (2.0f), 2.5f, 1.3f);
        g.fillEllipse (cx - 2.0f, cy - 2.0f, 4.0f, 4.0f);
    }
    else if (type.equalsIgnoreCase ("Container") || type.equalsIgnoreCase ("Packed Nodes"))
    {
        // 3D Isometric cube / Package container
        p.startNewSubPath (cx, b.getY() + 1.0f);
        p.lineTo (b.getRight() - 2.0f, cy - 3.0f);
        p.lineTo (b.getRight() - 2.0f, b.getBottom() - 3.0f);
        p.lineTo (cx, b.getBottom());
        p.lineTo (b.getX() + 2.0f, b.getBottom() - 3.0f);
        p.lineTo (b.getX() + 2.0f, cy - 3.0f);
        p.closeSubPath();
        p.startNewSubPath (cx, b.getY() + 1.0f);
        p.lineTo (cx, b.getBottom());
        p.startNewSubPath (cx, cy - 3.0f);
        p.lineTo (b.getX() + 2.0f, cy - 3.0f);
        p.startNewSubPath (cx, cy - 3.0f);
        p.lineTo (b.getRight() - 2.0f, cy - 3.0f);
        g.strokePath (p, juce::PathStrokeType (1.3f));
    }
    else
    {
        // Generic fallback icon
        g.drawRoundedRectangle (b.reduced (2.0f), 3.0f, 1.4f);
    }
}

// --- LibraryItem ---
LibraryItem::LibraryItem (juce::String category, juce::String type, juce::String description,
                           juce::Colour categoryColor, DoubleClickCallback onDoubleClick,
                           SingleClickCallback onClick)
    : moduleCategory (category), moduleType (type), moduleDescription (description),
      accentColor (categoryColor), doubleClickCallback (onDoubleClick), singleClickCallback (onClick)
{
    juce::String tip = tr (type);
    if (description.isNotEmpty())
        tip += " - " + description;
    setTooltip (tip);
}

void LibraryItem::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Compact mode (icon-only button)
    if (bounds.getWidth() < 50.0f)
    {
        auto box = bounds.reduced (2.0f);
        if (isHovered)
        {
            g.setColour (UITheme::appleBlue.withAlpha (0.25f));
            g.fillRoundedRectangle (box, 4.0f);
            g.setColour (accentColor);
            g.drawRoundedRectangle (box, 4.0f, 1.2f);
        }
        else
        {
            g.setColour (juce::Colour (0x12ffffff));
            g.fillRoundedRectangle (box, 4.0f);
        }
        auto iconArea = box.reduced (5.0f);
        drawModuleIcon (g, moduleCategory.equalsIgnoreCase ("Patch") ? "Patch" : moduleType, iconArea, accentColor);
        return;
    }

    // macOS Source List Selection / Hover Highlight
    if (isHovered)
    {
        g.setColour (UITheme::appleBlue.withAlpha (0.18f));
        g.fillRoundedRectangle (bounds.reduced (4.0f, 1.0f), 4.0f);
    }

    // Left vertical accent indicator bar
    g.setColour (accentColor);
    g.fillRoundedRectangle (bounds.getX() + 2.0f, bounds.getY() + 3.0f, 2.5f, bounds.getHeight() - 6.0f, 1.0f);

    // Icon area
    auto iconArea = bounds.withTrimmedLeft (10.0f).withWidth (22.0f).reduced (0.0f, 5.0f);
    drawModuleIcon (g, moduleCategory.equalsIgnoreCase ("Patch") ? "Patch" : moduleType, iconArea, accentColor);

    // Module name in Inter font
    g.setColour (isHovered ? juce::Colours::white : UITheme::textPrimary);
    g.setFont (UITheme::getFont (12.0f));
    juce::String displayTitle = tr (moduleType);
    g.drawText (displayTitle, bounds.withTrimmedLeft (38.0f).withTrimmedRight (28.0f), juce::Justification::centredLeft, true);

    // Subtle Info Button (i) on the right edge
    auto infoRect = getInfoButtonBounds().toFloat();
    g.setColour (isInfoHovered ? UITheme::appleBlue : juce::Colour (0x28ffffff));
    g.fillEllipse (infoRect);
    g.setColour (isInfoHovered ? UITheme::appleBlue : juce::Colour (0x44ffffff));
    g.drawEllipse (infoRect, 1.0f);
    g.setColour (isInfoHovered ? juce::Colours::white : UITheme::textTertiary);
    g.setFont (UITheme::getFont (9.5f, true));
    g.drawText ("i", infoRect, juce::Justification::centred);
}

juce::Rectangle<int> LibraryItem::getInfoButtonBounds() const
{
    if (getWidth() < 50)
        return {};
    return juce::Rectangle<int> (getWidth() - 22, (getHeight() - 16) / 2, 16, 16);
}

void LibraryItem::mouseDown (const juce::MouseEvent& e)
{
    if (getInfoButtonBounds().contains (e.getPosition()))
    {
        if (singleClickCallback != nullptr)
            singleClickCallback (moduleType);
        return;
    }

    if (e.mods.isRightButtonDown())
    {
        isRightClickCandidate = true;
        return;
    }
}

void LibraryItem::mouseUp (const juce::MouseEvent& e)
{
    if (isRightClickCandidate || e.mods.isPopupMenu())
    {
        bool wasRightClick = isRightClickCandidate;
        isRightClickCandidate = false;

        if (wasRightClick && e.getDistanceFromDragStart() < 6)
        {
            juce::PopupMenu m;
            bool isTr = LocalizationManager::instance().getLanguage() == Language::Turkish;
            m.addItem (1, isTr ? juce::String (juce::CharPointer_UTF8 ("Tuvale Ekle")) : "Add to Canvas");
            m.addItem (2, isTr ? juce::String (juce::CharPointer_UTF8 ("Detayl\xc4\xb1 Bilgi (Info)")) : "Detailed Info");
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [this] (int res) {
                if (res == 1 && doubleClickCallback != nullptr)
                    doubleClickCallback (moduleType);
                else if (res == 2 && singleClickCallback != nullptr)
                    singleClickCallback (moduleType);
            });
            return;
        }
    }
}

void LibraryItem::mouseDrag (const juce::MouseEvent& e)
{
    if (auto* dragContainer = juce::DragAndDropContainer::findParentDragContainerFor (this))
    {
        if (! dragContainer->isDragAndDropActive())
        {
            // Render a compact drag snapshot badge with the icon and title under the cursor
            juce::Image dragImage (juce::Image::ARGB, getWidth(), getHeight(), true);
            {
                juce::Graphics g (dragImage);
                g.fillAll (juce::Colour (0xff202026));
                paint (g);
            }

            juce::Point<int> offset (-getWidth() / 2, -getHeight() / 2);
            dragContainer->startDragging ("module:" + moduleType, this, juce::ScaledImage (dragImage), true, &offset, &e.source);
        }
    }
}

void LibraryItem::mouseDoubleClick (const juce::MouseEvent&)
{
    if (doubleClickCallback != nullptr)
        doubleClickCallback (moduleType);
}

void LibraryItem::mouseEnter (const juce::MouseEvent&)
{
    isHovered = true;
    repaint();
}

void LibraryItem::mouseExit (const juce::MouseEvent&)
{
    isHovered = false;
    isInfoHovered = false;
    repaint();
}

void LibraryItem::mouseMove (const juce::MouseEvent& e)
{
    bool hoverInfo = getInfoButtonBounds().contains (e.getPosition());
    if (isInfoHovered != hoverInfo)
    {
        isInfoHovered = hoverInfo;
        repaint();
    }
}

void LibraryItem::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    if (auto* sidebar = findParentComponentOfClass<SidebarComponent>())
        sidebar->scrollList (wheel.deltaY);
}

// --- CategoryHeader ---
CategoryHeader::CategoryHeader (juce::String name, juce::Colour color, bool isExpanded, std::function<void()> onToggle)
    : categoryName (name.toUpperCase()), accentColor (color), expanded (isExpanded), toggleCallback (onToggle)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void CategoryHeader::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // Hover effect on header
    if (isHovered)
    {
        g.setColour (juce::Colour (0x15ffffff));
        g.fillRect (bounds);
    }

    // Category title
    g.setColour (accentColor);
    g.setFont (UITheme::getFont (11.0f));
    g.drawText (categoryName, bounds.withTrimmedLeft (8.0f), juce::Justification::centredLeft);

    // Chevron arrow on the right
    float arrowX = bounds.getRight() - 16.0f;
    float arrowY = bounds.getCentreY();
    juce::Path chevron;
    if (expanded)
    {
        // Downward 'v'
        chevron.startNewSubPath (arrowX - 4.0f, arrowY - 2.0f);
        chevron.lineTo (arrowX, arrowY + 2.5f);
        chevron.lineTo (arrowX + 4.0f, arrowY - 2.0f);
    }
    else
    {
        // Rightward '>'
        chevron.startNewSubPath (arrowX - 2.0f, arrowY - 4.0f);
        chevron.lineTo (arrowX + 2.5f, arrowY);
        chevron.lineTo (arrowX - 2.0f, arrowY + 4.0f);
    }
    g.setColour (isHovered ? juce::Colours::white : juce::Colour (0xff757580));
    g.strokePath (chevron, juce::PathStrokeType (1.6f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));
}

void CategoryHeader::mouseDown (const juce::MouseEvent&)
{
    if (toggleCallback != nullptr)
        toggleCallback();
}

void CategoryHeader::mouseEnter (const juce::MouseEvent&)
{
    isHovered = true;
    repaint();
}

void CategoryHeader::mouseExit (const juce::MouseEvent&)
{
    isHovered = false;
    repaint();
}

void CategoryHeader::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    if (auto* sidebar = findParentComponentOfClass<SidebarComponent>())
        sidebar->scrollList (wheel.deltaY);
}

// --- SidebarComponent ---
SidebarComponent::SidebarComponent (PluginProcessor& p, NodeCanvas& canvas)
    : processor (p), nodeCanvas (canvas)
{
    // Search Box Setup
    searchBox.setTextToShowWhenEmpty (tr ("SEARCH_MODULES"), juce::Colour (0xff656575));
    searchBox.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    searchBox.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    searchBox.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    searchBox.setColour (juce::TextEditor::textColourId, juce::Colour (0xffececf2));
    searchBox.setFont (UITheme::getFont (12.0f));
    searchBox.setIndents (26, 0);
    searchBox.addListener (this);
    addAndMakeVisible (searchBox);

    // Header Collapse Button [ ‹ ]
    collapseBtn.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    collapseBtn.setColour (juce::TextButton::textColourOffId, UITheme::textSecondary);
    collapseBtn.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    collapseBtn.onClick = [this]
    {
        if (onCollapseRequested)
            onCollapseRequested();
    };
    addAndMakeVisible (collapseBtn);

    // Viewport Setup
    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&listContainer, false);
    viewport.setScrollBarsShown (true, false);

    LocalizationManager::instance().addListener (this);

    rebuildList();
}

SidebarComponent::~SidebarComponent()
{
    LocalizationManager::instance().removeListener (this);
}

void SidebarComponent::localizationChanged()
{
    searchBox.setTextToShowWhenEmpty (tr ("SEARCH_MODULES"), juce::Colour (0xff656575));
    rebuildList();
    repaint();
}

juce::Colour SidebarComponent::getCategoryColor (const juce::String& category) const
{
    if (category.equalsIgnoreCase ("Utility"))      return juce::Colour (0xff818cf8); // Indigo Lavender
    if (category.equalsIgnoreCase ("Dynamics"))     return juce::Colour (0xffa855f7); // Vivid Purple
    if (category.equalsIgnoreCase ("Frequency"))    return juce::Colour (0xff06b6d4); // Electric Cyan
    if (category.equalsIgnoreCase ("Cleanup"))      return juce::Colour (0xff38bdf8); // Ice Sky Blue
    if (category.equalsIgnoreCase ("Vocal Tone") ||
        category.equalsIgnoreCase ("Radio Tone"))   return juce::Colour (0xffc084fc); // Neon Orchid

    return UITheme::primaryIndigo;
}

void SidebarComponent::setCompactMode (bool compact)
{
    isCompactMode = compact;
    collapseBtn.setButtonText (isCompactMode ? juce::CharPointer_UTF8 ("\xe2\x80\xba") : juce::CharPointer_UTF8 ("\xe2\x80\xb9"));
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (isCompactMode ? 0 : 6);
    rebuildList();
    resized();
    repaint();
}

void SidebarComponent::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    scrollList (wheel.deltaY);
}

void SidebarComponent::scrollList (float deltaY)
{
    int maxScroll = juce::jmax (0, listContainer.getHeight() - viewport.getHeight());
    if (maxScroll > 0)
    {
        int currentY = viewport.getViewPositionY();
        int delta = (int) std::lround (deltaY * 90.0f);
        viewport.setViewPosition (viewport.getViewPositionX(), juce::jlimit (0, maxScroll, currentY - delta));
    }
}

void SidebarComponent::rebuildList()
{
    listItems.clear();

    if (isCompactMode)
    {
        auto& factory = ModuleFactory::instance();
        int yOffset = 4;
        int itemW = 32;
        int itemH = 32;
        int itemX = (getWidth() - itemW) / 2;
        if (itemX < 0) itemX = 7;

        for (const auto& category : factory.getCategories())
        {
            auto types = factory.getTypesInCategory (category);
            auto color = getCategoryColor (category);

            for (const auto& type : types)
            {
                if (type.equalsIgnoreCase ("Container"))
                    continue;

                auto desc = factory.getDescription (type);
                auto* item = listItems.add (new LibraryItem (category, type, desc, color,
                    [this] (const juce::String& t) {
                        nodeCanvas.addModuleAtViewCenter (t);
                    },
                    [this] (const juce::String& t) {
                        if (onModuleSelected != nullptr)
                            onModuleSelected (t);
                    }));

                listContainer.addAndMakeVisible (item);
                item->setBounds (itemX, yOffset, itemW, itemH);
                yOffset += itemH + 3;
            }
            yOffset += 4;
        }

        listContainer.setSize (getWidth(), yOffset + 12);
        return;
    }

    if (! vocalModulesExpanded)
    {
        listContainer.setSize (viewport.getMaximumVisibleWidth(), 10);
        resized();
        return;
    }

    auto& factory = ModuleFactory::instance();
    juce::String filter = searchBox.getText().trim().toLowerCase();

    int yOffset = 0;
    const int itemHeight = 28;
    const int headerHeight = 22;
    int containerWidth = juce::jmax (170, viewport.getMaximumVisibleWidth());

    for (const auto& category : factory.getCategories())
    {
        auto types = factory.getTypesInCategory (category);
        juce::StringArray matchingTypes;

        for (const auto& type : types)
        {
            if (type.equalsIgnoreCase ("Container"))
                continue;

            auto desc = factory.getDescription (type);
            if (filter.isEmpty() || type.toLowerCase().contains (filter) || desc.toLowerCase().contains (filter) || category.toLowerCase().contains (filter))
            {
                matchingTypes.add (type);
            }
        }

        if (matchingTypes.size() > 0)
        {
            auto color = getCategoryColor (category);

            // Default to expanded if not set
            if (categoryExpanded.find (category) == categoryExpanded.end())
                categoryExpanded[category] = true;

            bool isCatExpanded = categoryExpanded[category];

            juce::String catDisplay = category;
            if (category.equalsIgnoreCase ("Utility")) catDisplay = tr ("CAT_UTILITY");
            else if (category.equalsIgnoreCase ("Dynamics")) catDisplay = tr ("CAT_DYNAMICS");
            else if (category.equalsIgnoreCase ("Frequency")) catDisplay = tr ("CAT_FREQUENCY");
            else if (category.equalsIgnoreCase ("Cleanup")) catDisplay = tr ("CAT_CLEANUP");
            else if (category.equalsIgnoreCase ("Vocal Tone") || category.equalsIgnoreCase ("Radio Tone")) catDisplay = tr ("CAT_VOCAL_TONE");

            // Add Header
            auto* header = listItems.add (new CategoryHeader (catDisplay, color, isCatExpanded, [this, category] {
                categoryExpanded[category] = ! categoryExpanded[category];
                rebuildList();
            }));

            listContainer.addAndMakeVisible (header);
            header->setBounds (0, yOffset, containerWidth, headerHeight);
            yOffset += headerHeight + 2;

            // Add Module Items if expanded
            if (isCatExpanded)
            {
                for (const auto& type : matchingTypes)
                {
                    auto desc = factory.getDescription (type);
                    auto* item = listItems.add (new LibraryItem (category, type, desc, color,
                        [this] (const juce::String& t) {
                            nodeCanvas.addModuleAtViewCenter (t);
                        },
                        [this] (const juce::String& t) {
                            if (onModuleSelected != nullptr)
                                onModuleSelected (t);
                        }));

                    listContainer.addAndMakeVisible (item);
                    item->setBounds (0, yOffset, containerWidth, itemHeight);
                    yOffset += itemHeight + 2;
                }

                yOffset += 4; // Spacing between categories
            }
        }
    }

    listContainer.setSize (containerWidth, yOffset + 12);
    resized();
}

void SidebarComponent::textEditorTextChanged (juce::TextEditor&)
{
    rebuildList();
}

void SidebarComponent::paint (juce::Graphics& g)
{
    // macOS Source List Dark Background
    g.fillAll (UITheme::bgSidebar);

    // Right hairline separator
    g.setColour (UITheme::strokeHairline);
    g.drawVerticalLine (getWidth() - 1, 0.0f, (float) getHeight());

    if (isCompactMode)
    {
        g.drawHorizontalLine (34, 0.0f, (float) getWidth());
        return;
    }

    // Top Title: "Library" in Apple typography
    g.setColour (UITheme::textPrimary);
    g.setFont (UITheme::getFont (13.0f, true));
    g.drawText (tr ("LIBRARY"), 14, 8, getWidth() - 48, 20, juce::Justification::centredLeft);

    // Subheader: "VOCAL MODULES" with chevron
    auto vocalBounds = juce::Rectangle<float> (14.0f, 32.0f, (float) getWidth() - 28.0f, 20.0f);
    g.setColour (UITheme::textSecondary);
    g.setFont (UITheme::getFont (10.0f));
    g.drawText (tr ("VOCAL_MODULES"), vocalBounds, juce::Justification::centredLeft);

    // Chevron for VOCAL MODULES
    float arrowX = vocalBounds.getRight() - 4.0f;
    float arrowY = vocalBounds.getCentreY();
    juce::Path chevron;
    if (vocalModulesExpanded)
    {
        chevron.startNewSubPath (arrowX - 4.0f, arrowY - 2.0f);
        chevron.lineTo (arrowX, arrowY + 2.5f);
        chevron.lineTo (arrowX + 4.0f, arrowY - 2.0f);
    }
    else
    {
        chevron.startNewSubPath (arrowX - 2.0f, arrowY - 4.0f);
        chevron.lineTo (arrowX + 2.5f, arrowY);
        chevron.lineTo (arrowX - 2.0f, arrowY + 4.0f);
    }
    g.setColour (UITheme::textSecondary);
    g.strokePath (chevron, juce::PathStrokeType (1.4f, juce::PathStrokeType::mitered, juce::PathStrokeType::rounded));

    // macOS Spotlight Capsule Search Field
    auto scb = searchContainerBounds.toFloat();
    bool hasFocus = searchBox.hasKeyboardFocus (true);
    float pillRadius = scb.getHeight() / 2.0f;

    g.setColour (juce::Colour (0xff28282d));
    g.fillRoundedRectangle (scb, pillRadius);
    g.setColour (hasFocus ? UITheme::appleBlue : UITheme::strokeHairline);
    g.drawRoundedRectangle (scb, pillRadius, 1.0f);

    // Magnifying glass icon on the left
    float magX = scb.getX() + 10.0f;
    float magY = scb.getCentreY() - 1.0f;
    g.setColour (hasFocus ? UITheme::appleBlue : UITheme::textSecondary);
    g.drawEllipse (magX, magY - 4.0f, 8.0f, 8.0f, 1.3f);
    g.drawLine (magX + 6.0f, magY + 2.5f, magX + 10.0f, magY + 6.5f, 1.4f);

    // Clear 'X' button on the right if not empty
    if (searchBox.getText().isNotEmpty())
    {
        float clrX = scb.getRight() - 15.0f;
        float clrY = scb.getCentreY();
        g.setColour (UITheme::textSecondary);
        g.fillEllipse (clrX - 6.0f, clrY - 6.0f, 12.0f, 12.0f);
        g.setColour (juce::Colours::white);
        g.drawLine (clrX - 2.5f, clrY - 2.5f, clrX + 2.5f, clrY + 2.5f, 1.3f);
        g.drawLine (clrX + 2.5f, clrY - 2.5f, clrX - 2.5f, clrY + 2.5f, 1.3f);
    }
}

void SidebarComponent::mouseDown (const juce::MouseEvent& e)
{
    if (isCompactMode)
        return;

    // Check click on VOCAL MODULES subheader (y = 30 to 52)
    if (e.y >= 30 && e.y <= 54)
    {
        vocalModulesExpanded = ! vocalModulesExpanded;
        rebuildList();
        repaint();
        return;
    }

    // Check click on search clear 'X' button
    if (searchBox.getText().isNotEmpty())
    {
        auto clearArea = juce::Rectangle<int> (searchContainerBounds.getRight() - 24, searchContainerBounds.getY(), 24, searchContainerBounds.getHeight());
        if (clearArea.contains (e.getPosition()))
        {
            searchBox.setText ("");
            return;
        }
    }
}

void SidebarComponent::resized()
{
    if (isCompactMode)
    {
        collapseBtn.setBounds ((getWidth() - 22) / 2, 7, 22, 20);
        searchBox.setVisible (false);
        viewport.setBounds (0, 34, getWidth(), getHeight() - 34);
        
        int itemW = 32;
        int itemH = 32;
        int itemX = (getWidth() - itemW) / 2;
        if (itemX < 0) itemX = 7;

        int y = 4;
        for (auto* comp : listItems)
        {
            comp->setBounds (itemX, y, itemW, itemH);
            y += itemH + 3;
        }
        listContainer.setSize (getWidth(), y + 12);
        return;
    }

    searchBox.setVisible (true);
    collapseBtn.setBounds (getWidth() - 26, 8, 18, 18);

    auto bounds = getLocalBounds();
    bounds.removeFromTop (56); // Title + "VOCAL MODULES" header

    // Search container
    searchContainerBounds = bounds.removeFromTop (28).reduced (10, 0);
    searchBox.setBounds (searchContainerBounds);

    bounds.removeFromTop (8); // Space between search box & list

    viewport.setBounds (bounds.reduced (4, 2));

    int currentWidth = viewport.getMaximumVisibleWidth();
    listContainer.setSize (currentWidth, listContainer.getHeight());

    for (auto* comp : listItems)
    {
        comp->setSize (currentWidth, comp->getHeight());
    }
}