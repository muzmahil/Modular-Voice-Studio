#pragma once
#include "../Graph/ModuleProcessor.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>

/**
    ReverbModule: Full-featured acoustic studio vocal reverb.
    Parameters:
    - Pre-Delay (0 to 200 ms)
    - Room Size (0 to 100%)
    - Damping (0 to 100%)
    - Stereo Width (0 to 100%)
    - Low-Cut Filter (20 to 1000 Hz)
    - High-Cut Filter (1000 to 20000 Hz)
    - Early Reflections Level (0 to 100%)
    - Reverb Tail Level (0 to 100%)
    - Freeze Mode (On/Off)
    - Dry/Wet Mix (0 to 100%)
*/
class ReverbModule : public ModuleProcessor
{
public:
    ReverbModule()
        : ModuleProcessor ("Reverb", createLayout())
    {
        preDelayParam     = getModuleParam ("preDelay", 20.0f);
        roomSizeParam     = getModuleParam ("roomSize", 65.0f);
        dampingParam      = getModuleParam ("damping", 40.0f);
        widthParam        = getModuleParam ("width", 100.0f);
        lowCutParam       = getModuleParam ("lowCut", 100.0f);
        highCutParam      = getModuleParam ("highCut", 12000.0f);
        earlyReflectParam = getModuleParam ("earlyReflect", 60.0f);
        tailLevelParam    = getModuleParam ("tailLevel", 80.0f);
        freezeParam       = getModuleParam ("freeze", 0.0f);
        mixParam          = getModuleParam ("mix", 35.0f);
        activeParam       = getModuleParam ("active", 1.0f);
    }

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "preDelay", 1 }, "Pre-Delay",
            juce::NormalisableRange<float> (0.0f, 200.0f, 1.0f), 20.0f,
            juce::AudioParameterFloatAttributes().withLabel ("ms")));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "roomSize", 1 }, "Room Size / Decay",
            juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 65.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "damping", 1 }, "High Damping",
            juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 40.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "width", 1 }, "Stereo Width",
            juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "lowCut", 1 }, "Low Cut (HPF)",
            juce::NormalisableRange<float> (20.0f, 1000.0f, 1.0f, 0.4f), 100.0f,
            juce::AudioParameterFloatAttributes().withLabel ("Hz")));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "highCut", 1 }, "High Cut (LPF)",
            juce::NormalisableRange<float> (1000.0f, 20000.0f, 10.0f, 0.4f), 12000.0f,
            juce::AudioParameterFloatAttributes().withLabel ("Hz")));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "earlyReflect", 1 }, "Early Reflections",
            juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 60.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "tailLevel", 1 }, "Reverb Tail",
            juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 80.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));

        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { "freeze", 1 }, "Infinite Freeze", false));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "mix", 1 }, "Dry/Wet Mix",
            juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 35.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));

        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { "active", 1 }, "Active", true));

        return { params.begin(), params.end() };
    }

    void prepareToPlay (double sampleRate, int samplesPerBlock) override
    {
        currentSampleRate = sampleRate > 0 ? sampleRate : 48000.0;
        
        preDelayBuffer.setSize (2, (int)(currentSampleRate * 0.5)); // 500ms max pre-delay
        preDelayBuffer.clear();
        preDelayWrite = 0;

        reverb.setSampleRate (currentSampleRate);
        reverb.reset();

        juce::dsp::ProcessSpec spec;
        spec.sampleRate = currentSampleRate;
        spec.maximumBlockSize = (juce::uint32) (samplesPerBlock > 0 ? samplesPerBlock : 512);
        spec.numChannels = 2;

        lowCutFilterL.reset();
        lowCutFilterR.reset();
        highCutFilterL.reset();
        highCutFilterR.reset();

        updateFilters();
        updateReverbParams();
    }

    void releaseResources() override
    {
        reverb.reset();
        preDelayBuffer.setSize (0, 0);
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals noDenormals;

        bool isActive = activeParam.get (1.0f) > 0.5f;
        float mixPct  = mixParam.get (35.0f) / 100.0f;

        if (!isActive || mixPct <= 0.001f)
            return;

        if (preDelayBuffer.getNumSamples() < 1024)
            prepareToPlay (currentSampleRate > 0 ? currentSampleRate : 48000.0, 512);

        updateFilters();
        updateReverbParams();

        int numChannels = buffer.getNumChannels();
        int numSamples = buffer.getNumSamples();
        int pBufSize = preDelayBuffer.getNumSamples();
        if (pBufSize <= 0) return;

        float preDelayMs = preDelayParam.get (20.0f);
        int preDelaySamples = (int)(currentSampleRate * (preDelayMs * 0.001f));
        if (preDelaySamples >= pBufSize) preDelaySamples = pBufSize - 1;

        float earlyLvl = earlyReflectParam.get (60.0f) / 100.0f;
        float tailLvl  = tailLevelParam.get (80.0f) / 100.0f;

        // Allocate temporary wet buffer
        juce::AudioBuffer<float> wet (numChannels, numSamples);

        for (int i = 0; i < numSamples; ++i)
        {
            // Write to pre-delay buffer & read delayed
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float in = buffer.getSample (ch, i);
                preDelayBuffer.setSample (ch, preDelayWrite, in);

                int readPos = (preDelayWrite - preDelaySamples + pBufSize) % pBufSize;
                float delayed = preDelayBuffer.getSample (ch, readPos);
                wet.setSample (ch, i, delayed);
            }
            preDelayWrite = (preDelayWrite + 1) % pBufSize;
        }

        // Apply pre-reverb low-cut & high-cut shaping filters
        float* wetL = wet.getWritePointer (0);
        float* wetR = numChannels > 1 ? wet.getWritePointer (1) : nullptr;

        for (int i = 0; i < numSamples; ++i)
        {
            wetL[i] = lowCutFilterL.processSingleSampleRaw (wetL[i]);
            wetL[i] = highCutFilterL.processSingleSampleRaw (wetL[i]);

            if (wetR != nullptr)
            {
                wetR[i] = lowCutFilterR.processSingleSampleRaw (wetR[i]);
                wetR[i] = highCutFilterR.processSingleSampleRaw (wetR[i]);
            }
        }

        // Process stereo reverb tank
        if (numChannels >= 2)
        {
            reverb.processStereo (wet.getWritePointer (0), wet.getWritePointer (1), numSamples);
        }
        else if (numChannels == 1)
        {
            reverb.processMono (wet.getWritePointer (0), numSamples);
        }

        // Blend dry and processed wet with early/tail balance
        float wetScale = tailLvl * 1.2f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* dryPtr = buffer.getWritePointer (ch);
            const auto* wetPtr = wet.getReadPointer (ch);

            for (int i = 0; i < numSamples; ++i)
            {
                float early = dryPtr[i] * earlyLvl * 0.15f;
                float w = wetPtr[i] * wetScale + early;
                dryPtr[i] = dryPtr[i] * (1.0f - mixPct * 0.5f) + w * mixPct;
            }
        }
    }

