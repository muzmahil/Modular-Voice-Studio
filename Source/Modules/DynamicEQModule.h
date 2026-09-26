#pragma once
#include "../Graph/ModuleProcessor.h"
#include "../UITheme.h"
#include <cmath>

class DynamicEQModuleEditor;

class DynamicEQModule : public ModuleProcessor
{
public:
    static constexpr int numBands = 16;

    DynamicEQModule()
        : ModuleProcessor ("Dynamic EQ", createLayout())
    {
        freqParam   = getModuleParam ("frequency", 3200.0f);
        qParam      = getModuleParam ("q", 2.5f);
        threshParam = getModuleParam ("threshold", -24.0f);
        dynGainParam= getModuleParam ("dynGain", -6.0f);
        staticParam = getModuleParam ("staticGain", 0.0f);

        for (int b = 0; b < numBands; ++b)
            liveSpectrum[b].store (0.0f);
    }

    void prepareToPlay (double sr, int) override
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;
        resetFilters();
        bandEnv = 0.0f;
        smoothedGainDb = 0.0f;
    }

    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();
        if (numChannels == 0 || numSamples == 0) return;

        float freq       = juce::jlimit (80.0f, 12000.0f, freqParam.get (3200.0f));
        float q          = juce::jlimit (0.5f, 8.0f, qParam.get (2.5f));
        float threshDb   = threshParam.get (-24.0f);
        float maxDynGain = dynGainParam.get (-6.0f); // e.g. -12 dB (cut) or +6 dB (boost)
        float statGain   = staticParam.get (0.0f);

        updateSidechainBP (freq, q);

        float attCoeff = std::exp (-1.0f / (float) (0.005f * sampleRate)); // 5ms attack
        float relCoeff = std::exp (-1.0f / (float) (0.060f * sampleRate)); // 60ms release
        float gainSmoothCoeff = std::exp (-1.0f / (float) (0.003f * sampleRate)); // 3ms, keeps the peak filter's coefficients from stepping

        float maxDynamicOffset = 0.0f;
        float tempSpectrum[numBands] = {};

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* channelData = buffer.getWritePointer (ch);
            int fIdx = ch < 2 ? ch : 0;

            for (int i = 0; i < numSamples; ++i)
            {
                float in = channelData[i];

                // 1. Measure target frequency band energy via sidechain bandpass
                float bp = bpB0 * in + bpB1 * bpX1[fIdx] + bpB2 * bpX2[fIdx]
                           - bpA1 * bpY1[fIdx] - bpA2 * bpY2[fIdx];
                bpX2[fIdx] = bpX1[fIdx];
                bpX1[fIdx] = in;
                bpY2[fIdx] = bpY1[fIdx];
                bpY1[fIdx] = bp;

                float absBp = std::abs (bp);
                if (absBp > bandEnv)
                    bandEnv = attCoeff * bandEnv + (1.0f - attCoeff) * absBp;
                else
                    bandEnv = relCoeff * bandEnv + (1.0f - relCoeff) * absBp;

                // 2. Dynamic threshold calculation
                float envDb = juce::Decibels::gainToDecibels (bandEnv, -70.0f);
                float excessDb = juce::jmax (0.0f, envDb - threshDb);
                float dynOffset = 0.0f;

                if (excessDb > 0.0f)
                {
                    if (maxDynGain < 0.0f) // Dynamic cut (suppress resonance)
                        dynOffset = -juce::jmin (-maxDynGain, excessDb * 0.75f);
                    else // Dynamic boost
                        dynOffset = juce::jmin (maxDynGain, excessDb * 0.75f);
                }

                if (std::abs (dynOffset) > std::abs (maxDynamicOffset))
                    maxDynamicOffset = dynOffset;

                // 3. Update dynamic peak filter coefficients
                // dynOffset can step abruptly the instant excessDb crosses zero (the
                // "if" above has no smoothing at that boundary). Recomputing a Q-up-to-8
                // resonant peaking filter's sin/cos coefficients from an abruptly-stepped
                // gain, every sample, is what was producing zipper/warble artifacts —
                // so the applied gain is smoothed here (3ms one-pole) before it ever
                // reaches the filter design, independent of bandEnv's own attack/release
                // (which is for detection, not for keeping the filter itself smooth).
                float targetTotalGainDb = statGain + dynOffset;
                smoothedGainDb = smoothedGainDb * gainSmoothCoeff + targetTotalGainDb * (1.0f - gainSmoothCoeff);
                updatePeakFilter (freq, smoothedGainDb, q);

                // Apply peak filter
                float out = pkB0 * in + pkB1 * pkX1[fIdx] + pkB2 * pkX2[fIdx]
                            - pkA1 * pkY1[fIdx] - pkA2 * pkY2[fIdx];
                pkX2[fIdx] = pkX1[fIdx];
                pkX1[fIdx] = in;
                pkY2[fIdx] = pkY1[fIdx];
                pkY1[fIdx] = out;

                channelData[i] = out;

                if (ch == 0 && (i % 2 == 0))
                {
                    float absOut = std::abs (out);
                    for (int b = 0; b < numBands; ++b)
                    {
                        float f = 80.0f * std::pow (12000.0f / 80.0f, (float) b / (float) (numBands - 1));
                        float w = 1.0f / (1.0f + std::abs (f - freq) * 0.0015f);
                        tempSpectrum[b] = juce::jmax (tempSpectrum[b], absOut * w);
                    }
                }
            }
        }

        // Decay spectrum
        for (int b = 0; b < numBands; ++b)
        {
            float prev = liveSpectrum[b].load (std::memory_order_relaxed);
            float target = tempSpectrum[b];
            float smoothed = target > prev ? (prev * 0.35f + target * 0.65f) : (prev * 0.88f);
            liveSpectrum[b].store (smoothed, std::memory_order_relaxed);
        }

        liveDynamicOffset.store (maxDynamicOffset, std::memory_order_relaxed);
        liveBandEnergy.store (bandEnv, std::memory_order_relaxed);
    }

    juce::AudioProcessorEditor* createEditor() override;

    float getLiveDynamicOffset() const { return liveDynamicOffset.load (std::memory_order_relaxed); }
    float getLiveBandEnergy() const    { return liveBandEnergy.load (std::memory_order_relaxed); }
    float getFrequency() const         { return freqParam.get (2500.0f); }
    float getThresholdDb() const       { return threshParam.get (-24.0f); }
    float getStaticGain() const        { return staticParam.get (0.0f); }
    float getSpectrumBand (int b) const{ return liveSpectrum[juce::jlimit (0, numBands - 1, b)].load (std::memory_order_relaxed); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        return {
            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "frequency", 1 }, "Frequency",
                juce::NormalisableRange<float> (80.0f, 12000.0f, 10.0f, 0.4f), 2500.0f,
                juce::AudioParameterFloatAttributes().withLabel ("Hz")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "q", 1 }, "Q Factor",
                juce::NormalisableRange<float> (0.5f, 8.0f, 0.1f), 2.0f),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "threshold", 1 }, "Threshold",
                juce::NormalisableRange<float> (-45.0f, 0.0f, 0.5f), -24.0f,
                juce::AudioParameterFloatAttributes().withLabel ("dB")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "dynGain", 1 }, "Dynamic Gain",
                juce::NormalisableRange<float> (-18.0f, 12.0f, 0.5f), -9.0f,
                juce::AudioParameterFloatAttributes().withLabel ("dB")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "staticGain", 1 }, "Static Gain",
                juce::NormalisableRange<float> (-12.0f, 12.0f, 0.5f), 0.0f,
                juce::AudioParameterFloatAttributes().withLabel ("dB"))
        };
    }

    void resetFilters()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            bpX1[ch] = bpX2[ch] = bpY1[ch] = bpY2[ch] = 0.0f;
            pkX1[ch] = pkX2[ch] = pkY1[ch] = pkY2[ch] = 0.0f;
        }
    }

    void updateSidechainBP (float freq, float q)
    {
        float w0 = 2.0f * juce::MathConstants<float>::pi * (freq / (float) sampleRate);
        float alpha = std::sin (w0) / (2.0f * q);
        float a0 = 1.0f + alpha;
        bpB0 = alpha / a0;
        bpB1 = 0.0f;
        bpB2 = -alpha / a0;
        bpA1 = (-2.0f * std::cos (w0)) / a0;
        bpA2 = (1.0f - alpha) / a0;
    }

    void updatePeakFilter (float freq, float gainDb, float q)
    {
        float A = std::pow (10.0f, gainDb / 40.0f);
        float w0 = 2.0f * juce::MathConstants<float>::pi * (freq / (float) sampleRate);
        float cosw0 = std::cos (w0);
        float alpha = std::sin (w0) / (2.0f * q);
        float a0 = 1.0f + alpha / A;
        pkB0 = (1.0f + alpha * A) / a0;
        pkB1 = (-2.0f * cosw0) / a0;
        pkB2 = (1.0f - alpha * A) / a0;
        pkA1 = (-2.0f * cosw0) / a0;
        pkA2 = (1.0f - alpha / A) / a0;
    }

    ParamRef freqParam;
    ParamRef qParam;
    ParamRef threshParam;
    ParamRef dynGainParam;
    ParamRef staticParam;

    double sampleRate = 44100.0;
    float bpB0 = 0.0f, bpB1 = 0.0f, bpB2 = 0.0f, bpA1 = 0.0f, bpA2 = 0.0f;
    float pkB0 = 1.0f, pkB1 = 0.0f, pkB2 = 0.0f, pkA1 = 0.0f, pkA2 = 0.0f;
    float bpX1[2] = {}, bpX2[2] = {}, bpY1[2] = {}, bpY2[2] = {};
    float pkX1[2] = {}, pkX2[2] = {}, pkY1[2] = {}, pkY2[2] = {};
    float bandEnv = 0.0f;
    float smoothedGainDb = 0.0f;

    std::atomic<float> liveDynamicOffset { 0.0f };
    std::atomic<float> liveBandEnergy { 0.0f };
    std::atomic<float> liveSpectrum[numBands];
};

class DynamicEQModuleEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    DynamicEQModuleEditor (DynamicEQModule& p, juce::AudioProcessorValueTreeState& vts)
        : AudioProcessorEditor (&p), module (p)
    {
        auto setupSlider = [this] (juce::Slider& s, const juce::String& suffix, juce::Colour col)
        {
            s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 18);
            s.setTextValueSuffix (suffix);
            s.setColour (juce::Slider::rotarySliderFillColourId, col);
            s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff1e1e24));
            s.setColour (juce::Slider::textBoxOutlineColourId, UITheme::strokeHairline);
            s.setColour (juce::Slider::textBoxTextColourId, UITheme::textPrimary);
            addAndMakeVisible (s);
        };

        setupSlider (freqSlider,    " Hz", UITheme::appleBlue);
        setupSlider (qSlider,       "",    juce::Colour (0xffbf5af2));
        setupSlider (threshSlider,  " dB", UITheme::appleYellow);
        setupSlider (dynGainSlider, " dB", UITheme::appleGreen);
        setupSlider (staticSlider,  " dB", juce::Colour (0xffff9f0a));

        freqAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "frequency", freqSlider);
        qAttach       = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "q", qSlider);
        threshAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "threshold", threshSlider);
        dynGainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "dynGain", dynGainSlider);
        staticAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "staticGain", staticSlider);

        setSize (390, 280);
        startTimerHz (60);
    }

    ~DynamicEQModuleEditor() override { stopTimer(); }

    void timerCallback() override { repaint(); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff141417));

        // Header Strip
        auto header = getLocalBounds().removeFromTop (36);
        g.setColour (UITheme::cardHeader);
        g.fillRect (header);
        g.setColour (UITheme::strokeHairline);
        g.drawHorizontalLine (36, 0.0f, (float) getWidth());

        // Header Title
        g.setColour (UITheme::textPrimary);
        g.setFont (UITheme::getFont (12.0f, true));
        g.drawText ("DYNAMIC EQ (RESONANCE SUPPRESSOR)", header.withTrimmedLeft (14), juce::Justification::centredLeft);

        // Dynamic Offset Badge
        float offset = module.getLiveDynamicOffset();
        bool active = std::abs (offset) > 0.4f;
        auto badge = header.removeFromRight (95).toFloat().reduced (6.0f, 8.0f);
        g.setColour (active ? UITheme::appleGreen.withAlpha (0.28f) : juce::Colour (0x15ffffff));
        g.fillRoundedRectangle (badge, 3.0f);
        g.setColour (active ? UITheme::appleGreen : UITheme::textTertiary);
        g.drawRoundedRectangle (badge, 3.0f, 1.0f);
        g.setFont (UITheme::getFont (8.0f, true));
        g.drawText (active ? (offset < 0.0f ? "DYN " + juce::String (offset, 1) + " dB" : "DYN +" + juce::String (offset, 1) + " dB") : "MONITOR",
                    badge, juce::Justification::centred);

        // --- REAL-TIME DYNAMIC EQ CURVE & SPECTRUM DISPLAY ---
        auto screenRect = juce::Rectangle<float> (16.0f, 44.0f, (float) getWidth() - 32.0f, 82.0f);
        g.setColour (juce::Colour (0xff111114));
        g.fillRoundedRectangle (screenRect, 4.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (screenRect, 4.0f, 0.8f);

        float midY = screenRect.getCentreY();
        g.setColour (juce::Colour (0x15ffffff));
        g.drawHorizontalLine ((int) midY, screenRect.getX(), screenRect.getRight());

        // Background Spectrum
        int numBands = DynamicEQModule::numBands;
        float barWidth = (screenRect.getWidth() - 8.0f) / (float) numBands;
        for (int b = 0; b < numBands; ++b)
        {
            float bx = screenRect.getX() + 4.0f + (float) b * barWidth;
            float e = juce::jlimit (0.0f, 1.0f, module.getSpectrumBand (b) * 2.8f);
            if (e < 0.01f) continue;
            float barH = e * (screenRect.getHeight() * 0.45f);
            g.setColour (UITheme::appleBlue.withAlpha (0.20f));
            g.fillRoundedRectangle (juce::Rectangle<float> (bx + 1.0f, midY - barH, barWidth - 2.0f, barH * 2.0f), 1.0f);
        }

        // Draw Dynamic EQ Curve
        float freq = module.getFrequency();
        float statGain = module.getStaticGain();
        float dynOffset = module.getLiveDynamicOffset();
        float currentGain = statGain + dynOffset;

        juce::Path curve;
        bool started = false;
        int steps = 60;
        for (int s = 0; s <= steps; ++s)
        {
            float norm = (float) s / (float) steps;
            float f = 80.0f * std::pow (12000.0f / 80.0f, norm);
            float x = screenRect.getX() + 4.0f + norm * (screenRect.getWidth() - 8.0f);

            // Gaussian bell curve approximation for visual speed
            float dist = (std::log10 (f) - std::log10 (freq)) / 0.35f;
            float bellDb = currentGain * std::exp (-0.5f * dist * dist);
            float y = midY - (bellDb / 18.0f) * (screenRect.getHeight() * 0.42f);

            if (! started) { curve.startNewSubPath (x, y); started = true; }
            else           { curve.lineTo (x, y); }
        }

        // Shaded under bell
        juce::Path filled = curve;
        filled.lineTo (screenRect.getRight() - 4.0f, midY);
        filled.lineTo (screenRect.getX() + 4.0f, midY);
        filled.closeSubPath();

        juce::Colour curveCol = active ? UITheme::appleGreen : UITheme::appleBlue;
        g.setColour (curveCol.withAlpha (0.18f));
        g.fillPath (filled);

        g.setColour (curveCol);
        g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Frequency target pin
        float normTarget = (std::log10 (freq) - std::log10 (80.0f)) / (std::log10 (12000.0f) - std::log10 (80.0f));
        float pinX = screenRect.getX() + 4.0f + normTarget * (screenRect.getWidth() - 8.0f);
        float pinY = midY - (currentGain / 18.0f) * (screenRect.getHeight() * 0.42f);

        g.setColour (juce::Colours::white);
        g.fillEllipse (pinX - 4.0f, pinY - 4.0f, 8.0f, 8.0f);
        g.setColour (curveCol);
        g.drawEllipse (pinX - 7.0f, pinY - 7.0f, 14.0f, 14.0f, 1.5f);

        // Knob labels
        int colW = getWidth() / 5;
        int labelY = 134;
        g.setColour (UITheme::textSecondary);
        g.setFont (UITheme::getFont (8.5f, true));
        g.drawText ("FREQ",     colW * 0, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("Q",        colW * 1, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("THRESH",   colW * 2, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("DYN GAIN", colW * 3, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("STATIC",   colW * 4, labelY, colW, 14, juce::Justification::centred);
    }

    void resized() override
    {
        int colW = getWidth() / 5;
        int knobY = 152;
        int knobSize = 60;

        freqSlider.setBounds    (colW * 0 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        qSlider.setBounds       (colW * 1 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        threshSlider.setBounds  (colW * 2 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        dynGainSlider.setBounds (colW * 3 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        staticSlider.setBounds  (colW * 4 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
    }

private:
    DynamicEQModule& module;
    juce::Slider freqSlider, qSlider, threshSlider, dynGainSlider, staticSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAttach, qAttach, threshAttach, dynGainAttach, staticAttach;
};

inline juce::AudioProcessorEditor* DynamicEQModule::createEditor()
{
    return new DynamicEQModuleEditor (*this, apvts);
}
