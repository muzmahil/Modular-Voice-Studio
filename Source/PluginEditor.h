#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_opengl/juce_opengl.h>
#include "PluginProcessor.h"
#include "UI/NodeCanvas.h"
#include "UI/SidebarComponent.h"
#include "UI/NodeInspectorPanel.h"
#include "UI/CanvasMinimap.h"
#include "UI/GlobalOscilloscopeHUD.h"
#include "UI/PreferencesModal.h"
#include "UI/SplashScreenOverlay.h"
#include "UI/ModuleInfoCard.h"
#include "UITheme.h"
#include "Localization.h"
#include "Mixer/StandaloneMixerComponent.h"

class PluginEditor : public juce::AudioProcessorEditor, 
                     public juce::DragAndDropContainer, 
                     public LocalizationManager::Listener,
                     public juce::ChangeListener,
                     private juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void localizationChanged() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    bool keyPressed (const juce::KeyPress& key) override;
    void parentHierarchyChanged() override;
    void refreshCanvasView();

private:
    PluginProcessor& processor;
    NodeCanvas canvas; // large logical grid; panned/zoomed via its own transform, not resized to fit
    SidebarComponent sidebar;
    NodeInspectorPanel inspector;
    CanvasMinimap minimap;
    GlobalOscilloscopeHUD oscilloscopeHud;

    juce::OpenGLContext openGLContext; // reduces tearing/stutter from software-only compositing on Windows

    struct CanvasContainerComponent : public juce::Component, public juce::DragAndDropTarget
    {
        CanvasContainerComponent (NodeCanvas& c) : canvas (c) {}

        bool isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails& details) override
        {
            return details.description.toString().startsWith ("module:");
        }

        void itemDropped (const juce::DragAndDropTarget::SourceDetails& details) override
        {
            juce::String type = details.description.toString().substring (7);
            auto canvasPos = canvas.getLocalPoint (this, details.localPosition);
            canvas.dropModule (type, canvasPos);
        }

        NodeCanvas& canvas;
    };

    CanvasContainerComponent canvasContainer;

    CustomLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow { this, 400 };

    // -- top bar --------------------------------------------------------
    static constexpr int topBarHeight = 46;

    PowerButton powerButton;
    juce::ComboBox presetBox;
    juce::TextButton prevPresetBtn { "<" }, nextPresetBtn { ">" };
    juce::TextButton loadPresetBtn { "Load" };
    juce::TextButton savePresetBtn { "Save" };
    juce::TextButton saveAsPresetBtn { "Save As" };
    juce::TextButton undoBtn { "Undo" }, redoBtn { "Redo" };
    juce::TextButton settingsBtn { "Settings" };

    // Interactive Sidebar Splitter
    struct SidebarSplitter : public juce::Component
    {
        SidebarSplitter (std::function<void(int)> onDragDelta)
            : dragCallback (onDragDelta)
        {
            setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
        }

        void mouseEnter (const juce::MouseEvent&) override { isHovered = true; repaint(); }
        void mouseExit (const juce::MouseEvent&) override  { isHovered = false; repaint(); }
        void mouseDown (const juce::MouseEvent& e) override { dragStartX = e.getScreenX(); }
        void mouseDrag (const juce::MouseEvent& e) override
        {
            int currentScreenX = e.getScreenX();
            int delta = currentScreenX - dragStartX;
            dragStartX = currentScreenX;
            if (dragCallback)
                dragCallback (delta);
        }

        void paint (juce::Graphics& g) override
        {
            if (isHovered)
            {
                g.setColour (UITheme::appleBlue.withAlpha (0.6f));
                g.fillAll();
            }
        }

        std::function<void(int)> dragCallback;
        int dragStartX = 0;
        bool isHovered = false;
    };

    int leftSidebarWidth = 230;
    int rightInspectorWidth = 230;
    bool isLeftSidebarCompact = false;
    bool isRightInspectorCollapsed = false;

    juce::TextButton expandRightInspectorBtn { juce::CharPointer_UTF8 ("\xe2\x80\xb9") }; // '‹'

    SidebarSplitter leftSplitter;
    SidebarSplitter rightSplitter;

    juce::TextButton zoomOutBtn { "-" }, zoomInBtn { "+" };
    juce::TextButton snapToggleBtn { "Snap" }; // toggles node grid-snap

    void updateLocalizedStrings();
    void toggleGlobalBypass();
    void updateZoomReadout();
    void updatePresetUI();
    void saveCurrentPresetDirectly();
    void showSavePresetDialog();
    void showLoadPresetDialog();
    void showModuleInfoCard (const juce::String& moduleType);
    void hideModuleInfoCard();
    void openPreferences (PreferencesModal::Tab tab);

    std::unique_ptr<PreferencesModal> preferencesModal;
    std::unique_ptr<juce::Component> infoBackdrop;
    std::unique_ptr<ModuleInfoCard> moduleInfoCard;
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::map<juce::AudioProcessorGraph::NodeID, bool> savedBypassStates;

    juce::MemoryBlock compareSlotA, compareSlotB;
    bool compareIsB = false;

    void timerCallback() override;

    float currentMeterLevel = 0.0f;
    float peakHoldLevel = 0.0f;
    int peakHoldTimer = 0;
    bool globalBypassed = false;
    juce::Rectangle<int> bottomPanelBounds;
    juce::Rectangle<int> topBarBounds;
    juce::Rectangle<int> moduleCountBounds, zoomReadoutBounds;

    juce::Slider macroKnobs[4];
    std::unique_ptr<juce::SliderParameterAttachment> macroAttachments[4];

    // Standalone Calm Mixer
    bool isStandaloneMode = false;
    bool isStandaloneMixerViewActive = false;
    std::unique_ptr<StandaloneMixerComponent> standaloneMixer;
    juce::TextButton backToMixerBtn { "← Mixer" };
    juce::Label dspChainTitleLabel;
    void updateDspChainTitle();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