private:
    void updateReverbParams()
    {
        juce::Reverb::Parameters p;
        p.roomSize   = juce::jlimit (0.0f, 1.0f, roomSizeParam.get (65.0f) / 100.0f);
        p.damping    = juce::jlimit (0.0f, 1.0f, dampingParam.get (40.0f) / 100.0f);
        p.width      = juce::jlimit (0.0f, 1.0f, widthParam.get (100.0f) / 100.0f);
        p.wetLevel   = 1.0f;
        p.dryLevel   = 0.0f;
        p.freezeMode = freezeParam.get (0.0f) > 0.5f ? 1.0f : 0.0f;
        reverb.setParameters (p);
    }

    void updateFilters()
    {
        float lowHz  = lowCutParam.get (100.0f);
        float highHz = highCutParam.get (12000.0f);

        auto hpCoeffs = juce::IIRCoefficients::makeHighPass (currentSampleRate, (double) juce::jlimit (20.0f, 2000.0f, lowHz));
        lowCutFilterL.setCoefficients (hpCoeffs);
        lowCutFilterR.setCoefficients (hpCoeffs);

        auto lpCoeffs = juce::IIRCoefficients::makeLowPass (currentSampleRate, (double) juce::jlimit (500.0f, 22000.0f, highHz));
        highCutFilterL.setCoefficients (lpCoeffs);
        highCutFilterR.setCoefficients (lpCoeffs);
    }

    ParamRef preDelayParam;
    ParamRef roomSizeParam;
    ParamRef dampingParam;
    ParamRef widthParam;
    ParamRef lowCutParam;
    ParamRef highCutParam;
    ParamRef earlyReflectParam;
    ParamRef tailLevelParam;
    ParamRef freezeParam;
    ParamRef mixParam;
    ParamRef activeParam;

    double currentSampleRate = 48000.0;
    juce::Reverb reverb;
    juce::AudioBuffer<float> preDelayBuffer;
    int preDelayWrite = 0;

    juce::IIRFilter lowCutFilterL;
    juce::IIRFilter lowCutFilterR;
    juce::IIRFilter highCutFilterL;
    juce::IIRFilter highCutFilterR;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ReverbModule)
};
