#include "MVS_Core_Export.h"
#include "../PluginProcessor.h"
#include "../PluginEditor.h"
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>

class ModularCanvasWindow : public juce::DocumentWindow
{
public:
    ModularCanvasWindow (PluginProcessor& proc)
        : juce::DocumentWindow ("Modular Voice Studio - Modular DSP Canvas",
                                juce::Colour (0xff1e222d),
                                juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton)
    {
        setUsingNativeTitleBar (true);
        editor = dynamic_cast<PluginEditor*> (proc.createEditor());
        setContentOwned (editor, true);
        setResizable (true, true);
        centreWithSize (1280, 800);
    }

    void refreshCanvas()
    {
        if (editor != nullptr)
            editor->refreshCanvasView();
    }

    void closeButtonPressed() override
    {
        setVisible (false);
    }

private:
    PluginEditor* editor = nullptr;
};

class AudioSettingsWindow : public juce::DocumentWindow
{
public:
    AudioSettingsWindow (juce::AudioDeviceManager& dm)
        : juce::DocumentWindow ("Modular Voice Studio - Audio Engine & Hardware Setup",
                                juce::Colour (0xff1e222d),
                                juce::DocumentWindow::closeButton)
    {
        setUsingNativeTitleBar (true);
        auto* selector = new juce::AudioDeviceSelectorComponent (dm, 1, 2, 1, 2, false, false, true, false);
        selector->setSize (520, 360);
        setContentOwned (selector, true);
        centreWithSize (520, 360);
    }

    void closeButtonPressed() override
    {
        setVisible (false);
    }
};

struct MVSEngineContext
{
    std::unique_ptr<PluginProcessor> processor;
    juce::AudioProcessorPlayer player;
    juce::AudioDeviceManager deviceManager;
    std::unique_ptr<ModularCanvasWindow> canvasWindow;
    std::unique_ptr<AudioSettingsWindow> audioSettingsWindow;
    bool initialised = false;
};

static std::unique_ptr<MVSEngineContext> gEngine;
static std::mutex gEngineMutex;

MVS_API int MVS_Init (double sampleRate, int blockSize)
{
    std::lock_guard<std::mutex> lock (gEngineMutex);
    if (gEngine != nullptr)
        return 1;

    juce::initialiseJuce_GUI();

    gEngine = std::make_unique<MVSEngineContext>();
    gEngine->processor = std::make_unique<PluginProcessor>();

    gEngine->player.setProcessor (gEngine->processor.get());
    gEngine->deviceManager.addAudioCallback (&gEngine->player);

    // Initialise audio device manager with 2 in / 2 out
    juce::String err = gEngine->deviceManager.initialise (2, 2, nullptr, true);
    if (err.isNotEmpty())
    {
        DBG ("Device init warning: " + err);
    }

    // Auto-select standard rates & blocks (low latency 128/256 by default)
    {
        auto setup = gEngine->deviceManager.getAudioDeviceSetup();
        setup.sampleRate = sampleRate > 0 ? sampleRate : 48000.0;
        setup.bufferSize = blockSize > 0 ? blockSize : 128;
        gEngine->deviceManager.setAudioDeviceSetup (setup, true);
    }

    // Auto-detect & set VB-Cable if present
    if (auto* currentType = gEngine->deviceManager.getCurrentDeviceTypeObject())
    {
        auto outNames = currentType->getDeviceNames (false);
        for (const auto& dev : outNames)
        {
            if (dev.containsIgnoreCase ("CABLE Input"))
            {
                if (gEngine->processor->getOutputWorker (0) != nullptr)
                    gEngine->processor->getOutputWorker (0)->setDevice (dev);
                break;
            }
        }
    }

    // Pre-warm windows in RAM on startup for 0ms instantaneous opening
    gEngine->canvasWindow = std::make_unique<ModularCanvasWindow> (*gEngine->processor);
    gEngine->canvasWindow->setVisible (false);

    gEngine->audioSettingsWindow = std::make_unique<AudioSettingsWindow> (gEngine->deviceManager);
    gEngine->audioSettingsWindow->setVisible (false);

    gEngine->initialised = true;
    return 1;
}

