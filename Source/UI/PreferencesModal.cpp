#include "PreferencesModal.h"
#include "NodeCanvas.h"
#include "CanvasMinimap.h"
#include "../PluginProcessor.h"
#include "../UITheme.h"
#include "../Localization.h"
#include <BinaryData.h>

PreferencesModal::PreferencesModal (PluginProcessor& proc,
                                    NodeCanvas& canvas,
                                    CanvasMinimap& mm,
                                    std::function<void()> onSettingsChanged,
                                    std::function<void()> onClose)
    : processor (proc),
      nodeCanvas (canvas),
      minimap (mm),
      onSettingsChangedCallback (onSettingsChanged),
      onCloseCallback (onClose)
{
    setAlwaysOnTop (true);

    // Tab buttons
    settingsTabBtn.onClick  = [this] { setTab (Tab::Settings); };
    shortcutsTabBtn.onClick = [this] { setTab (Tab::Shortcuts); };
    aboutTabBtn.onClick     = [this] { setTab (Tab::About); };
    addAndMakeVisible (settingsTabBtn);
    addAndMakeVisible (shortcutsTabBtn);
    addAndMakeVisible (aboutTabBtn);

    closeBtn.onClick = [this]
    {
        if (onCloseCallback)
            onCloseCallback();
    };
    addAndMakeVisible (closeBtn);

    // Language Selector
    languageSelector.addItem ("English", 1);
    languageSelector.addItem (juce::String (juce::CharPointer_UTF8 ("T\xc3\xbcrk\xc3\xa7" "e")), 2);
    languageSelector.addItem (juce::String (juce::CharPointer_UTF8 ("\xd0\xa0\xd1\x83\xd1\x81\xd1\x81\xd0\xba\xd0\xb8\xd0\xb9")), 3);
    languageSelector.addItem (juce::String (juce::CharPointer_UTF8 ("\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e")), 4);

    int langId = 1;
    switch (LocalizationManager::instance().getLanguage())
    {
        case Language::English:  langId = 1; break;
        case Language::Turkish:  langId = 2; break;
        case Language::Russian:  langId = 3; break;
        case Language::Japanese: langId = 4; break;
    }
    languageSelector.setSelectedId (langId, juce::dontSendNotification);
    languageSelector.onChange = [this]
    {
        int id = languageSelector.getSelectedId();
        Language selectedLang = Language::English;
        if (id == 2) selectedLang = Language::Turkish;
        else if (id == 3) selectedLang = Language::Russian;
        else if (id == 4) selectedLang = Language::Japanese;

        LocalizationManager::instance().setLanguage (selectedLang);

        settingsTabBtn.setButtonText (tr ("TAB_SETTINGS"));
        shortcutsTabBtn.setButtonText (tr ("TAB_SHORTCUTS"));
        aboutTabBtn.setButtonText (tr ("TAB_ABOUT"));
        closeBtn.setButtonText (tr ("CLOSE"));
        openExplorerBtn.setButtonText (tr ("OPEN_EXPLORER"));
        changeFolderBtn.setButtonText (tr ("CHANGE_FOLDER"));
        snapToggle.setButtonText (tr ("SNAP_TOGGLE"));
        minimapToggle.setButtonText (tr ("MINIMAP_TOGGLE"));
        cableGlowToggle.setButtonText (tr ("GLOW_TOGGLE"));
        resetViewBtn.setButtonText (tr ("RESET_VIEW_BTN"));
        clearGraphBtn.setButtonText (tr ("CLEAR_GRAPH_BTN"));

        if (onSettingsChangedCallback)
            onSettingsChangedCallback();

        repaint();
    };
    addAndMakeVisible (languageSelector);

    // Preset Path Box
    presetPathBox.setReadOnly (true);
    presetPathBox.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff141416));
    presetPathBox.setColour (juce::TextEditor::outlineColourId, UITheme::strokeHairline);
    presetPathBox.setColour (juce::TextEditor::textColourId, UITheme::textSecondary);
    presetPathBox.setFont (UITheme::getFont (10.5f));
    addAndMakeVisible (presetPathBox);

    openExplorerBtn.onClick = [this]
    {
        processor.getPresetManager().openPresetsFolderInExplorer();
    };
    addAndMakeVisible (openExplorerBtn);

    changeFolderBtn.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Select Presets Folder",
            processor.getPresetManager().getPresetsDirectory(),
            "");

        auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories;
        fileChooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& fc)
        {
            auto result = fc.getResult();
            if (result.exists())
            {
                processor.getPresetManager().setCustomPresetsDirectory (result);
                updatePathDisplay();
                if (onSettingsChangedCallback)
                    onSettingsChangedCallback();
            }
        });
    };
    addAndMakeVisible (changeFolderBtn);

    // Canvas Options
    snapToggle.setToggleState (nodeCanvas.isSnapToGrid(), juce::dontSendNotification);
    snapToggle.onClick = [this]
    {
        nodeCanvas.setSnapToGrid (snapToggle.getToggleState());
        if (onSettingsChangedCallback)
            onSettingsChangedCallback();
    };
    addAndMakeVisible (snapToggle);

    minimapToggle.setToggleState (minimap.isVisible(), juce::dontSendNotification);
    minimapToggle.onClick = [this]
    {
        minimap.setVisible (minimapToggle.getToggleState());
    };
    addAndMakeVisible (minimapToggle);

    cableGlowToggle.setToggleState (nodeCanvas.isShowCableGlow(), juce::dontSendNotification);
    cableGlowToggle.onClick = [this]
    {
        nodeCanvas.setShowCableGlow (cableGlowToggle.getToggleState());
    };
    addAndMakeVisible (cableGlowToggle);

    // Canvas Actions
    resetViewBtn.onClick = [this]
    {
        nodeCanvas.resetView();
        if (onSettingsChangedCallback)
            onSettingsChangedCallback();
    };
    addAndMakeVisible (resetViewBtn);

    clearGraphBtn.onClick = [this]
    {
        auto& g = processor.getGraph();
        juce::Array<juce::AudioProcessorGraph::NodeID> toRemove;
        for (auto* n : g.getNodes())
        {
            if (n->nodeID != processor.getAudioInputNodeID() && n->nodeID != processor.getAudioOutputNodeID())
                toRemove.add (n->nodeID);
        }
        for (auto id : toRemove)
            processor.removeModule (id);

        nodeCanvas.setSelectedNodeID ({});
        updatePathDisplay();
        if (onSettingsChangedCallback)
            onSettingsChangedCallback();
    };
    addAndMakeVisible (clearGraphBtn);

    audioInfoLabel.setFont (UITheme::getFont (11.5f));
    audioInfoLabel.setColour (juce::Label::textColourId, UITheme::textSecondary);
    addAndMakeVisible (audioInfoLabel);

    updatePathDisplay();
    setTab (Tab::Settings);
}

