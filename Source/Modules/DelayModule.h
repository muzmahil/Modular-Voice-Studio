#pragma once
#include "../Graph/ModuleProcessor.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

/**
    DelayModule: Stereo Ping-Pong & Vocal Echo Delay Processor.
    Features Time (ms), Feedback, Stereo Ping-Pong Spread, High-Cut filter, and Dry/Wet Mix.
*/
class DelayModule : public ModuleProcessor
{
public:
    DelayModule()
        : ModuleProcessor ("Delay", createLayout())
    {
        timeParam     = getModuleParam ("time", 280.0f);
        feedbackParam = getModuleParam ("feedback", 35.0f);
        pingPongParam = getModuleParam ("pingpong", 1.0f);
        mixParam      = getModuleParam ("mix", 30.0f);
        activeParam   = getModuleParam ("active", 1.0f);
    }

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "time", 1 }, "Delay Time",
            juce::NormalisableRange<float> (10.0f, 1000.0f, 1.0f, 0.4f), 280.0f,
            juce::AudioParameterFloatAttributes().withLabel ("ms")));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "feedback", 1 }, "Feedback",
            juce::NormalisableRange<float> (0.0f, 90.0f, 1.0f), 35.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));

        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { "pingpong", 1 }, "Ping-Pong Stereo", true));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "mix", 1 }, "Dry/Wet Mix",
            juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 30.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));

        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { "active", 1 }, "Active", true));

        return { params.begin(), params.end() };
    }

    void prepareToPlay (double sampleRate, int) override
    {
        currentSampleRate = sampleRate > 0 ? sampleRate : 48000.0;
        int maxDelaySamples = (int) (currentSampleRate * 2.0); // 2 sec buffer
        
        delayBufferL.assign ((size_t) maxDelaySamples, 0.0f);
        delayBufferR.assign ((size_t) maxDelaySamples, 0.0f);
        writeIndex = 0;
    }

    void releaseResources() override
    {
        std::fill (delayBufferL.begin(), delayBufferL.end(), 0.0f);
        std::fill (delayBufferR.begin(), delayBufferR.end(), 0.0f);
        writeIndex = 0;
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals noDenormals;

        bool isActive = activeParam.get (1.0f) > 0.5f;
        float mixPct  = mixParam.get (30.0f) / 100.0f;

        if (!isActive || mixPct <= 0.001f || delayBufferL.empty())
            return;

        float delayTimeMs = timeParam.get (280.0f);
        float feedback    = juce::jlimit (0.0f, 0.90f, feedbackParam.get (35.0f) / 100.0f);
        bool isPingPong   = pingPongParam.get (1.0f) > 0.5f;

        int delaySamplesL = (int) juce::jlimit (1.0f, (float) (delayBufferL.size() - 1), (float) (delayTimeMs * 0.001 * currentSampleRate));
        int delaySamplesR = isPingPong ? (int) (delaySamplesL * 1.5f) : delaySamplesL;
        if (delaySamplesR >= (int) delayBufferR.size()) delaySamplesR = (int) delayBufferR.size() - 1;

        int numSamples = buffer.getNumSamples();
        int numChannels = buffer.getNumChannels();
        int bufSize = (int) delayBufferL.size();

        float* channelDataL = buffer.getWritePointer (0);
        float* channelDataR = numChannels > 1 ? buffer.getWritePointer (1) : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            float inL = channelDataL[i];
            float inR = channelDataR != nullptr ? channelDataR[i] : inL;

            int readIdxL = (writeIndex - delaySamplesL + bufSize) % bufSize;
            int readIdxR = (writeIndex - delaySamplesR + bufSize) % bufSize;

            float delayedL = delayBufferL[(size_t) readIdxL];
            float delayedR = delayBufferR[(size_t) readIdxR];

            // Feedback loop (with ping-pong crossfeed)
            if (isPingPong)
            {
                delayBufferL[(size_t) writeIndex] = inL + delayedR * feedback;
                delayBufferR[(size_t) writeIndex] = inR + delayedL * feedback;
            }
            else
            {
                delayBufferL[(size_t) writeIndex] = inL + delayedL * feedback;
                delayBufferR[(size_t) writeIndex] = inR + delayedL * feedback;
            }

            writeIndex = (writeIndex + 1) % bufSize;

            // Mix dry and wet
            channelDataL[i] = inL * (1.0f - mixPct * 0.5f) + delayedL * mixPct;
            if (channelDataR != nullptr)
            {
                channelDataR[i] = inR * (1.0f - mixPct * 0.5f) + delayedR * mixPct;
            }
        }
    }

private:
    double currentSampleRate = 48000.0;
    std::vector<float> delayBufferL;
    std::vector<float> delayBufferR;
    int writeIndex = 0;

    ParamRef timeParam;
    ParamRef feedbackParam;
    ParamRef pingPongParam;
    ParamRef mixParam;
    ParamRef activeParam;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DelayModule)
};