MVS_API void MVS_Shutdown()
{
    std::lock_guard<std::mutex> lock (gEngineMutex);
    if (gEngine == nullptr)
        return;

    gEngine->canvasWindow.reset();
    gEngine->audioSettingsWindow.reset();
    gEngine->deviceManager.removeAudioCallback (&gEngine->player);
    gEngine->deviceManager.closeAudioDevice();
    gEngine->player.setProcessor (nullptr);
    gEngine->processor.reset();
    gEngine.reset();

    juce::shutdownJuce_GUI();
}

MVS_API int MVS_GetInputDevices (char* outBuffer, int maxLen)
{
    std::lock_guard<std::mutex> lock (gEngineMutex);
    if (gEngine == nullptr || outBuffer == nullptr || maxLen <= 0)
        return 0;

    juce::StringArray names;
    if (auto* currentType = gEngine->deviceManager.getCurrentDeviceTypeObject())
        names = currentType->getDeviceNames (true);

    juce::String joined = names.joinIntoString ("\n");
    juce::zeromem (outBuffer, maxLen);
    strncpy_s (outBuffer, maxLen, joined.toRawUTF8(), _TRUNCATE);
    return names.size();
}

MVS_API int MVS_GetOutputDevices (char* outBuffer, int maxLen)
{
    std::lock_guard<std::mutex> lock (gEngineMutex);
    if (gEngine == nullptr || outBuffer == nullptr || maxLen <= 0)
        return 0;

    juce::StringArray names;
    if (auto* currentType = gEngine->deviceManager.getCurrentDeviceTypeObject())
        names = currentType->getDeviceNames (false);

    juce::String joined = names.joinIntoString ("\n");
    juce::zeromem (outBuffer, maxLen);
    strncpy_s (outBuffer, maxLen, joined.toRawUTF8(), _TRUNCATE);
    return names.size();
}

MVS_API int MVS_SetInputDevice (const char* deviceName)
{
    std::lock_guard<std::mutex> lock (gEngineMutex);
    if (gEngine == nullptr || deviceName == nullptr)
        return 0;

    auto setup = gEngine->deviceManager.getAudioDeviceSetup();
    setup.inputDeviceName = juce::String::fromUTF8 (deviceName);
    setup.useDefaultInputChannels = true;
    auto err = gEngine->deviceManager.setAudioDeviceSetup (setup, true);
    return err.isEmpty() ? 1 : 0;
}

MVS_API int MVS_SetVBCableOutputDevice (const char* deviceName)
{
    std::lock_guard<std::mutex> lock (gEngineMutex);
    if (gEngine == nullptr || deviceName == nullptr)
        return 0;

    if (gEngine->processor != nullptr && gEngine->processor->getOutputWorker (0) != nullptr)
    {
        gEngine->processor->getOutputWorker (0)->setDevice (juce::String::fromUTF8 (deviceName));
        return 1;
    }
    return 0;
}

MVS_API int MVS_SetMonitorOutputDevice (const char* deviceName)
{
    std::lock_guard<std::mutex> lock (gEngineMutex);
    if (gEngine == nullptr || deviceName == nullptr)
        return 0;

    if (gEngine->processor != nullptr && gEngine->processor->getOutputWorker (1) != nullptr)
    {
        gEngine->processor->getOutputWorker (1)->setDevice (juce::String::fromUTF8 (deviceName));
        return 1;
    }
    return 0;
}

MVS_API void MVS_SetMicFaderGain (float gainLinear)
{
    if (gEngine != nullptr && gEngine->processor != nullptr)
        gEngine->processor->getChannel (0).faderGainLinear.store (gainLinear);
}

MVS_API void MVS_SetMicMute (int isMuted)
{
    if (gEngine != nullptr && gEngine->processor != nullptr)
        gEngine->processor->getChannel (0).muted.store (isMuted != 0);
}

MVS_API void MVS_SetMicBypass (int isBypassed)
{
    if (gEngine != nullptr && gEngine->processor != nullptr)
        gEngine->processor->getChannel (0).isBypassed.store (isBypassed != 0);
}

MVS_API void MVS_SetVBCableFaderGain (float gainLinear)
{
    juce::ignoreUnused (gainLinear);
}

