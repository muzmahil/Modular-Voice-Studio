#include "ChannelStripComponent.h"
#if JucePlugin_Build_Standalone
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

ChannelStripComponent::ChannelStripComponent(int idx, const juce::String& defaultName, bool isPreAssigned, bool hasDefaultDSP)
    : channelIndex(idx), channelName(defaultName), isAssigned(isPreAssigned), hasDSP(hasDefaultDSP)
{
    // Mini Screen
    miniChainScreen.setChannelInfo(channelName, isAssigned, hasDSP);
    miniChainScreen.onOpenCanvasRequested = [this]()
    {
        if (onOpenCanvasRequested)
            onOpenCanvasRequested(channelIndex);
    };
    miniChainScreen.onBypassToggled = [this](bool bypassed)
    {
        if (onBypassChanged)
            onBypassChanged(channelIndex, bypassed);
    };
    addAndMakeVisible(miniChainScreen);

    // Source Selector
    sourceSelector.setLookAndFeel(&calmLnF);
    refreshSourceList();

    sourceSelector.onChange = [this]()
    {
        int id = sourceSelector.getSelectedId();
        if (id == 99)
        {
            if (onOpenSettingsRequested)
                onOpenSettingsRequested();
            return;
        }

        auto it = deviceMap.find(id);
        if (it != deviceMap.end())
        {
            const auto& entry = it->second;
            if (entry.deviceName.isNotEmpty())
            {
                if (channelIndex == 0)
                {
                   #if JucePlugin_Build_Standalone
                    if (auto* holder = juce::StandalonePluginHolder::getInstance())
                    {
                        auto currentSetup = holder->deviceManager.getAudioDeviceSetup();
                        if (entry.isInput)
                        {
                            currentSetup.inputDeviceName = entry.deviceName;
                            currentSetup.useDefaultInputChannels = true;
                        }
                        else
                        {
                            currentSetup.outputDeviceName = entry.deviceName;
                            currentSetup.useDefaultOutputChannels = true;
                        }
                        holder->deviceManager.setAudioDeviceSetup(currentSetup, true);
                    }
                   #endif
                }
                else
                {
                    if (onSourceSelected)
                        onSourceSelected(channelIndex, entry.deviceName);
                }

                juce::String prefix = (channelIndex == 0 ? "MIC 1" : channelIndex == 1 ? "DESKTOP" : "AUX " + juce::String(channelIndex + 1));
                setChannelAssigned(true, prefix);
            }
        }
    };
    addAndMakeVisible(sourceSelector);

    // Main Channel Fader (-60 dB to +18 dB)
    faderSlider.setLookAndFeel(&calmLnF);
    faderSlider.setSliderStyle(juce::Slider::LinearVertical);
    faderSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    faderSlider.setRange(-60.0, 18.0, 0.1);
    faderSlider.setValue(0.0);
    faderSlider.setDoubleClickReturnValue(true, 0.0);
    addAndMakeVisible(faderSlider);

    dbValueLabel.setText("0.0 dB", juce::dontSendNotification);
    dbValueLabel.setFont(juce::FontOptions(11.0f).withStyle("Bold"));
    dbValueLabel.setJustificationType(juce::Justification::centred);
    dbValueLabel.setColour(juce::Label::textColourId, CalmTheme::cyanSoft);
    addAndMakeVisible(dbValueLabel);

    faderSlider.onValueChange = [this]()
    {
        float val = static_cast<float>(faderSlider.getValue());
        if (val <= -59.5f)
            dbValueLabel.setText("-inf dB", juce::dontSendNotification);
        else
            dbValueLabel.setText(juce::String(val > 0.0f ? "+" : "") + juce::String(val, 1) + " dB", juce::dontSendNotification);
        if (onFaderChanged)
            onFaderChanged(channelIndex, val);
    };

    // Tactile Mute Button
    muteButton.setClickingTogglesState(true);
    muteButton.setColour(juce::TextButton::buttonColourId, CalmTheme::bgCardElevated);
    muteButton.setColour(juce::TextButton::buttonOnColourId, CalmTheme::coralRed);
    muteButton.setColour(juce::TextButton::textColourOffId, CalmTheme::textSecondary);
    muteButton.setColour(juce::TextButton::textColourOnId, CalmTheme::textPrimary);
    muteButton.onClick = [this]()
    {
        if (onMuteChanged)
            onMuteChanged(channelIndex, muteButton.getToggleState());
    };
    addAndMakeVisible(muteButton);

    // Assign Button for empty slots with rich context popup menu
    assignButton.setColour(juce::TextButton::buttonColourId, CalmTheme::bgCardElevated);
    assignButton.setColour(juce::TextButton::textColourOffId, CalmTheme::cyanSoft);
    assignButton.onClick = [this]()
    {
        juce::PopupMenu m;
        juce::StringArray inputDevices;
        juce::StringArray outputDevices;

       #if JucePlugin_Build_Standalone
        if (auto* holder = juce::StandalonePluginHolder::getInstance())
        {
            if (auto* currentType = holder->deviceManager.getCurrentDeviceTypeObject())
            {
                inputDevices = currentType->getDeviceNames(true);
                outputDevices = currentType->getDeviceNames(false);
            }
        }
       #endif

        int id = 1;
        std::map<int, DeviceEntry> popupMap;

        m.addSectionHeader("INPUT SOURCES (Capture & Mics):");
        if (inputDevices.size() > 0)
        {
            for (const auto& dev : inputDevices)
            {
                m.addItem(id, "IN: " + dev);
                popupMap[id] = { true, dev };
                id++;
            }
        }
        else
        {
            m.addItem(id, "IN: Microphone / Line In (MIC " + juce::String(channelIndex + 1) + ")");
            popupMap[id] = { true, "" };
            id++;
        }

        int desktopId = id++;
        m.addItem(desktopId, "Desktop Audio (VB-Cable / Loopback)");
        popupMap[desktopId] = { true, "Desktop Audio" };

        int stereoMixId = id++;
        m.addItem(stereoMixId, "Stereo Mix (Realtek Audio)");
        popupMap[stereoMixId] = { true, "Stereo Mix" };

        int cableOutId = id++;
        m.addItem(cableOutId, "Virtual CABLE Output");
        popupMap[cableOutId] = { true, "CABLE Output" };

        m.addSeparator();
        m.addSectionHeader("OUTPUT DESTINATIONS (Headphones / Busses):");
        int firstOutId = id;
        if (outputDevices.size() > 0)
        {
            for (const auto& dev : outputDevices)
            {
                m.addItem(id, "OUT: " + dev);
                popupMap[id] = { false, dev };
                id++;
            }
        }
        else
        {
            m.addItem(id, "OUT: Headphones / Main Speakers (A1)");
            popupMap[id] = { false, "" };
            id++;
        }

        int cableInId = id++;
        m.addItem(cableInId, "Virtual CABLE Input (Mic Feed)");
        popupMap[cableInId] = { false, "CABLE Input" };

        m.addSeparator();
        m.addItem(99, "Audio Engine Settings...");

        juce::Component::SafePointer<ChannelStripComponent> self (this);
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&assignButton),
            [self, desktopId, stereoMixId, cableOutId, firstOutId, cableInId, popupMap](int result)
            {
                if (self == nullptr)
                    return;

                if (result == 99)
                {
                    if (self->onOpenSettingsRequested)
                        self->onOpenSettingsRequested();
                    return;
                }
                if (result > 0)
                {
                    auto it = popupMap.find(result);
                    if (it != popupMap.end() && it->second.deviceName.isNotEmpty())
                    {
                        if (self->channelIndex == 0)
                        {
                           #if JucePlugin_Build_Standalone
                            if (auto* holder = juce::StandalonePluginHolder::getInstance())
                            {
                                auto currentSetup = holder->deviceManager.getAudioDeviceSetup();
                                if (it->second.isInput)
                                {
                                    currentSetup.inputDeviceName = it->second.deviceName;
                                    currentSetup.useDefaultInputChannels = true;
                                }
                                else
                                {
                                    currentSetup.outputDeviceName = it->second.deviceName;
                                    currentSetup.useDefaultOutputChannels = true;
                                }
                                auto err = holder->deviceManager.setAudioDeviceSetup(currentSetup, true);
                                if (err.isNotEmpty())
                                    DBG("Set audio device error: " + err);
                            }
                           #endif
                        }
                    }

                    juce::String assignedTitle;
                    if (result == desktopId)
                        assignedTitle = "DESKTOP";
                    else if (result == stereoMixId)
                        assignedTitle = "STEREO MIX";
                    else if (result == cableOutId)
                        assignedTitle = "VIRTUAL";
                    else if (result >= firstOutId && result < cableInId)
                        assignedTitle = "OUTPUT";
                    else if (result == cableInId)
                        assignedTitle = "CABLE IN";
                    else
                        assignedTitle = "MIC " + juce::String(self->channelIndex + 1);

                    self->setChannelAssigned(true, assignedTitle);
                    self->sourceSelector.setSelectedId(result, juce::dontSendNotification);

                    if (self->onAssignRequested)
                        self->onAssignRequested(self->channelIndex);
                }
            });
    };
    addChildComponent(assignButton);

    setChannelAssigned(isAssigned, channelName);
    startTimerHz(60);
}

