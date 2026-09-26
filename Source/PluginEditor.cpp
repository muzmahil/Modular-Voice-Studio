#include "PluginEditor.h"
#include "./Graph/ModuleFactory.h"
#include "Localization.h"
#include <BinaryData.h>

PluginEditor::PluginEditor (PluginProcessor& p)
    : juce::AudioProcessorEditor (p), processor (p), canvas (p), sidebar (p, canvas),
      inspector (p, canvas), minimap (canvas, p), oscilloscopeHud (p), canvasContainer (canvas),
      leftSplitter ([this] (int delta)
      {
          leftSidebarWidth += delta;
          if (leftSidebarWidth < 120)
          {
              isLeftSidebarCompact = true;
              leftSidebarWidth = 230;
          }
          else
          {
              isLeftSidebarCompact = false;
              leftSidebarWidth = juce::jlimit (140, 500, leftSidebarWidth);
          }
          resized();
      }),
      rightSplitter ([this] (int delta)
      {
          rightInspectorWidth -= delta;
          if (rightInspectorWidth < 100)
          {
              isRightInspectorCollapsed = true;
              rightInspectorWidth = 230;
          }
          else
          {
              isRightInspectorCollapsed = false;
              rightInspectorWidth = juce::jlimit (140, 500, rightInspectorWidth);
          }
          resized();
      })
{
    juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);
    setLookAndFeel (&lookAndFeel);

    // -- power / global bypass ------------------------------------------------
    addAndMakeVisible (powerButton);
    powerButton.setToggleState (!processor.pluginBypassed.load(), juce::dontSendNotification); 
    powerButton.onClick = [this] { toggleGlobalBypass(); };

    // -- preset strip -----------------------------------------------------
    addAndMakeVisible (prevPresetBtn);
    addAndMakeVisible (nextPresetBtn);
    prevPresetBtn.onClick = [this]
    {
        processor.getPresetManager().loadPreviousPreset (processor);
        canvas.rebuildFromGraph();
        updatePresetUI();
        repaint();
    };
    nextPresetBtn.onClick = [this]
    {
        processor.getPresetManager().loadNextPreset (processor);
        canvas.rebuildFromGraph();
        updatePresetUI();
        repaint();
    };

    addAndMakeVisible (presetBox);
    presetBox.setJustificationType (juce::Justification::centredLeft);
    presetBox.onChange = [this]
    {
        int id = presetBox.getSelectedId();
        if (id == 1)
        {
            // <Empty / Blank Canvas>
            processor.clearGraphToDefault();
            processor.getPresetManager().clearCurrentPresetIndex();
            canvas.rebuildFromGraph();
            updatePresetUI();
            repaint();
        }
        else if (id > 1)
        {
            int idx = id - 2;
            processor.getPresetManager().loadPreset (idx, processor);
            canvas.rebuildFromGraph();
            updatePresetUI();
            repaint();
        }
    };

    addAndMakeVisible (loadPresetBtn);
    loadPresetBtn.onClick = [this] { showLoadPresetDialog(); };

    addAndMakeVisible (savePresetBtn);
    savePresetBtn.onClick = [this] { saveCurrentPresetDirectly(); };

    addAndMakeVisible (saveAsPresetBtn);
    saveAsPresetBtn.onClick = [this] { showSavePresetDialog(); };

    addAndMakeVisible (settingsBtn);
    settingsBtn.onClick = [this] { openPreferences (PreferencesModal::Tab::Settings); };

    updatePresetUI();

    // -- Undo / Redo buttons ----------------------------------------------
    addAndMakeVisible (undoBtn);
    addAndMakeVisible (redoBtn);
    undoBtn.onClick = [this]
    {
        processor.getHistoryManager().performUndo (processor);
        updatePresetUI();
    };
    redoBtn.onClick = [this]
    {
        processor.getHistoryManager().performRedo (processor);
        updatePresetUI();
    };
    undoBtn.setEnabled (false);
    redoBtn.setEnabled (false);

    // -- zoom + snap (right side) ------------------------------------------
    zoomOutBtn.onClick = [this] { canvas.zoomOut(); updateZoomReadout(); };
    zoomInBtn.onClick  = [this] { canvas.zoomIn();  updateZoomReadout(); };
    addAndMakeVisible (zoomOutBtn);
    addAndMakeVisible (zoomInBtn);

    addAndMakeVisible (snapToggleBtn);
    snapToggleBtn.setClickingTogglesState (true);
    snapToggleBtn.onClick = [this] { canvas.setSnapToGrid (snapToggleBtn.getToggleState()); };

    // Canvas Container (Hosts large infinite canvas + floating bottom-right Minimap & Oscilloscope HUD)
    addAndMakeVisible (canvasContainer);
    canvasContainer.addAndMakeVisible (canvas);
    canvasContainer.addAndMakeVisible (minimap);
    canvasContainer.addAndMakeVisible (oscilloscopeHud);
    canvas.setTopLeftPosition (0, 0);

    // Left Sidebar (Module Library & Patches)
    addAndMakeVisible (sidebar);
    sidebar.onCollapseRequested = [this]
    {
        isLeftSidebarCompact = ! isLeftSidebarCompact;
        resized();
    };
    sidebar.onModuleSelected = [this] (const juce::String& type)
    {
        showModuleInfoCard (type);
    };

    // Right Sidebar (Node Parameter Inspector)
    addAndMakeVisible (inspector);
    inspector.onCollapseRequested = [this]
    {
        isRightInspectorCollapsed = true;
        resized();
    };

    // Resizing Splitters
    addAndMakeVisible (leftSplitter);
    addAndMakeVisible (rightSplitter);

    expandRightInspectorBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xdd202026));
    expandRightInspectorBtn.setColour (juce::TextButton::textColourOffId, UITheme::appleBlue);
    expandRightInspectorBtn.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    expandRightInspectorBtn.onClick = [this]
    {
        isRightInspectorCollapsed = false;
        resized();
    };
    addChildComponent (expandRightInspectorBtn);

    // Wire node selection on canvas to right inspector panel & dismiss info card if needed
    canvas.onNodeSelected = [this] (auto id)
    {
        inspector.inspectNode (id);
        if (id.uid != 0)
            hideModuleInfoCard();
    };

    // Initialize 4 DAW-automatable Macro Knobs
    juce::AudioParameterFloat* macros[4] = { processor.macro1, processor.macro2, processor.macro3, processor.macro4 };
    for (int i = 0; i < 4; ++i)
    {
        macroKnobs[i].setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        macroKnobs[i].setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        macroKnobs[i].setTooltip ("Macro " + juce::String (i + 1));
        addAndMakeVisible (macroKnobs[i]);
        if (macros[i] != nullptr)
            macroAttachments[i] = std::make_unique<juce::SliderParameterAttachment> (*macros[i], macroKnobs[i]);
    }

    updateLocalizedStrings();
    LocalizationManager::instance().addListener (this);
    processor.addChangeListener (this);

    // Standalone Calm 4-Channel Mixer initialization
    isStandaloneMode = (processor.wrapperType == juce::AudioProcessor::wrapperType_Standalone);
    if (isStandaloneMode)
    {
        isStandaloneMixerViewActive = true;
        standaloneMixer = std::make_unique<StandaloneMixerComponent> (processor);
        standaloneMixer->onOpenModularCanvas = [this] (int channelIndex)
        {
            processor.switchChannelGraph (channelIndex);
            isStandaloneMixerViewActive = false;
            if (standaloneMixer != nullptr)
                standaloneMixer->setVisible (false);
            backToMixerBtn.setVisible (true);
            dspChainTitleLabel.setVisible (true);
            updateDspChainTitle();
            canvas.rebuildFromGraph();
            updatePresetUI();
            setSize (1260, 780);
            if (auto* dw = findParentComponentOfClass<juce::DocumentWindow>())
                dw->centreWithSize (1260, 780);
            resized();
            repaint();
        };
        standaloneMixer->onOpenSettings = [this]()
        {
            openPreferences (PreferencesModal::Tab::AudioDevice);
        };
        addAndMakeVisible (*standaloneMixer);

        backToMixerBtn.setButtonText ("← Mixer");
        backToMixerBtn.setColour (juce::TextButton::buttonColourId, CalmTheme::cyanSoft);
        backToMixerBtn.setColour (juce::TextButton::textColourOffId, CalmTheme::bgApp);
        backToMixerBtn.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        backToMixerBtn.onClick = [this]()
        {
            isStandaloneMixerViewActive = true;
            if (standaloneMixer != nullptr)
                standaloneMixer->setVisible (true);
            backToMixerBtn.setVisible (false);
            dspChainTitleLabel.setVisible (false);
            setSize (780, 580);
            if (auto* dw = findParentComponentOfClass<juce::DocumentWindow>())
                dw->centreWithSize (780, 580);
            resized();
            repaint();
        };
        addChildComponent (backToMixerBtn);

        dspChainTitleLabel.setFont (UITheme::getFont (13.5f).boldened());
        dspChainTitleLabel.setColour (juce::Label::textColourId, CalmTheme::cyanSoft);
        dspChainTitleLabel.setJustificationType (juce::Justification::centredLeft);
        addChildComponent (dspChainTitleLabel);
    }

    openGLContext.attachTo (*this);
    setResizable (!isStandaloneMode, !isStandaloneMode);
    startTimerHz (30);

    if (isStandaloneMode)
        setSize (780, 580);
    else
        setSize (1300, 800);
}

