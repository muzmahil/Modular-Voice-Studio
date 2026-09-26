#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>

class PluginProcessor;
class NodeCanvas;
class CanvasMinimap;

/**
    Modern Apple Pro Settings & About modal overlay.
    Includes patch library path configuration with instant Explorer integration,
    live canvas workflow preferences, cable glow toggles, view reset,
    audio engine telemetry, and a sleek About showcase.
*/
class PreferencesModal : public juce::Component
{
public:
    PreferencesModal (PluginProcessor& processor,
                      NodeCanvas& canvas,
                      CanvasMinimap& minimap,
                      std::function<void()> onSettingsChanged,
                      std::function<void()> onClose);
    ~PreferencesModal() override = default;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;

    enum class Tab { AudioDevice, Settings, Shortcuts, About };
    void setTab (Tab newTab);

private:
    PluginProcessor& processor;
    NodeCanvas& nodeCanvas;
    CanvasMinimap& minimap;
    std::function<void()> onSettingsChangedCallback;
    std::function<void()> onCloseCallback;

    Tab currentTab = Tab::AudioDevice;

    // Window card bounds
    juce::Rectangle<int> cardBounds;

    // Top Header Buttons
    juce::TextButton audioDeviceTabBtn { "Audio Hardware" };
    juce::TextButton settingsTabBtn    { "Settings" };
    juce::TextButton shortcutsTabBtn   { "Shortcuts" };
    juce::TextButton aboutTabBtn       { "About" };
    juce::TextButton closeBtn          { "Close" };

    // --- Audio Engine & Master Output Tab Controls ---
    juce::ComboBox   driverTypeBox;
    juce::ComboBox   outputDeviceBox;
    juce::ComboBox   sampleRateBox;
    juce::ComboBox   bufferSizeBox;
    juce::TextButton asioControlPanelBtn { "Launch ASIO Driver Panel" };
    juce::Label      engineStatusLabel;
    juce::Label      dawNoticeLabel;

    // --- Settings Tab Controls ---
    juce::ComboBox   languageSelector;
    juce::TextEditor presetPathBox;
    juce::TextButton openExplorerBtn { "Open in Explorer" };
    juce::TextButton changeFolderBtn { "Change Folder..." };

    juce::ToggleButton snapToggle { "Enable Canvas Grid Snapping" };
    juce::ToggleButton minimapToggle { "Show Canvas Minimap HUD (Navigator)" };
    juce::ToggleButton cableGlowToggle { "Enable Cable Specular Glow Effect" };

    juce::TextButton resetViewBtn { "Reset View & Zoom to 100%" };
    juce::TextButton clearGraphBtn { "Clear All Canvas Modules" };

    juce::Label audioInfoLabel;

    std::unique_ptr<juce::FileChooser> fileChooser;

    void updatePathDisplay();
    void updateAudioEngineControls();
    void drawAudioHardwareTab (juce::Graphics& g, juce::Rectangle<int> contentArea);
    void drawSettingsTab (juce::Graphics& g, juce::Rectangle<int> contentArea);
    void drawShortcutsTab (juce::Graphics& g, juce::Rectangle<int> contentArea);
    void drawAboutTab (juce::Graphics& g, juce::Rectangle<int> contentArea);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PreferencesModal)
};
