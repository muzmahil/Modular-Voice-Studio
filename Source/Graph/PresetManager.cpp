#include "PresetManager.h"
#include "../PluginProcessor.h"

PresetManager::PresetManager()
{
    refreshPresets();
}

juce::File PresetManager::getDefaultPresetsDirectory()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("ModularVoiceStudio")
                   .getChildFile ("Presets");

    if (! dir.exists())
        dir.createDirectory();

    return dir;
}

juce::File PresetManager::getPresetsDirectory() const
{
    if (customDir.exists())
        return customDir;

    return getDefaultPresetsDirectory();
}

void PresetManager::setCustomPresetsDirectory (const juce::File& dir)
{
    customDir = dir;
    if (! customDir.exists())
        customDir.createDirectory();
    refreshPresets();
}

void PresetManager::openPresetsFolderInExplorer() const
{
    auto dir = getPresetsDirectory();
    if (! dir.exists())
        dir.createDirectory();

    dir.revealToUser();
    dir.startAsProcess();
}

void PresetManager::refreshPresets()
{
    createDefaultPresetsIfNeeded();

    presetFiles.clear();
    presetNames.clear();

    auto dir = getPresetsDirectory();
    auto files = dir.findChildFiles (juce::File::findFiles, false, "*" + getExtension());
    files.sort();

    for (const auto& f : files)
    {
        presetFiles.add (f);
        presetNames.add (f.getFileNameWithoutExtension());
    }
}