void PluginEditor::parentHierarchyChanged()
{
    if (isStandaloneMode)
    {
        if (auto* dw = findParentComponentOfClass<juce::DocumentWindow>())
        {
            dw->setTitleBarHeight (0);
            dw->setResizable (false, false);
            dw->setSize (780, 580);
            dw->centreWithSize (780, 580);
        }
    }
}

PluginEditor::~PluginEditor()
{
    processor.removeChangeListener (this);
    LocalizationManager::instance().removeListener (this);
    stopTimer();
    openGLContext.detach();
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void PluginEditor::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    juce::ignoreUnused (source);
    updatePresetUI();
    canvas.rebuildFromGraph();
    canvas.setPanAndZoom (processor.canvasPanX, processor.canvasPanY, processor.canvasZoom);
    updateZoomReadout();
    repaint();
}

void PluginEditor::refreshCanvasView()
{
    updatePresetUI();
    canvas.rebuildFromGraph();
    canvas.setPanAndZoom (processor.canvasPanX, processor.canvasPanY, processor.canvasZoom);
    updateZoomReadout();
    repaint();
}

void PluginEditor::localizationChanged()
{
    updateLocalizedStrings();
    repaint();
}

void PluginEditor::updateLocalizedStrings()
{
    loadPresetBtn.setButtonText (tr ("LOAD"));
    savePresetBtn.setButtonText (tr ("SAVE"));
    saveAsPresetBtn.setButtonText (tr ("SAVE_AS"));
    undoBtn.setButtonText (tr ("UNDO"));
    redoBtn.setButtonText (tr ("REDO"));
    snapToggleBtn.setButtonText (tr ("SNAP"));
    settingsBtn.setButtonText (tr ("SETTINGS"));
    backToMixerBtn.setButtonText ("← Mixer");
    powerButton.setTooltip (powerButton.getToggleState() ? tr ("ACTIVE") : tr ("BYPASS"));
    if (moduleInfoCard != nullptr)
        moduleInfoCard->updateLocalizedText();
    updatePresetUI();
}

