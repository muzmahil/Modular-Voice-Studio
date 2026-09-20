#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

/**
    UITheme centralises the two things you asked for:
      1. A single place that loads and hands out the custom typeface, so every
         piece of hand-drawn text (node labels, meter, top bar) and every stock
         JUCE widget (buttons, combo boxes) uses the same font.
      2. A couple of small custom-painted controls used by the new top bar
         (the round power/bypass button) that don't have a stock JUCE equivalent.

    --- The font, as actually wired up in this project --------------------
    Embedded via CMake's juce_add_binary_data (ModularAudioOS_Data target,
    Source/Font/InterMedium.ttf), linked into the plugin target, and loaded here
    through the generated BinaryData::InterMedium_ttf / _ttfSize. If you swap in a
    different weight or file, update the one call in UITheme.cpp's
    loadEmbeddedTypeface() (and the SOURCES path in CMakeLists.txt) to match —
    the exact symbol name JUCE generates depends on the file name, so check
    the generated BinaryData.h if you rename it.

    That's it — CustomLookAndFeel::getTypefaceForFont() and UITheme::getFont()
    both route through the same cached juce::Typeface::Ptr, so the whole plugin
    picks it up at once.
*/
namespace UITheme
{
    /** Returns the shared custom typeface (or the platform default sans as a
        fallback if none has been embedded yet). Cached after first call. */
    juce::Typeface::Ptr getTypeface();

    /** Convenience font accessor for manual g.setFont(...) calls, always backed
        by the custom typeface. */
    juce::Font getFont (float heightPx, bool bold = false);

    // =========================================================================
    // APPLE macOS PRO DESIGN SYSTEM (Logic Pro / Sequoia Pro Aesthetic)
    // =========================================================================
    // Surfaces & Materials
    const juce::Colour bgCanvas        (0xff19191c); // Deep matte macOS dark canvas
    const juce::Colour bgSidebar       (0xff1e1e22); // macOS Source List sidebar material
    const juce::Colour bgToolbar       (0xff25252a); // Unified macOS pro toolbar
    const juce::Colour cardSurface     (0xff2a2a2e); // Apple card / node body
    const juce::Colour cardHeader      (0xff323237); // Apple card header surface
    const juce::Colour displayRecessed (0xff1c1c1f); // Inset display screen
    const juce::Colour strokeHairline  (0xff3a3a40); // 1px structural separator
    const juce::Colour specularRim     (0x18ffffff); // Apple top keyline highlight

    // Apple System Accents
    const juce::Colour appleBlue       (0xff0a84ff); // Apple System Blue (focus & active controls)
    const juce::Colour appleGreen      (0xff30d158); // Apple System Green (audio level meters)
    const juce::Colour appleYellow     (0xffffd60a); // Apple System Yellow (cautionary audio levels)
    const juce::Colour appleRed        (0xffff453a); // Apple System Red (clipping warning & close)
    const juce::Colour appleOrange     (0xffff9f0a); // Apple System Orange
    const juce::Colour applePurple     (0xffbf5af2); // Apple System Purple
    const juce::Colour appleCyan       (0xff64d2ff); // Apple System Cyan

    // Studio Pro Cables (Neutral Space Gray / Studio Graphite - prevents color clashing)
    const juce::Colour cableSleeve     (0xff44444c); // Dark studio rubber sleeve
    const juce::Colour cableCore       (0xff888894); // Specular highlight ridge
    const juce::Colour appleCable      = cableSleeve;

    // Apple Typography Hierarchy (Inter font)
    const juce::Colour textPrimary     (0xfff5f5f7); // Apple Label White
    const juce::Colour textSecondary   (0xff98989d); // Apple Secondary Label
    const juce::Colour textTertiary    (0xff636366); // Apple Tertiary / Placeholder Label

    // Backward-compatibility aliases
    const juce::Colour bgDominant      = bgCanvas;
    const juce::Colour panelDark       = bgSidebar;
    const juce::Colour nodeBg          = cardSurface;
    const juce::Colour primaryIndigo   = appleBlue;
    const juce::Colour indigoSurface   = cardHeader;
    const juce::Colour indigoOutline   = strokeHairline;
    const juce::Colour accentCyan      = appleBlue;
    const juce::Colour accentSignal    = appleGreen;
    const juce::Colour accentWarning   = appleRed;
    const juce::Colour accentBlue      = appleBlue;
    const juce::Colour textDim         = textSecondary;
    const juce::Colour textBright      = textPrimary;
}

/** LookAndFeel that routes every stock-widget font (buttons, combo boxes,
    labels, popup menus, ...) through UITheme's custom typeface, and gives
    buttons/combo boxes a flatter, darker look consistent with the rest of
    the UI. Install once with setLookAndFeel() in the editor's constructor. */
class CustomLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CustomLookAndFeel();

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                                bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getLabelFont (juce::Label&) override;

    juce::PopupMenu::Options getOptionsForComboBoxPopupMenu (juce::ComboBox& box, juce::Label& label) override;
    void drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height) override;
};

/** Small round power/bypass icon button, styled after the blue-ring power
    icon in typical plugin headers. Toggling state (setToggleState) drives the
    ring/glyph colour — wire it up exactly like a normal juce::Button. */
class PowerButton : public juce::Button
{
public:
    PowerButton() : juce::Button ("power") { setClickingTogglesState (true); }
    void paintButton (juce::Graphics&, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};