MVS_API void MVS_SetVBCableMute (int isMuted)
{
    if (gEngine != nullptr && gEngine->processor != nullptr)
        gEngine->processor->getChannel (0).isBusB1Active.store (isMuted == 0);
}

MVS_API void MVS_SetVBCableActive (int isActive)
{
    if (gEngine != nullptr && gEngine->processor != nullptr)
        gEngine->processor->getChannel (0).isBusB1Active.store (isActive != 0);
}

MVS_API void MVS_SetMonitorFaderGain (float gainLinear)
{
    juce::ignoreUnused (gainLinear);
}

MVS_API void MVS_SetMonitorActive (int isActive)
{
    if (gEngine != nullptr && gEngine->processor != nullptr)
        gEngine->processor->getChannel (0).isMonitoringActive.store (isActive != 0);
}

MVS_API void MVS_GetLiveLevels (float* inLvl, float* outLvl, float* vbLvl, float* monLvl)
{
    if (gEngine == nullptr || gEngine->processor == nullptr)
    {
        if (inLvl) *inLvl = 0.0f;
        if (outLvl) *outLvl = 0.0f;
        if (vbLvl) *vbLvl = 0.0f;
        if (monLvl) *monLvl = 0.0f;
        return;
    }

    float inL = gEngine->processor->getChannel (0).inLevel.load();
    float outL = gEngine->processor->getChannel (0).outLevel.load();
    bool isB1 = gEngine->processor->getChannel (0).isBusB1Active.load();
    bool isMon = gEngine->processor->getChannel (0).isMonitoringActive.load();

    if (inLvl) *inLvl = inL;
    if (outLvl) *outLvl = outL;
    if (vbLvl) *vbLvl = isB1 ? outL : 0.0f;
    if (monLvl) *monLvl = isMon ? outL : 0.0f;
}

MVS_API void MVS_ResetDSPChain()
{
    std::lock_guard<std::mutex> lock (gEngineMutex);
    if (gEngine != nullptr && gEngine->processor != nullptr)
    {
        gEngine->processor->resetAllToFactoryDefaults();
        if (gEngine->canvasWindow != nullptr)
            gEngine->canvasWindow->refreshCanvas();
    }
}

MVS_API int MVS_GetActiveDSPNodes (char* outBuffer, int maxLen)
{
    std::lock_guard<std::mutex> lock (gEngineMutex);
    if (gEngine == nullptr || gEngine->processor == nullptr || outBuffer == nullptr || maxLen <= 0)
        return 0;

    auto names = gEngine->processor->getChannelModuleNames (0);
    juce::StringArray arr;
    for (const auto& n : names)
        arr.add (n);

    juce::String joined = arr.joinIntoString (", ");
    juce::zeromem (outBuffer, maxLen);
    strncpy_s (outBuffer, maxLen, joined.toRawUTF8(), _TRUNCATE);
    return names.size();
}

MVS_API void MVS_OpenModularCanvasWindow()
{
    auto doOpen = []()
    {
        std::lock_guard<std::mutex> lock (gEngineMutex);
        if (gEngine != nullptr)
        {
            if (gEngine->canvasWindow == nullptr && gEngine->processor != nullptr)
                gEngine->canvasWindow = std::make_unique<ModularCanvasWindow> (*gEngine->processor);

            if (gEngine->canvasWindow != nullptr)
            {
                gEngine->canvasWindow->refreshCanvas();
                gEngine->canvasWindow->setVisible (true);
                gEngine->canvasWindow->toFront (true);
                gEngine->canvasWindow->grabKeyboardFocus();
            }
        }
    };

    if (juce::MessageManager::getInstanceWithoutCreating() != nullptr &&
        juce::MessageManager::getInstance()->isThisTheMessageThread())
        doOpen();
    else
        juce::MessageManager::callAsync (doOpen);
}

MVS_API void MVS_CloseModularCanvasWindow()
{
    auto doClose = []()
    {
        std::lock_guard<std::mutex> lock (gEngineMutex);
        if (gEngine != nullptr && gEngine->canvasWindow != nullptr)
            gEngine->canvasWindow->setVisible (false);
    };

    if (juce::MessageManager::getInstanceWithoutCreating() != nullptr &&
        juce::MessageManager::getInstance()->isThisTheMessageThread())
        doClose();
    else
        juce::MessageManager::callAsync (doClose);
}

