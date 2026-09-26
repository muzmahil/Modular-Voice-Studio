#pragma once
#include "../Graph/ModuleProcessor.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

/**
    RobotVoiceModule: Sci-Fi / Robotic / Ring Modulation / Bitcrush Vocal Processor.
    Transforms spoken voice into robotic, synthesized or cybernetic characters.
*/
class RobotVoiceModule : public ModuleProcessor
{
public:
    RobotVoiceModule()
        : ModuleProcessor ("Robot Voice", createLayout())
    {
        carrierFreqParam = getModuleParam ("freq", 120.0f);
        bitDepthParam    = getModuleParam ("crush", 0.0f);
        mixParam         = getModuleParam ("mix", 60.0f);
        activeParam      = getModuleParam ("active", 1.0f);
    }

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "freq", 1 }, "Modulation Pitch",
            juce::NormalisableRange<float> (30.0f, 800.0f, 1.0f, 0.5f), 120.0f,
            juce::AudioParameterFloatAttributes().withLabel ("Hz")));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "crush", 1 }, "Digital Grit (Crush)",
            juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "mix", 1 }, "Dry/Wet Mix",
            juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 60.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));

        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { "active", 1 }, "Active", true));

        return { params.begin(), params.end() };
    }

    void prepareToPlay (double sampleRate, int) override
    {
        currentSampleRate = sampleRate > 0 ? sampleRate : 48000.0;
        carrierPhase = 0.0f;
    }

    void releaseResources() override
    {
        carrierPhase = 0.0f;
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals noDenormals;

        bool isActive = activeParam.get (1.0f) > 0.5f;
        float mixPct  = mixParam.get (60.0f) / 100.0f;

        if (!isActive || mixPct <= 0.001f)
            return;

        float freq = carrierFreqParam.get (120.0f);
        float phaseInc = (float) (juce::MathConstants<double>::twoPi * freq / currentSampleRate);
        float crushPct = bitDepthParam.get (0.0f) / 100.0f;
        float quantLevels = crushPct > 0.01f ? (16.0f - crushPct * 12.0f) : 64.0f;

        int numSamples = buffer.getNumSamples();
        int numChannels = buffer.getNumChannels();

        float* channelDataL = buffer.getWritePointer (0);
        float* channelDataR = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            float carrier = std::sin (carrierPhase);
            carrierPhase += phaseInc;
            if (carrierPhase >= juce::MathConstants<float>::twoPi)
                carrierPhase -= juce::MathConstants<float>::twoPi;

            float inL = channelDataL[i];
            float inR = channelDataR != nullptr ? channelDataR[i] : inL;

            // Ring modulation with square wave blend for robotic timbre
            float modL = inL * (carrier * 0.8f + (carrier > 0.0f ? 0.3f : -0.3f));
            float modR = inR * (carrier * 0.8f + (carrier > 0.0f ? 0.3f : -0.3f));

            // Bitcrush / grit if active
            if (crushPct > 0.05f)
            {
                modL = std::round (modL * quantLevels) / quantLevels;
                modR = std::round (modR * quantLevels) / quantLevels;
            }

            channelDataL[i] = inL * (1.0f - mixPct) + modL * mixPct;
            if (channelDataR != nullptr)
            {
                channelDataR[i] = inR * (1.0f - mixPct) + modR * mixPct;
            }
        }
    }

private:
    double currentSampleRate = 48000.0;
    float carrierPhase = 0.0f;

    ParamRef carrierFreqParam;
    ParamRef bitDepthParam;
    ParamRef mixParam;
    ParamRef activeParam;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RobotVoiceModule)
};
