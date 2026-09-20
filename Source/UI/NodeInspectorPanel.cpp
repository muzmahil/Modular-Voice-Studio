#include "NodeInspectorPanel.h"
#include "NodeCanvas.h"
#include "SidebarComponent.h"
#include "../PluginProcessor.h"
#include "../UITheme.h"
#include "../Graph/ModuleFactory.h"
#include "../Graph/MixWrapperProcessor.h"
#include "../Localization.h"

//==============================================================================
NodeInspectorPanel::ParamRow::ParamRow (juce::AudioProcessorParameter* param)
    : parameter (param)
{
    nameLabel.setText (param->getName (32), juce::dontSendNotification);
    nameLabel.setFont (UITheme::getFont (11.0f));
    nameLabel.setColour (juce::Label::textColourId, UITheme::textPrimary);
    addAndMakeVisible (nameLabel);

    juce::String valText = param->getCurrentValueAsText();
    if (param->getLabel().isNotEmpty())
        valText += " " + param->getLabel();
    valueLabel.setText (valText, juce::dontSendNotification);
    valueLabel.setFont (UITheme::getFont (10.5f));
    valueLabel.setColour (juce::Label::textColourId, UITheme::textSecondary);
    valueLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (valueLabel);

    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRange (0.0, 1.0, 0.001);
    slider.setValue (param->getValue(), juce::dontSendNotification);
    slider.setColour (juce::Slider::trackColourId, UITheme::appleBlue);
    slider.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff252528));
    slider.setColour (juce::Slider::thumbColourId, UITheme::textPrimary);

    slider.onValueChange = [this]
    {
        if (parameter != nullptr)
        {
            parameter->setValueNotifyingHost ((float) slider.getValue());
            juce::String txt = parameter->getCurrentValueAsText();
            if (parameter->getLabel().isNotEmpty())
                txt += " " + parameter->getLabel();
            valueLabel.setText (txt, juce::dontSendNotification);
        }
    };
    addAndMakeVisible (slider);
}

void NodeInspectorPanel::ParamRow::paint (juce::Graphics& g)
{
    // Subtle row bottom hairline
    g.setColour (UITheme::strokeHairline.withAlpha (0.4f));
    g.drawLine (0.0f, (float) getHeight() - 0.5f, (float) getWidth(), (float) getHeight() - 0.5f, 1.0f);
}

void NodeInspectorPanel::ParamRow::resized()
{
    auto b = getLocalBounds().reduced (4, 2);
    auto topRow = b.removeFromTop (18);
    nameLabel.setBounds (topRow.removeFromLeft (topRow.getWidth() - 65));
    valueLabel.setBounds (topRow);
    slider.setBounds (b.removeFromBottom (16));
}

//==============================================================================
NodeInspectorPanel::NodeInspectorPanel (PluginProcessor& proc, NodeCanvas& canvas)
    : processor (proc), nodeCanvas (canvas)
{
    paramViewport.setViewedComponent (&paramContainer, false);
    paramViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (paramViewport);

    // Header Collapse Button [ › ]
    collapseBtn.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    collapseBtn.setColour (juce::TextButton::textColourOffId, UITheme::textSecondary);
    collapseBtn.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    collapseBtn.onClick = [this]
    {
        if (onCollapseRequested)
            onCollapseRequested();
    };
    addAndMakeVisible (collapseBtn);

    powerBtn.setButtonText (tr ("ACTIVE"));
    powerBtn.onClick = [this]
    {
        if (auto* node = processor.getGraph().getNodeForId (currentNodeID))
        {
            node->setBypassed (! node->isBypassed());
            powerBtn.setButtonText (node->isBypassed() ? tr ("BYPASS") : tr ("ACTIVE"));
            nodeCanvas.repaint();
            repaint();
        }
    };
    addAndMakeVisible (powerBtn);

    openWindowBtn.setButtonText (tr ("OPEN_UI"));
    openWindowBtn.onClick = [this]
    {
        nodeCanvas.openModuleEditor (currentNodeID);
    };
    addAndMakeVisible (openWindowBtn);

    deleteBtn.setButtonText (tr ("DELETE"));
    deleteBtn.onClick = [this]
    {
        if (currentNodeID.uid != 0)
        {
            auto id = currentNodeID;
            clearInspection();
            processor.removeModule (id);
        }
    };
    addAndMakeVisible (deleteBtn);

    LocalizationManager::instance().addListener (this);

    clearInspection();
}

NodeInspectorPanel::~NodeInspectorPanel()
{
    LocalizationManager::instance().removeListener (this);
}

void NodeInspectorPanel::localizationChanged()
{
    openWindowBtn.setButtonText (tr ("OPEN_UI"));
    deleteBtn.setButtonText (tr ("DELETE"));
    if (auto* node = processor.getGraph().getNodeForId (currentNodeID))
        powerBtn.setButtonText (node->isBypassed() ? tr ("BYPASS") : tr ("ACTIVE"));
    else
        powerBtn.setButtonText (tr ("ACTIVE"));
    repaint();
}

void NodeInspectorPanel::clearInspection()
{
    currentNodeID = {};
    paramRows.clear();
    powerBtn.setVisible (false);
    openWindowBtn.setVisible (false);
    deleteBtn.setVisible (false);
    paramViewport.setVisible (false);
    repaint();
}

