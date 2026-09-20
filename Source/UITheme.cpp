#include "UITheme.h"
#include "Localization.h"
#include <BinaryData.h> // Font verisine ulaşmak için dahil ettik

namespace
{
    juce::Typeface::Ptr loadEmbeddedTypeface()
    {
        // 1. Projeye gömülü fontu yaratıyoruz
        return juce::Typeface::createSystemTypefaceFor (
            BinaryData::InterMedium_ttf,
            BinaryData::InterMedium_ttfSize
        );
    }
}

juce::Typeface::Ptr UITheme::getTypeface()
{
    static juce::Typeface::Ptr cached = loadEmbeddedTypeface();
    return cached;
}

juce::Font UITheme::getFont (float heightPx, bool bold)
{
    auto lang = LocalizationManager::instance().getLanguage();
    if (lang == Language::Japanese)
    {
        return juce::Font (juce::FontOptions ("Yu Gothic UI", juce::jmax (12.5f, heightPx * 1.08f), bold ? juce::Font::bold : juce::Font::plain));
    }
    if (lang == Language::Russian)
    {
        return juce::Font (juce::FontOptions ("Segoe UI", juce::jmax (12.0f, heightPx * 1.05f), bold ? juce::Font::bold : juce::Font::plain));
    }

    // English & Turkish: use the crisp embedded Inter font!
    float minH = (lang == Language::Turkish ? 12.0f : 11.0f);
    float adjustedHeight = juce::jmax (minH, heightPx * (lang == Language::Turkish ? 1.06f : 1.0f));

    if (auto tf = getTypeface())
        return juce::Font (juce::FontOptions (tf).withHeight (adjustedHeight));
    
    return juce::Font (juce::FontOptions ("Segoe UI", adjustedHeight, bold ? juce::Font::bold : juce::Font::plain));
}

//==============================================================================
CustomLookAndFeel::CustomLookAndFeel()
{
    // macOS Dark Segmented Controls & Buttons
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff2c2c30));
    setColour (juce::TextButton::buttonOnColourId, UITheme::appleBlue);
    setColour (juce::TextButton::textColourOffId, UITheme::textSecondary);
    setColour (juce::TextButton::textColourOnId, juce::Colours::white);

    // macOS Pop-up Menu & Combo Box
    setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff28282c));
    setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff3c3c42));
    setColour (juce::ComboBox::textColourId, UITheme::textPrimary);

    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff222226));
    setColour (juce::PopupMenu::textColourId, UITheme::textPrimary);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, UITheme::appleBlue);
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);

    // macOS Translucent Capsule Scrollbars
    setColour (juce::ScrollBar::backgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::ScrollBar::thumbColourId, juce::Colour (0x35ffffff));
    setColour (juce::ScrollBar::trackColourId, juce::Colours::transparentBlack);
}

juce::Typeface::Ptr CustomLookAndFeel::getTypefaceForFont (const juce::Font& font)
{
    auto lang = LocalizationManager::instance().getLanguage();
    if (lang == Language::Japanese || lang == Language::Russian)
        return juce::Font::getDefaultTypefaceForFont (font);

    if (font.getTypefaceName().isNotEmpty() && font.getTypefaceName() != juce::Font::getDefaultSansSerifFontName())
        return juce::Font::getDefaultTypefaceForFont (font);

    if (auto tf = UITheme::getTypeface())
        return tf;

    return juce::Font::getDefaultTypefaceForFont (font);
}

juce::Font CustomLookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return UITheme::getFont (juce::jmin (13.0f, (float)buttonHeight - 8.0f));
}

juce::Font CustomLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return UITheme::getFont (12.5f);
}

juce::Font CustomLookAndFeel::getPopupMenuFont()
{
    return UITheme::getFont (13.0f);
}

juce::Font CustomLookAndFeel::getLabelFont (juce::Label& label)
{
    return UITheme::getFont (label.getFont().getHeight());
}

void CustomLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                               bool isHighlighted, bool isDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    const float corner = 5.0f;

    bool on = button.getToggleState();
    juce::Colour base = on ? UITheme::appleBlue : juce::Colour (0xff2c2c31);
    if (isDown)              base = base.darker (0.08f);
    else if (isHighlighted) base = base.brighter (0.06f);

    g.setColour (base);
    g.fillRoundedRectangle (bounds, corner);

    // Apple subtle keyline
    g.setColour (on ? UITheme::appleBlue : juce::Colour (0xff3a3a40));
    g.drawRoundedRectangle (bounds, corner, 1.0f);
}

void CustomLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPosProportional, float rotaryStartAngle,
                                          float rotaryEndAngle, juce::Slider& /*slider*/)
{
    auto radius = ((float) juce::jmin (width, height) * 0.5f) - 2.0f;
    auto centreX = (float) x + (float) width * 0.5f;
    auto centreY = (float) y + (float) height * 0.5f;
    auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // Track background
    juce::Path trackPath;
    trackPath.addCentredArc (centreX, centreY, radius - 1.5f, radius - 1.5f, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour (0xff25252a));
    g.strokePath (trackPath, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Active value arc (Electric Apple Blue)
    if (sliderPosProportional > 0.005f)
    {
        juce::Path activePath;
        activePath.addCentredArc (centreX, centreY, radius - 1.5f, radius - 1.5f, 0.0f, rotaryStartAngle, angle, true);
        g.setColour (UITheme::appleBlue);
        g.strokePath (activePath, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Dial Body (Titanium Disc)
    auto dialRadius = radius - 4.5f;
    auto dialRect = juce::Rectangle<float> (centreX - dialRadius, centreY - dialRadius, dialRadius * 2.0f, dialRadius * 2.0f);
    g.setColour (juce::Colour (0xff1c1c20));
    g.fillEllipse (dialRect);
    g.setColour (UITheme::strokeHairline);
    g.drawEllipse (dialRect, 1.0f);

    // Pointer Dot
    juce::Path p;
    auto pointerLength = dialRadius * 0.70f;
    p.addEllipse (-1.2f, -pointerLength, 2.4f, 2.4f);
    p.applyTransform (juce::AffineTransform::rotation (angle).translated (centreX, centreY));
    g.setColour (juce::Colours::white);
    g.fillPath (p);
}

juce::PopupMenu::Options CustomLookAndFeel::getOptionsForComboBoxPopupMenu (juce::ComboBox& box, juce::Label& label)
{
    return juce::PopupMenu::Options()
        .withTargetComponent (&box)
        .withTargetScreenArea (box.getScreenBounds())
        .withMinimumWidth (juce::jmax (180, box.getWidth()))
        .withMaximumNumColumns (1)
        .withStandardItemHeight (juce::jmax (24, label.getHeight()));
}

void CustomLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    auto b = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (1.0f);
    g.setColour (juce::Colour (0xf01a1a20));
    g.fillRoundedRectangle (b, 4.0f);
    g.setColour (UITheme::strokeHairline);
    g.drawRoundedRectangle (b, 4.0f, 1.0f);

    g.setColour (UITheme::textPrimary);
    g.setFont (UITheme::getFont (11.0f));
    g.drawFittedText (text, b.toNearestInt().reduced (6, 2), juce::Justification::centred, 3);
}

//==============================================================================
void PowerButton::paintButton (juce::Graphics& g, bool isHighlighted, bool /*isDown*/)
{
    auto b = getLocalBounds().toFloat().reduced (3.0f);
    bool on = getToggleState();

    auto ringColour = on ? UITheme::appleBlue : UITheme::textTertiary;
    g.setColour (ringColour.withAlpha (isHighlighted ? 1.0f : 0.85f));
    g.drawEllipse (b, 1.6f);

    if (on)
    {
        g.setColour (ringColour.withAlpha (0.18f));
        g.fillEllipse (b);
    }

    auto centre = b.getCentre();
    float r = b.getWidth() * 0.26f;

    juce::Path glyph;
    glyph.addArc (centre.x - r, centre.y - r, r * 2.0f, r * 2.0f,
                  juce::MathConstants<float>::pi * 0.22f, juce::MathConstants<float>::pi * 1.78f, true);

    g.setColour (ringColour);
    g.strokePath (glyph, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.drawLine (centre.x, centre.y - r - 1.5f, centre.x, centre.y - r * 0.1f, 1.7f);
}