#pragma once
#include "../Graph/ModuleProcessor.h"
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>

/**
    PitchShiftModule: Studio-grade real-time vocal pitch & formant shifter.
    Features:
    - Semitone Pitch Shift (-24 to +24 st)
    - Fine-Tuning Cents (-100 to +100 ct)
    - Formant / Timbre Shift (-12 to +12 st)
    - Grain Window Length (10 to 80 ms)
    - Dry/Wet Mix (0 to 100%)
*/
class PitchShiftModule : public ModuleProcessor
{
public:
    PitchShiftModule()
        : ModuleProcessor ("Pitch Shifter", createLayout())
    {
        pitchParam   = getModuleParam ("pitch", 0.0f);
        fineParam    = getModuleParam ("fine", 0.0f);
        formantParam = getModuleParam ("formant", 0.0f);
        grainParam   = getModuleParam ("grain", 15.0f);
        mixParam     = getModuleParam ("mix", 100.0f);
        activeParam  = getModuleParam ("active", 1.0f);
    }

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "pitch", 1 }, "Pitch Shift",
            juce::NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("st")));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "fine", 1 }, "Fine Tune",
            juce::NormalisableRange<float> (-100.0f, 100.0f, 1.0f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("ct")));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "formant", 1 }, "Formant / Gender",
            juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("st")));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "grain", 1 }, "Grain Window",
            juce::NormalisableRange<float> (4.0f, 50.0f, 1.0f), 15.0f,
            juce::AudioParameterFloatAttributes().withLabel ("ms")));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "mix", 1 }, "Dry/Wet Mix",
            juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f,
            juce::AudioParameterFloatAttributes().withLabel ("%")));

        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { "active", 1 }, "Active", true));

        return { params.begin(), params.end() };
    }

    void prepareToPlay (double sampleRate, int) override
    {
        currentSampleRate = sampleRate > 0 ? sampleRate : 48000.0;
        
        bufferSize = (int)(currentSampleRate * 0.5); // 500ms max buffer
        if (bufferSize < 4096) bufferSize = 4096;

        delayBuffer.setSize (2, bufferSize);
        delayBuffer.clear();
        writePos = 0;

        phase1 = 0.0f;
        phase2 = 0.5f;

        // Reset formant filter states
        for (int ch = 0; ch < 2; ++ch)
        {
            formantZ1[ch] = 0.0f;
            formantZ2[ch] = 0.0f;
            formantZ3[ch] = 0.0f;
            formantZ4[ch] = 0.0f;
        }
    }

    void releaseResources() override
    {
        delayBuffer.setSize (0, 0);
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        juce::ScopedNoDenormals noDenormals;

        bool isActive   = activeParam.get (1.0f) > 0.5f;
        float pitchSemi = pitchParam.get (0.0f);
        float fineCt    = fineParam.get (0.0f);
        float formantSt = formantParam.get (0.0f);
        float grainMs   = grainParam.get (15.0f);
        float mixPct    = mixParam.get (100.0f) / 100.0f;
        float totalSemi = pitchSemi + (fineCt / 100.0f);

        if (delayBuffer.getNumSamples() < 4096 || bufferSize <= 0)
            prepareToPlay (currentSampleRate > 0 ? currentSampleRate : 48000.0, 512);

        if (!isActive || mixPct <= 0.001f || bufferSize <= 0)
            return;

        // Low latency zero-shift bypass
        if (std::abs (totalSemi) < 0.01f && std::abs (formantSt) < 0.01f)
            return;

        float pitchRatio = std::pow (2.0f, totalSemi / 12.0f);
        float deltaRate = 1.0f - pitchRatio;

        int currentGrainSamples = (int)(currentSampleRate * (grainMs * 0.001f));
        if (currentGrainSamples < 64) currentGrainSamples = 64;
        if (currentGrainSamples > bufferSize / 4) currentGrainSamples = bufferSize / 4;
        float grainLen = (float)currentGrainSamples;
        float baseOffset = grainLen * 0.5f; // Ultra low latency grain lookback

        // Formant filter setup
        float formantRatio = std::pow (2.0f, formantSt / 12.0f);
        float f1Center = juce::jlimit (200.0f, (float)(currentSampleRate * 0.45), 700.0f * formantRatio);
        float formantGainDb = std::abs (formantSt) > 0.1f ? (formantSt > 0 ? 4.5f : -4.5f) : 0.0f;
        
        // Low formant peaking biquad coefficients
        float w0_1 = (float)(juce::MathConstants<double>::twoPi * f1Center / currentSampleRate);
        float cos1 = std::cos (w0_1);
        float sin1 = std::sin (w0_1);
        float A1 = std::pow (10.0f, formantGainDb / 40.0f);
        float alpha1 = sin1 / (2.0f * 1.8f); // Q = 1.8

        float b0_1 = 1.0f + alpha1 * A1;
        float b1_1 = -2.0f * cos1;
        float b2_1 = 1.0f - alpha1 * A1;
        float a0_1 = 1.0f + alpha1 / A1;
        float a1_1 = -2.0f * cos1;
        float a2_1 = 1.0f - alpha1 / A1;

        b0_1 /= a0_1; b1_1 /= a0_1; b2_1 /= a0_1;
        a1_1 /= a0_1; a2_1 /= a0_1;

        int numChannels = buffer.getNumChannels();
        int numSamples = buffer.getNumSamples();

        for (int i = 0; i < numSamples; ++i)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                delayBuffer.setSample (ch, writePos, buffer.getSample (ch, i));
            }

            float d1 = phase1 * grainLen;
            float d2 = phase2 * grainLen;

            // Smooth Hann crossfade envelope
            float w1 = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi * phase1));
            float w2 = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::twoPi * phase2));

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float dry = buffer.getSample (ch, i);

                // Tap 1 with linear interpolation
                float rPos1 = (float)writePos - baseOffset - d1;
                while (rPos1 < 0.0f) rPos1 += (float)bufferSize;
                while (rPos1 >= (float)bufferSize) rPos1 -= (float)bufferSize;

                int iPos1 = (int)rPos1;
                float frac1 = rPos1 - (float)iPos1;
                int nextPos1 = (iPos1 + 1) % bufferSize;
                float s1 = delayBuffer.getSample (ch, iPos1) * (1.0f - frac1) + delayBuffer.getSample (ch, nextPos1) * frac1;

                // Tap 2 with linear interpolation
                float rPos2 = (float)writePos - baseOffset - d2;
                while (rPos2 < 0.0f) rPos2 += (float)bufferSize;
                while (rPos2 >= (float)bufferSize) rPos2 -= (float)bufferSize;

                int iPos2 = (int)rPos2;
                float frac2 = rPos2 - (float)iPos2;
                int nextPos2 = (iPos2 + 1) % bufferSize;
                float s2 = delayBuffer.getSample (ch, iPos2) * (1.0f - frac2) + delayBuffer.getSample (ch, nextPos2) * frac2;

                float wet = (s1 * w1 + s2 * w2);

                // Apply real-time Formant resonator filtering if active
                if (std::abs (formantSt) > 0.1f)
                {
                    int c = ch < 2 ? ch : 0;
                    float filtered = b0_1 * wet + formantZ1[c];
                    formantZ1[c] = b1_1 * wet - a1_1 * filtered + formantZ2[c];
                    formantZ2[c] = b2_1 * wet - a2_1 * filtered;
                    wet = filtered;
                }

                float out = dry * (1.0f - mixPct) + wet * mixPct;
                buffer.setSample (ch, i, out);
            }

            phase1 += deltaRate / grainLen;
            while (phase1 >= 1.0f) phase1 -= 1.0f;
            while (phase1 < 0.0f) phase1 += 1.0f;

            phase2 += deltaRate / grainLen;
            while (phase2 >= 1.0f) phase2 -= 1.0f;
            while (phase2 < 0.0f) phase2 += 1.0f;

            writePos = (writePos + 1) % bufferSize;
        }
    }

private:
    ParamRef pitchParam;
    ParamRef fineParam;
    ParamRef formantParam;
    ParamRef grainParam;
    ParamRef mixParam;
    ParamRef activeParam;

    double currentSampleRate = 48000.0;
    juce::AudioBuffer<float> delayBuffer;
    int bufferSize = 16384;
    int writePos = 0;
    float phase1 = 0.0f;
    float phase2 = 0.5f;

    // Formant filter state variables
    float formantZ1[2] { 0.0f, 0.0f };
    float formantZ2[2] { 0.0f, 0.0f };
    float formantZ3[2] { 0.0f, 0.0f };
    float formantZ4[2] { 0.0f, 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchShiftModule)
};
