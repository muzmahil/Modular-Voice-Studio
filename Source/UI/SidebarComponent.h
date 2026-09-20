#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include "../Graph/ModuleFactory.h"
#include "../Localization.h"
#include <map>

class NodeCanvas;

// Compact module tile for sidebar with DSP icon
class LibraryItem : public juce::Component, public juce::SettableTooltipClient
{
public:
    using DoubleClickCallback = std::function<void(const juce::String&)>;
    using SingleClickCallback = std::function<void(const juce::String&)>;

    LibraryItem (juce::String category, juce::String type, juce::String description,
                 juce::Colour categoryColor, DoubleClickCallback onDoubleClick = nullptr,
                 SingleClickCallback onClick = nullptr);

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseEnter (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    juce::Rectangle<int> getInfoButtonBounds() const;

    const juce::String& getType() const { return moduleType; }
    const juce::String& getCategory() const { return moduleCategory; }
    const juce::String& getDescription() const { return moduleDescription; }

    static void drawModuleIcon (juce::Graphics& g, const juce::String& type, juce::Rectangle<float> area, juce::Colour color);

private:
    juce::String moduleCategory;
    juce::String moduleType;
    juce::String moduleDescription;
    juce::Colour accentColor;
    DoubleClickCallback doubleClickCallback;
    SingleClickCallback singleClickCallback;
    bool isHovered = false;
    bool isInfoHovered = false;
    bool isRightClickCandidate = false;
};

// Collapsible section header badge inside module list
class CategoryHeader : public juce::Component
{
public:
    CategoryHeader (juce::String name, juce::Colour color, bool isExpanded, std::function<void()> onToggle = nullptr);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseEnter (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

private:
    juce::String categoryName;
    juce::Colour accentColor;
    bool expanded = true;
    bool isHovered = false;
    std::function<void()> toggleCallback;
};

// Main Left Sidebar Panel
class SidebarComponent : public juce::Component,
                         public juce::TextEditor::Listener,
                         public LocalizationManager::Listener
{
public:
    SidebarComponent (PluginProcessor& p, NodeCanvas& canvas);
    ~SidebarComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    void textEditorTextChanged (juce::TextEditor&) override;
    void localizationChanged() override;

    std::function<void()> onCollapseRequested;
    std::function<void(const juce::String&)> onModuleSelected;

    void setCompactMode (bool compact);
    bool getCompactMode() const { return isCompactMode; }

    void scrollList (float deltaY);

private:
    PluginProcessor& processor;
    NodeCanvas& nodeCanvas;
    bool isCompactMode = false;

    juce::TextButton collapseBtn { juce::CharPointer_UTF8 ("\xe2\x80\xb9") }; // '‹'
    juce::TextEditor searchBox;

    juce::Viewport viewport;
    juce::Component listContainer;

    juce::OwnedArray<juce::Component> listItems;
    std::map<juce::String, bool> categoryExpanded;
    bool vocalModulesExpanded = true;
    juce::Rectangle<int> searchContainerBounds;

    juce::Colour getCategoryColor (const juce::String& category) const;
    void rebuildList();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SidebarComponent)
};