void PresetManager::createDefaultPresetsIfNeeded()
{
    auto dir = getPresetsDirectory();
    if (! dir.exists())
        dir.createDirectory();

    struct NodeInfo {
        int id;
        juce::String type;
        int x;
        int y;
        juce::String color;
    };

    struct PresetInfo {
        juce::String name;
        int inX, inY;
        int outX, outY;
        std::vector<NodeInfo> nodes;
    };

    std::vector<PresetInfo> factoryPresets = {
        {
            "Broadcast Radio Host",
            1400, 2000, 3400, 2000,
            {
                { 10, "De-Plosive",         1620, 2000, "blue" },
                { 11, "Noise Suppression",   1840, 2000, "blue" },
                { 12, "Gate",                2060, 2000, "purple" },
                { 13, "Parametric EQ",       2280, 2000, "yellow" },
                { 14, "Proximity Effect",    2500, 2000, "purple" },
                { 15, "Saturation",          2720, 2000, "orange" },
                { 16, "Compressor",          2940, 2000, "purple" },
                { 17, "Limiter",             3160, 2000, "red" }
            }
        },
        {
            "Streamer Crisp Clear",
            1300, 2000, 3500, 2000,
            {
                { 10, "De-Plosive",         1520, 2000, "blue" },
                { 11, "De-Click",           1740, 2000, "blue" },
                { 12, "Noise Suppression",   1960, 2000, "blue" },
                { 13, "Gate",                2180, 2000, "purple" },
                { 14, "De-Breath",           2400, 2000, "blue" },
                { 15, "Spectral Clarity",    2620, 2000, "yellow" },
                { 16, "De-Esser",            2840, 2000, "yellow" },
                { 17, "Upward Compressor",   3060, 2000, "purple" },
                { 18, "Limiter",             3280, 2000, "red" }
            }
        },
        {
            "Warm Podcast Dialogue",
            1400, 2000, 3400, 2000,
            {
                { 10, "Noise Suppression",   1620, 2000, "blue" },
                { 11, "Gate",                1840, 2000, "purple" },
                { 12, "Parametric EQ",       2060, 2000, "yellow" },
                { 13, "Proximity Effect",    2280, 2000, "orange" },
                { 14, "Phantom Sub",         2500, 2000, "orange" },
                { 15, "AGC",                 2720, 2000, "purple" },
                { 16, "Compressor",          2940, 2000, "purple" },
                { 17, "Limiter",             3160, 2000, "red" }
            }
        },
        {
            "Acoustic Live Stage",
            1400, 2000, 3400, 2000,
            {
                { 10, "AEC",                 1620, 2000, "blue" },
                { 11, "De-reverb",           1840, 2000, "blue" },
                { 12, "Gate",                2060, 2000, "purple" },
                { 13, "Parametric EQ",       2280, 2000, "yellow" },
                { 14, "Dynamic EQ",          2500, 2000, "yellow" },
                { 15, "De-Esser",            2720, 2000, "yellow" },
                { 16, "Compressor",          2940, 2000, "purple" },
                { 17, "Limiter",             3160, 2000, "red" }
            }
        },
        {
            "Modern In-Your-Face Vocal",
            1400, 2000, 3400, 2000,
            {
                { 10, "Noise Suppression",   1620, 2000, "blue" },
                { 11, "Spectral Clarity",    1840, 2000, "yellow" },
                { 12, "Aural Exciter",       2060, 2000, "yellow" },
                { 13, "Saturation",          2280, 2000, "orange" },
                { 14, "Upward Compressor",   2500, 2000, "purple" },
                { 15, "Vocal Doubler",       2720, 2000, "purple" },
                { 16, "Limiter",             2940, 2000, "red" }
            }
        },
        {
            "Vocal Clarity & Tone",
            1400, 2000, 3400, 2000,
            {
                { 10, "De-Plosive",          1620, 2000, "blue" },
                { 11, "Noise Suppression",   1840, 2000, "blue" },
                { 12, "Spectral Clarity",    2060, 2000, "yellow" },
                { 13, "Crossover Splitter",  2280, 2000, "yellow" },
                { 14, "Dynamic EQ",          2500, 2000, "yellow" },
                { 15, "Aural Exciter",       2720, 2000, "yellow" },
                { 16, "Compressor",          2940, 2000, "purple" },
                { 17, "Limiter",             3160, 2000, "red" }
            }
        }
    };

    for (const auto& p : factoryPresets)
    {
        auto file = dir.getChildFile (p.name + getExtension());
        if (! file.existsAsFile())
        {
            juce::ValueTree state ("MODULAR_AUDIO_OS");
            state.setProperty ("inputX", p.inX, nullptr);
            state.setProperty ("inputY", p.inY, nullptr);
            state.setProperty ("outputX", p.outX, nullptr);
            state.setProperty ("outputY", p.outY, nullptr);
            state.setProperty ("inputID", 1, nullptr);
            state.setProperty ("outputID", 2, nullptr);
            state.setProperty ("panX", -1400.0f, nullptr);
            state.setProperty ("panY", -1500.0f, nullptr);
            state.setProperty ("zoom", 1.0f, nullptr);
            state.setProperty ("pluginBypassed", false, nullptr);
            state.setProperty ("language", "en", nullptr);
            state.setProperty ("macro1", 0.0f, nullptr);
            state.setProperty ("macro2", 0.0f, nullptr);
            state.setProperty ("macro3", 0.0f, nullptr);
            state.setProperty ("macro4", 0.0f, nullptr);

            juce::ValueTree nodes ("NODES");
            for (const auto& nd : p.nodes)
            {
                juce::ValueTree n ("NODE");
                n.setProperty ("id", nd.id, nullptr);
                n.setProperty ("type", nd.type, nullptr);
                n.setProperty ("x", nd.x, nullptr);
                n.setProperty ("y", nd.y, nullptr);
                n.setProperty ("bypassed", false, nullptr);
                n.setProperty ("color", nd.color, nullptr);
                n.setProperty ("mix", 1.0f, nullptr);
                n.setProperty ("state", "", nullptr);
                nodes.appendChild (n, nullptr);
            }
            state.appendChild (nodes, nullptr);

            juce::ValueTree conns ("CONNECTIONS");
            int prevNodeId = 1; // Input
            for (const auto& nd : p.nodes)
            {
                juce::ValueTree c0 ("CONNECTION");
                c0.setProperty ("srcNode", prevNodeId, nullptr);
                c0.setProperty ("srcCh", 0, nullptr);
                c0.setProperty ("dstNode", nd.id, nullptr);
                c0.setProperty ("dstCh", 0, nullptr);
                conns.appendChild (c0, nullptr);

                juce::ValueTree c1 ("CONNECTION");
                c1.setProperty ("srcNode", prevNodeId, nullptr);
                c1.setProperty ("srcCh", 1, nullptr);
                c1.setProperty ("dstNode", nd.id, nullptr);
                c1.setProperty ("dstCh", 1, nullptr);
                conns.appendChild (c1, nullptr);

                prevNodeId = nd.id;
            }

            // Connect last node to Output (2)
            juce::ValueTree outC0 ("CONNECTION");
            outC0.setProperty ("srcNode", prevNodeId, nullptr);
            outC0.setProperty ("srcCh", 0, nullptr);
            outC0.setProperty ("dstNode", 2, nullptr);
            outC0.setProperty ("dstCh", 0, nullptr);
            conns.appendChild (outC0, nullptr);

            juce::ValueTree outC1 ("CONNECTION");
            outC1.setProperty ("srcNode", prevNodeId, nullptr);
            outC1.setProperty ("srcCh", 1, nullptr);
            outC1.setProperty ("dstNode", 2, nullptr);
            outC1.setProperty ("dstCh", 1, nullptr);
            conns.appendChild (outC1, nullptr);

            state.appendChild (conns, nullptr);

            juce::MemoryBlock destData;
            if (auto xml = state.createXml())
            {
                juce::AudioProcessor::copyXmlToBinary (*xml, destData);
                file.replaceWithData (destData.getData(), destData.getSize());
            }
        }
    }
}