void PreferencesModal::updatePathDisplay()
{
    auto path = processor.getPresetManager().getPresetsDirectory().getFullPathName();
    presetPathBox.setText (path, juce::dontSendNotification);

    double sr = processor.currentSampleRate.load();
    if (sr <= 0.0) sr = processor.getSampleRate();
    if (sr <= 0.0) sr = 48000.0;

    int bs = processor.currentBlockSize.load();
    if (bs <= 0) bs = processor.getBlockSize();
    if (bs <= 0) bs = 256;

    float latencyMs = ((float) bs / (float) sr) * 1000.0f;
    int nodesCount = juce::jmax (0, (int) processor.getGraph().getNodes().size() - 2); // exclude in/out

    juce::String dspInfo = "Sample Rate: " + juce::String ((int) sr) + " Hz  |  "
                         + "Buffer: " + juce::String (bs) + " smp (" + juce::String (latencyMs, 1) + " ms)  |  "
                         + "Active Modules: " + juce::String (nodesCount);
    audioInfoLabel.setText (dspInfo, juce::dontSendNotification);
}

void PreferencesModal::setTab (Tab newTab)
{
    currentTab = newTab;

    bool isSettings = (currentTab == Tab::Settings);
    languageSelector.setVisible (isSettings);
    presetPathBox.setVisible (isSettings);
    openExplorerBtn.setVisible (isSettings);
    changeFolderBtn.setVisible (isSettings);
    snapToggle.setVisible (isSettings);
    minimapToggle.setVisible (isSettings);
    cableGlowToggle.setVisible (isSettings);
    resetViewBtn.setVisible (isSettings);
    clearGraphBtn.setVisible (isSettings);
    audioInfoLabel.setVisible (isSettings);

    settingsTabBtn.setColour  (juce::TextButton::buttonColourId, currentTab == Tab::Settings  ? UITheme::appleBlue : juce::Colour (0xff252528));
    shortcutsTabBtn.setColour (juce::TextButton::buttonColourId, currentTab == Tab::Shortcuts ? UITheme::appleBlue : juce::Colour (0xff252528));
    aboutTabBtn.setColour     (juce::TextButton::buttonColourId, currentTab == Tab::About     ? UITheme::appleBlue : juce::Colour (0xff252528));

    resized();
    repaint();
}

