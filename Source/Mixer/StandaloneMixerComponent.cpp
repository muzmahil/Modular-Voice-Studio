#include "StandaloneMixerComponent.h"
#include <BinaryData.h>

#if JucePlugin_Build_Standalone
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

StandaloneMixerComponent::StandaloneMixerComponent (PluginProcessor& proc)
    : processor (proc)
{
    // Minimal TopBar Menu Button
    menuBtn.setColour (juce::TextButton::buttonColourId, CalmTheme::bgCardElevated);
    menuBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0x1FFFFFFF));
    menuBtn.setColour (juce::TextButton::textColourOffId, CalmTheme::cyanSoft);
    menuBtn.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    addAndMakeVisible (menuBtn);

    menuBtn.onClick = [this]()
    {
        juce::PopupMenu m;
        m.setLookAndFeel (&calmLnF);
        m.addItem (1, "Reset DSP Chain to Blank (Default)", true, false);
        m.addSeparator();
        m.addItem (2, "Audio Device Settings...", true, false);
        m.addItem (3, "VB-CABLE Virtual Mic Guide...", true, false);
        m.addSeparator();
        m.addItem (4, "About Modular Voice Studio");
        m.addSeparator();
        m.addItem (5, "Exit");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&menuBtn),
            [this](int result)
            {
                if (result == 1)
                {
                    processor.resetAllToFactoryDefaults();
                    if (micStrip != nullptr)
                        micStrip->setDspNodes ({});

                    #if JucePlugin_Build_Standalone
                    if (auto* holder = juce::StandalonePluginHolder::getInstance())
                    {
                        holder->savePluginState();
                    }
                    #endif
                }
                else if (result == 2 && onOpenSettings)
                {
                    onOpenSettings();
                }
                else if (result == 3)
                {
                    juce::AlertWindow::showMessageBoxAsync (
                        juce::AlertWindow::InfoIcon,
                        "VB-CABLE Virtual Mic Guide",
                        "1. Select your physical microphone under 'MIC 1'.\n"
                        "2. In the 'VIRTUAL MIC (VB-CABLE)' section, ensure 'CABLE Input' is selected.\n"
                        "3. In Discord, OBS, or Games, select 'CABLE Output (VB-Audio Virtual Cable)' as your Microphone.\n"
                        "4. Your processed voice with all DSP effects will be sent directly to your stream/chat!",
                        "Got It");
                }
                else if (result == 4)
                {
                    juce::AlertWindow::showMessageBoxAsync (
                        juce::AlertWindow::InfoIcon,
                        "About Modular Voice Studio",
                        "Modular Voice Studio v2.0\nProfessional Modular Audio OS & Voice Processing Suite.\nZero-latency DSP matrix with VB-CABLE integration.",
                        "OK");
                }
                else if (result == 5)
                {
                    juce::JUCEApplicationBase::quit();
                }
            });
    };

    // Window control buttons
    auto setupWindowBtn = [this](juce::TextButton& btn, juce::Colour hoverCol = juce::Colour (0x1FFFFFFF))
    {
        btn.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        btn.setColour (juce::TextButton::buttonOnColourId, hoverCol);
        btn.setColour (juce::TextButton::textColourOffId, juce::Colour (0xFFB0B0B8));
        btn.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        addAndMakeVisible (btn);
    };

    setupWindowBtn (btnMin);
    setupWindowBtn (btnClose, CalmTheme::coralRed.withAlpha (0.6f));

    btnMin.onClick = [this] { if (auto* w = findParentComponentOfClass<juce::DocumentWindow>()) w->setMinimised (true); };
    btnClose.onClick = [this] { juce::JUCEApplicationBase::quit(); };

    // 1. Single Primary Voice Strip (MIC 1)
    micStrip = std::make_unique<ChannelStripComponent> (0, "MIC 1 (VOICE)", true, false);

    #if JucePlugin_Build_Standalone
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
    {
        holder->getMuteInputValue().setValue (false);
    }
    #endif

    micStrip->onFaderChanged = [this](int, float faderDb)
    {
        processor.getChannel (0).faderGainLinear.store (faderDb <= -59.0f ? 0.0f : juce::Decibels::decibelsToGain (faderDb));
    };
    micStrip->onMuteChanged = [this](int, bool muted)
    {
        processor.getChannel (0).muted.store (muted);
    };
    micStrip->onBypassChanged = [this](int, bool bypassed)
    {
        processor.getChannel (0).isBypassed.store (bypassed);
    };
    micStrip->onOpenCanvasRequested = [this](int)
    {
        processor.switchChannelGraph (0);
        if (onOpenModularCanvas)
            onOpenModularCanvas (0);
    };
    micStrip->onOpenSettingsRequested = [this]()
    {
        if (onOpenSettings)
            onOpenSettings();
    };
    addAndMakeVisible (*micStrip);

    // 2. VB-CABLE Virtual Mic Output Controls
    vbCableHeader.setText ("VIRTUAL MIC OUTPUT (VB-CABLE)", juce::dontSendNotification);
    vbCableHeader.setFont (juce::FontOptions (12.5f).withStyle ("Bold"));
    vbCableHeader.setColour (juce::Label::textColourId, CalmTheme::cyanSoft);
    addAndMakeVisible (vbCableHeader);

    vbCableDeviceBox.setLookAndFeel (&calmLnF);
    addAndMakeVisible (vbCableDeviceBox);

    vbCableFader.setLookAndFeel (&calmLnF);
    vbCableFader.setSliderStyle (juce::Slider::LinearHorizontal);
    vbCableFader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    vbCableFader.setRange (-60.0, 6.0, 0.1);
    vbCableFader.setValue (0.0);
    vbCableFader.onValueChange = [this]
    {
        float val = static_cast<float> (vbCableFader.getValue());
        if (val <= -59.5f)
            vbCableDbLabel.setText ("-inf dB", juce::dontSendNotification);
        else
            vbCableDbLabel.setText (juce::String (val > 0.0f ? "+" : "") + juce::String (val, 1) + " dB", juce::dontSendNotification);
    };
    addAndMakeVisible (vbCableFader);

    vbCableDbLabel.setText ("0.0 dB", juce::dontSendNotification);
    vbCableDbLabel.setFont (juce::FontOptions (11.0f).withStyle ("Bold"));
    vbCableDbLabel.setColour (juce::Label::textColourId, CalmTheme::textSecondary);
    addAndMakeVisible (vbCableDbLabel);

    vbCableMuteBtn.setClickingTogglesState (true);
    vbCableMuteBtn.setColour (juce::TextButton::buttonColourId, CalmTheme::bgCardElevated);
    vbCableMuteBtn.setColour (juce::TextButton::buttonOnColourId, CalmTheme::coralRed);
    vbCableMuteBtn.setColour (juce::TextButton::textColourOffId, CalmTheme::textSecondary);
    vbCableMuteBtn.setColour (juce::TextButton::textColourOnId, CalmTheme::textPrimary);
    vbCableMuteBtn.onClick = [this]
    {
        processor.getChannel (0).isBusB1Active.store (! vbCableMuteBtn.getToggleState());
    };
    addAndMakeVisible (vbCableMuteBtn);

    vbCableStatusLabel.setText ("Active Feed -> In Discord / OBS select 'CABLE Output'", juce::dontSendNotification);
    vbCableStatusLabel.setFont (juce::FontOptions (11.0f));
    vbCableStatusLabel.setColour (juce::Label::textColourId, CalmTheme::sageGreen);
    addAndMakeVisible (vbCableStatusLabel);

    // 3. Headphone Monitor (A1) Output Controls
    monitorHeader.setText ("HEADPHONE MONITOR (A1)", juce::dontSendNotification);
    monitorHeader.setFont (juce::FontOptions (12.5f).withStyle ("Bold"));
    monitorHeader.setColour (juce::Label::textColourId, CalmTheme::amberSoft);
    addAndMakeVisible (monitorHeader);

    monitorDeviceBox.setLookAndFeel (&calmLnF);
    addAndMakeVisible (monitorDeviceBox);

    monitorFader.setLookAndFeel (&calmLnF);
    monitorFader.setSliderStyle (juce::Slider::LinearHorizontal);
    monitorFader.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    monitorFader.setRange (-60.0, 6.0, 0.1);
    monitorFader.setValue (0.0);
    monitorFader.onValueChange = [this]
    {
        float val = static_cast<float> (monitorFader.getValue());
        if (val <= -59.5f)
            monitorDbLabel.setText ("-inf dB", juce::dontSendNotification);
        else
            monitorDbLabel.setText (juce::String (val > 0.0f ? "+" : "") + juce::String (val, 1) + " dB", juce::dontSendNotification);
    };
    addAndMakeVisible (monitorFader);

    monitorDbLabel.setText ("0.0 dB", juce::dontSendNotification);
    monitorDbLabel.setFont (juce::FontOptions (11.0f).withStyle ("Bold"));
    monitorDbLabel.setColour (juce::Label::textColourId, CalmTheme::textSecondary);
    addAndMakeVisible (monitorDbLabel);

    monitorEnableBtn.setColour (juce::ToggleButton::textColourId, CalmTheme::textPrimary);
    monitorEnableBtn.setColour (juce::ToggleButton::tickColourId, CalmTheme::amberSoft);
    monitorEnableBtn.setToggleState (false, juce::dontSendNotification);
    monitorEnableBtn.onClick = [this]
    {
        bool on = monitorEnableBtn.getToggleState();
        processor.getChannel (0).isMonitoringActive.store (on);
    };
    addAndMakeVisible (monitorEnableBtn);

    // 4. Action Buttons
    openCanvasBtn.setColour (juce::TextButton::buttonColourId, CalmTheme::cyanSoft.withAlpha (0.25f));
    openCanvasBtn.setColour (juce::TextButton::textColourOffId, CalmTheme::cyanSoft);
    openCanvasBtn.onClick = [this]
    {
        processor.switchChannelGraph (0);
        if (onOpenModularCanvas)
            onOpenModularCanvas (0);
    };
    addAndMakeVisible (openCanvasBtn);

    resetDspBtn.setColour (juce::TextButton::buttonColourId, CalmTheme::bgCardElevated);
    resetDspBtn.setColour (juce::TextButton::textColourOffId, CalmTheme::coralRed);
    resetDspBtn.onClick = [this]
    {
        processor.resetAllToFactoryDefaults();
        if (micStrip != nullptr)
            micStrip->setDspNodes ({});

        #if JucePlugin_Build_Standalone
        if (auto* holder = juce::StandalonePluginHolder::getInstance())
        {
            holder->savePluginState();
        }
        #endif
    };
    addAndMakeVisible (resetDspBtn);

    audioSettingsBtn.setColour (juce::TextButton::buttonColourId, CalmTheme::bgCardElevated);
    audioSettingsBtn.setColour (juce::TextButton::textColourOffId, CalmTheme::textSecondary);
    audioSettingsBtn.onClick = [this]
    {
        if (onOpenSettings)
            onOpenSettings();
    };
    addAndMakeVisible (audioSettingsBtn);

    // Refresh and auto-assign VB-CABLE
    refreshOutputDeviceLists();

    // Listen for graph updates
    processor.addChangeListener (this);
    processor.getChannel (0).addChangeListener (this);
    micStrip->setDspNodes (processor.getChannelModuleNames (0));

    startTimerHz (60);
}