MVS_API void MVS_OpenAudioSettingsWindow()
{
    auto doOpen = []()
    {
        std::lock_guard<std::mutex> lock (gEngineMutex);
        if (gEngine != nullptr)
        {
            if (gEngine->audioSettingsWindow == nullptr)
                gEngine->audioSettingsWindow = std::make_unique<AudioSettingsWindow> (gEngine->deviceManager);

            if (gEngine->audioSettingsWindow != nullptr)
            {
                gEngine->audioSettingsWindow->setVisible (true);
                gEngine->audioSettingsWindow->toFront (true);
                gEngine->audioSettingsWindow->grabKeyboardFocus();
            }
        }
    };

    if (juce::MessageManager::getInstanceWithoutCreating() != nullptr &&
        juce::MessageManager::getInstance()->isThisTheMessageThread())
        doOpen();
    else
        juce::MessageManager::callAsync (doOpen);
}

MVS_API void MVS_CloseAudioSettingsWindow()
{
    auto doClose = []()
    {
        std::lock_guard<std::mutex> lock (gEngineMutex);
        if (gEngine != nullptr && gEngine->audioSettingsWindow != nullptr)
            gEngine->audioSettingsWindow->setVisible (false);
    };

    if (juce::MessageManager::getInstanceWithoutCreating() != nullptr &&
        juce::MessageManager::getInstance()->isThisTheMessageThread())
        doClose();
    else
        juce::MessageManager::callAsync (doClose);
}

MVS_API void MVS_SaveAudioDeviceState (const char* filePath)
{
    std::lock_guard<std::mutex> lock (gEngineMutex);
    if (gEngine == nullptr || filePath == nullptr)
        return;

    auto xml = gEngine->deviceManager.createStateXml();
    if (xml != nullptr)
    {
        juce::File f (juce::String::fromUTF8 (filePath));
        if (! f.getParentDirectory().exists())
            f.getParentDirectory().createDirectory();
        xml->writeTo (f);
    }
}

MVS_API void MVS_LoadAudioDeviceState (const char* filePath)
{
    std::lock_guard<std::mutex> lock (gEngineMutex);
    if (gEngine == nullptr || filePath == nullptr)
        return;

    juce::File f (juce::String::fromUTF8 (filePath));
    if (f.existsAsFile())
    {
        auto xml = juce::XmlDocument::parse (f);
        if (xml != nullptr)
        {
            gEngine->deviceManager.initialise (2, 2, xml.get(), true);
        }
    }
}

MVS_API int MVS_GetCurrentAudioDeviceType (char* outBuffer, int maxLen)
{
    std::lock_guard<std::mutex> lock (gEngineMutex);
    if (gEngine == nullptr || outBuffer == nullptr || maxLen <= 0)
        return 0;

    juce::String typeName;
    if (auto* currentType = gEngine->deviceManager.getCurrentDeviceTypeObject())
        typeName = currentType->getTypeName();

    juce::zeromem (outBuffer, maxLen);
    strncpy_s (outBuffer, maxLen, typeName.toRawUTF8(), _TRUNCATE);
    return typeName.isNotEmpty() ? 1 : 0;
}

MVS_API void MVS_SaveStateToFile (const char* filePath)
{
    std::lock_guard<std::mutex> lock (gEngineMutex);
    if (gEngine == nullptr || gEngine->processor == nullptr || filePath == nullptr)
        return;

    juce::MemoryBlock block;
    gEngine->processor->getStateInformation (block);
    juce::File f (juce::String::fromUTF8 (filePath));
    if (! f.getParentDirectory().exists())
        f.getParentDirectory().createDirectory();
    f.replaceWithData (block.getData(), block.getSize());
}