void PluginEditor::resized()
{
    if (isStandaloneMode && isStandaloneMixerViewActive && standaloneMixer != nullptr)
    {
        standaloneMixer->setVisible (true);
        standaloneMixer->setBounds (getLocalBounds());
        
        sidebar.setVisible (false);
        leftSplitter.setVisible (false);
        canvasContainer.setVisible (false);
        inspector.setVisible (false);
        rightSplitter.setVisible (false);
        minimap.setVisible (false);
        oscilloscopeHud.setVisible (false);
        powerButton.setVisible (false);
        prevPresetBtn.setVisible (false);
        nextPresetBtn.setVisible (false);
        presetBox.setVisible (false);
        loadPresetBtn.setVisible (false);
        savePresetBtn.setVisible (false);
        saveAsPresetBtn.setVisible (false);
        undoBtn.setVisible (false);
        redoBtn.setVisible (false);
        settingsBtn.setVisible (false);
        zoomInBtn.setVisible (false);
        zoomOutBtn.setVisible (false);
        snapToggleBtn.setVisible (false);
        for (int i = 0; i < 4; ++i) macroKnobs[i].setVisible (false);
        backToMixerBtn.setVisible (false);
        dspChainTitleLabel.setVisible (false);
        return;
    }

    if (standaloneMixer != nullptr)
        standaloneMixer->setVisible (false);

    sidebar.setVisible (true);
    canvasContainer.setVisible (true);
    powerButton.setVisible (true);
    prevPresetBtn.setVisible (true);
    nextPresetBtn.setVisible (true);
    presetBox.setVisible (true);
    loadPresetBtn.setVisible (true);
    savePresetBtn.setVisible (true);
    saveAsPresetBtn.setVisible (true);
    undoBtn.setVisible (true);
    redoBtn.setVisible (true);
    settingsBtn.setVisible (true);
    zoomInBtn.setVisible (true);
    zoomOutBtn.setVisible (true);
    snapToggleBtn.setVisible (true);
    for (int i = 0; i < 4; ++i) macroKnobs[i].setVisible (true);

    topBarBounds = getLocalBounds().removeFromTop (topBarHeight);
    auto bar = topBarBounds.reduced (12, 8);

    if (isStandaloneMode)
    {
        backToMixerBtn.setVisible (true);
        backToMixerBtn.setBounds (bar.removeFromLeft (86).reduced (0, 2));
        bar.removeFromLeft (10);

        dspChainTitleLabel.setVisible (true);
        dspChainTitleLabel.setBounds (bar.removeFromLeft (180).reduced (0, 2));
        bar.removeFromLeft (12);
        updateDspChainTitle();
    }
    else
    {
        backToMixerBtn.setVisible (false);
        dspChainTitleLabel.setVisible (false);
    }

    // -- left section: power button | preset navigation -----------------------
    powerButton.setBounds (bar.removeFromLeft (30).reduced (1, 1));
    bar.removeFromLeft (8);

    prevPresetBtn.setBounds (bar.removeFromLeft (22));
    bar.removeFromLeft (2);
    nextPresetBtn.setBounds (bar.removeFromLeft (22));
    bar.removeFromLeft (4);
    presetBox.setBounds (bar.removeFromLeft (130));
    bar.removeFromLeft (4);
    loadPresetBtn.setBounds (bar.removeFromLeft (48));
    bar.removeFromLeft (3);
    savePresetBtn.setBounds (bar.removeFromLeft (48));
    bar.removeFromLeft (3);
    saveAsPresetBtn.setBounds (bar.removeFromLeft (68));

    bar.removeFromLeft (14);

    // Undo / Redo buttons
    undoBtn.setBounds (bar.removeFromLeft (48));
    bar.removeFromLeft (4);
    redoBtn.setBounds (bar.removeFromLeft (48));

    // 4 DAW-automatable Macro Knobs
    bar.removeFromLeft (16);
    for (int i = 0; i < 4; ++i)
    {
        macroKnobs[i].setBounds (bar.removeFromLeft (26).reduced (0, 1));
        bar.removeFromLeft (6);
    }

    // -- right section: settings | module count | snap | zoom -----------------------------
    settingsBtn.setBounds (bar.removeFromRight (74));
    bar.removeFromRight (10);

    zoomInBtn.setBounds (bar.removeFromRight (22));
    zoomReadoutBounds = bar.removeFromRight (38);
    zoomOutBtn.setBounds (bar.removeFromRight (22));
    bar.removeFromRight (6);

    snapToggleBtn.setBounds (bar.removeFromRight (54));
    bar.removeFromRight (10);

    moduleCountBounds = bar.removeFromRight (95);

    // Bottom panel (32px)
    bottomPanelBounds = getLocalBounds().removeFromBottom (32);

    // Dynamic 3-Pane Layout with Draggable Splitters & Collapsible Sidebars!
    int contentY = topBarHeight;
    int contentH = getHeight() - topBarHeight - 32;

    int leftW = isLeftSidebarCompact ? 46 : juce::jlimit (140, 500, leftSidebarWidth);
    int rightW = isRightInspectorCollapsed ? 0 : juce::jlimit (140, 500, rightInspectorWidth);

    sidebar.setCompactMode (isLeftSidebarCompact);
    sidebar.setBounds (0, contentY, leftW, contentH);
    leftSplitter.setVisible (! isLeftSidebarCompact);
    if (! isLeftSidebarCompact)
        leftSplitter.setBounds (leftW - 3, contentY, 6, contentH);

    inspector.setVisible (! isRightInspectorCollapsed);
    rightSplitter.setVisible (! isRightInspectorCollapsed);
    expandRightInspectorBtn.setVisible (isRightInspectorCollapsed);

    if (! isRightInspectorCollapsed)
    {
        inspector.setBounds (getWidth() - rightW, contentY, rightW, contentH);
        rightSplitter.setBounds (getWidth() - rightW - 3, contentY, 6, contentH);
    }
    else
    {
        expandRightInspectorBtn.setBounds (getWidth() - 18, contentY + 8, 18, 36);
    }

    canvasContainer.setBounds (leftW, contentY, getWidth() - leftW - rightW, contentH);
    canvas.setSize (4000, 4000);

    static bool isFirstLayout = true;
    if (isFirstLayout)
    {
        isFirstLayout = false;
        if (processor.canvasPanX == -1400.0f && processor.canvasPanY == -1500.0f)
        {
            canvas.centerViewOnNodes();
        }
    }

    // Floating Minimap & Waveform Monitor HUD in bottom-right corner of canvasContainer!
    minimap.setBounds (canvasContainer.getWidth() - 188, canvasContainer.getHeight() - 128, 180, 120);
    oscilloscopeHud.setBounds (canvasContainer.getWidth() - 392, canvasContainer.getHeight() - 128, 196, 120);

    // Floating Module Info Card Overlay & Click-Outside Backdrop
    if (infoBackdrop != nullptr && infoBackdrop->isVisible())
    {
        infoBackdrop->setBounds (getLocalBounds());
        infoBackdrop->toFront (false);
    }

    if (moduleInfoCard != nullptr && moduleInfoCard->isVisible())
    {
        int cardX = isLeftSidebarCompact ? 58 : leftW + 12;
        int cardY = contentY + 12;
        int cardW = 380;
        int cardH = juce::jmin (510, contentH - 24);
        moduleInfoCard->setBounds (cardX, cardY, cardW, cardH);
        moduleInfoCard->toFront (true);
    }

    if (preferencesModal != nullptr)
        preferencesModal->setBounds (getLocalBounds());
}

