#pragma once
#include "../Graph/ModuleProcessor.h"
#include "../UITheme.h"
#include <cmath>

class PhaseRotatorModuleEditor;

class PhaseRotatorModule : public ModuleProcessor
{
public:
    static constexpr int maxStages = 6;

    PhaseRotatorModule()
        : ModuleProcessor ("Phase Rotator", createLayout())
    {
        freqParam   = apvts.getRawParameterValue ("frequency");
        stagesParam = apvts.getRawParameterValue ("stages");
        mixParam    = apvts.getRawParameterValue ("mix");
    }

    void prepareToPlay (double sr, int) override
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;
        resetFilters();
    }

    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();
        if (numChannels == 0 || numSamples == 0) return;

        float fc = juce::jlimit (80.0f, 1200.0f, freqParam->load());
        int numStages = juce::jlimit (2, maxStages, (int) stagesParam->load());
        float mixNorm = juce::jlimit (0.0f, 1.0f, mixParam->load() * 0.01f);

        // 1st-order allpass coefficient: a = (tan(pi * fc / fs) - 1) / (tan(pi * fc / fs) + 1)
        float w0 = juce::MathConstants<float>::pi * (fc / (float) sampleRate);
        float tanw0 = std::tan (w0);
        float a = (tanw0 - 1.0f) / (tanw0 + 1.0f);

        float blockPosPeak = 0.0f;
        float blockNegPeak = 0.0f;
        float blockDryPos  = 0.0f;
        float blockDryNeg  = 0.0f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* channelData = buffer.getWritePointer (ch);
            int chIdx = ch < 2 ? ch : 0;

            for (int i = 0; i < numSamples; ++i)
            {
                float dry = channelData[i];
                if (dry > 0.0f) blockDryPos = juce::jmax (blockDryPos, dry);
                else            blockDryNeg = juce::jmax (blockDryNeg, -dry);

                float s = dry;
                for (int st = 0; st < numStages; ++st)
                {
                    float y = a * s + apX1[chIdx][st] - a * apY1[chIdx][st];
                    apX1[chIdx][st] = s;
                    apY1[chIdx][st] = y;
                    s = y;
                }

                float wet = s;
                float out = (1.0f - mixNorm) * dry + mixNorm * wet;
                channelData[i] = out;

                if (out > 0.0f) blockPosPeak = juce::jmax (blockPosPeak, out);
                else            blockNegPeak = juce::jmax (blockNegPeak, -out);
            }
        }

        // Smooth decay for live display
        float prevPos = livePosPeak.load (std::memory_order_relaxed);
        float prevNeg = liveNegPeak.load (std::memory_order_relaxed);
        livePosPeak.store (blockPosPeak > prevPos ? blockPosPeak : (prevPos * 0.90f), std::memory_order_relaxed);
        liveNegPeak.store (blockNegPeak > prevNeg ? blockNegPeak : (prevNeg * 0.90f), std::memory_order_relaxed);

        float dryMax = juce::jmax (blockDryPos, blockDryNeg);
        float wetMax = juce::jmax (blockPosPeak, blockNegPeak);
        float headroomGain = (dryMax > 0.05f && wetMax > 0.001f) ? juce::Decibels::gainToDecibels (dryMax / wetMax) : 0.0f;
        liveHeadroomGained.store (juce::jlimit (0.0f, 9.0f, headroomGain), std::memory_order_relaxed);
    }

    juce::AudioProcessorEditor* createEditor() override;

    float getLivePosPeak() const       { return livePosPeak.load (std::memory_order_relaxed); }
    float getLiveNegPeak() const       { return liveNegPeak.load (std::memory_order_relaxed); }
    float getLiveHeadroomGained() const{ return liveHeadroomGained.load (std::memory_order_relaxed); }
    float getFrequency() const         { return freqParam ? freqParam->load() : 280.0f; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        return {
            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "frequency", 1 }, "Rotation Freq",
                juce::NormalisableRange<float> (80.0f, 1000.0f, 5.0f, 0.4f), 280.0f,
                juce::AudioParameterFloatAttributes().withLabel ("Hz")),

            std::make_unique<juce::AudioParameterChoice> (
                juce::ParameterID { "stages", 1 }, "Cascade Stages",
                juce::StringArray { "2 Stages", "4 Stages", "6 Stages" }, 1),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "mix", 1 }, "Mix",
                juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f,
                juce::AudioParameterFloatAttributes().withLabel ("%"))
        };
    }

    void resetFilters()
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int st = 0; st < maxStages; ++st)
                apX1[ch][st] = apY1[ch][st] = 0.0f;
    }

    std::atomic<float>* freqParam   = nullptr;
    std::atomic<float>* stagesParam = nullptr;
    std::atomic<float>* mixParam    = nullptr;

    double sampleRate = 44100.0;
    float apX1[2][maxStages] = {};
    float apY1[2][maxStages] = {};

    std::atomic<float> livePosPeak { 0.0f };
    std::atomic<float> liveNegPeak { 0.0f };
    std::atomic<float> liveHeadroomGained { 0.0f };
};

class PhaseRotatorModuleEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    PhaseRotatorModuleEditor (PhaseRotatorModule& p, juce::AudioProcessorValueTreeState& vts)
        : AudioProcessorEditor (&p), module (p)
    {
        freqSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        freqSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 18);
        freqSlider.setTextValueSuffix (" Hz");
        freqSlider.setColour (juce::Slider::rotarySliderFillColourId, UITheme::appleBlue);
        freqSlider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff1e1e24));
        freqSlider.setColour (juce::Slider::textBoxOutlineColourId, UITheme::strokeHairline);
        freqSlider.setColour (juce::Slider::textBoxTextColourId, UITheme::textPrimary);
        addAndMakeVisible (freqSlider);

        stagesCombo.addItem ("2 Stages", 1);
        stagesCombo.addItem ("4 Stages", 2);
        stagesCombo.addItem ("6 Stages", 3);
        stagesCombo.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff1e1e24));
        stagesCombo.setColour (juce::ComboBox::outlineColourId, UITheme::strokeHairline);
        stagesCombo.setColour (juce::ComboBox::textColourId, UITheme::textPrimary);
        addAndMakeVisible (stagesCombo);

        mixSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        mixSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 18);
        mixSlider.setTextValueSuffix (" %");
        mixSlider.setColour (juce::Slider::rotarySliderFillColourId, UITheme::appleGreen);
        mixSlider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff1e1e24));
        mixSlider.setColour (juce::Slider::textBoxOutlineColourId, UITheme::strokeHairline);
        mixSlider.setColour (juce::Slider::textBoxTextColourId, UITheme::textPrimary);
        addAndMakeVisible (mixSlider);

        freqAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "frequency", freqSlider);
        stagesAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (vts, "stages", stagesCombo);
        mixAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "mix", mixSlider);

        setSize (330, 270);
        startTimerHz (60);
    }

    ~PhaseRotatorModuleEditor() override { stopTimer(); }

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

        // Title
        g.setColour (UITheme::textPrimary);
        g.setFont (UITheme::getFont (12.0f, true));
        g.drawText ("PHASE ROTATOR (RADIO SYMMETRY)", header.withTrimmedLeft (14), juce::Justification::centredLeft);

        // Headroom Gained Badge
        float gainDb = module.getLiveHeadroomGained();
        auto badge = header.removeFromRight (96).toFloat().reduced (6.0f, 8.0f);
        g.setColour (gainDb > 0.5f ? UITheme::appleGreen.withAlpha (0.28f) : juce::Colour (0x15ffffff));
        g.fillRoundedRectangle (badge, 3.0f);
        g.setColour (gainDb > 0.5f ? UITheme::appleGreen : UITheme::textTertiary);
        g.drawRoundedRectangle (badge, 3.0f, 1.0f);
        g.setFont (UITheme::getFont (8.0f, true));
        g.drawText ("+" + juce::String (gainDb, 1) + " dB HEADROOM", badge, juce::Justification::centred);

        // --- REAL-TIME WAVEFORM SYMMETRY SCOPE ---
        auto screenRect = juce::Rectangle<float> (16.0f, 44.0f, (float) getWidth() - 32.0f, 76.0f);
        g.setColour (juce::Colour (0xff111114));
        g.fillRoundedRectangle (screenRect, 4.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (screenRect, 4.0f, 0.8f);

        float midY = screenRect.getCentreY();
        g.setColour (juce::Colour (0x25ffffff));
        g.drawHorizontalLine ((int) midY, screenRect.getX(), screenRect.getRight());

        float posPeak = juce::jlimit (0.0f, 1.0f, module.getLivePosPeak());
        float negPeak = juce::jlimit (0.0f, 1.0f, module.getLiveNegPeak());

        float totalPeak = posPeak + negPeak;
        float symmetryPct = 100.0f;
        if (totalPeak > 0.05f)
        {
            float ratio = posPeak / totalPeak; // 0.5 is perfect symmetry
            symmetryPct = juce::jlimit (0.0f, 100.0f, (1.0f - std::abs (ratio - 0.5f) * 2.0f) * 100.0f);
        }

        // Draw Symmetrical Peak Bars
        float barW = 50.0f;
        float barX = screenRect.getCentreX() - barW * 0.5f;

        float posH = posPeak * (screenRect.getHeight() * 0.45f);
        float negH = negPeak * (screenRect.getHeight() * 0.45f);

        auto posRect = juce::Rectangle<float> (barX, midY - posH, barW, posH);
        auto negRect = juce::Rectangle<float> (barX, midY, barW, negH);

        juce::Colour symColor = symmetryPct > 85.0f ? UITheme::appleGreen : (symmetryPct > 65.0f ? UITheme::appleYellow : UITheme::appleRed);

        g.setColour (symColor.withAlpha (0.45f));
        g.fillRoundedRectangle (posRect, 2.0f);
        g.fillRoundedRectangle (negRect, 2.0f);

        g.setColour (symColor);
        g.drawRoundedRectangle (posRect, 2.0f, 1.0f);
        g.drawRoundedRectangle (negRect, 2.0f, 1.0f);

        // Labels
        g.setColour (UITheme::textTertiary);
        g.setFont (UITheme::getFont (8.0f, true));
        g.drawText ("+ POS", juce::Rectangle<float> (screenRect.getX() + 8.0f, midY - 20.0f, 40.0f, 14.0f), juce::Justification::centredLeft);
        g.drawText ("- NEG", juce::Rectangle<float> (screenRect.getX() + 8.0f, midY + 6.0f,  40.0f, 14.0f), juce::Justification::centredLeft);

        g.setColour (symColor);
        g.drawText (juce::String ((int) symmetryPct) + "% SYMMETRIC", juce::Rectangle<float> (screenRect.getRight() - 95.0f, midY - 7.0f, 90.0f, 14.0f), juce::Justification::centredRight);

        // Knob labels
        int colW = getWidth() / 3;
        int labelY = 126;
        g.setColour (UITheme::textSecondary);
        g.setFont (UITheme::getFont (9.0f, true));
        g.drawText ("ROTATION FREQ", 0, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("STAGES", colW, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("MIX", colW * 2, labelY, colW, 14, juce::Justification::centred);
    }

    void resized() override
    {
        int colW = getWidth() / 3;
        int knobY = 144;
        int knobSize = 74;

        freqSlider.setBounds  (colW * 0 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        stagesCombo.setBounds (colW * 1 + 10, knobY + 22, colW - 20, 24);
        mixSlider.setBounds   (colW * 2 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
    }

private:
    PhaseRotatorModule& module;
    juce::Slider freqSlider, mixSlider;
    juce::ComboBox stagesCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAttach, mixAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> stagesAttach;
};

inline juce::AudioProcessorEditor* PhaseRotatorModule::createEditor()
{
    return new PhaseRotatorModuleEditor (*this, apvts);
}
