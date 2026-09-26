#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

/**
    Base class for every "mini-plugin" node that can live inside the graph
    (Compressor, Limiter, DeReverb, etc.). Each one is a real juce::AudioProcessor,
    so it can be dropped straight into juce::AudioProcessorGraph as a node.

    Concrete modules just override the DSP + parameter layout; all the
    AudioProcessor boilerplate lives here.
*/
class ModuleProcessor : public juce::AudioProcessor
{
public:
    ModuleProcessor (const juce::String& displayName,
                      juce::AudioProcessorValueTreeState::ParameterLayout layout)
        : ModuleProcessor (displayName,
                           BusesProperties()
                               .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                               .withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                           std::move (layout))
    {
    }

    ModuleProcessor (const juce::String& displayName,
                      BusesProperties buses,
                      juce::AudioProcessorValueTreeState::ParameterLayout layout)
        : juce::AudioProcessor (buses),
          name (displayName),
          apvts (*this, nullptr, "PARAMS", std::move (layout))
    {
    }

    ~ModuleProcessor() override = default;

    // -- identity ----------------------------------------------------
    const juce::String getName() const override { return name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // -- editor --------------------------------------------------------
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override
    {
        return new juce::GenericAudioProcessorEditor (*this);
    }

    // -- programs --------------------------------------------------------
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    // -- state --------------------------------------------------------
    void getStateInformation (juce::MemoryBlock& destData) override
    {
        if (auto state = apvts.copyState().createXml())
            copyXmlToBinary (*state, destData);
    }

    void setStateInformation (const void* data, int sizeInBytes) override
    {
        if (auto xml = getXmlFromBinary (data, sizeInBytes))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
            && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    const juce::AudioProcessorValueTreeState& getAPVTS() const { return apvts; }

    std::atomic<float>* getRawParam (const juce::String& paramId)
    {
        if (auto* raw = apvts.getRawParameterValue (paramId))
            return raw;
        if (auto* raw2 = apvts.getRawParameterValue (paramId + ";1"))
            return raw2;

        juce::String cleanTarget = paramId;
        if (cleanTarget.contains (";")) cleanTarget = cleanTarget.upToFirstOccurrenceOf (";", false, false);

        for (auto* p : getParameters())
        {
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                juce::String pid = ranged->getParameterID();
                juce::String cleanPid = pid;
                if (cleanPid.contains (";")) cleanPid = cleanPid.upToFirstOccurrenceOf (";", false, false);

                if (cleanPid.equalsIgnoreCase (cleanTarget) || pid.equalsIgnoreCase (paramId))
                {
                    if (auto* raw3 = apvts.getRawParameterValue (pid))
                        return raw3;
                    if (auto* raw4 = apvts.getRawParameterValue (cleanPid))
                        return raw4;
                    if (auto* raw5 = apvts.getRawParameterValue (cleanPid + ";1"))
                        return raw5;
                }
            }
        }
        return nullptr;
    }

    juce::RangedAudioParameter* getRangedParam (const juce::String& paramId)
    {
        juce::String cleanTarget = paramId;
        if (cleanTarget.contains (";")) cleanTarget = cleanTarget.upToFirstOccurrenceOf (";", false, false);

        for (auto* p : getParameters())
        {
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                juce::String pid = ranged->getParameterID();
                juce::String cleanPid = pid;
                if (cleanPid.contains (";")) cleanPid = cleanPid.upToFirstOccurrenceOf (";", false, false);

                if (cleanPid.equalsIgnoreCase (cleanTarget) || pid.equalsIgnoreCase (paramId))
                    return ranged;
            }
        }
        return nullptr;
    }

    float getParamValue (const juce::String& paramId, float defaultVal = 0.0f) const
    {
        juce::String cleanTarget = paramId;
        if (cleanTarget.contains (";")) cleanTarget = cleanTarget.upToFirstOccurrenceOf (";", false, false);

        for (auto* p : getParameters())
        {
            if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                juce::String pid = ranged->getParameterID();
                juce::String cleanPid = pid;
                if (cleanPid.contains (";")) cleanPid = cleanPid.upToFirstOccurrenceOf (";", false, false);

                if (cleanPid.equalsIgnoreCase (cleanTarget) || pid.equalsIgnoreCase (paramId))
                {
                    return ranged->getNormalisableRange().convertFrom0to1 (ranged->getValue());
                }
            }
        }

        if (auto* raw = const_cast<ModuleProcessor*> (this)->getRawParam (paramId))
            return raw->load();

        return defaultVal;
    }

    struct ParamRef
    {
        juce::RangedAudioParameter* ranged = nullptr;
        std::atomic<float>* raw = nullptr;
        float defaultValue = 0.0f;

        inline float get (float fallbackVal = 0.0f) const noexcept
        {
            if (ranged != nullptr)
                return ranged->getNormalisableRange().convertFrom0to1 (ranged->getValue());
            if (raw != nullptr)
                return raw->load (std::memory_order_relaxed);
            return fallbackVal != 0.0f ? fallbackVal : defaultValue;
        }

        inline void set (float userVal) noexcept
        {
            if (ranged != nullptr)
            {
                auto range = ranged->getNormalisableRange();
                float norm = range.convertTo0to1 (juce::jlimit (range.start, range.end, userVal));
                ranged->setValue (norm);
                ranged->setValueNotifyingHost (norm);
            }
            if (raw != nullptr)
                raw->store (userVal, std::memory_order_relaxed);
        }

        inline operator float() const noexcept { return get(); }
    };

    ParamRef getModuleParam (const juce::String& paramId, float defVal = 0.0f)
    {
        ParamRef p;
        p.ranged = getRangedParam (paramId);
        p.raw = getRawParam (paramId);
        p.defaultValue = defVal;
        return p;
    }

protected:
    juce::String name;
    juce::AudioProcessorValueTreeState apvts;
};