void PluginEditor::paint (juce::Graphics& g)
{
    if (isStandaloneMode && isStandaloneMixerViewActive)
        return;

    g.fillAll (UITheme::bgCanvas); // macOS Dark Canvas Base

    // -- Top bar (macOS Unified Toolbar) -----------------------------------
    g.setColour (UITheme::bgToolbar);
    g.fillRect (topBarBounds);

    // Apple Keyline: Top highlight and bottom subtle hairline divider
    g.setColour (UITheme::specularRim);
    g.drawLine (0, 0, (float) getWidth(), 0, 1.0f);
    g.setColour (UITheme::strokeHairline);
    g.drawLine (0, (float) topBarHeight, (float) getWidth(), (float) topBarHeight, 1.0f);

    // Subtle vertical divider line between presets and toolbar
    g.setColour (UITheme::strokeHairline);
    g.drawLine (268.0f, 10.0f, 268.0f, (float) topBarHeight - 10.0f, 1.0f);

    // Separator line before Macros & Macro Labels
    if (macroKnobs[0].getX() > 0)
    {
        float divX = (float) macroKnobs[0].getX() - 7.0f;
        g.setColour (UITheme::strokeHairline);
        g.drawLine (divX, 10.0f, divX, (float) topBarHeight - 10.0f, 1.0f);

        for (int i = 0; i < 4; ++i)
        {
            auto kb = macroKnobs[i].getBounds().toFloat();
            g.setColour (UITheme::textTertiary);
            g.setFont (UITheme::getFont (7.5f, true));
            g.drawText ("M" + juce::String (i + 1), kb.withHeight (9.0f).translated (0.0f, -7.0f), juce::Justification::centred);
        }
    }

    // Module count pill badge (macOS Status Capsule)
    if (moduleCountBounds.getWidth() > 0)
    {
        int moduleCount = juce::jmax (0, (int) processor.getGraph().getNodes().size() - 2);
        auto badge = moduleCountBounds.toFloat().reduced (2.0f, 5.0f);
        g.setColour (juce::Colour (0xff2c2c31));
        g.fillRoundedRectangle (badge, 4.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (badge, 4.0f, 1.0f);

        g.setColour (UITheme::appleGreen); // Apple System Green dot
        g.fillEllipse (badge.getX() + 8.0f, badge.getCentreY() - 3.0f, 6.0f, 6.0f);

        g.setColour (UITheme::textSecondary);
        g.setFont (UITheme::getFont (10.5f));
        g.drawText (juce::String (moduleCount) + " " + tr ("MODULES_COUNT"),
                    badge.withTrimmedLeft (18.0f), juce::Justification::centredLeft);
    }

    // Zoom readout
    g.setColour (UITheme::textPrimary);
    g.setFont (UITheme::getFont (11.5f));
    g.drawText (juce::String (canvas.getZoomPercent()) + "%", zoomReadoutBounds, juce::Justification::centred);

    // =========================================================================
    // --- BOTTOM PANEL & PRO MASTER OUTPUT dB METER ---
    // =========================================================================
    auto bpb = bottomPanelBounds.toFloat();
    g.setColour (UITheme::bgToolbar);
    g.fillRect (bpb);

    // Top hairline divider
    g.setColour (UITheme::strokeHairline);
    g.drawLine (0, bpb.getY(), (float) getWidth(), bpb.getY(), 1.0f);

    // Left Title
    g.setColour (UITheme::textSecondary);
    g.setFont (UITheme::getFont (11.0f, true));
    g.drawText (tr ("MASTER_OUT"), juce::Rectangle<float> (16.0f, bpb.getY(), 95.0f, bpb.getHeight()), juce::Justification::centredLeft);

    // Decreased meter area to make plenty of room for telemetry indicators
    float meterX = 116.0f;
    float meterY = bpb.getY() + 14.0f;
    float meterW = juce::jlimit (170.0f, 280.0f, (float) getWidth() * 0.22f);
    float meterH = 12.0f;

    if (meterW > 80.0f)
    {
        // 1. Draw rich reference dB calibration markings & text
        struct DbTick { float db; const char* label; bool isMajor; };
        static const DbTick ticks[] = {
            { -60.0f, "-60", true  },
            { -54.0f, "",    false },
            { -48.0f, "-48", true  },
            { -42.0f, "",    false },
            { -36.0f, "-36", true  },
            { -30.0f, "",    false },
            { -24.0f, "-24", true  },
            { -18.0f, "",    false },
            { -12.0f, "-12", true  },
            {  -8.0f, "",    false },
            {  -6.0f,  "-6", true  },
            {  -4.0f, "",    false },
            {  -2.0f, "",    false },
            {   0.0f,   "0", true  },
            {  +2.0f, "",    false },
            {  +3.0f,  "+3", true  }
        };

        for (const auto& tick : ticks)
        {
            float normTick = juce::jlimit (0.0f, 1.0f, (tick.db + 60.0f) / 63.0f);
            float tx = meterX + meterW * normTick;

            juce::Colour col = UITheme::textSecondary;
            if (tick.db >= 0.0f)
                col = UITheme::appleRed;
            else if (tick.db >= -12.0f)
                col = UITheme::appleYellow;

            if (tick.isMajor && tick.label[0] != '\0')
            {
                g.setColour (col);
                g.setFont (UITheme::getFont (7.0f));
                g.drawText (tick.label, juce::Rectangle<float> (tx - 11.0f, bpb.getY() + 1.0f, 22.0f, 9.0f), juce::Justification::centred);
            }

            // Tick mark pointing into the meter
            float tickTop = tick.isMajor ? (bpb.getY() + 9.5f) : (bpb.getY() + 11.5f);
            g.setColour (col.withAlpha (tick.isMajor ? 0.55f : 0.30f));
            g.drawLine (tx, tickTop, tx, bpb.getY() + 13.5f, 1.0f);
        }

        auto meterTrack = juce::Rectangle<float> (meterX, meterY, meterW, meterH);

        // Apple Dark Inset Track
        g.setColour (UITheme::displayRecessed);
        g.fillRoundedRectangle (meterTrack, 2.5f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (meterTrack, 2.5f, 1.0f);

        // Decibel mapping (-60 dB to +3 dB)
        float db = juce::Decibels::gainToDecibels (currentMeterLevel, -60.0f);
        float normLevel = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 63.0f);

        // Peak Hold logic
        if (normLevel >= peakHoldLevel)
        {
            peakHoldLevel = normLevel;
            peakHoldTimer = 25; // Hold for ~25 frames (~800ms)
        }
        else
        {
            if (peakHoldTimer > 0)
                --peakHoldTimer;
            else
                peakHoldLevel = juce::jmax (0.0f, peakHoldLevel - 0.015f);
        }

        // Pro Audio Green -> Yellow -> Red Level Bar
        if (normLevel > 0.001f)
        {
            float activeW = meterW * normLevel;

            juce::ColourGradient meterGrad (
                UITheme::appleGreen, meterX, meterY,
                UITheme::appleRed, meterX + meterW, meterY,
                false
            );
            meterGrad.addColour (0.0,    UITheme::appleGreen);
            meterGrad.addColour (0.70,   UITheme::appleGreen);
            meterGrad.addColour (0.80,   UITheme::appleYellow);
            meterGrad.addColour (0.89,   juce::Colour (0xffff9f0a));
            meterGrad.addColour (0.952,  UITheme::appleRed);
            meterGrad.addColour (1.0,    juce::Colour (0xffff3b30));

            g.setGradientFill (meterGrad);
            g.fillRoundedRectangle (meterX, meterY, activeW, meterH, 2.0f);

            // Subtle 1px LED segment cuts
            g.setColour (juce::Colour (0x35000000));
            for (float sx = meterX + 4.0f; sx < meterX + activeW; sx += 4.0f)
                g.drawVerticalLine ((int) sx, meterY, meterY + meterH);
        }

        // Peak Hold line
        if (peakHoldLevel > 0.01f)
        {
            float peakX = meterX + meterW * peakHoldLevel;
            juce::Colour peakCol = peakHoldLevel >= 0.952f ? UITheme::appleRed 
                                 : (peakHoldLevel >= 0.762f ? UITheme::appleYellow : juce::Colours::white);
            g.setColour (peakCol);
            g.drawVerticalLine ((int) peakX, meterY, meterY + meterH);
        }

        // Numeric dB readout
        auto readoutArea = juce::Rectangle<float> (meterTrack.getRight() + 8.0f, bpb.getY(), 64.0f, bpb.getHeight());
        juce::String dbStr;
        juce::Colour readoutCol = UITheme::textPrimary;

        if (currentMeterLevel <= 0.0001f)
        {
            dbStr = "-inf dB";
            readoutCol = UITheme::textTertiary;
        }
        else
        {
            if (db >= 0.0f)
            {
                dbStr = "+" + juce::String (db, 1) + " dB";
                readoutCol = UITheme::appleRed;
            }
            else if (db >= -12.0f)
            {
                dbStr = juce::String (db, 1) + " dB";
                readoutCol = UITheme::appleYellow;
            }
            else
            {
                dbStr = juce::String (db, 1) + " dB";
                readoutCol = UITheme::appleGreen;
            }
        }

        g.setColour (readoutCol);
        g.setFont (UITheme::getFont (11.0f, true));
        g.drawText (dbStr, readoutArea, juce::Justification::centredLeft);

        // Vertical hairline separator between master output and telemetry
        float separatorX = readoutArea.getRight() + 10.0f;
        g.setColour (UITheme::strokeHairline);
        g.drawLine (separatorX, bpb.getY() + 6.0f, separatorX, bpb.getBottom() - 6.0f, 1.0f);

        // =========================================================================
        // --- TELEMETRY & SYSTEM PERFORMANCE INDICATORS (RIGHT SECTION) ---
        // =========================================================================
        struct TelemetryBadge {
            juce::String tag;
            juce::String value;
            juce::Colour valCol;
            bool hasLed = false;
            juce::Colour ledCol = juce::Colours::transparentBlack;
            float width = 95.0f;
        };

        float cpu = processor.currentCpuUsage.load();
        juce::Colour cpuCol = cpu > 65.0f ? UITheme::appleRed : (cpu > 30.0f ? UITheme::appleYellow : UITheme::appleGreen);

        float procMs = processor.currentProcessTimeMs.load();
        juce::Colour procCol = procMs > 5.0f ? UITheme::appleRed : (procMs > 2.0f ? UITheme::appleYellow : UITheme::textPrimary);
        juce::String procStr = procMs <= 0.001f ? "< 0.01 ms" : (juce::String (procMs, 2) + " ms");

        float sr = processor.currentSampleRate.load();
        juce::String srStr = sr >= 1000.0f ? (juce::String (sr / 1000.0f, 1) + " kHz") : (juce::String ((int) sr) + " Hz");

        int bs = processor.currentBlockSize.load();
        float bufMs = sr > 0.0f ? ((float) bs * 1000.0f / sr) : 0.0f;
        juce::String bufStr = juce::String (bs) + " spls (" + juce::String (bufMs, 1) + "ms)";

        int activeNodes = 0;
        for (auto* n : processor.getGraph().getNodes())
        {
            if (n->nodeID != processor.getAudioInputNodeID() && n->nodeID != processor.getAudioOutputNodeID())
                activeNodes++;
        }

        std::vector<TelemetryBadge> badges = {
            { tr ("CPU_LABEL"),   juce::String (cpu, 1) + " %", cpuCol, true, cpuCol, 96.0f },
            { tr ("RESP_LABEL"),  procStr,                      procCol, false, {},   102.0f },
            { tr ("SR_LABEL"),    srStr,                        UITheme::appleBlue, false, {}, 95.0f },
            { tr ("BUF_LABEL"),   bufStr,                       UITheme::textPrimary, false, {}, 122.0f },
            { "NODES", juce::String (activeNodes) + " " + tr ("ACTIVE"), UITheme::textSecondary, false, {}, 95.0f }
        };

        float curRight = (float) getWidth() - 16.0f;
        float badgeY = bpb.getY() + 6.0f;
        float badgeH = 20.0f;

        for (const auto& b : badges)
        {
            float badgeX = curRight - b.width;
            if (badgeX < separatorX + 16.0f)
                break; // Stop drawing badges if window is too narrow

            auto badgeRect = juce::Rectangle<float> (badgeX, badgeY, b.width, badgeH);

            // Apple Frosted Pill Surface
            g.setColour (juce::Colour (0x1effffff));
            g.fillRoundedRectangle (badgeRect, 3.5f);
            g.setColour (UITheme::strokeHairline);
            g.drawRoundedRectangle (badgeRect, 3.5f, 1.0f);

            // Left Tag Segment
            float tagW = b.tag.length() <= 3 ? 30.0f : 38.0f;
            auto tagRect = badgeRect.removeFromLeft (tagW);
            g.setColour (juce::Colour (0x18ffffff));
            g.fillRoundedRectangle (tagRect, 3.5f);
            g.fillRect (tagRect.getRight() - 3.5f, tagRect.getY(), 3.5f, tagRect.getHeight());

            g.setColour (UITheme::textSecondary);
            g.setFont (UITheme::getFont (8.0f, true));
            g.drawText (b.tag, tagRect, juce::Justification::centred);

            // Right Value Segment
            auto valRect = badgeRect.reduced (4.0f, 0.0f);
            if (b.hasLed)
            {
                // Tiny glowing LED status dot
                g.setColour (b.ledCol.withAlpha (0.40f));
                g.fillEllipse (valRect.getX() - 1.0f, valRect.getCentreY() - 3.5f, 7.0f, 7.0f);
                g.setColour (b.ledCol);
                g.fillEllipse (valRect.getX(), valRect.getCentreY() - 2.5f, 5.0f, 5.0f);
                valRect.removeFromLeft (8.0f);
            }

            g.setColour (b.valCol);
            g.setFont (UITheme::getFont (9.0f, true));
            g.drawText (b.value, valRect, juce::Justification::centred);

            curRight = badgeX - 8.0f;
        }
    }
}

void PluginEditor::toggleGlobalBypass()
{
    // Master plugin bypass: disables/enables the entire plugin.
    // When disabled (off), incoming audio from the DAW passes straight through untouched (bit-perfect dry).
    // It does NOT touch or alter individual module bypass states in the modular canvas!
    bool isPoweredOn = powerButton.getToggleState();
    processor.pluginBypassed.store (!isPoweredOn);
    repaint();
}

void PluginEditor::updateZoomReadout()
{
    repaint (zoomReadoutBounds);
}

void PluginEditor::timerCallback()
{
    float newLevel = processor.outputLevel.load();

    if (std::abs (newLevel - currentMeterLevel) > 0.002f || peakHoldLevel > 0.01f)
    {
        currentMeterLevel = newLevel;
        repaint (bottomPanelBounds);
    }

    undoBtn.setEnabled (processor.getHistoryManager().canUndo());
    redoBtn.setEnabled (processor.getHistoryManager().canRedo());

    canvas.refreshMeters();
}

void PluginEditor::updatePresetUI()
{
    auto& pm = processor.getPresetManager();
    presetBox.clear (juce::dontSendNotification);
    presetBox.addItem ("<Empty / New Patch>", 1);

    auto names = pm.getPresetNames();
    for (int i = 0; i < names.size(); ++i)
        presetBox.addItem (names[i], i + 2);

    int curIdx = pm.getCurrentPresetIndex();
    if (curIdx >= 0 && curIdx < names.size())
        presetBox.setSelectedId (curIdx + 2, juce::dontSendNotification);
    else
        presetBox.setSelectedId (1, juce::dontSendNotification);

    prevPresetBtn.setEnabled (names.size() > 0);
    nextPresetBtn.setEnabled (names.size() > 0);
}

void PluginEditor::showLoadPresetDialog()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Select .mvs Patch File to Load",
        processor.getPresetManager().getPresetsDirectory(),
        "*.mvs");

    auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    fileChooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file.existsAsFile())
        {
            processor.getPresetManager().loadFromFile (file, processor);
            updatePresetUI();
        }
    });
}