StandaloneMixerComponent::~StandaloneMixerComponent()
{
    stopTimer();
    processor.removeChangeListener (this);
    processor.getChannel (0).removeChangeListener (this);

    menuBtn.setLookAndFeel (nullptr);
    vbCableDeviceBox.setLookAndFeel (nullptr);
    vbCableFader.setLookAndFeel (nullptr);
    monitorDeviceBox.setLookAndFeel (nullptr);
    monitorFader.setLookAndFeel (nullptr);
}

void StandaloneMixerComponent::refreshOutputDeviceLists()
{
    vbCableDeviceBox.clear();
    monitorDeviceBox.clear();

    juce::StringArray outNames;

    #if JucePlugin_Build_Standalone
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
    {
        if (auto* currentType = holder->deviceManager.getCurrentDeviceTypeObject())
        {
            outNames = currentType->getDeviceNames (false);
        }
    }
    #endif

    int cableDefaultId = 0;
    int monitorDefaultId = 0;
    int cableIdCounter = 1;
    int monitorIdCounter = 1;

    // 1. VB-CABLE: Only list Virtual Audio Devices (CABLE, Virtual, VB-Audio, VoiceMeeter, VAC)
    for (int i = 0; i < outNames.size(); ++i)
    {
        const auto& dev = outNames[i];
        bool isVirtual = dev.containsIgnoreCase ("CABLE") || dev.containsIgnoreCase ("Virtual")
                      || dev.containsIgnoreCase ("VB-Audio") || dev.containsIgnoreCase ("VoiceMeeter")
                      || dev.containsIgnoreCase ("VAC") || dev.containsIgnoreCase ("BlackHole");

        if (isVirtual)
        {
            vbCableDeviceBox.addItem (dev, cableIdCounter);
            if (dev.containsIgnoreCase ("CABLE Input") || cableDefaultId == 0)
                cableDefaultId = cableIdCounter;
            cableIdCounter++;
        }
    }

    if (vbCableDeviceBox.getNumItems() == 0)
    {
        vbCableDeviceBox.addItem ("CABLE Input (VB-Audio Virtual Cable)", 1);
        cableDefaultId = 1;
    }

    // 2. Headphone Monitor (A1): List physical playback devices (skip CABLE / Virtual)
    // Prioritize device names containing "Headphone", "Kulaklık", "Headset", "Earphone"
    for (int i = 0; i < outNames.size(); ++i)
    {
        const auto& dev = outNames[i];
        bool isVirtual = dev.containsIgnoreCase ("CABLE") || dev.containsIgnoreCase ("Virtual")
                      || dev.containsIgnoreCase ("VB-Audio") || dev.containsIgnoreCase ("VoiceMeeter")
                      || dev.containsIgnoreCase ("VAC") || dev.containsIgnoreCase ("BlackHole");

        if (! isVirtual)
        {
            monitorDeviceBox.addItem (dev, monitorIdCounter);
            bool isHeadphone = dev.containsIgnoreCase ("Headphone") || dev.containsIgnoreCase ("Kulaklık")
                            || dev.containsIgnoreCase ("Headset") || dev.containsIgnoreCase ("Earphone");
            if (isHeadphone || monitorDefaultId == 0)
            {
                if (isHeadphone)
                    monitorDefaultId = monitorIdCounter;
                else if (monitorDefaultId == 0)
                    monitorDefaultId = monitorIdCounter;
            }
            monitorIdCounter++;
        }
    }

    if (monitorDeviceBox.getNumItems() == 0)
    {
        for (int i = 0; i < outNames.size(); ++i)
            monitorDeviceBox.addItem (outNames[i], i + 1);
        monitorDefaultId = 1;
    }

    vbCableDeviceBox.setSelectedId (cableDefaultId > 0 ? cableDefaultId : 1, juce::dontSendNotification);
    monitorDeviceBox.setSelectedId (monitorDefaultId > 0 ? monitorDefaultId : 1, juce::dontSendNotification);

    // Wire VB-CABLE to output worker
    auto selectedCableName = vbCableDeviceBox.getText();
    if (selectedCableName.isNotEmpty() && processor.getOutputWorker (0) != nullptr)
    {
        processor.getOutputWorker (0)->setDevice (selectedCableName);
    }

    // Ensure physical audio device output is selected
    #if JucePlugin_Build_Standalone
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
    {
        auto currentSetup = holder->deviceManager.getAudioDeviceSetup();
        auto preferredMonitor = monitorDeviceBox.getText();
        if (preferredMonitor.isNotEmpty() && (currentSetup.outputDeviceName.isEmpty() || currentSetup.outputDeviceName.containsIgnoreCase ("CABLE")))
        {
            currentSetup.outputDeviceName = preferredMonitor;
            currentSetup.useDefaultOutputChannels = true;
            holder->deviceManager.setAudioDeviceSetup (currentSetup, true);
        }
    }
    #endif

    vbCableDeviceBox.onChange = [this]
    {
        auto name = vbCableDeviceBox.getText();
        if (name.isNotEmpty() && processor.getOutputWorker (0) != nullptr)
            processor.getOutputWorker (0)->setDevice (name);
    };

    monitorDeviceBox.onChange = [this]
    {
        #if JucePlugin_Build_Standalone
        if (auto* holder = juce::StandalonePluginHolder::getInstance())
        {
            auto currentSetup = holder->deviceManager.getAudioDeviceSetup();
            currentSetup.outputDeviceName = monitorDeviceBox.getText();
            currentSetup.useDefaultOutputChannels = true;
            holder->deviceManager.setAudioDeviceSetup (currentSetup, true);
        }
        #endif
    };
}