MVS_API void MVS_LoadStateFromFile (const char* filePath)
{
    std::lock_guard<std::mutex> lock (gEngineMutex);
    if (gEngine == nullptr || gEngine->processor == nullptr || filePath == nullptr)
        return;

    juce::File f (juce::String::fromUTF8 (filePath));
    if (f.existsAsFile())
    {
        juce::MemoryBlock block;
        f.loadFileAsData (block);
        gEngine->processor->setStateInformation (block.getData(), (int) block.getSize());
        if (gEngine->canvasWindow != nullptr)
            gEngine->canvasWindow->refreshCanvas();
    }
}

MVS_API void MVS_QuickFx_SetPitch (float semitones, float mixPct, int active)
{
    std::lock_guard<std::mutex> lock (gEngineMutex);
    if (gEngine != nullptr && gEngine->processor != nullptr)
    {
        auto& pitch = gEngine->processor->getQuickPitch();
        pitch.getModuleParam ("pitch").set (semitones);
        pitch.getModuleParam ("mix").set (mixPct);
        pitch.getModuleParam ("active").set (active ? 1.0f : 0.0f);
    }
}

MVS_API void MVS_QuickFx_SetReverb (float roomSize, float damping, float mixPct, int active)
{
    std::lock_guard<std::mutex> lock (gEngineMutex);
    if (gEngine != nullptr && gEngine->processor != nullptr)
    {
        auto& rev = gEngine->processor->getQuickReverb();
        rev.getModuleParam ("roomSize").set (roomSize);
        rev.getModuleParam ("damping").set (damping);
        rev.getModuleParam ("mix").set (mixPct);
        rev.getModuleParam ("active").set (active ? 1.0f : 0.0f);
    }
}

MVS_API int MVS_QuickFx_LoadVST (const char* filePath, char* errorBuffer, int maxErrorLen)
{
    std::lock_guard<std::mutex> lock (gEngineMutex);
    if (gEngine == nullptr || gEngine->processor == nullptr || filePath == nullptr)
        return 0;

    juce::String err;
    bool ok = gEngine->processor->getQuickVST().loadPluginFile (juce::String::fromUTF8 (filePath), err);
    if (errorBuffer != nullptr && maxErrorLen > 0)
    {
        strncpy_s (errorBuffer, maxErrorLen, err.toRawUTF8(), _TRUNCATE);
    }
    return ok ? 1 : 0;
}

MVS_API void MVS_QuickFx_OpenVSTEditor()
{
    std::lock_guard<std::mutex> lock (gEngineMutex);
    if (gEngine != nullptr && gEngine->processor != nullptr)
    {
        gEngine->processor->getQuickVST().openEditorWindow();
    }
}

MVS_API int MVS_QuickFx_GetLoadedVSTName (char* outBuffer, int maxLen)
{
    std::lock_guard<std::mutex> lock (gEngineMutex);
    if (gEngine == nullptr || gEngine->processor == nullptr || outBuffer == nullptr || maxLen <= 0)
        return 0;

    auto name = gEngine->processor->getQuickVST().getLoadedPluginName();
    strncpy_s (outBuffer, maxLen, name.toRawUTF8(), _TRUNCATE);
    return (int) name.length();
}

MVS_API void MVS_QuickFx_SetVSTActive (int active)
{
    std::lock_guard<std::mutex> lock (gEngineMutex);
    if (gEngine != nullptr && gEngine->processor != nullptr)
    {
        gEngine->processor->getQuickVST().getModuleParam ("active").set (active ? 1.0f : 0.0f);
    }
}

MVS_API void MVS_QuickFx_SetVSTMix (float mixPct)
{
    std::lock_guard<std::mutex> lock (gEngineMutex);
    if (gEngine != nullptr && gEngine->processor != nullptr)
    {
        gEngine->processor->getQuickVST().getModuleParam ("mix").set (mixPct);
    }
}

MVS_API void MVS_QuickFx_RemoveVST()
{
    std::lock_guard<std::mutex> lock (gEngineMutex);
    if (gEngine != nullptr && gEngine->processor != nullptr)
    {
        gEngine->processor->getQuickVST().unloadPlugin();
    }
}