void PreferencesModal::mouseDown (const juce::MouseEvent& e)
{
    if (! cardBounds.contains (e.getPosition()))
    {
        if (onCloseCallback)
            onCloseCallback();
    }
}

void PreferencesModal::paint (juce::Graphics& g)
{
    // 1. Dark ambient scrim over the whole plugin
    g.fillAll (juce::Colour (0x99000000));

    // 2. Center Card (Frosted deep macOS window)
    auto b = cardBounds.toFloat();
    const float corner = 12.0f;

    // Card drop shadow
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (b.translated (0.0f, 6.0f), corner);

    // Card surface
    g.setColour (juce::Colour (0xf418181c));
    g.fillRoundedRectangle (b, corner);

    // Subtle 1px outline
    g.setColour (UITheme::strokeHairline);
    g.drawRoundedRectangle (b, corner, 1.0f);

    // Top rim specular reflection
    g.setColour (UITheme::specularRim);
    g.drawLine (b.getX() + corner, b.getY() + 0.5f, b.getRight() - corner, b.getY() + 0.5f, 1.0f);

    // Top Header hairline divider
    g.setColour (UITheme::strokeHairline);
    g.drawLine (b.getX(), b.getY() + 44.0f, b.getRight(), b.getY() + 44.0f, 1.0f);

    auto contentArea = cardBounds.withTrimmedTop (48).reduced (20, 14);

    if (currentTab == Tab::Settings)
        drawSettingsTab (g, contentArea);
    else if (currentTab == Tab::Shortcuts)
        drawShortcutsTab (g, contentArea);
    else
        drawAboutTab (g, contentArea);
}

void PreferencesModal::drawSettingsTab (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (UITheme::textPrimary);
    g.setFont (UITheme::getFont (12.5f));

    // Language Section
    g.drawText (tr ("LANGUAGE").toUpperCase(), area.getX(), area.getY() + 2, 200, 18, juce::Justification::centredLeft);

    // Section 1: Presets
    int y1 = area.getY() + 38;
    g.setColour (UITheme::strokeHairline);
    g.drawLine ((float) area.getX(), (float) y1, (float) area.getRight(), (float) y1, 1.0f);

    g.setColour (UITheme::textPrimary);
    g.setFont (UITheme::getFont (12.5f));
    g.drawText (tr ("PRESET_FOLDER").toUpperCase() + " (.MVS)", area.getX(), y1 + 8, area.getWidth(), 18, juce::Justification::centredLeft);

    // Section 2: Workflow
    int y2 = y1 + 105;
    g.setColour (UITheme::strokeHairline);
    g.drawLine ((float) area.getX(), (float) y2, (float) area.getRight(), (float) y2, 1.0f);

    g.setColour (UITheme::textPrimary);
    g.setFont (UITheme::getFont (12.5f));
    g.drawText (tr ("CANVAS_WORKFLOW").toUpperCase(), area.getX(), y2 + 8, area.getWidth(), 18, juce::Justification::centredLeft);

    // Section 3: Audio DSP Engine
    int y3 = y2 + 160;
    g.setColour (UITheme::strokeHairline);
    g.drawLine ((float) area.getX(), (float) y3, (float) area.getRight(), (float) y3, 1.0f);

    g.setColour (UITheme::textPrimary);
    g.setFont (UITheme::getFont (12.5f));
    g.drawText (tr ("DSP_TELEMETRY").toUpperCase(), area.getX(), y3 + 8, area.getWidth(), 18, juce::Justification::centredLeft);
}