void StandaloneMixerComponent::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &processor || source == &processor.getChannel (0))
    {
        if (micStrip != nullptr)
            micStrip->setDspNodes (processor.getChannelModuleNames (0));
        repaint();
    }
}

void StandaloneMixerComponent::paint (juce::Graphics& g)
{
    // Studio Slate background
    g.fillAll (CalmTheme::bgApp);

    // Minimal TopBar
    g.setColour (CalmTheme::bgTopbar);
    g.fillRect (0, 0, getWidth(), 38);
    g.setColour (CalmTheme::borderSubtle);
    g.drawHorizontalLine (38, 0.0f, (float) getWidth());

    // Pro Studio Brand Title with Live Emerald Dot
    g.setColour (CalmTheme::cyanSoft);
    g.fillEllipse (14.0f, 15.0f, 8.0f, 8.0f);

    g.setColour (CalmTheme::textPrimary);
    g.setFont (juce::FontOptions (12.5f).withStyle ("Bold"));
    g.drawText ("MODULAR VOICE STUDIO", 28, 0, 160, 38, juce::Justification::centredLeft);

    // Cards background
    auto area = getLocalBounds().withTrimmedTop (46).reduced (14);
    int micWidth = 270;
    auto rightArea = area.withTrimmedLeft (micWidth + 14);

    // VB-Cable Card
    auto cableCardRect = rightArea.removeFromTop (160);
    g.setColour (CalmTheme::bgCard);
    g.fillRoundedRectangle (cableCardRect.toFloat(), 6.0f);
    g.setColour (CalmTheme::borderSubtle);
    g.drawRoundedRectangle (cableCardRect.toFloat(), 6.0f, 1.0f);

    // Draw VB-Cable Live Output LED Meter Bar
    auto cableMeterBounds = cableCardRect.withTrimmedLeft (cableCardRect.getWidth() - 18).reduced (4);
    g.setColour (CalmTheme::bgSunken);
    g.fillRoundedRectangle (cableMeterBounds.toFloat(), 3.0f);
    float cableMeterLevel = MeterMath::dbToLevel (juce::Decibels::gainToDecibels (vbCablePeak + 1e-6f, -60.0f));
    int litH = (int) (cableMeterBounds.getHeight() * juce::jlimit (0.0f, 1.0f, cableMeterLevel));
    if (litH > 0)
    {
        auto litRect = cableMeterBounds.withTrimmedTop (cableMeterBounds.getHeight() - litH);
        g.setColour (CalmTheme::cyanSoft);
        g.fillRoundedRectangle (litRect.toFloat(), 3.0f);
    }

    rightArea.removeFromTop (10);

    // Headphone Monitor Card
    auto monitorCardRect = rightArea.removeFromTop (160);
    g.setColour (CalmTheme::bgCard);
    g.fillRoundedRectangle (monitorCardRect.toFloat(), 6.0f);
    g.setColour (CalmTheme::borderSubtle);
    g.drawRoundedRectangle (monitorCardRect.toFloat(), 6.0f, 1.0f);

    // Draw Monitor Live Output LED Meter Bar
    auto monMeterBounds = monitorCardRect.withTrimmedLeft (monitorCardRect.getWidth() - 18).reduced (4);
    g.setColour (CalmTheme::bgSunken);
    g.fillRoundedRectangle (monMeterBounds.toFloat(), 3.0f);
    float monMeterLevel = MeterMath::dbToLevel (juce::Decibels::gainToDecibels (monitorPeak + 1e-6f, -60.0f));
    int monLitH = (int) (monMeterBounds.getHeight() * juce::jlimit (0.0f, 1.0f, monMeterLevel));
    if (monLitH > 0)
    {
        auto monLitRect = monMeterBounds.withTrimmedTop (monMeterBounds.getHeight() - monLitH);
        g.setColour (CalmTheme::amberSoft);
        g.fillRoundedRectangle (monLitRect.toFloat(), 3.0f);
    }

    rightArea.removeFromTop (10);

    // Action Card
    auto actionCardRect = rightArea;
    g.setColour (CalmTheme::bgCard);
    g.fillRoundedRectangle (actionCardRect.toFloat(), 6.0f);
    g.setColour (CalmTheme::borderSubtle);
    g.drawRoundedRectangle (actionCardRect.toFloat(), 6.0f, 1.0f);
}