MVS_API int MVS_VST_ScanInstalledPlugins (char* outJsonBuffer, int maxBufferLen)
{
    if (outJsonBuffer == nullptr || maxBufferLen <= 0)
        return 0;

    juce::AudioPluginFormatManager formatManager;
    formatManager.addDefaultFormats();

    juce::StringArray searchDirectories;
    searchDirectories.add ("C:\\Program Files\\Common Files\\VST3");
    searchDirectories.add ("C:\\Program Files (x86)\\Common Files\\VST3");
    searchDirectories.add ("C:\\Program Files\\VSTPlugins");
    searchDirectories.add ("C:\\Program Files\\Steinberg\\VSTPlugins");
    searchDirectories.add ("C:\\Program Files (x86)\\VSTPlugins");

    auto appDataDir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
    auto localAppData = appDataDir.getParentDirectory().getChildFile ("Local");
    searchDirectories.add (localAppData.getChildFile ("Programs/Common/VST3").getFullPathName());
    searchDirectories.add (appDataDir.getChildFile ("VST3").getFullPathName());

    juce::Array<juce::var> pluginArray;
    juce::StringArray scannedNames;
    juce::StringArray scannedPaths;

    for (const auto& dirPath : searchDirectories)
    {
        juce::File dir (dirPath);
        if (! dir.isDirectory())
            continue;

        juce::Array<juce::File> foundFiles;
        dir.findChildFiles (foundFiles, juce::File::findFilesAndDirectories, true, "*.vst3");

        for (const auto& f : foundFiles)
        {
            auto filePath = f.getFullPathName();
            
            // Avoid duplicate scanning of inner binaries inside already-scanned .vst3 bundles
            if (filePath.containsIgnoreCase ("\\Contents\\") && ! f.isDirectory())
            {
                juce::File parentBundle = f.getParentDirectory().getParentDirectory().getParentDirectory();
                if (parentBundle.exists() && parentBundle.getFileName().endsWithIgnoreCase (".vst3"))
                {
                    filePath = parentBundle.getFullPathName();
                }
            }

            if (scannedPaths.contains (filePath))
                continue;
            scannedPaths.add (filePath);

            juce::String baseName = f.getFileNameWithoutExtension();
            if (baseName.equalsIgnoreCase ("Modular Voice Studio") || baseName.isEmpty())
                continue;

            bool added = false;
            try
            {
                juce::OwnedArray<juce::PluginDescription> descriptions;
                for (int i = 0; i < formatManager.getNumFormats(); ++i)
                {
                    auto* format = formatManager.getFormat (i);
                    if (format != nullptr && format->fileMightContainThisPluginType (filePath))
                    {
                        format->findAllTypesForFile (descriptions, filePath);
                    }
                }

                for (auto* desc : descriptions)
                {
                    if (desc != nullptr)
                    {
                        juce::String pName = desc->name.isNotEmpty() ? desc->name : baseName;
                        if (! scannedNames.contains (pName))
                        {
                            scannedNames.add (pName);
                            auto* obj = new juce::DynamicObject();
                            obj->setProperty ("name", pName);
                            obj->setProperty ("vendor", desc->manufacturerName.isNotEmpty() ? desc->manufacturerName : "VST3");
                            obj->setProperty ("category", desc->category.isNotEmpty() ? desc->category : "Fx");
                            obj->setProperty ("path", desc->fileOrIdentifier.isNotEmpty() ? desc->fileOrIdentifier : filePath);
                            obj->setProperty ("isInstrument", desc->isInstrument);
                            pluginArray.add (juce::var (obj));
                            added = true;
                        }
                    }
                }
            }
            catch (...)
            {
            }

            // Fallback entry if JUCE query returned 0 descriptions but the file is a valid VST3
            if (! added && ! scannedNames.contains (baseName))
            {
                scannedNames.add (baseName);
                juce::String vendor = f.getParentDirectory().getFileName();
                if (vendor.equalsIgnoreCase ("VST3") || vendor.isEmpty())
                    vendor = "VST3";

                auto* obj = new juce::DynamicObject();
                obj->setProperty ("name", baseName);
                obj->setProperty ("vendor", vendor);
                obj->setProperty ("category", "Fx");
                obj->setProperty ("path", filePath);
                obj->setProperty ("isInstrument", false);
                pluginArray.add (juce::var (obj));
            }
        }
    }

    juce::String jsonStr = juce::JSON::toString (juce::var (pluginArray));
    strncpy_s (outJsonBuffer, maxBufferLen, jsonStr.toRawUTF8(), _TRUNCATE);
    return (int) jsonStr.length();
}