void ChannelStripComponent::refreshSourceList()
{
    sourceSelector.clear();
    deviceMap.clear();

    juce::StringArray inputDevices;
    juce::StringArray outputDevices;

   #if JucePlugin_Build_Standalone
    if (auto* holder = juce::StandalonePluginHolder::getInstance())
    {
        if (auto* currentType = holder->deviceManager.getCurrentDeviceTypeObject())
        {
            inputDevices = currentType->getDeviceNames(true);
            outputDevices = currentType->getDeviceNames(false);
        }
    }
   #endif

    int itemId = 1;
    int defaultSelectedId = 1;

    // --- INPUT SOURCES ---
    sourceSelector.addSectionHeading("INPUT SOURCES (Capture & Mics)");
    if (inputDevices.size() > 0)
    {
        for (int i = 0; i < inputDevices.size(); ++i)
        {
            const auto& dev = inputDevices[i];
            sourceSelector.addItem("IN: " + dev, itemId);
            deviceMap[itemId] = { true, dev };

            if (channelIndex == 0 && i == 0)
                defaultSelectedId = itemId;
            else if (channelIndex == 1 && (dev.containsIgnoreCase("Stereo Mix") || dev.containsIgnoreCase("CABLE Output") || dev.containsIgnoreCase("What U Hear") || (i == 1 && defaultSelectedId == 1)))
                defaultSelectedId = itemId;
            else if (channelIndex == 2 && dev.containsIgnoreCase("CABLE-A") || dev.containsIgnoreCase("Line"))
                defaultSelectedId = itemId;

            itemId++;
        }
    }
    else
    {
        sourceSelector.addItem("IN: Default Microphone / Input", itemId);
        deviceMap[itemId] = { true, "" };
        defaultSelectedId = itemId;
        itemId++;
    }

    // --- SETTINGS ---
    sourceSelector.addSeparator();
    sourceSelector.addItem("Audio Engine Settings...", 99);

    sourceSelector.setSelectedId(defaultSelectedId, juce::dontSendNotification);
}

