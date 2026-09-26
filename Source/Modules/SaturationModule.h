#pragma once
#include "../Graph/ModuleProcessor.h"
#include "../UITheme.h"
#include <cmath>

class SaturationModuleEditor;

class SaturationModule : public ModuleProcessor
{
public:
    SaturationModule()
        : ModuleProcessor ("Saturation", createLayout())
    {
        driveParam  = getModuleParam ("drive", 25.0f);
        warmthParam = getModuleParam ("warmth", 35.0f);
        mixParam    = getModuleParam ("mix", 100.0f);
        toneParam   = getModuleParam ("tone", 12000.0f);
    }

    void prepareToPlay (double sr, int) override
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;
        toneFilterState[0] = toneFilterState[1] = 0.0f;
    }

    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();
        if (numChannels == 0 || numSamples == 0) return;

        float driveNorm  = juce::jlimit (0.0f, 1.0f, driveParam.get (25.0f) * 0.01f);
        float warmthNorm = juce::jlimit (0.0f, 1.0f, warmthParam.get (35.0f) * 0.01f);
        float mixNorm    = juce::jlimit (0.0f, 1.0f, mixParam.get (100.0f) * 0.01f);
        float toneCutoff = toneParam.get (12000.0f);

        // Drive gain multiplier: 1.0x to 8.0x
        float driveGain = 1.0f + driveNorm * 7.0f;
        // Asymmetry bias for tube even-harmonics
        float bias = warmthNorm * 0.22f;
        // Automatic makeup attenuation to maintain stable vocal volume
        float makeup = 1.0f / std::sqrt (1.0f + driveNorm * 3.5f);

        // One-pole smoothing lowpass for tone control
        float toneCoeff = std::exp (-2.0f * juce::MathConstants<float>::pi * (toneCutoff / (float) sampleRate));

        float blockInputPeak  = 0.0f;
        float blockOutputPeak = 0.0f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* channelData = buffer.getWritePointer (ch);
            int filterIdx = ch < 2 ? ch : 0;

            for (int i = 0; i < numSamples; ++i)
            {
                float dry = channelData[i];
                blockInputPeak = juce::jmax (blockInputPeak, std::abs (dry));

                // 1. Apply drive & tube bias
                float x = (dry * driveGain) + bias;

                // 2. Soft-knee hyperbolic tangent saturation
                float wet = std::tanh (x);

                // Remove DC bias
                wet -= std::tanh (bias);

                // 3. Post tone filter
                toneFilterState[filterIdx] = toneCoeff * toneFilterState[filterIdx] + (1.0f - toneCoeff) * wet;
                wet = toneFilterState[filterIdx] * makeup;

                // 4. Dry/Wet blend
                float out = (1.0f - mixNorm) * dry + mixNorm * wet;
                channelData[i] = out;

                blockOutputPeak = juce::jmax (blockOutputPeak, std::abs (out));
            }
        }

        // Smooth decaying tracker
        float prevIn  = liveInputPeak.load (std::memory_order_relaxed);
        float prevOut = liveOutputPeak.load (std::memory_order_relaxed);
        liveInputPeak.store  (blockInputPeak > prevIn ? blockInputPeak : (prevIn * 0.90f), std::memory_order_relaxed);
        liveOutputPeak.store (blockOutputPeak > prevOut ? blockOutputPeak : (prevOut * 0.90f), std::memory_order_relaxed);

        float glow = juce::jlimit (0.0f, 1.0f, blockOutputPeak * (0.25f + driveNorm * 0.75f));
        liveGlowIntensity.store (glow, std::memory_order_relaxed);
    }

    juce::AudioProcessorEditor* createEditor() override;

    float getLiveGlow() const { return liveGlowIntensity.load (std::memory_order_relaxed); }
    float getLiveInputPeak() const { return liveInputPeak.load (std::memory_order_relaxed); }
    float getLiveOutputPeak() const { return liveOutputPeak.load (std::memory_order_relaxed); }
    float getDriveNorm() const { return driveParam.get (25.0f) * 0.01f; }
    float getWarmthNorm() const { return warmthParam.get (35.0f) * 0.01f; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        return {
            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "drive", 1 }, "Drive",
                juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 25.0f,
                juce::AudioParameterFloatAttributes().withLabel ("%")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "warmth", 1 }, "Warmth",
                juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 35.0f,
                juce::AudioParameterFloatAttributes().withLabel ("%")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "mix", 1 }, "Mix",
                juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f,
                juce::AudioParameterFloatAttributes().withLabel ("%")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "tone", 1 }, "Air Tone",
                juce::NormalisableRange<float> (2000.0f, 18000.0f, 50.0f, 0.4f), 12000.0f,
                juce::AudioParameterFloatAttributes().withLabel ("Hz"))
        };
    }

    ParamRef driveParam;
    ParamRef warmthParam;
    ParamRef mixParam;
    ParamRef toneParam;

    double sampleRate = 44100.0;
    float toneFilterState[2] = { 0.0f, 0.0f };

    std::atomic<float> liveGlowIntensity { 0.0f };
    std::atomic<float> liveInputPeak { 0.0f };
    std::atomic<float> liveOutputPeak { 0.0f };
};

class SaturationModuleEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    SaturationModuleEditor (SaturationModule& p, juce::AudioProcessorValueTreeState& vts)
        : AudioProcessorEditor (&p), module (p)
    {
        auto setupSlider = [this] (juce::Slider& s, const juce::String& suffix, juce::Colour fillCol)
        {
            s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 18);
            s.setTextValueSuffix (suffix);
            s.setColour (juce::Slider::rotarySliderFillColourId, fillCol);
            s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff1e1e24));
            s.setColour (juce::Slider::textBoxOutlineColourId, UITheme::strokeHairline);
            s.setColour (juce::Slider::textBoxTextColourId, UITheme::textPrimary);
            addAndMakeVisible (s);
        };

        setupSlider (driveSlider, " %", juce::Colour (0xffff9f0a));
        setupSlider (warmthSlider, " %", juce::Colour (0xffff6934));
        setupSlider (mixSlider, " %", UITheme::appleBlue);
        setupSlider (toneSlider, " Hz", UITheme::appleYellow);

        driveAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "drive", driveSlider);
        warmthAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "warmth", warmthSlider);
        mixAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "mix", mixSlider);
        toneAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "tone", toneSlider);

        setSize (350, 270);
        startTimerHz (60);
    }

    ~SaturationModuleEditor() override { stopTimer(); }

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
        g.drawText ("VOCAL TUBE WARMTH & HARMONICS", header.withTrimmedLeft (14), juce::Justification::centredLeft);

        // --- REAL-TIME DYNAMIC VISUALIZER AREA ---
        // Left: X-Y Waveshaper Transfer Function Scope
        // Right: Glowing Vacuum Tube Visualizer
        auto scopeRect = juce::Rectangle<float> (16.0f, 44.0f, 175.0f, 76.0f);
        auto tubeRect  = juce::Rectangle<float> (scopeRect.getRight() + 12.0f, 44.0f, (float) getWidth() - scopeRect.getRight() - 28.0f, 76.0f);

        // 1. Waveshaper Scope Box
        g.setColour (juce::Colour (0xff111114));
        g.fillRoundedRectangle (scopeRect, 4.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (scopeRect, 4.0f, 0.8f);

        // Center crosshairs
        float midX = scopeRect.getCentreX();
        float midY = scopeRect.getCentreY();
        g.setColour (juce::Colour (0x15ffffff));
        g.drawVerticalLine ((int) midX, scopeRect.getY(), scopeRect.getBottom());
        g.drawHorizontalLine ((int) midY, scopeRect.getX(), scopeRect.getRight());

        // Draw Dynamic Waveshaper Transfer Curve
        float driveNorm  = module.getDriveNorm();
        float warmthNorm = module.getWarmthNorm();
        float driveGain  = 1.0f + driveNorm * 7.0f;
        float bias       = warmthNorm * 0.22f;

        juce::Path transferCurve;
        bool started = false;
        int numSteps = 40;

        for (int s = 0; s <= numSteps; ++s)
        {
            float inVal = -1.0f + 2.0f * ((float) s / (float) numSteps); // -1..+1
            float satVal = std::tanh ((inVal * driveGain) + bias) - std::tanh (bias);

            float cx = midX + inVal * (scopeRect.getWidth() * 0.45f);
            float cy = midY - satVal * (scopeRect.getHeight() * 0.42f);

            if (! started) { transferCurve.startNewSubPath (cx, cy); started = true; }
            else           { transferCurve.lineTo (cx, cy); }
        }

        // Transfer curve line (glowing amber)
        g.setColour (juce::Colour (0xffff9f0a).withAlpha (0.90f));
        g.strokePath (transferCurve, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Animated Vocal Peak Particle Trace on the Curve
        float inPeak = juce::jlimit (0.0f, 1.0f, module.getLiveInputPeak());
        if (inPeak > 0.02f)
        {
            float activeSat = std::tanh ((inPeak * driveGain) + bias) - std::tanh (bias);
            float ballX = midX + inPeak * (scopeRect.getWidth() * 0.45f);
            float ballY = midY - activeSat * (scopeRect.getHeight() * 0.42f);

            // Glowing dynamic ball
            g.setColour (juce::Colour (0x55ff9f0a));
            g.fillEllipse (ballX - 5.0f, ballY - 5.0f, 10.0f, 10.0f);
            g.setColour (juce::Colours::white);
            g.fillEllipse (ballX - 2.5f, ballY - 2.5f, 5.0f, 5.0f);
        }

        g.setColour (UITheme::textTertiary);
        g.setFont (UITheme::getFont (7.5f));
        g.drawText ("TRANSFER CURVE", scopeRect.withTrimmedLeft (4), juce::Justification::topLeft);

        // 2. Vacuum Tube Visualizer Box
        g.setColour (juce::Colour (0xff111114));
        g.fillRoundedRectangle (tubeRect, 4.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (tubeRect, 4.0f, 0.8f);

        float glow = module.getLiveGlow();
        auto innerTube = tubeRect.reduced (8.0f, 6.0f);

        // Glass Envelope
        g.setColour (juce::Colour (0x15ffffff));
        g.fillRoundedRectangle (innerTube, 12.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (innerTube, 12.0f, 1.0f);

        // Dynamic Tube Glow (Reacts to audio)
        float glowAlpha = juce::jlimit (0.22f, 0.95f, 0.25f + glow * 0.70f);
        juce::Colour glowColour = juce::Colour (0xffff7a00).withAlpha (glowAlpha);

        g.setColour (glowColour.withAlpha (glowAlpha * 0.45f));
        g.fillEllipse (innerTube.getCentreX() - 20.0f, innerTube.getCentreY() - 15.0f, 40.0f, 30.0f);

        // Glowing Cathode Filament
        g.setColour (glowColour);
        g.drawLine (innerTube.getCentreX() - 8.0f, innerTube.getY() + 12.0f, innerTube.getCentreX() - 3.0f, innerTube.getBottom() - 12.0f, 2.0f);
        g.drawLine (innerTube.getCentreX() + 3.0f, innerTube.getY() + 12.0f, innerTube.getCentreX() + 8.0f, innerTube.getBottom() - 12.0f, 2.0f);

        g.setColour (UITheme::textTertiary);
        g.setFont (UITheme::getFont (7.5f));
        g.drawText ("TUBE GLOW", tubeRect.withTrimmedLeft (4), juce::Justification::topLeft);

        // Knob labels
        int colW = getWidth() / 4;
        int labelY = 126;
        g.setColour (UITheme::textSecondary);
        g.setFont (UITheme::getFont (9.0f, true));
        g.drawText ("DRIVE", colW * 0, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("WARMTH", colW * 1, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("AIR TONE", colW * 2, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("MIX", colW * 3, labelY, colW, 14, juce::Justification::centred);
    }

    void resized() override
    {
        int colW = getWidth() / 4;
        int knobY = 144;
        int knobSize = 68;

        driveSlider.setBounds  (colW * 0 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        warmthSlider.setBounds (colW * 1 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        toneSlider.setBounds   (colW * 2 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        mixSlider.setBounds    (colW * 3 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
    }

private:
    SaturationModule& module;
    juce::Slider driveSlider, warmthSlider, mixSlider, toneSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAttach, warmthAttach, mixAttach, toneAttach;
};

inline juce::AudioProcessorEditor* SaturationModule::createEditor()
{
    return new SaturationModuleEditor (*this, apvts);
}