// =============================================================================
// Dynamic Effect Chain API Implementations
// =============================================================================

MVS_API int MVS_EffectChain_AddStrip (const char* typeName, const char* optionalVstPath, char* outStripId, int maxIdLen)
{
    std::lock_guard<std::mutex> lock (gEngineMutex);
    if (gEngine == nullptr || gEngine->processor == nullptr || typeName == nullptr)
        return -1;

    juce::String t (juce::String::fromUTF8 (typeName));
    juce::String p = optionalVstPath != nullptr ? juce::String::fromUTF8 (optionalVstPath) : juce::String();

    int idx = gEngine->processor->getEffectChain().addStrip (t, p);
    if (idx >= 0 && outStripId != nullptr && maxIdLen > 0)
    {
        if (auto* s = gEngine->processor->getEffectChain().getStrip (idx))
        {
            strncpy_s (outStripId, maxIdLen, s->stripId.toRawUTF8(), _TRUNCATE);
        }
    }
    return idx;
}

MVS_API int MVS_EffectChain_RemoveStrip (int index)
{
    std::lock_guard<std::mutex> lock (gEngineMutex);
    if (gEngine == nullptr || gEngine->processor == nullptr)
        return 0;

    return gEngine->processor->getEffectChain().removeStrip (index) ? 1 : 0;
}

MVS_API void MVS_EffectChain_Clear()
{
    std::lock_guard<std::mutex> lock (gEngineMutex);
    if (gEngine != nullptr && gEngine->processor != nullptr)
    {
        gEngine->processor->getEffectChain().clear();
    }
}

MVS_API int MVS_EffectChain_GetCount()
{
    std::lock_guard<std::mutex> lock (gEngineMutex);
    if (gEngine == nullptr || gEngine->processor == nullptr)
        return 0;

    return gEngine->processor->getEffectChain().getNumStrips();
}

MVS_API int MVS_EffectChain_GetStripInfo (int index, char* outType, int maxTypeLen, char* outName, int maxNameLen, float* outGainLinear, float* outPan, int* outBypassed, int* outMuted, int* outSolo, float* outMeterL, float* outMeterR)
{
    std::lock_guard<std::mutex> lock (gEngineMutex);
    if (gEngine == nullptr || gEngine->processor == nullptr)
        return 0;

    auto* s = gEngine->processor->getEffectChain().getStrip (index);
    if (s == nullptr)
        return 0;

    if (outType != nullptr && maxTypeLen > 0)
        strncpy_s (outType, maxTypeLen, s->typeId.toRawUTF8(), _TRUNCATE);

    if (outName != nullptr && maxNameLen > 0)
        strncpy_s (outName, maxNameLen, s->customName.toRawUTF8(), _TRUNCATE);

    if (outGainLinear != nullptr) *outGainLinear = s->faderGainLinear.load();
    if (outPan != nullptr)        *outPan = s->pan.load();
    if (outBypassed != nullptr)   *outBypassed = s->isBypassed.load() ? 1 : 0;
    if (outMuted != nullptr)      *outMuted = s->isMuted.load() ? 1 : 0;
    if (outSolo != nullptr)       *outSolo = s->isSolo.load() ? 1 : 0;
    if (outMeterL != nullptr)     *outMeterL = s->meterPeakL.load();
    if (outMeterR != nullptr)     *outMeterR = s->meterPeakR.load();

    return 1;
}

MVS_API void MVS_EffectChain_SetStripGain (int index, float gainLinear)
{
    std::lock_guard<std::mutex> lock (gEngineMutex);
    if (gEngine != nullptr && gEngine->processor != nullptr)
    {
        if (auto* s = gEngine->processor->getEffectChain().getStrip (index))
            s->faderGainLinear.store (gainLinear);
    }
}

MVS_API void MVS_EffectChain_SetStripPan (int index, float pan)
{
    std::lock_guard<std::mutex> lock (gEngineMutex);
    if (gEngine != nullptr && gEngine->processor != nullptr)
    {
        if (auto* s = gEngine->processor->getEffectChain().getStrip (index))
            s->pan.store (pan);
    }
}

