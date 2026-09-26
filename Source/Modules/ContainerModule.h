#pragma once
#include "../Graph/ModuleProcessor.h"
#include "../Graph/ModuleFactory.h"
#include "../UITheme.h"
#include "../Localization.h"
#include <vector>
#include <memory>

class ContainerModuleEditor;

/**
    ContainerModule (Packed Nodes / Sub-Rack Container).
    Encapsulates an ordered series of inner ModuleProcessors inside a single node.
    Provides custom container labeling, overall wet/dry mix dial, master bypass,
    and complete serialization to .mvs presets.
*/
class ContainerModule : public ModuleProcessor
{
public:
    struct InnerModule
    {
        juce::String typeId;
        bool isBypassed = false;
        std::unique_ptr<ModuleProcessor> processor;
    };

    ContainerModule()
        : ModuleProcessor ("Container", createLayout())
    {
        mixParam = getRawParam ("mix");
    }

    ~ContainerModule() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override
    {
        currentSampleRate = sampleRate;
        currentBlockSize = samplesPerBlock;
        const juce::ScopedLock sl (processLock);
        for (auto& mod : innerModules)
        {
            if (mod.processor != nullptr)
                mod.processor->prepareToPlay (sampleRate, samplesPerBlock);
        }
    }

    void releaseResources() override
    {
        const juce::ScopedLock sl (processLock);
        for (auto& mod : innerModules)
        {
            if (mod.processor != nullptr)
                mod.processor->releaseResources();
        }
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();
        if (numChannels == 0 || numSamples == 0) return;

        float mix = mixParam != nullptr ? mixParam->load() : 1.0f;

        if (mix <= 0.001f)
            return; // 100% dry passthrough

        const juce::ScopedLock sl (processLock);

        if (innerModules.empty())
            return;

        // If mix < 100%, preserve dry copy
        juce::AudioBuffer<float> dryCopy;
        if (mix < 0.999f)
            dryCopy.makeCopyOf (buffer, true);

        // Process sequentially through each active inner module
        for (auto& mod : innerModules)
        {
            if (mod.processor != nullptr && ! mod.isBypassed)
            {
                mod.processor->processBlock (buffer, midi);
            }
        }

        // Apply Wet/Dry blend
        if (mix < 0.999f)
        {
            float wet = mix;
            float dry = 1.0f - mix;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                buffer.applyGain (ch, 0, numSamples, wet);
                buffer.addFrom (ch, 0, dryCopy, ch, 0, numSamples, dry);
            }
        }
    }

    void addInnerModule (const juce::String& typeId)
    {
        auto newProc = ModuleFactory::instance().create (typeId);
        if (newProc != nullptr)
        {
            if (currentSampleRate > 0.0)
                newProc->prepareToPlay (currentSampleRate, currentBlockSize);

            const juce::ScopedLock sl (processLock);
            innerModules.push_back ({ typeId, false, std::move (newProc) });
        }
    }

    void adoptProcessor (const juce::String& typeId, std::unique_ptr<ModuleProcessor> proc, bool bypassed = false)
    {
        if (proc != nullptr)
        {
            if (currentSampleRate > 0.0)
                proc->prepareToPlay (currentSampleRate, currentBlockSize);

            const juce::ScopedLock sl (processLock);
            innerModules.push_back ({ typeId, bypassed, std::move (proc) });
        }
    }

    void removeInnerModule (int index)
    {
        const juce::ScopedLock sl (processLock);
        if (index >= 0 && index < (int) innerModules.size())
            innerModules.erase (innerModules.begin() + index);
    }

    void moveInnerModuleUp (int index)
    {
        const juce::ScopedLock sl (processLock);
        if (index > 0 && index < (int) innerModules.size())
            std::swap (innerModules[(size_t) index], innerModules[(size_t) (index - 1)]);
    }

    void moveInnerModuleDown (int index)
    {
        const juce::ScopedLock sl (processLock);
        if (index >= 0 && index + 1 < (int) innerModules.size())
            std::swap (innerModules[(size_t) index], innerModules[(size_t) (index + 1)]);
    }

    void moveInnerModuleTo (int fromIndex, int toIndex)
    {
        const juce::ScopedLock sl (processLock);
        if (fromIndex < 0 || fromIndex >= (int) innerModules.size()) return;
        if (toIndex < 0 || toIndex >= (int) innerModules.size()) return;
        if (fromIndex == toIndex) return;

        auto item = std::move (innerModules[(size_t) fromIndex]);
        innerModules.erase (innerModules.begin() + fromIndex);
        innerModules.insert (innerModules.begin() + toIndex, std::move (item));
    }

    std::function<void(int innerIndex)> onExtractRequested;

    int getNumInnerModules() const
    {
        const juce::ScopedLock sl (processLock);
        return (int) innerModules.size();
    }

    InnerModule* getInnerModule (int index)
    {
        const juce::ScopedLock sl (processLock);
        if (index >= 0 && index < (int) innerModules.size())
            return &innerModules[(size_t) index];
        return nullptr;
    }

    juce::String getCustomLabel() const { return customLabel.isNotEmpty() ? customLabel : name; }
    void setCustomLabel (const juce::String& newLabel) { customLabel = newLabel; }

    juce::AudioProcessorEditor* createEditor() override;

    // --- State Serialization ---
    void getStateInformation (juce::MemoryBlock& destData) override
    {
        auto xml = apvts.copyState().createXml();
        if (xml == nullptr)
            xml = std::make_unique<juce::XmlElement> ("CONTAINER_STATE");

        xml->setAttribute ("customLabel", customLabel);

        auto* innerXml = xml->createNewChildElement ("INNER_MODULES");
        const juce::ScopedLock sl (processLock);
        for (const auto& mod : innerModules)
        {
            if (mod.processor != nullptr)
            {
                auto* modXml = innerXml->createNewChildElement ("MODULE");
                modXml->setAttribute ("typeId", mod.typeId);
                modXml->setAttribute ("bypassed", mod.isBypassed);
                
                juce::MemoryBlock modState;
                mod.processor->getStateInformation (modState);
                if (modState.getSize() > 0)
                    modXml->setAttribute ("stateData", modState.toBase64Encoding());
            }
        }

        copyXmlToBinary (*xml, destData);
    }

    void setStateInformation (const void* data, int sizeInBytes) override
    {
        if (auto xml = getXmlFromBinary (data, sizeInBytes))
        {
            customLabel = xml->getStringAttribute ("customLabel", "Container");
            
            auto paramsTree = xml->getChildByName (apvts.state.getType());
            if (paramsTree != nullptr)
                apvts.replaceState (juce::ValueTree::fromXml (*paramsTree));

            if (auto* innerXml = xml->getChildByName ("INNER_MODULES"))
            {
                const juce::ScopedLock sl (processLock);
                innerModules.clear();

                for (auto* modXml : innerXml->getChildIterator())
                {
                    if (modXml->hasTagName ("MODULE"))
                    {
                        auto typeId = modXml->getStringAttribute ("typeId");
                        bool bypassed = modXml->getBoolAttribute ("bypassed", false);
                        auto stateB64 = modXml->getStringAttribute ("stateData");

                        auto proc = ModuleFactory::instance().create (typeId);
                        if (proc != nullptr)
                        {
                            if (currentSampleRate > 0.0)
                                proc->prepareToPlay (currentSampleRate, currentBlockSize);

                            if (stateB64.isNotEmpty())
                            {
                                juce::MemoryBlock modState;
                                if (modState.fromBase64Encoding (stateB64))
                                    proc->setStateInformation (modState.getData(), (int) modState.getSize());
                            }

                            innerModules.push_back ({ typeId, bypassed, std::move (proc) });
                        }
                    }
                }
            }
        }
    }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        return {
            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "mix", 1 },
                "Mix",
                juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f),
                1.0f,
                juce::AudioParameterFloatAttributes().withLabel ("%")
            )
        };
    }

    std::atomic<float>* mixParam = nullptr;
    juce::String customLabel { "Container" };
    double currentSampleRate = 48000.0;
    int currentBlockSize = 256;

    juce::CriticalSection processLock;
    std::vector<InnerModule> innerModules;
};