ChannelStripComponent::~ChannelStripComponent()
{
    stopTimer();
    faderSlider.setLookAndFeel(nullptr);
    sourceSelector.setLookAndFeel(nullptr);
}

void ChannelStripComponent::setChannelAssigned(bool assigned, const juce::String& name)
{
    isAssigned = assigned;
    if (name.isNotEmpty())
        channelName = name;

    miniChainScreen.setChannelInfo(channelName, isAssigned, hasDSP);
    
    sourceSelector.setVisible(isAssigned);
    faderSlider.setVisible(isAssigned);
    dbValueLabel.setVisible(isAssigned);
    muteButton.setVisible(isAssigned);

    assignButton.setVisible(!isAssigned);
    repaint();
}

void ChannelStripComponent::setDspNodes(const std::vector<juce::String>& nodes)
{
    miniChainScreen.setDspNodes(nodes);
}

void ChannelStripComponent::setAudioPeak(float leftPeak, float rightPeak)
{
    leftPeakLevel = juce::jmax(leftPeakLevel, leftPeak);
    rightPeakLevel = juce::jmax(rightPeakLevel, rightPeak);
}

void ChannelStripComponent::timerCallback()
{
    // Convert linear peak amplitude to audio dB with studio ballistic curve
    float rawPeak = juce::jmax(leftPeakLevel, rightPeakLevel);
    float db = (rawPeak > 0.00001f) ? juce::Decibels::gainToDecibels(rawPeak, -60.0f) : -60.0f;
    float targetLevel = MeterMath::dbToLevel (db);

    // Fast instant attack, smooth natural studio decay
    if (targetLevel > smoothedMeterLevel)
        smoothedMeterLevel = targetLevel;
    else
        smoothedMeterLevel = smoothedMeterLevel * 0.84f + targetLevel * 0.16f;

    // Peak hold indicator
    if (smoothedMeterLevel > peakHoldLevel)
    {
        peakHoldLevel = smoothedMeterLevel;
        peakHoldTimer = 35; // ~550ms at 60Hz
    }
    else if (peakHoldTimer > 0)
    {
        --peakHoldTimer;
    }
    else
    {
        peakHoldLevel = juce::jmax(0.0f, peakHoldLevel - 0.02f);
    }

    miniChainScreen.setAudioLevel(smoothedMeterLevel);

    leftPeakLevel *= 0.80f;
    rightPeakLevel *= 0.80f;

    if (isShowing() && isAssigned)
        repaint();
}

void ChannelStripComponent::resized()
{
    auto area = getLocalBounds().reduced(8, 8);

    // Channel Header Chip
    area.removeFromTop(26);
    area.removeFromTop(6);

    // Mini Chain Screen
    miniChainScreen.setBounds(area.removeFromTop(84));
    area.removeFromTop(8);

    if (!isAssigned)
    {
        assignButton.setBounds(area.reduced(12, 36));
        return;
    }

    // Source Selector
    sourceSelector.setBounds(area.removeFromTop(24));
    area.removeFromTop(8);

    // Bottom Mute Button
    muteButton.setBounds(area.removeFromBottom(28).reduced(6, 1));
    area.removeFromBottom(6);

    // dB value readout
    dbValueLabel.setBounds(area.removeFromBottom(18));
    area.removeFromBottom(4);

    // Fader & Meter Section
    faderSlider.setBounds(area.withTrimmedLeft(34));
}

void ChannelStripComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(2.0f);

    // Knox Surface Card Background
    g.setColour(CalmTheme::bgCard);
    g.fillRoundedRectangle(bounds, 9.0f);

    // 1px Subtle Border
    g.setColour(CalmTheme::borderSubtle);
    g.drawRoundedRectangle(bounds, 9.0f, 1.0f);

    // Header Chip
    auto headerArea = bounds.removeFromTop(28).reduced(6, 2);
    g.setColour(CalmTheme::bgCardElevated);
    g.fillRoundedRectangle(headerArea, 5.0f);
    g.setColour(CalmTheme::borderSubtle);
    g.drawRoundedRectangle(headerArea, 5.0f, 1.0f);

    g.setFont(juce::FontOptions(11.5f).withStyle("Bold"));
    g.setColour(channelIndex == 0 ? CalmTheme::cyanSoft :
                channelIndex == 1 ? CalmTheme::amberSoft :
                CalmTheme::textSecondary);
    g.drawText(channelName, headerArea, juce::Justification::centred, false);

    if (!isAssigned)
        return;

    // Segmented LED VU-Meter
    auto faderArea = faderSlider.getBounds().toFloat();
    float meterX = bounds.getX() + 8.0f;
    float meterY = faderArea.getY() + 4.0f;
    float meterW = 7.0f;
    float meterH = faderArea.getHeight() - 8.0f;

    // Recessed Meter Slot
    g.setColour(CalmTheme::bgSunken);
    g.fillRoundedRectangle(meterX - 1, meterY - 1, meterW + 2, meterH + 2, 2.0f);

    int numSegments = 20;
    float segSpacing = 1.0f;
    float segH = (meterH - (numSegments - 1) * segSpacing) / numSegments;

    int litCount = juce::roundToInt(smoothedMeterLevel * numSegments);
    int peakHoldIdx = juce::roundToInt(peakHoldLevel * numSegments) - 1;

    for (int s = 0; s < numSegments; ++s)
    {
        int segIdx = numSegments - 1 - s;
        float segY = meterY + s * (segH + segSpacing);

        float segTopLevel = static_cast<float>(segIdx + 1) / static_cast<float>(numSegments);
        float segTopDb = MeterMath::levelToDb (segTopLevel);

        juce::Colour segCol;
        if (segTopDb > 0.0f) // Red (> 0 dB)
            segCol = CalmTheme::coralRed;
        else if (segTopDb > -12.0f) // Amber (-12 dB to 0 dB)
            segCol = CalmTheme::amberSoft;
        else // Green (<= -12 dB)
            segCol = CalmTheme::sageGreen;

        bool isLit = (segIdx < litCount);
        bool isPeakHold = (segIdx == peakHoldIdx && peakHoldIdx >= 0);

        if (isLit)
        {
            g.setColour(segCol);
            g.fillRoundedRectangle(meterX + 0.5f, segY, meterW - 1.0f, segH, 1.0f);
        }
        else if (isPeakHold)
        {
            g.setColour(segCol.brighter(0.25f));
            g.fillRoundedRectangle(meterX + 0.5f, segY, meterW - 1.0f, segH, 1.0f);
        }
        else
        {
            g.setColour(segCol.withAlpha(0.10f));
            g.fillRoundedRectangle(meterX + 0.5f, segY, meterW - 1.0f, segH, 1.0f);
        }
    }

    // Calibrated dB Scale Markings
    float scaleX = meterX + meterW + 3.0f;
    g.setFont(juce::FontOptions(7.5f).withStyle("Bold"));
    g.setColour(CalmTheme::textMuted);

    struct ScaleMark { float dbVal; const char* txt; };
    ScaleMark marks[] = {
        {  18.0f, "+18" },
        {  12.0f, "+12" },
        {   6.0f, " +6" },
        {   0.0f, "  0" },
        {  -6.0f, " -6" },
        { -12.0f, "-12" },
        { -24.0f, "-24" },
        { -60.0f, "-inf" }
    };

    for (const auto& m : marks)
    {
        float relY = 1.0f - MeterMath::dbToLevel (m.dbVal);
        float yPos = meterY + relY * meterH;
        g.drawLine(scaleX, yPos, scaleX + 2.0f, yPos, 1.0f);
        g.drawText(m.txt, juce::Rectangle<float>(scaleX + 3.0f, yPos - 5.0f, 18.0f, 10.0f), juce::Justification::centredLeft, false);
    }
}