MVS_API void MVS_EffectChain_SetStripBypass (int index, int bypassed)
{
    std::lock_guard<std::mutex> lock (gEngineMutex);
    if (gEngine != nullptr && gEngine->processor != nullptr)
    {
        if (auto* s = gEngine->processor->getEffectChain().getStrip (index))
            s->isBypassed.store (bypassed != 0);
    }
}

MVS_API void MVS_EffectChain_SetStripMute (int index, int muted)
{
    std::lock_guard<std::mutex> lock (gEngineMutex);
    if (gEngine != nullptr && gEngine->processor != nullptr)
    {
        if (auto* s = gEngine->processor->getEffectChain().getStrip (index))
            s->isMuted.store (muted != 0);
    }
}

MVS_API void MVS_EffectChain_SetStripSolo (int index, int solo)
{
    std::lock_guard<std::mutex> lock (gEngineMutex);
    if (gEngine != nullptr && gEngine->processor != nullptr)
    {
        if (auto* s = gEngine->processor->getEffectChain().getStrip (index))
            s->isSolo.store (solo != 0);
    }
}

MVS_API void MVS_EffectChain_OpenStripEditor (int index)
{
    std::lock_guard<std::mutex> lock (gEngineMutex);
    if (gEngine != nullptr && gEngine->processor != nullptr)
    {
        gEngine->processor->getEffectChain().openStripEditor (index);
    }
}

MVS_API int MVS_EffectChain_GetStripParamCount (int index)
{
    std::lock_guard<std::mutex> lock (gEngineMutex);
    if (gEngine == nullptr || gEngine->processor == nullptr)
        return 0;

    return gEngine->processor->getEffectChain().getStripParamCount (index);
}

MVS_API int MVS_EffectChain_GetStripParamInfo (int stripIndex, int paramIndex,
                                              char* outParamId, int maxIdLen,
                                              char* outName, int maxNameLen,
                                              float* outVal, float* outMin, float* outMax,
                                              float* outDefault,
                                              char* outLabel, int maxLabelLen)
{
    std::lock_guard<std::mutex> lock (gEngineMutex);
    if (gEngine == nullptr || gEngine->processor == nullptr)
        return 0;

    juce::String id, name, label;
    float val = 0.0f, minV = 0.0f, maxV = 1.0f, defV = 0.0f;
    bool ok = gEngine->processor->getEffectChain().getStripParamInfo (
        stripIndex, paramIndex, id, name, val, minV, maxV, defV, label);

    if (! ok) return 0;

    if (outParamId != nullptr && maxIdLen > 0)
        strncpy_s (outParamId, maxIdLen, id.toRawUTF8(), _TRUNCATE);

    if (outName != nullptr && maxNameLen > 0)
        strncpy_s (outName, maxNameLen, name.toRawUTF8(), _TRUNCATE);

    if (outLabel != nullptr && maxLabelLen > 0)
        strncpy_s (outLabel, maxLabelLen, label.toRawUTF8(), _TRUNCATE);

    if (outVal != nullptr)     *outVal = val;
    if (outMin != nullptr)     *outMin = minV;
    if (outMax != nullptr)     *outMax = maxV;
    if (outDefault != nullptr) *outDefault = defV;

    return 1;
}

MVS_API void MVS_EffectChain_SetStripParamValue (int stripIndex, const char* paramId, float value)
{
    std::lock_guard<std::mutex> lock (gEngineMutex);
    if (gEngine != nullptr && gEngine->processor != nullptr && paramId != nullptr)
    {
        gEngine->processor->getEffectChain().setStripParamValue (stripIndex, juce::String::fromUTF8 (paramId), value);
    }
}

MVS_API void MVS_EffectChain_SetStripParamValueByIndex (int stripIndex, int paramIndex, float value)
{
    std::lock_guard<std::mutex> lock (gEngineMutex);
    if (gEngine != nullptr && gEngine->processor != nullptr)
    {
        gEngine->processor->getEffectChain().setStripParamValueByIndex (stripIndex, paramIndex, value);
    }
}