juce::String PresetManager::getCurrentPresetName() const
{
    if (juce::isPositiveAndBelow (currentPresetIndex, presetNames.size()))
        return presetNames[currentPresetIndex];

    return "<no preset>";
}

bool PresetManager::loadFromFile (const juce::File& file, PluginProcessor& processor)
{
    if (! file.existsAsFile())
        return false;

    juce::MemoryBlock block;
    if (file.loadFileAsData (block))
    {
        processor.setStateInformation (block.getData(), (int) block.getSize());
        refreshPresets();
        currentPresetIndex = presetNames.indexOf (file.getFileNameWithoutExtension());
        return true;
    }
    return false;
}

bool PresetManager::saveToFile (const juce::File& file, PluginProcessor& processor)
{
    juce::MemoryBlock data;
    processor.getStateInformation (data);

    if (file.replaceWithData (data.getData(), data.getSize()))
    {
        refreshPresets();
        currentPresetIndex = presetNames.indexOf (file.getFileNameWithoutExtension());
        return true;
    }
    return false;
}

bool PresetManager::loadPreset (int index, PluginProcessor& processor)
{
    if (! juce::isPositiveAndBelow (index, presetFiles.size()))
        return false;

    return loadFromFile (presetFiles[index], processor);
}

bool PresetManager::loadPresetByName (const juce::String& name, PluginProcessor& processor)
{
    int idx = presetNames.indexOf (name);
    if (idx >= 0)
        return loadPreset (idx, processor);

    auto file = getPresetsDirectory().getChildFile (name + getExtension());
    return loadFromFile (file, processor);
}

bool PresetManager::savePreset (const juce::String& name, PluginProcessor& processor)
{
    if (name.trim().isEmpty())
        return false;

    auto cleanName = juce::File::createLegalFileName (name.trim());
    auto file = getPresetsDirectory().getChildFile (cleanName + getExtension());
    return saveToFile (file, processor);
}

bool PresetManager::deletePreset (int index)
{
    if (! juce::isPositiveAndBelow (index, presetFiles.size()))
        return false;

    bool ok = presetFiles[index].deleteFile();
    refreshPresets();
    if (currentPresetIndex >= presetNames.size())
        currentPresetIndex = presetNames.size() - 1;

    return ok;
}

bool PresetManager::loadNextPreset (PluginProcessor& processor)
{
    if (presetFiles.isEmpty())
        return false;

    int nextIndex = (currentPresetIndex + 1) % presetFiles.size();
    return loadPreset (nextIndex, processor);
}

bool PresetManager::loadPreviousPreset (PluginProcessor& processor)
{
    if (presetFiles.isEmpty())
        return false;

    int prevIndex = currentPresetIndex <= 0 ? (presetFiles.size() - 1) : (currentPresetIndex - 1);
    return loadPreset (prevIndex, processor);
}
