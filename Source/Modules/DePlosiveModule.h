#pragma once
#include "../Graph/ModuleProcessor.h"
#include "../UITheme.h"
#include <cmath>
#include <array>

class DePlosiveModuleEditor;

class DePlosiveModule : public ModuleProcessor
{
public:
    static constexpr int numBands = 18;

    DePlosiveModule()
        : ModuleProcessor ("De-Plosive", createLayout())
    {
        freqParam   = apvts.getRawParameterValue ("frequency");
        slopeParam  = apvts.getRawParameterValue ("slope");
        dampParam   = apvts.getRawParameterValue ("damp");

        for (int b = 0; b < numBands; ++b)
            liveSpectrum[b].store (0.0f);
    }

    void prepareToPlay (double sr, int) override
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;
        resetFilters();
        for (int b = 0; b < numBands; ++b)
        {
            bandState[b][0] = bandState[b][1] = 0.0f;
            liveSpectrum[b].store (0.0f);
        }
    }

    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();
        if (numChannels == 0 || numSamples == 0) return;

        float cutoff = juce::jlimit (20.0f, 300.0f, freqParam->load());
        bool steepSlope = slopeParam->load() > 0.5f; // 0 = 12dB/oct, 1 = 24dB/oct
        float dampAmt = juce::jlimit (0.0f, 1.0f, dampParam->load() * 0.01f);

        updateCoefficients (cutoff);

        float maxPlosiveDetected = 0.0f;
        float inputSubBassEnergy = 0.0f;
        float outputSubBassEnergy = 0.0f;

        // Temporary band energy accumulators
        float tempBands[numBands] = {};

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* channelData = buffer.getWritePointer (ch);
            int filterIdx = ch < 2 ? ch : 0;

            for (int i = 0; i < numSamples; ++i)
            {
                float in = channelData[i];

                // 1. High-Pass Filtering Stage 1 (12dB/oct)
                float hp1 = b0 * in + b1 * x1[filterIdx][0] + b2 * x2[filterIdx][0]
                            - a1 * y1[filterIdx][0] - a2 * y2[filterIdx][0];
                x2[filterIdx][0] = x1[filterIdx][0];
                x1[filterIdx][0] = in;
                y2[filterIdx][0] = y1[filterIdx][0];
                y1[filterIdx][0] = hp1;

                float out = hp1;

                // Stage 2 (24dB/oct steep plosive wall)
                if (steepSlope)
                {
                    float hp2 = b0 * hp1 + b1 * x1[filterIdx][1] + b2 * x2[filterIdx][1]
                                - a1 * y1[filterIdx][1] - a2 * y2[filterIdx][1];
                    x2[filterIdx][1] = x1[filterIdx][1];
                    x1[filterIdx][1] = hp1;
                    y2[filterIdx][1] = y1[filterIdx][1];
                    y1[filterIdx][1] = hp2;
                    out = hp2;
                }

                // Dynamic sub-bass surge suppression (Plosive absorption)
                float lowEnergy = std::abs (in - out);
                inputSubBassEnergy = juce::jmax (inputSubBassEnergy, lowEnergy);

                if (lowEnergy > 0.20f && dampAmt > 0.01f)
                {
                    float attenuation = 1.0f / (1.0f + (lowEnergy - 0.20f) * 4.5f * dampAmt);
                    out *= attenuation;
                    maxPlosiveDetected = juce::jmax (maxPlosiveDetected, lowEnergy);
                }

                outputSubBassEnergy = juce::jmax (outputSubBassEnergy, std::abs (out));
                channelData[i] = out;

                // Measure low-frequency spectrum bins for dynamic display
                if (ch == 0 && (i % 2 == 0))
                {
                    float absIn = std::abs (in);
                    // Approximate frequency band excitation
                    for (int b = 0; b < numBands; ++b)
                    {
                        float centerHz = 20.0f * std::pow (600.0f / 20.0f, (float) b / (float) (numBands - 1));
                        float bandWeight = 1.0f / (1.0f + std::abs (centerHz - cutoff) * 0.04f);
                        tempBands[b] = juce::jmax (tempBands[b], absIn * bandWeight);
                    }
                }
            }
        }

        // Smooth decay for spectrum display
        for (int b = 0; b < numBands; ++b)
        {
            float prev = liveSpectrum[b].load (std::memory_order_relaxed);
            float target = tempBands[b];
            float smoothed = target > prev ? (prev * 0.3f + target * 0.7f) : (prev * 0.88f);
            liveSpectrum[b].store (smoothed, std::memory_order_relaxed);
        }

        livePlosiveActivity.store (maxPlosiveDetected, std::memory_order_relaxed);
        livePreEnergy.store (inputSubBassEnergy, std::memory_order_relaxed);
        livePostEnergy.store (outputSubBassEnergy, std::memory_order_relaxed);
    }

    juce::AudioProcessorEditor* createEditor() override;

    float getLivePlosiveActivity() const { return livePlosiveActivity.load (std::memory_order_relaxed); }
    float getCutoffFreq() const { return freqParam ? freqParam->load() : 80.0f; }
    bool isSteepSlope() const { return slopeParam && slopeParam->load() > 0.5f; }
    float getBandEnergy (int b) const { return liveSpectrum[juce::jlimit (0, numBands - 1, b)].load (std::memory_order_relaxed); }
    float getPreEnergy() const { return livePreEnergy.load (std::memory_order_relaxed); }
    float getPostEnergy() const { return livePostEnergy.load (std::memory_order_relaxed); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        return {
            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "frequency", 1 }, "Cutoff",
                juce::NormalisableRange<float> (20.0f, 250.0f, 1.0f, 0.45f), 80.0f,
                juce::AudioParameterFloatAttributes().withLabel ("Hz")),

            std::make_unique<juce::AudioParameterChoice> (
                juce::ParameterID { "slope", 1 }, "Slope",
                juce::StringArray { "12 dB/oct", "24 dB/oct" }, 1),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "damp", 1 }, "Plosive Dampen",
                juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 50.0f,
                juce::AudioParameterFloatAttributes().withLabel ("%"))
        };
    }

    void resetFilters()
    {
        for (int ch = 0; ch < 2; ++ch)
            for (int s = 0; s < 2; ++s)
                x1[ch][s] = x2[ch][s] = y1[ch][s] = y2[ch][s] = 0.0f;
    }

    void updateCoefficients (float freq)
    {
        float w0 = 2.0f * juce::MathConstants<float>::pi * (freq / (float) sampleRate);
        float cosw0 = std::cos (w0);
        float sinw0 = std::sin (w0);
        float alpha = sinw0 / (2.0f * 0.7071f); // Q = 0.707

        float a0 = 1.0f + alpha;
        b0 = ((1.0f + cosw0) * 0.5f) / a0;
        b1 = (-(1.0f + cosw0)) / a0;
        b2 = ((1.0f + cosw0) * 0.5f) / a0;
        a1 = (-2.0f * cosw0) / a0;
        a2 = (1.0f - alpha) / a0;
    }

    std::atomic<float>* freqParam  = nullptr;
    std::atomic<float>* slopeParam = nullptr;
    std::atomic<float>* dampParam  = nullptr;

    double sampleRate = 44100.0;
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f;
    float x1[2][2] = {}, x2[2][2] = {}, y1[2][2] = {}, y2[2][2] = {};
    float bandState[numBands][2] = {};

    std::atomic<float> livePlosiveActivity { 0.0f };
    std::atomic<float> liveSpectrum[numBands];
    std::atomic<float> livePreEnergy { 0.0f };
    std::atomic<float> livePostEnergy { 0.0f };
};

class DePlosiveModuleEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    DePlosiveModuleEditor (DePlosiveModule& p, juce::AudioProcessorValueTreeState& vts)
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

        dampSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        dampSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 18);
        dampSlider.setTextValueSuffix (" %");
        dampSlider.setColour (juce::Slider::rotarySliderFillColourId, UITheme::appleYellow);
        dampSlider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff1e1e24));
        dampSlider.setColour (juce::Slider::textBoxOutlineColourId, UITheme::strokeHairline);
        dampSlider.setColour (juce::Slider::textBoxTextColourId, UITheme::textPrimary);
        addAndMakeVisible (dampSlider);

        slopeCombo.addItem ("12 dB/oct", 1);
        slopeCombo.addItem ("24 dB/oct", 2);
        slopeCombo.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff1e1e24));
        slopeCombo.setColour (juce::ComboBox::outlineColourId, UITheme::strokeHairline);
        slopeCombo.setColour (juce::ComboBox::textColourId, UITheme::textPrimary);
        addAndMakeVisible (slopeCombo);

        freqAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "frequency", freqSlider);
        dampAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "damp", dampSlider);
        slopeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (vts, "slope", slopeCombo);

        setSize (330, 270);
        startTimerHz (60);
    }

    ~DePlosiveModuleEditor() override { stopTimer(); }

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
        g.drawText ("VOCAL DE-PLOSIVE & RUMBLE", header.withTrimmedLeft (14), juce::Justification::centredLeft);

        // Plosive Warning Indicator
        float pop = module.getLivePlosiveActivity();
        bool isPopping = pop > 0.15f;
        auto popBadge = header.removeFromRight (94).toFloat().reduced (8.0f, 8.0f);
        g.setColour (isPopping ? UITheme::appleYellow.withAlpha (0.28f) : juce::Colour (0x15ffffff));
        g.fillRoundedRectangle (popBadge, 3.0f);
        g.setColour (isPopping ? UITheme::appleYellow : UITheme::textTertiary);
        g.drawRoundedRectangle (popBadge, 3.0f, 1.0f);
        g.setFont (UITheme::getFont (8.0f, true));
        g.drawText (isPopping ? "POP CLAMP" : "CLEAN", popBadge, juce::Justification::centred);

        // --- REAL-TIME DYNAMIC SPECTRUM & FILTER CURVE DISPLAY ---
        auto screenRect = juce::Rectangle<float> (16.0f, 44.0f, (float) getWidth() - 32.0f, 76.0f);
        g.setColour (juce::Colour (0xff111114));
        g.fillRoundedRectangle (screenRect, 4.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (screenRect, 4.0f, 0.8f);

        // Grid lines
        g.setColour (juce::Colour (0x12ffffff));
        for (float gx = screenRect.getX() + 45.0f; gx < screenRect.getRight(); gx += 50.0f)
            g.drawVerticalLine ((int) gx, screenRect.getY(), screenRect.getBottom());
        g.drawHorizontalLine ((int) (screenRect.getY() + screenRect.getHeight() * 0.5f), screenRect.getX(), screenRect.getRight());

        float cutoff = module.getCutoffFreq();
        bool steep = module.isSteepSlope();
        float cutoffNorm = (std::log10 (cutoff) - std::log10 (20.0f)) / (std::log10 (600.0f) - std::log10 (20.0f));
        float cutoffX = screenRect.getX() + screenRect.getWidth() * juce::jlimit (0.05f, 0.95f, cutoffNorm);

        // 1. Draw Real-time Spectrum Energy Bars under the curve
        int numBands = DePlosiveModule::numBands;
        float barWidth = (screenRect.getWidth() - 8.0f) / (float) numBands;

        for (int b = 0; b < numBands; ++b)
        {
            float bx = screenRect.getX() + 4.0f + (float) b * barWidth;
            float energy = juce::jlimit (0.0f, 1.0f, module.getBandEnergy (b) * 2.2f);
            if (energy < 0.02f) continue;

            float barH = energy * (screenRect.getHeight() - 8.0f);
            auto barRect = juce::Rectangle<float> (bx + 1.0f, screenRect.getBottom() - barH - 4.0f, barWidth - 2.0f, barH);

            // Red if below cutoff (suppressed plosive), cyan/green if above cutoff (vocal passed)
            bool isFiltered = (bx + barWidth * 0.5f) < cutoffX;
            g.setColour (isFiltered ? UITheme::appleRed.withAlpha (0.45f) : UITheme::appleBlue.withAlpha (0.35f));
            g.fillRoundedRectangle (barRect, 1.5f);
        }

        // 2. Draw Dynamic High-Pass Roll-Off Curve
        float startX = screenRect.getX();
        float endX   = screenRect.getRight();
        float baseY  = screenRect.getBottom() - 6.0f;
        float flatY  = screenRect.getY() + 12.0f;

        float curveSteepness = steep ? 16.0f : 28.0f;
        juce::Path curve;
        curve.startNewSubPath (startX, baseY);
        curve.cubicTo (cutoffX - curveSteepness, baseY, cutoffX - 4.0f, flatY + 4.0f, cutoffX + 12.0f, flatY);
        curve.lineTo (endX, flatY);

        // Gradient filled roll-off
        juce::Path filledCurve = curve;
        filledCurve.lineTo (endX, screenRect.getBottom());
        filledCurve.lineTo (startX, screenRect.getBottom());
        filledCurve.closeSubPath();

        juce::ColourGradient fillGrad (
            UITheme::appleBlue.withAlpha (0.04f), startX, screenRect.getY(),
            UITheme::appleBlue.withAlpha (0.16f), endX, screenRect.getY(), false);
        g.setGradientFill (fillGrad);
        g.fillPath (filledCurve);

        // Active filter line
        g.setColour (UITheme::appleBlue);
        g.strokePath (curve, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Cutoff Marker Line & Tag
        g.setColour (UITheme::appleBlue.withAlpha (0.75f));
        float dashes[] = { 3.0f, 2.0f };
        g.drawDashedLine (juce::Line<float> (cutoffX, screenRect.getY() + 4.0f, cutoffX, screenRect.getBottom() - 4.0f), dashes, 2, 1.2f);

        // Cutoff Tag
        g.setFont (UITheme::getFont (7.5f, true));
        g.drawText (juce::String ((int) cutoff) + " Hz",
                    juce::Rectangle<float> (cutoffX + 4.0f, flatY + 4.0f, 38.0f, 10.0f),
                    juce::Justification::centredLeft);

        // Frequency Labels
        g.setColour (UITheme::textTertiary);
        g.setFont (UITheme::getFont (8.0f));
        g.drawText ("20 Hz", screenRect.withTrimmedLeft (6), juce::Justification::bottomLeft);
        g.drawText ("500 Hz", screenRect.withTrimmedRight (6), juce::Justification::bottomRight);

        // Knob labels
        int colW = getWidth() / 3;
        int labelY = 126;
        g.setColour (UITheme::textSecondary);
        g.setFont (UITheme::getFont (9.0f, true));
        g.drawText ("CUTOFF", 0, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("SLOPE", colW, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("DAMPEN", colW * 2, labelY, colW, 14, juce::Justification::centred);
    }

    void resized() override
    {
        int colW = getWidth() / 3;
        int knobY = 144;
        int knobSize = 74;

        freqSlider.setBounds (colW * 0 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        slopeCombo.setBounds (colW * 1 + 10, knobY + 22, colW - 20, 24);
        dampSlider.setBounds (colW * 2 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
    }

private:
    DePlosiveModule& module;
    juce::Slider freqSlider, dampSlider;
    juce::ComboBox slopeCombo;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAttach, dampAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> slopeAttach;
};

inline juce::AudioProcessorEditor* DePlosiveModule::createEditor()
{
    return new DePlosiveModuleEditor (*this, apvts);
}
