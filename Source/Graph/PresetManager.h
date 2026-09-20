#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

class PluginProcessor;

class PresetManager
{
public:
    PresetManager();
    ~PresetManager() = default;

    static juce::File getDefaultPresetsDirectory();
    juce::File getPresetsDirectory() const;
    void setCustomPresetsDirectory (const juce::File& dir);
    void openPresetsFolderInExplorer() const;

    static juce::String getExtension() { return ".mvs"; }

    void refreshPresets();
    void createDefaultPresetsIfNeeded();
    const juce::StringArray& getPresetNames() const { return presetNames; }
    int getCurrentPresetIndex() const { return currentPresetIndex; }
    void clearCurrentPresetIndex() { currentPresetIndex = -1; }
    juce::String getCurrentPresetName() const;

    bool loadPreset (int index, PluginProcessor& processor);
    bool loadPresetByName (const juce::String& name, PluginProcessor& processor);
    bool loadFromFile (const juce::File& file, PluginProcessor& processor);
    bool saveToFile (const juce::File& file, PluginProcessor& processor);
    bool savePreset (const juce::String& name, PluginProcessor& processor);
    bool deletePreset (int index);

    bool loadNextPreset (PluginProcessor& processor);
    bool loadPreviousPreset (PluginProcessor& processor);

private:
    juce::File customDir;
    juce::Array<juce::File> presetFiles;
    juce::StringArray presetNames;
    int currentPresetIndex = -1;
};
