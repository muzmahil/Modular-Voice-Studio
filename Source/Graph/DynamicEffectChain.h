#pragma once
#include "ModuleProcessor.h"
#include "ModuleFactory.h"
#include "../Modules/VSTPluginModule.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include <memory>
#include <mutex>
#include <atomic>
#include <cmath>

struct DynamicStrip
{
    juce::String stripId;
    juce::String typeId;
    juce::String customName;
    std::unique_ptr<ModuleProcessor> processor;
    
    std::atomic<float> faderGainLinear { 1.0f };
    std::atomic<float> pan { 0.0f }; // -1.0 (Left) to +1.0 (Right)
    std::atomic<bool> isBypassed { false };
    std::atomic<bool> isMuted { false };
    std::atomic<bool> isSolo { false };

    std::atomic<float> meterPeakL { 0.0f };
    std::atomic<float> meterPeakR { 0.0f };
};

/**
    DynamicEffectChain: Serial DSP rack that allows users to dynamically
    add, configure, and blend effect channel strips in series.
    Audio flows: Input -> Strip 0 -> Strip 1 -> ... -> Strip N -> Output.
    Fader acts as smooth Wet/Dry Effect Level to ensure voice is never cut.
*/
class DynamicEffectChain
{
public:
    DynamicEffectChain() = default;
    ~DynamicEffectChain()
    {
        clear();
    }

    void prepareToPlay (double sampleRate, int samplesPerBlock)
    {
        currentSampleRate = sampleRate > 0 ? sampleRate : 48000.0;
        currentBlockSize = samplesPerBlock > 0 ? samplesPerBlock : 512;

        std::lock_guard<std::mutex> lock (chainMutex);
        tempWetBuffer.setSize (2, currentBlockSize);
        for (auto& s : strips)
        {
            if (s->processor != nullptr)
                s->processor->prepareToPlay (currentSampleRate, currentBlockSize);
        }
    }

    void releaseResources()
    {
        std::lock_guard<std::mutex> lock (chainMutex);
        for (auto& s : strips)
        {
            if (s->processor != nullptr)
                s->processor->releaseResources();
        }
        tempWetBuffer.setSize (0, 0);
    }

    int addStrip (const juce::String& typeName, const juce::String& optionalVstPath = {})
    {
        std::unique_ptr<ModuleProcessor> proc;
        juce::String finalName = typeName;

        if (typeName.equalsIgnoreCase ("VST3 Host") || optionalVstPath.isNotEmpty())
        {
            auto vstModule = std::make_unique<VSTPluginModule>();
            if (optionalVstPath.isNotEmpty())
            {
                juce::String err;
                if (vstModule->loadPluginFile (optionalVstPath, err))
                {
                    finalName = vstModule->getLoadedPluginName();
                }
            }
            proc = std::move (vstModule);
        }
        else
        {
            proc = ModuleFactory::instance().create (typeName);
        }

        if (proc == nullptr)
            return -1;

        if (currentSampleRate > 0 && currentBlockSize > 0)
            proc->prepareToPlay (currentSampleRate, currentBlockSize);

        auto strip = std::make_unique<DynamicStrip>();
        strip->stripId = juce::Uuid().toString();
        strip->typeId = typeName;
        strip->customName = finalName;
        strip->processor = std::move (proc);

        std::lock_guard<std::mutex> lock (chainMutex);
        strips.push_back (std::move (strip));
        return (int) strips.size() - 1;
    }

    bool removeStrip (int index)
    {
        std::lock_guard<std::mutex> lock (chainMutex);
        if (index >= 0 && index < (int) strips.size())
        {
            strips.erase (strips.begin() + index);
            return true;
        }
        return false;
    }

    void clear()
    {
        std::lock_guard<std::mutex> lock (chainMutex);
        strips.clear();
    }

    int getNumStrips() const
    {
        std::lock_guard<std::mutex> lock (chainMutex);
        return (int) strips.size();
    }

    DynamicStrip* getStrip (int index)
    {
        std::lock_guard<std::mutex> lock (chainMutex);
        if (index >= 0 && index < (int) strips.size())
            return strips[(size_t) index].get();
        return nullptr;
    }

    void openStripEditor (int index)
    {
        std::lock_guard<std::mutex> lock (chainMutex);
        if (index >= 0 && index < (int) strips.size())
        {
            auto* s = strips[(size_t) index].get();
            if (s != nullptr && s->processor != nullptr)
            {
                if (s->typeId.equalsIgnoreCase ("VST3 Host") || s->typeId.containsIgnoreCase ("VST"))
                {
                    if (auto* vst = dynamic_cast<VSTPluginModule*> (s->processor.get()))
                    {
                        vst->openEditorWindow();
                    }
                }
            }
        }
    }