class InnerModuleWindow : public juce::DocumentWindow
{
public:
    InnerModuleWindow (const juce::String& name, juce::Colour bg, int buttons)
        : juce::DocumentWindow (name, bg, buttons)
    {}

    void closeButtonPressed() override
    {
        delete this;
    }
};

/**
    Modern Apple Pro Sub-Rack Editor Window for ContainerModule.
*/
class ContainerModuleEditor : public juce::AudioProcessorEditor,
                              public juce::TextEditor::Listener,
                              public juce::DragAndDropContainer
{
public:
    ContainerModuleEditor (ContainerModule& proc, juce::AudioProcessorValueTreeState& vts)
        : AudioProcessorEditor (&proc), container (proc), apvtsRef (vts)
    {
        // Container Label Editor
        nameEditor.setText (container.getCustomLabel(), juce::dontSendNotification);
        nameEditor.setFont (UITheme::getFont (12.0f, true));
        nameEditor.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff1e1e24));
        nameEditor.setColour (juce::TextEditor::outlineColourId, UITheme::strokeHairline);
        nameEditor.setColour (juce::TextEditor::textColourId, UITheme::textPrimary);
        nameEditor.addListener (this);
        addAndMakeVisible (nameEditor);

        // Mix Slider
        mixSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        mixSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 55, 18);
        mixSlider.setColour (juce::Slider::rotarySliderFillColourId, UITheme::appleGreen);
        mixSlider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff1e1e24));
        mixSlider.setColour (juce::Slider::textBoxOutlineColourId, UITheme::strokeHairline);
        mixSlider.setColour (juce::Slider::textBoxTextColourId, UITheme::textPrimary);
        addAndMakeVisible (mixSlider);
        mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvtsRef, "mix", mixSlider);

        // Rack Viewport
        rackViewport.setViewedComponent (&rackContainer, false);
        rackViewport.setScrollBarsShown (true, false);
        addAndMakeVisible (rackViewport);

        rebuildRackList();
        setSize (440, 440);
    }

    ~ContainerModuleEditor() override = default;

    void textEditorTextChanged (juce::TextEditor&) override
    {
        container.setCustomLabel (nameEditor.getText());
    }

    void rebuildRackList()
    {
        rackContainer.removeAllChildren();
        rackRows.clear();

        int num = container.getNumInnerModules();
        int y = 4;
        int rowH = 42;
        int w = 390;

        for (int i = 0; i < num; ++i)
        {
            auto* mod = container.getInnerModule (i);
            if (mod == nullptr) continue;

            auto* bgComp = new InnerModuleRowComponent (*this, container, i, mod->typeId, mod->isBypassed, mod->processor.get());
            bgComp->setBounds (0, y, w, rowH);
            rackContainer.addAndMakeVisible (bgComp);
            rackRows.add (bgComp);
            y += rowH + 6;
        }

        rackContainer.setSize (w, juce::jmax (220, y + 10));
        resized();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff141418));

        // Header Panel
        auto header = getLocalBounds().removeFromTop (74).toFloat();
        g.setColour (UITheme::cardHeader);
        g.fillRect (header);
        g.setColour (UITheme::strokeHairline);
        g.drawLine (0.0f, header.getBottom(), (float) getWidth(), header.getBottom(), 1.0f);

        // Header Labels
        g.setColour (UITheme::textSecondary);
        g.setFont (UITheme::getFont (10.0f, true));
        bool isTr = LocalizationManager::instance().getLanguage() == Language::Turkish;
        g.drawText (isTr ? juce::String (juce::CharPointer_UTF8 ("PAKET \xc4\xb0SM\xc4\xb0")) : "CONTAINER NAME",
                    14, 10, 150, 14, juce::Justification::centredLeft);
        g.drawText (isTr ? juce::String (juce::CharPointer_UTF8 ("GENEL M\xc4\xb0X")) : "MASTER MIX",
                    getWidth() - 95, 10, 80, 14, juce::Justification::centred);

        // Rack Section Title
        g.setColour (UITheme::textPrimary);
        g.setFont (UITheme::getFont (12.5f, true));
        juce::String chainTitle = isTr ? juce::String (juce::CharPointer_UTF8 ("Z\xc4\xb0NC\xc4\xb0R MOD\xc3\x9cLLER\xc4\xb0 (S\xc3\xbcr\xc3\xbckleyerek S\xc4\xb1rala)"))
                                       : "CHAINED MODULES (Drag to Reorder)";
        g.drawText (chainTitle, 14, 84, getWidth() - 28, 18, juce::Justification::centredLeft);
    }

    void resized() override
    {
        nameEditor.setBounds (14, 28, getWidth() - 130, 26);
        mixSlider.setBounds (getWidth() - 95, 18, 75, 52);

        rackViewport.setBounds (14, 108, getWidth() - 28, getHeight() - 118);
    }