void NodeInspectorPanel::inspectNode (juce::AudioProcessorGraph::NodeID nodeID)
{
    auto* node = processor.getGraph().getNodeForId (nodeID);
    if (node == nullptr)
    {
        clearInspection();
        return;
    }

    currentNodeID = nodeID;
    bool isInput = nodeID == processor.getAudioInputNodeID();
    bool isOutput = nodeID == processor.getAudioOutputNodeID();

    auto* proc = MixWrapperProcessor::getActualProcessor (node->getProcessor());

    powerBtn.setVisible (! (isInput || isOutput));
    powerBtn.setButtonText (node->isBypassed() ? "Bypassed" : "Active");
    openWindowBtn.setVisible (proc != nullptr && proc->hasEditor());
    deleteBtn.setVisible (! (isInput || isOutput));
    paramViewport.setVisible (true);

    rebuildParams();
    resized();
    repaint();
}

void NodeInspectorPanel::rebuildParams()
{
    paramRows.clear();

    if (auto* node = processor.getGraph().getNodeForId (currentNodeID))
    {
        if (auto* proc = MixWrapperProcessor::getActualProcessor (node->getProcessor()))
        {
            for (auto* p : proc->getParameters())
                paramRows.add (new ParamRow (p));
        }
    }

    for (auto* row : paramRows)
        paramContainer.addAndMakeVisible (row);

    int totalH = paramRows.size() * 44;
    paramContainer.setSize (paramViewport.getWidth() > 0 ? paramViewport.getWidth() - 10 : 220, totalH);

    for (int i = 0; i < paramRows.size(); ++i)
        paramRows[i]->setBounds (0, i * 44, paramContainer.getWidth(), 42);
}

void NodeInspectorPanel::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    // macOS Dark Source List background
    g.fillAll (UITheme::bgSidebar);

    // Left structural hairline divider
    g.setColour (UITheme::strokeHairline);
    g.drawLine (0.0f, 0.0f, 0.0f, bounds.getBottom(), 1.0f);

    // Top Header Keyline
    g.setColour (UITheme::specularRim);
    g.drawLine (1.0f, 0.5f, bounds.getRight(), 0.5f, 1.0f);

    auto* node = processor.getGraph().getNodeForId (currentNodeID);

    if (node == nullptr)
    {
        // Empty state placeholder
        g.setColour (UITheme::textTertiary);
        g.setFont (UITheme::getFont (12.0f));
        auto emptyArea = getLocalBounds().reduced (20);
        g.drawText (tr ("INSPECTOR").toUpperCase(), 32, 10, 120, 16, juce::Justification::centredLeft);

        g.setColour (UITheme::strokeHairline);
        g.drawLine (14.0f, 36.0f, (float) getWidth() - 14.0f, 36.0f, 1.0f);

        g.setColour (UITheme::textTertiary);
        g.setFont (UITheme::getFont (11.0f));
        g.drawFittedText (tr ("NO_MODULE_SELECTED") + "\n\n" + tr ("CLICK_NODE_TO_INSPECT"),
                          emptyArea.withTrimmedTop (80), juce::Justification::centred, 4);
        return;
    }

    // Header Title
    g.setColour (UITheme::textTertiary);
    g.setFont (UITheme::getFont (9.0f));
    g.drawText (tr ("INSPECTOR").toUpperCase(), 32, 10, 120, 14, juce::Justification::centredLeft);

    juce::String title = node->properties["type"].toString();
    if (node->nodeID == processor.getAudioInputNodeID())  title = tr ("AUDIO_IN");
    if (node->nodeID == processor.getAudioOutputNodeID()) title = tr ("AUDIO_OUT");

    // Module Name
    g.setColour (UITheme::textPrimary);
    g.setFont (UITheme::getFont (14.0f));
    g.drawText (tr (title), 14, 28, getWidth() - 80, 20, juce::Justification::centredLeft);

    // Category badge
    juce::String cat = ModuleFactory::instance().getCategory (node->properties["type"].toString()).toUpperCase();
    if (cat.isNotEmpty())
    {
        auto badgeArea = juce::Rectangle<float> ((float) getWidth() - 75.0f, 30.0f, 60.0f, 16.0f);
        g.setColour (juce::Colour (0x20ffffff));
        g.fillRoundedRectangle (badgeArea, 3.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (badgeArea, 3.0f, 1.0f);
        g.setColour (UITheme::textSecondary);
        g.setFont (UITheme::getFont (8.5f));
        g.drawText (cat, badgeArea, juce::Justification::centred);
    }

    g.setColour (UITheme::strokeHairline);
    g.drawLine (14.0f, 56.0f, (float) getWidth() - 14.0f, 56.0f, 1.0f);
}

void NodeInspectorPanel::resized()
{
    collapseBtn.setBounds (8, 8, 18, 18);

    if (currentNodeID.uid == 0)
    {
        powerBtn.setBounds (0, 0, 0, 0);
        openWindowBtn.setBounds (0, 0, 0, 0);
        deleteBtn.setBounds (0, 0, 0, 0);
        paramViewport.setBounds (0, 0, 0, 0);
        return;
    }

    int y = 66;
    int availableW = getWidth() - 28;
    int btnW = (availableW - 12) / 3;
    powerBtn.setBounds (14, y, btnW, 24);
    openWindowBtn.setBounds (14 + btnW + 6, y, btnW, 24);
    deleteBtn.setBounds (14 + (btnW + 6) * 2, y, btnW, 24);

    y += 32;

    int remainingH = getHeight() - y - 10;
    paramViewport.setBounds (12, y, getWidth() - 24, juce::jmax (50, remainingH));

    int totalH = paramRows.size() * 44;
    paramContainer.setSize (paramViewport.getWidth(), totalH);
    for (int i = 0; i < paramRows.size(); ++i)
        paramRows[i]->setBounds (0, i * 44, paramContainer.getWidth(), 42);
}