    int getStripParamCount (int index)
    {
        std::lock_guard<std::mutex> lock (chainMutex);
        if (index >= 0 && index < (int) strips.size())
        {
            if (auto* proc = strips[(size_t) index]->processor.get())
            {
                return (int) proc->getParameters().size();
            }
        }
        return 0;
    }

    bool getStripParamInfo (int stripIndex, int paramIndex,
                            juce::String& outId,
                            juce::String& outName,
                            float& outVal,
                            float& outMin,
                            float& outMax,
                            float& outDefault,
                            juce::String& outLabel)
    {
        std::lock_guard<std::mutex> lock (chainMutex);
        if (stripIndex >= 0 && stripIndex < (int) strips.size())
        {
            if (auto* proc = strips[(size_t) stripIndex]->processor.get())
            {
                const auto& params = proc->getParameters();
                if (paramIndex >= 0 && paramIndex < params.size())
                {
                    auto* p = params[paramIndex];
                    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
                    {
                        juce::String id = ranged->getParameterID();
                        if (id.contains (";")) id = id.upToFirstOccurrenceOf (";", false, false);
                        outId = id;
                        outName = ranged->getName (128);
                        auto range = ranged->getNormalisableRange();
                        outMin = range.start;
                        outMax = range.end;
                        outDefault = range.convertFrom0to1 (ranged->getDefaultValue());
                        outVal = range.convertFrom0to1 (ranged->getValue());
                        outLabel = ranged->getLabel();
                        return true;
                    }
                    else
                    {
                        outId = juce::String (paramIndex);
                        outName = p->getName (128);
                        outMin = 0.0f;
                        outMax = 1.0f;
                        outDefault = p->getDefaultValue();
                        outVal = p->getValue();
                        outLabel = p->getLabel();
                        return true;
                    }
                }
            }
        }
        return false;
    }

    void setStripParamValue (int stripIndex, const juce::String& paramId, float value)
    {
        std::lock_guard<std::mutex> lock (chainMutex);
        if (stripIndex >= 0 && stripIndex < (int) strips.size())
        {
            if (auto* proc = strips[(size_t) stripIndex]->processor.get())
            {
                juce::String cleanId = paramId;
                if (cleanId.contains (";")) cleanId = cleanId.upToFirstOccurrenceOf (";", false, false);

                for (auto* p : proc->getParameters())
                {
                    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
                    {
                        juce::String pid = ranged->getParameterID();
                        if (pid.contains (";")) pid = pid.upToFirstOccurrenceOf (";", false, false);
                        if (pid.equalsIgnoreCase (cleanId) || ranged->getParameterID().equalsIgnoreCase (paramId))
                        {
                            auto range = ranged->getNormalisableRange();
                            float clamped = juce::jlimit (range.start, range.end, value);
                            float norm = range.convertTo0to1 (clamped);
                            ranged->setValue (norm);
                            ranged->setValueNotifyingHost (norm);
                            break;
                        }
                    }
                }

                if (auto* mp = dynamic_cast<ModuleProcessor*> (proc))
                {
                    if (auto* raw = mp->getRawParam (cleanId))
                        raw->store (value);
                    if (auto* raw2 = mp->getRawParam (paramId))
                        raw2->store (value);
                }
            }
        }
    }

    void setStripParamValueByIndex (int stripIndex, int paramIndex, float value)
    {
        std::lock_guard<std::mutex> lock (chainMutex);
        if (stripIndex >= 0 && stripIndex < (int) strips.size())
        {
            if (auto* proc = strips[(size_t) stripIndex]->processor.get())
            {
                const auto& params = proc->getParameters();
                if (paramIndex >= 0 && paramIndex < params.size())
                {
                    auto* p = params[paramIndex];
                    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
                    {
                        auto range = ranged->getNormalisableRange();
                        float clamped = juce::jlimit (range.start, range.end, value);
                        float norm = range.convertTo0to1 (clamped);
                        ranged->setValue (norm);
                        ranged->setValueNotifyingHost (norm);

                        if (auto* mp = dynamic_cast<ModuleProcessor*> (proc))
                        {
                            juce::String id = ranged->getParameterID();
                            if (id.contains (";")) id = id.upToFirstOccurrenceOf (";", false, false);
                            if (auto* raw = mp->getRawParam (id))
                                raw->store (value);
                            if (auto* raw2 = mp->getRawParam (ranged->getParameterID()))
                                raw2->store (value);
                        }
                    }
                    else
                    {
                        p->setValue (value);
                        p->setValueNotifyingHost (value);
                    }
                }
            }
        }
    }