private:
    struct InnerModuleRowComponent : public juce::Component,
                                     public juce::DragAndDropTarget
    {
        InnerModuleRowComponent (ContainerModuleEditor& ed, ContainerModule& cont, int idx, const juce::String& type, bool bypassed, ModuleProcessor* proc)
            : editor (ed), container (cont), index (idx), moduleType (type), isBypassed (bypassed), processor (proc)
        {
            bool isTr = LocalizationManager::instance().getLanguage() == Language::Turkish;

            bypassBtn.setButtonText (isBypassed ? "BYP" : "ON");
            bypassBtn.setColour (juce::TextButton::buttonColourId, isBypassed ? juce::Colour (0xff333339) : UITheme::appleBlue);
            bypassBtn.onClick = [this]
            {
                if (auto* m = container.getInnerModule (index))
                {
                    m->isBypassed = ! m->isBypassed;
                    isBypassed = m->isBypassed;
                    bypassBtn.setButtonText (isBypassed ? "BYP" : "ON");
                    bypassBtn.setColour (juce::TextButton::buttonColourId, isBypassed ? juce::Colour (0xff333339) : UITheme::appleBlue);
                    repaint();
                }
            };
            addAndMakeVisible (bypassBtn);

            editBtn.setButtonText ("Open UI");
            editBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0x28ffffff));
            editBtn.onClick = [this]
            {
                if (processor != nullptr)
                {
                    auto* win = new InnerModuleWindow (
                        moduleType + " (" + container.getCustomLabel() + ")",
                        juce::Colour (0xff1a1a20),
                        juce::DocumentWindow::closeButton);
                    win->setUsingNativeTitleBar (true);
                    win->setContentOwned (processor->createEditorIfNeeded(), true);
                    win->setResizable (false, false);
                    win->centreWithSize (win->getWidth(), win->getHeight());
                    win->setVisible (true);
                }
            };
            addAndMakeVisible (editBtn);

            // Extract to Canvas button
            extractBtn.setButtonText (juce::CharPointer_UTF8 ("\xe2\x86\x97")); // ↗
            extractBtn.setTooltip (isTr ? juce::String (juce::CharPointer_UTF8 ("Tuvale \xc3\x87\xc4\xb1kar (Extract to Canvas)")) : "Extract to Canvas");
            extractBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0x220a84ff));
            extractBtn.setColour (juce::TextButton::textColourOffId, UITheme::appleBlue);
            extractBtn.onClick = [this]
            {
                int idx = index;
                juce::Component::SafePointer<ContainerModuleEditor> safeEditor (&editor);
                juce::MessageManager::callAsync ([&cont = this->container, safeEditor, idx] {
                    if (cont.onExtractRequested != nullptr)
                        cont.onExtractRequested (idx);
                    if (safeEditor != nullptr)
                        safeEditor->rebuildRackList();
                });
            };
            addAndMakeVisible (extractBtn);

            deleteBtn.setButtonText (juce::CharPointer_UTF8 ("\xe2\x9c\x95")); // ✕
            deleteBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0x22ff3b30));
            deleteBtn.setColour (juce::TextButton::textColourOffId, UITheme::appleRed);
            deleteBtn.onClick = [this]
            {
                int idx = index;
                juce::Component::SafePointer<ContainerModuleEditor> safeEditor (&editor);
                juce::MessageManager::callAsync ([&cont = this->container, safeEditor, idx] {
                    cont.removeInnerModule (idx);
                    if (safeEditor != nullptr)
                        safeEditor->rebuildRackList();
                });
            };
            addAndMakeVisible (deleteBtn);
        }

        bool isInterestedInDragSource (const SourceDetails& dragSourceDetails) override
        {
            return dragSourceDetails.description.toString().startsWith ("rack_idx:");
        }

        void itemDragEnter (const SourceDetails&) override
        {
            isDragOver = true;
            repaint();
        }

        void itemDragExit (const SourceDetails&) override
        {
            isDragOver = false;
            repaint();
        }

        void itemDropped (const SourceDetails& dragSourceDetails) override
        {
            isDragOver = false;
            auto desc = dragSourceDetails.description.toString();
            if (desc.startsWith ("rack_idx:"))
            {
                int srcIdx = desc.substring (9).getIntValue();
                if (srcIdx != index)
                {
                    container.moveInnerModuleTo (srcIdx, index);
                    editor.rebuildRackList();
                }
            }
        }

        void mouseDrag (const juce::MouseEvent&) override
        {
            if (auto* dragContainer = juce::DragAndDropContainer::findParentDragContainerFor (this))
            {
                if (! dragContainer->isDragAndDropActive())
                {
                    juce::Image dragImage (juce::Image::ARGB, getWidth(), getHeight(), true);
                    {
                        juce::Graphics g (dragImage);
                        g.fillAll (juce::Colour (0xff2c2c34));
                        g.setColour (UITheme::appleBlue);
                        g.drawRoundedRectangle (getLocalBounds().toFloat(), 4.0f, 1.5f);
                        g.setColour (UITheme::textPrimary);
                        g.setFont (UITheme::getFont (12.0f, true));
                        g.drawText (moduleType, 16, 0, getWidth() - 32, getHeight(), juce::Justification::centredLeft);
                    }
                    dragContainer->startDragging ("rack_idx:" + juce::String (index), this, juce::ScaledImage (dragImage), true);
                }
            }
        }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat();
            g.setColour (isDragOver ? juce::Colour (0xff282834) : juce::Colour (0xff202026));
            g.fillRoundedRectangle (b, 4.0f);
            g.setColour (isDragOver ? UITheme::appleBlue : UITheme::strokeHairline);
            g.drawRoundedRectangle (b, 4.0f, isDragOver ? 1.5f : 1.0f);

            // Left Grip Handle (≡) for drag-reorder
            g.setColour (UITheme::textTertiary);
            float gripX = 8.0f;
            float cy = b.getCentreY();
            g.drawLine (gripX, cy - 4.0f, gripX + 10.0f, cy - 4.0f, 1.4f);
            g.drawLine (gripX, cy,        gripX + 10.0f, cy,        1.4f);
            g.drawLine (gripX, cy + 4.0f, gripX + 10.0f, cy + 4.0f, 1.4f);

            // Index badge
            g.setFont (UITheme::getFont (10.0f, true));
            g.drawText (juce::String (index + 1), juce::Rectangle<float> (22.0f, 0.0f, 18.0f, b.getHeight()), juce::Justification::centred);

            // Module Title
            g.setColour (isBypassed ? UITheme::textTertiary : UITheme::textPrimary);
            g.setFont (UITheme::getFont (12.0f, true));
            g.drawText (moduleType, juce::Rectangle<float> (44.0f, 0.0f, 130.0f, b.getHeight()), juce::Justification::centredLeft);
        }

        void resized() override
        {
            int r = getWidth() - 6;
            deleteBtn.setBounds (r - 24, 9, 24, 24);
            r -= 28;
            extractBtn.setBounds (r - 24, 9, 24, 24);
            r -= 28;
            editBtn.setBounds (r - 58, 9, 54, 24);
            r -= 62;
            bypassBtn.setBounds (r - 38, 9, 34, 24);
        }

        ContainerModuleEditor& editor;
        ContainerModule& container;
        int index;
        juce::String moduleType;
        bool isBypassed;
        ModuleProcessor* processor;
        bool isDragOver = false;

        juce::TextButton bypassBtn;
        juce::TextButton editBtn;
        juce::TextButton extractBtn;
        juce::TextButton deleteBtn;
    };

    ContainerModule& container;
    juce::AudioProcessorValueTreeState& apvtsRef;
    juce::TextEditor nameEditor;
    juce::Slider mixSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAttachment;

    juce::Viewport rackViewport;
    juce::Component rackContainer;
    juce::OwnedArray<juce::Component> rackRows;
};

inline juce::AudioProcessorEditor* ContainerModule::createEditor()
{
    return new ContainerModuleEditor (*this, apvts);
}
