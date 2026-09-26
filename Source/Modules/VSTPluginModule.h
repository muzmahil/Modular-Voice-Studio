#pragma once
#include "../Graph/ModuleProcessor.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <memory>
#include <mutex>

class VSTPluginWindow : public juce::DocumentWindow
{
public:
    VSTPluginWindow (juce::AudioPluginInstance& plugin, const juce::String& pluginName)
        : juce::DocumentWindow (pluginName, juce::Colour (0xff1e222d), juce::DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar (true);
        if (auto* ed = plugin.createEditorIfNeeded())
        {
            setContentOwned (ed, true);
            setResizable (ed->isResizable(), false);
        }
        centreWithSize (getWidth() > 100 ? getWidth() : 600, getHeight() > 100 ? getHeight() : 450);
    }

    void closeButtonPressed() override
    {
        setVisible (false);
    }
};

/**
    VSTPluginModule: Hosts external VST3 / VST audio effect plugins.
    Allows real-time DSP processing and opening the native plugin UI.
*/
class VSTPluginModule : public ModuleProcessor
{
public:
    VSTPluginModule()
        : ModuleProcessor ("VST3 Host", createLayout())
    {
        activeParam = getRawParam ("active");
        mixParam    = getRawParam ("mix");

        formatManager.addDefaultFormats();
    }

    ~VSTPluginModule() override
    {
        closeEditorWindow();
        std::lock_guard<std::mutex> lock (instanceMutex);
        pluginInstance.reset();
    }

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { "active", 1 }, "Active", true));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "mix", 1 }, "Dry/Wet Mix",
            juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));

        return { params.begin(), params.end() };
    }

    bool loadPluginFile (const juce::String& filePath, juce::String& errorMessage)
    {
        juce::File file (filePath);
        if (! file.existsAsFile())
        {
            errorMessage = "File does not exist: " + filePath;
            return false;
        }

        juce::OwnedArray<juce::PluginDescription> descriptions;
        juce::KnownPluginList pluginList;

        for (int i = 0; i < formatManager.getNumFormats(); ++i)
        {
            auto* format = formatManager.getFormat (i);
            if (format != nullptr && format->fileMightContainThisPluginType (filePath))
            {
                format->findAllTypesForFile (descriptions, filePath);
            }
        }

        if (descriptions.isEmpty())
        {
            errorMessage = "No valid VST/VST3 plugin found in file.";
            return false;
        }

        auto* desc = descriptions[0];
        std::unique_ptr<juce::AudioPluginInstance> instance;
        instance = formatManager.createPluginInstance (*desc, currentSampleRate, currentBlockSize, errorMessage);

        if (instance == nullptr)
        {
            if (errorMessage.isEmpty())
                errorMessage = "Failed to instantiate VST plugin.";
            return false;
        }

        closeEditorWindow();

        {
            std::lock_guard<std::mutex> lock (instanceMutex);
            pluginInstance = std::move (instance);
            pluginName = desc->name;
            pluginPath = filePath;

            pluginInstance->prepareToPlay (currentSampleRate, currentBlockSize);
        }

        return true;
    }

    void openEditorWindow()
    {
        juce::MessageManager::callAsync ([this]()
        {
            std::lock_guard<std::mutex> lock (instanceMutex);
            if (pluginInstance == nullptr)
                return;

            if (editorWindow == nullptr)
            {
                editorWindow = std::make_unique<VSTPluginWindow> (*pluginInstance, pluginName.isNotEmpty() ? pluginName : "VST Plugin");
            }

            editorWindow->setVisible (true);
            editorWindow->toFront (true);
        });
    }

    void closeEditorWindow()
    {
        if (editorWindow != nullptr)
        {
            editorWindow->setVisible (false);
            editorWindow.reset();
        }
    }

    void unloadPlugin()
    {
        closeEditorWindow();
        std::lock_guard<std::mutex> lock (instanceMutex);
        pluginInstance.reset();
        pluginName = "";
        pluginPath = "";
    }

    juce::String getLoadedPluginName() const
    {
        std::lock_guard<std::mutex> lock (instanceMutex);
        return pluginName;
    }

    bool isPluginLoaded() const
    {
        std::lock_guard<std::mutex> lock (instanceMutex);
        return pluginInstance != nullptr;
    }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override
    {
        currentSampleRate = sampleRate > 0 ? sampleRate : 48000.0;
        currentBlockSize = samplesPerBlock > 0 ? samplesPerBlock : 512;

        std::lock_guard<std::mutex> lock (instanceMutex);
        if (pluginInstance != nullptr)
            pluginInstance->prepareToPlay (currentSampleRate, currentBlockSize);
    }

    void releaseResources() override
    {
        std::lock_guard<std::mutex> lock (instanceMutex);
        if (pluginInstance != nullptr)
            pluginInstance->releaseResources();
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    {
        juce::ScopedNoDenormals noDenormals;

        bool isActive = activeParam != nullptr ? (activeParam->load() > 0.5f) : (getParamValue ("active", 1.0f) > 0.5f);
        float mixPct = (mixParam != nullptr ? mixParam->load() : getParamValue ("mix", 100.0f)) / 100.0f;

        if (!isActive)
            return;

        std::unique_lock<std::mutex> lock (instanceMutex, std::try_to_lock);
        if (!lock.owns_lock() || pluginInstance == nullptr)
            return;

        if (mixPct >= 0.999f)
        {
            pluginInstance->processBlock (buffer, midi);
        }
        else if (mixPct > 0.001f)
        {
            juce::AudioBuffer<float> dryCopy (buffer);
            pluginInstance->processBlock (buffer, midi);

            int numChannels = buffer.getNumChannels();
            int numSamples = buffer.getNumSamples();

            for (int ch = 0; ch < numChannels; ++ch)
            {
                auto* wetPtr = buffer.getWritePointer (ch);
                auto* dryPtr = dryCopy.getReadPointer (ch);
                for (int i = 0; i < numSamples; ++i)
                {
                    wetPtr[i] = dryPtr[i] * (1.0f - mixPct) + wetPtr[i] * mixPct;
                }
            }
        }
    }

private:
    std::atomic<float>* activeParam = nullptr;
    std::atomic<float>* mixParam = nullptr;

    juce::AudioPluginFormatManager formatManager;
    mutable std::mutex instanceMutex;
    std::unique_ptr<juce::AudioPluginInstance> pluginInstance;
    std::unique_ptr<VSTPluginWindow> editorWindow;
    juce::String pluginName;
    juce::String pluginPath;

    double currentSampleRate = 48000.0;
    int currentBlockSize = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VSTPluginModule)
};