void PluginEditor::showSavePresetDialog()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Save Current Patch as .mvs",
        processor.getPresetManager().getPresetsDirectory().getChildFile ("My Custom Patch.mvs"),
        "*.mvs");

    auto chooserFlags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting;
    fileChooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file != juce::File())
        {
            if (! file.hasFileExtension (".mvs"))
                file = file.withFileExtension (".mvs");

            processor.getPresetManager().saveToFile (file, processor);
            updatePresetUI();
        }
    });
}

void PluginEditor::openPreferences (PreferencesModal::Tab tab)
{
    if (preferencesModal != nullptr)
    {
        preferencesModal.reset();
        return;
    }

    preferencesModal = std::make_unique<PreferencesModal> (
        processor,
        canvas,
        minimap,
        [this]
        {
            snapToggleBtn.setToggleState (canvas.isSnapToGrid(), juce::dontSendNotification);
            updatePresetUI();
            updateZoomReadout();
            repaint();
        },
        [this]
        {
            preferencesModal.reset();
        });

    preferencesModal->setTab (tab);
    addAndMakeVisible (*preferencesModal);
    preferencesModal->setBounds (getLocalBounds());
}

void PluginEditor::saveCurrentPresetDirectly()
{
    auto& pm = processor.getPresetManager();
    auto curName = pm.getCurrentPresetName();
    if (curName.isNotEmpty() && pm.getCurrentPresetIndex() >= 0)
    {
        pm.savePreset (curName, processor);
        updatePresetUI();
    }
    else
    {
        showSavePresetDialog();
    }
}