void PreferencesModal::drawShortcutsTab (juce::Graphics& g, juce::Rectangle<int> area)
{
    g.setColour (UITheme::textPrimary);
    g.setFont (UITheme::getFont (13.5f, true));
    g.drawText (tr ("SHORTCUTS_TITLE").toUpperCase(), area.getX(), area.getY() + 2, area.getWidth(), 20, juce::Justification::centredLeft);

    struct ShortcutRow
    {
        juce::String keys;
        juce::String desc;
    };

    const std::vector<ShortcutRow> shortcuts = {
        { "Ctrl + D",              "Duplicate Selected Module(s) with parameters" },
        { "Delete / Backspace",    "Remove Selected Module(s)" },
        { "Shift + Delete",        "Dissolve & Reconnect (Bypass Removal)" },
        { "Left Drag (Empty)",     "Rubber-Band Box Multi-Selection" },
        { "Shift + Left Click",    "Add / Toggle Module in Selection" },
        { "Middle Click / Alt",    "Pan Canvas Camera" },
        { "Mouse Wheel",           "Zoom In / Out at Cursor position" },
        { "Top Bar + / -",         "Centered Canvas Zoom In / Out" },
        { "Right Click (Cable)",   "Disconnect Stereo Cable (1 Click) / Insert Module" },
        { "Right Click (Module)",  "Color Palette, Duplicate, Bypass, Open UI" },
        { "Top Left Power",        "Master Plugin Bypass (Raw DAW Passthrough)" }
    };

    int startY = area.getY() + 30;
    int rowH = 34;

    for (size_t i = 0; i < shortcuts.size(); ++i)
    {
        int y = startY + (int) i * rowH;
        if (y + rowH > area.getBottom()) break;

        // Row background
        if (i % 2 == 0)
        {
            g.setColour (juce::Colour (0x0cffffff));
            g.fillRoundedRectangle ((float) area.getX(), (float) y, (float) area.getWidth(), (float) (rowH - 4), 4.0f);
        }

        // Key badge
        auto badgeRect = juce::Rectangle<float> ((float) area.getX() + 6.0f, (float) y + 3.0f, 155.0f, 22.0f);
        g.setColour (juce::Colour (0xff202026));
        g.fillRoundedRectangle (badgeRect, 4.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (badgeRect, 4.0f, 1.0f);

        g.setColour (UITheme::appleBlue);
        g.setFont (UITheme::getFont (10.0f, true));
        g.drawText (shortcuts[i].keys, badgeRect, juce::Justification::centred);

        // Description
        g.setColour (UITheme::textPrimary);
        g.setFont (UITheme::getFont (11.0f));
        g.drawText (shortcuts[i].desc, juce::Rectangle<float> ((float) area.getX() + 172.0f, (float) y + 2.0f, (float) area.getWidth() - 176.0f, 24.0f), juce::Justification::centredLeft);
    }
}

void PreferencesModal::drawAboutTab (juce::Graphics& g, juce::Rectangle<int> area)
{
    auto b = area.toFloat();

    // 1. Sleek Glowing Logo Badge
    float logoSize = 64.0f;
    auto logoRect = juce::Rectangle<float> (b.getCentreX() - logoSize * 0.5f, b.getY() + 10.0f, logoSize, logoSize);

    // Soft glowing halo
    g.setColour (UITheme::appleBlue.withAlpha (0.18f));
    g.fillEllipse (logoRect.expanded (8.0f));

    auto logoImg = juce::ImageCache::getFromMemory (BinaryData::icon_png, BinaryData::icon_pngSize);
    if (logoImg.isValid())
    {
        g.drawImageWithin (logoImg, (int) logoRect.getX(), (int) logoRect.getY(), (int) logoRect.getWidth(), (int) logoRect.getHeight(),
                           juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
    }
    else
    {
        // Logo background circle
        g.setColour (juce::Colour (0xff202026));
        g.fillEllipse (logoRect);
        g.setColour (UITheme::appleBlue);
        g.drawEllipse (logoRect, 1.5f);

        // Dynamic wave motif inside logo
        juce::Path wave;
        wave.startNewSubPath (logoRect.getX() + 16.0f, logoRect.getCentreY());
        wave.quadraticTo (logoRect.getX() + 24.0f, logoRect.getY() + 16.0f, logoRect.getX() + 32.0f, logoRect.getCentreY());
        wave.quadraticTo (logoRect.getX() + 40.0f, logoRect.getBottom() - 16.0f, logoRect.getRight() - 16.0f, logoRect.getCentreY());
        g.setColour (UITheme::appleBlue);
        g.strokePath (wave, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // 2. Title & Version
    g.setColour (UITheme::textPrimary);
    g.setFont (UITheme::getFont (18.0f, true));
    g.drawText ("Modular Voice Studio", juce::Rectangle<float> (b.getX(), logoRect.getBottom() + 8.0f, b.getWidth(), 22.0f), juce::Justification::centred);

    g.setColour (UITheme::appleBlue);
    g.setFont (UITheme::getFont (11.0f, true));
    g.drawText (juce::String::fromUTF8 ("VERSION 1.5.0 \xe2\x80\xa2 PROFESSIONAL DSP EDITION"), juce::Rectangle<float> (b.getX(), logoRect.getBottom() + 30.0f, b.getWidth(), 16.0f), juce::Justification::centred);

    // 3. Author / Lead Engineer Badge
    auto authorRect = juce::Rectangle<float> (b.getCentreX() - 175.0f, logoRect.getBottom() + 50.0f, 350.0f, 28.0f);
    g.setColour (juce::Colour (0x24ffffff));
    g.fillRoundedRectangle (authorRect, 5.0f);
    g.setColour (UITheme::appleBlue.withAlpha (0.4f));
    g.drawRoundedRectangle (authorRect, 5.0f, 1.0f);

    g.setColour (UITheme::textSecondary);
    g.setFont (UITheme::getFont (10.5f));
    g.drawText ("Created & Engineered by", authorRect.removeFromLeft (140.0f), juce::Justification::centredRight);

    g.setColour (juce::Colours::white);
    g.setFont (UITheme::getFont (11.5f, true));
    g.drawText (juce::String::fromUTF8 ("Furkan \"rootcf\" \xc3\x87" "entek"), authorRect.withTrimmedLeft (8.0f), juce::Justification::centredLeft);

    // 4. Feature Badges (Pills)
    juce::StringArray badges = { "64-BIT VST3 / STANDALONE", "VECTOR DIRECTWRITE", "SIMD DSP ENGINE", ".MVS PATCH SYSTEM" };
    float badgeW = 126.0f;
    float totalBadgesW = badges.size() * badgeW + (badges.size() - 1) * 6.0f;
    float startX = b.getCentreX() - (totalBadgesW * 0.5f);
    float badgeY = logoRect.getBottom() + 86.0f;

    for (int i = 0; i < badges.size(); ++i)
    {
        auto badgeArea = juce::Rectangle<float> (startX + i * (badgeW + 6.0f), badgeY, badgeW, 18.0f);
        g.setColour (juce::Colour (0x1cffffff));
        g.fillRoundedRectangle (badgeArea, 3.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (badgeArea, 3.0f, 1.0f);

        g.setColour (UITheme::textSecondary);
        g.setFont (UITheme::getFont (8.0f, true));
        g.drawText (badges[i], badgeArea, juce::Justification::centred);
    }

    // 5. Description & Open Source Libraries
    g.setColour (UITheme::textSecondary);
    g.setFont (UITheme::getFont (10.5f));
    auto descArea = juce::Rectangle<float> (b.getX() + 20.0f, badgeY + 26.0f, b.getWidth() - 40.0f, 38.0f);
    g.drawFittedText ("Designed for voiceover artists, podcasters, broadcast engineers, and audio purists.\n"
                      "Construct custom dynamic chains, surgical frequency correction, 3D spatial room modeling, and AI denoising.",
                      descArea.toNearestInt(), juce::Justification::centred, 2);

    // Third-Party Credits
    auto libArea = juce::Rectangle<float> (b.getX() + 20.0f, descArea.getBottom() + 6.0f, b.getWidth() - 40.0f, 28.0f);
    g.setColour (UITheme::textTertiary);
    g.setFont (UITheme::getFont (9.0f));
    g.drawFittedText (juce::String::fromUTF8 ("Powered by JUCE 8 (GPLv3) \xe2\x80\xa2 RNNoise Deep Learning Denoise (BSD 3-Clause) \xe2\x80\xa2 Inter Font (SIL OFL 1.1)"),
                      libArea.toNearestInt(), juce::Justification::centred, 2);

    // 6. Footer / Copyright
    g.setColour (UITheme::strokeHairline);
    g.drawLine (b.getX() + 40.0f, b.getBottom() - 32.0f, b.getRight() - 40.0f, b.getBottom() - 32.0f, 1.0f);

    g.setColour (UITheme::textSecondary);
    g.setFont (UITheme::getFont (9.5f));
    g.drawText (juce::String::fromUTF8 ("Copyright \xc2\xa9 2026 Furkan \"rootcf\" \xc3\x87" "entek. All rights reserved."),
                juce::Rectangle<float> (b.getX(), b.getBottom() - 26.0f, b.getWidth(), 16.0f), juce::Justification::centred);
}

void PreferencesModal::resized()
{
    // Center the 580x500 modal card inside the plugin
    cardBounds = getLocalBounds().withSizeKeepingCentre (580, 500);

    // Header buttons
    int hX = cardBounds.getX() + 16;
    int hY = cardBounds.getY() + 10;
    settingsTabBtn.setBounds  (hX, hY, 80, 24);
    shortcutsTabBtn.setBounds (hX + 86, hY, 80, 24);
    aboutTabBtn.setBounds     (hX + 172, hY, 80, 24);
    closeBtn.setBounds        (cardBounds.getRight() - 66, hY, 50, 24);

    auto area = cardBounds.withTrimmedTop (48).reduced (20, 12);

    // Language dropdown
    languageSelector.setBounds (area.getRight() - 140, area.getY(), 140, 24);

    // Section 1: Presets
    int y1 = area.getY() + 38;
    presetPathBox.setBounds (area.getX(), y1 + 32, area.getWidth(), 24);
    openExplorerBtn.setBounds (area.getX(), y1 + 60, 140, 24);
    changeFolderBtn.setBounds (area.getX() + 148, y1 + 60, 120, 24);

    // Section 2: Workflow
    int y2 = y1 + 105;
    snapToggle.setBounds (area.getX(), y2 + 30, area.getWidth(), 20);
    minimapToggle.setBounds (area.getX(), y2 + 53, area.getWidth(), 20);
    cableGlowToggle.setBounds (area.getX(), y2 + 76, area.getWidth(), 20);

    resetViewBtn.setBounds (area.getX(), y2 + 104, 185, 24);
    clearGraphBtn.setBounds (area.getX() + 195, y2 + 104, 185, 24);

    // Section 3: Audio DSP Engine Telemetry
    int y3 = y2 + 160;
    audioInfoLabel.setBounds (area.getX(), y3 + 28, area.getWidth(), 22);
}