void StandaloneMixerComponent::resized()
{
    // Minimal Topbar
    menuBtn.setBounds (194, 5, 75, 28);

    int rightEdge = getWidth() - 4;
    btnClose.setBounds (rightEdge - 32, 5, 30, 28);
    btnMin.setBounds (rightEdge - 66, 5, 30, 28);

    auto area = getLocalBounds().withTrimmedTop (46).reduced (14);
    int micWidth = 270;

    if (micStrip != nullptr)
        micStrip->setBounds (area.removeFromLeft (micWidth));

    area.removeFromLeft (14);

    // 1. VB-Cable Card Layout
    auto cableCard = area.removeFromTop (160).reduced (12);
    cableCard.removeFromRight (18); // space for meter bar
    vbCableHeader.setBounds (cableCard.removeFromTop (22));
    cableCard.removeFromTop (4);
    vbCableDeviceBox.setBounds (cableCard.removeFromTop (26));
    cableCard.removeFromTop (6);
    auto faderRow = cableCard.removeFromTop (26);
    vbCableMuteBtn.setBounds (faderRow.removeFromLeft (54));
    faderRow.removeFromLeft (8);
    vbCableDbLabel.setBounds (faderRow.removeFromRight (50));
    vbCableFader.setBounds (faderRow);
    cableCard.removeFromTop (4);
    vbCableStatusLabel.setBounds (cableCard.removeFromTop (18));

    area.removeFromTop (10);

    // 2. Headphone Monitor Card Layout
    auto monitorCard = area.removeFromTop (160).reduced (12);
    monitorCard.removeFromRight (18); // space for meter bar
    monitorHeader.setBounds (monitorCard.removeFromTop (22));
    monitorCard.removeFromTop (4);
    monitorDeviceBox.setBounds (monitorCard.removeFromTop (26));
    monitorCard.removeFromTop (6);
    auto monFaderRow = monitorCard.removeFromTop (26);
    monitorEnableBtn.setBounds (monFaderRow.removeFromLeft (260));
    monitorDbLabel.setBounds (monFaderRow.removeFromRight (50));
    monitorFader.setBounds (monFaderRow);

    area.removeFromTop (10);

    // 3. Actions Card Layout
    auto actionCard = area.reduced (12);
    openCanvasBtn.setBounds (actionCard.removeFromTop (36));
    actionCard.removeFromTop (6);
    auto subBtns = actionCard.removeFromTop (30);
    resetDspBtn.setBounds (subBtns.removeFromLeft ((subBtns.getWidth() - 8) / 2));
    subBtns.removeFromLeft (8);
    audioSettingsBtn.setBounds (subBtns);
}

void StandaloneMixerComponent::timerCallback()
{
    updateAudioMeters();
}

void StandaloneMixerComponent::updateAudioMeters()
{
    float inL = processor.getChannel (0).inLevel.load();
    float outL = processor.getChannel (0).outLevel.load();

    if (micStrip != nullptr)
        micStrip->setAudioPeak (inL, outL);

    vbCablePeak = processor.getChannel (0).isBusB1Active.load() ? outL : 0.0f;
    monitorPeak = processor.getChannel (0).isMonitoringActive.load() ? outL : 0.0f;

    repaint();
}

void StandaloneMixerComponent::mouseDown (const juce::MouseEvent& e)
{
    if (e.y <= 38)
        windowDragger.startDraggingComponent (findParentComponentOfClass<juce::DocumentWindow>(), e);
}

void StandaloneMixerComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (e.y <= 38)
        windowDragger.dragComponent (findParentComponentOfClass<juce::DocumentWindow>(), e, nullptr);
}