class InfoCardDismissBackdrop : public juce::Component
{
public:
    InfoCardDismissBackdrop (std::function<void()> onDismiss) : dismissCb (std::move (onDismiss)) {}
    void mouseDown (const juce::MouseEvent&) override
    {
        if (dismissCb != nullptr)
            dismissCb();
    }
private:
    std::function<void()> dismissCb;
};

void PluginEditor::showModuleInfoCard (const juce::String& moduleType)
{
    if (infoBackdrop == nullptr)
    {
        infoBackdrop = std::make_unique<InfoCardDismissBackdrop> ([this] { hideModuleInfoCard(); });
        addAndMakeVisible (*infoBackdrop);
    }
    infoBackdrop->setBounds (getLocalBounds());
    infoBackdrop->setVisible (true);
    infoBackdrop->toFront (false);

    if (moduleInfoCard == nullptr)
    {
        moduleInfoCard = std::make_unique<ModuleInfoCard> (
            moduleType,
            [this] (const juce::String& type)
            {
                canvas.addModuleAtViewCenter (type);
                hideModuleInfoCard();
            },
            [this]
            {
                hideModuleInfoCard();
            });
        addAndMakeVisible (*moduleInfoCard);
    }
    else
    {
        moduleInfoCard->updateInfo (moduleType);
        moduleInfoCard->setVisible (true);
    }

    resized();
    moduleInfoCard->toFront (true);
}