    void processChain (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
    {
        std::unique_lock<std::mutex> lock (chainMutex, std::try_to_lock);
        if (! lock.owns_lock() || strips.empty())
            return;

        int numSamples = buffer.getNumSamples();
        int numChannels = buffer.getNumChannels();
        if (numSamples <= 0 || numChannels <= 0)
            return;

        if (tempWetBuffer.getNumChannels() < numChannels || tempWetBuffer.getNumSamples() < numSamples)
        {
            tempWetBuffer.setSize (numChannels, numSamples, false, true, true);
        }

        bool anySolo = false;
        for (const auto& s : strips)
        {
            if (s->isSolo.load())
            {
                anySolo = true;
                break;
            }
        }

        for (auto& s : strips)
        {
            bool isMuted = s->isMuted.load();
            bool isSolo = s->isSolo.load();
            bool isBypassed = s->isBypassed.load();

            if (anySolo && !isSolo)
            {
                // Silenced by solo on another strip: dry voice unaffected
                s->meterPeakL.store (0.0f);
                s->meterPeakR.store (0.0f);
                continue;
            }

            if (isMuted || isBypassed || s->processor == nullptr)
            {
                // When muted or bypassed, dry voice passes through untouched
                s->meterPeakL.store (0.0f);
                s->meterPeakR.store (0.0f);
                continue;
            }

            // Copy dry stream to wet buffer for DSP processing
            for (int ch = 0; ch < numChannels; ++ch)
            {
                tempWetBuffer.copyFrom (ch, 0, buffer.getReadPointer (ch), numSamples);
            }

            // Process effect module
            s->processor->processBlock (tempWetBuffer, midi);

            float faderGain = s->faderGainLinear.load();
            float panVal = s->pan.load(); // -1.0 to +1.0
            float panL = std::cos ((panVal + 1.0f) * (juce::MathConstants<float>::halfPi * 0.5f));
            float panR = std::sin ((panVal + 1.0f) * (juce::MathConstants<float>::halfPi * 0.5f));

            if (numChannels >= 2)
            {
                tempWetBuffer.applyGain (0, 0, numSamples, panL);
                tempWetBuffer.applyGain (1, 0, numSamples, panR);
            }

            // Effect Rack Wet/Dry Blend:
            // Fader at 1.0 (0 dB): 100% processed effect
            // Fader at 0.0 (-inf dB): 0% effect / 100% clean voice (voice is never muted!)
            // 0.0 < Fader < 1.0: smooth blend between dry incoming voice and wet effect
            if (faderGain < 1.0f)
            {
                float wetAmount = juce::jlimit (0.0f, 1.0f, faderGain);
                float dryAmount = 1.0f - wetAmount;

                for (int ch = 0; ch < numChannels; ++ch)
                {
                    auto* dryPtr = buffer.getWritePointer (ch);
                    const auto* wetPtr = tempWetBuffer.getReadPointer (ch);

                    for (int i = 0; i < numSamples; ++i)
                    {
                        dryPtr[i] = dryPtr[i] * dryAmount + wetPtr[i] * wetAmount;
                    }
                }
            }
            else
            {
                // Boost mode (> 0 dB)
                for (int ch = 0; ch < numChannels; ++ch)
                {
                    auto* dryPtr = buffer.getWritePointer (ch);
                    const auto* wetPtr = tempWetBuffer.getReadPointer (ch);

                    for (int i = 0; i < numSamples; ++i)
                    {
                        dryPtr[i] = wetPtr[i] * faderGain;
                    }
                }
            }

            // Measure live output meter of this strip
            float peakL = tempWetBuffer.getMagnitude (0, 0, numSamples);
            float peakR = numChannels > 1 ? tempWetBuffer.getMagnitude (1, 0, numSamples) : peakL;
            
            float prevL = s->meterPeakL.load();
            float prevR = s->meterPeakR.load();
            s->meterPeakL.store (peakL > prevL ? peakL : prevL * 0.88f);
            s->meterPeakR.store (peakR > prevR ? peakR : prevR * 0.88f);
        }
    }

private:
    mutable std::mutex chainMutex;
    std::vector<std::unique_ptr<DynamicStrip>> strips;
    juce::AudioBuffer<float> tempWetBuffer;
    double currentSampleRate = 48000.0;
    int currentBlockSize = 512;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DynamicEffectChain)
};