void PluginEditor::hideModuleInfoCard()
{
    if (infoBackdrop != nullptr)
        infoBackdrop->setVisible (false);
    if (moduleInfoCard != nullptr)
        moduleInfoCard->setVisible (false);
}

bool PluginEditor::keyPressed (const juce::KeyPress& key)
{
    if (key.isKeyCode (juce::KeyPress::escapeKey))
    {
        if (moduleInfoCard != nullptr && moduleInfoCard->isVisible())
        {
            hideModuleInfoCard();
            return true;
        }
        if (preferencesModal != nullptr)
        {
            preferencesModal.reset();
            return true;
        }
    }

    if (key == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0) ||
        key == juce::KeyPress ('Z', juce::ModifierKeys::commandModifier, 0))
    {
        processor.getHistoryManager().performUndo (processor);
        updatePresetUI();
        return true;
    }
    if (key == juce::KeyPress ('y', juce::ModifierKeys::commandModifier, 0) ||
        key == juce::KeyPress ('Y', juce::ModifierKeys::commandModifier, 0) ||
        key == juce::KeyPress ('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0) ||
        key == juce::KeyPress ('Z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0))
    {
        processor.getHistoryManager().performRedo (processor);
        updatePresetUI();
        return true;
    }

    return AudioProcessorEditor::keyPressed (key);
}

void PluginEditor::updateDspChainTitle()
{
    const juce::String tabNames[4] = { "MIC 1", "DESKTOP", "AUX 3", "AUX 4" };
    int idx = processor.getActiveChannelIndex();
    juce::String name = (idx >= 0 && idx < 4) ? tabNames[idx] : "CHANNEL " + juce::String (idx + 1);
    dspChainTitleLabel.setText (name + "  —  DSP CHAIN", juce::dontSendNotification);
}
