#pragma once
#include "../Graph/ModuleProcessor.h"
#include "../UITheme.h"
#include <cmath>
#include <vector>

class ThreeBandEQModuleEditor;

/**
    3-Band Musical Equalizer (Low Shelf, Mid Peaking Bell, High Shelf).
    Offers fast, musical tonal balancing with intuitive crossover controls and interactive curve display.
*/
class ThreeBandEQModule : public ModuleProcessor
{
public:
    ThreeBandEQModule()
        : ModuleProcessor ("3-Band EQ", createLayout())
    {
        lowGainParam    = getModuleParam ("lowGain", 0.0f);
        lowFreqParam    = getModuleParam ("lowFreq", 150.0f);
        midGainParam    = getModuleParam ("midGain", 0.0f);
        midFreqParam    = getModuleParam ("midFreq", 1000.0f);
        midQParam       = getModuleParam ("midQ", 1.0f);
        highGainParam   = getModuleParam ("highGain", 0.0f);
        highFreqParam   = getModuleParam ("highFreq", 5000.0f);
        outputGainParam = getModuleParam ("outputGain", 0.0f);
    }

    ~ThreeBandEQModule() override = default;

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

        float lowGain    = lowGainParam.get (0.0f);
        float lowFreq    = lowFreqParam.get (150.0f);
        float midGain    = midGainParam.get (0.0f);
        float midFreq    = midFreqParam.get (1000.0f);
        float midQ       = midQParam.get (1.0f);
        float highGain   = highGainParam.get (0.0f);
        float highFreq   = highFreqParam.get (5000.0f);
        float outputGain = outputGainParam.get (0.0f);

        updateCoefficients (lowGain, lowFreq, midGain, midFreq, midQ, highGain, highFreq);

        float outMult = juce::Decibels::decibelsToGain (outputGain);
        float peak = 0.0f;

        for (int ch = 0; ch < juce::jmin (2, numChannels); ++ch)
        {
            float* channelData = buffer.getWritePointer (ch);

            for (int i = 0; i < numSamples; ++i)
            {
                float sample = channelData[i];

                // 1. Low Shelf
                sample = processBiquad (sample, lsB0, lsB1, lsB2, lsA1, lsA2,
                                        lsX1[ch], lsX2[ch], lsY1[ch], lsY2[ch]);

                // 2. Mid Peaking Bell
                sample = processBiquad (sample, midB0, midB1, midB2, midA1, midA2,
                                        midX1[ch], midX2[ch], midY1[ch], midY2[ch]);

                // 3. High Shelf
                sample = processBiquad (sample, hsB0, hsB1, hsB2, hsA1, hsA2,
                                        hsX1[ch], hsX2[ch], hsY1[ch], hsY2[ch]);

                sample *= outMult;
                channelData[i] = sample;

                peak = juce::jmax (peak, std::abs (sample));
            }
        }

        liveLevel.store (peak, std::memory_order_relaxed);
    }

    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    float getLiveLevel() const { return liveLevel.load (std::memory_order_relaxed); }

    // Evaluates EQ magnitude response at given frequency (in Hz) for UI curve display
    float getMagnitudeAtFrequency (float freqHz) const
    {
        double w = 2.0 * juce::MathConstants<double>::pi * (double) freqHz / sampleRate;
        double cosW = std::cos (w);
        double cos2W = std::cos (2.0 * w);

        auto evalFilter = [cosW, cos2W] (float b0, float b1, float b2, float a1, float a2) -> double
        {
            double num = b0*b0 + b1*b1 + b2*b2 + 2.0*(b0*b1 + b1*b2)*cosW + 2.0*b0*b2*cos2W;
            double den = 1.0 + a1*a1 + a2*a2 + 2.0*(a1 + a1*a2)*cosW + 2.0*a2*cos2W;
            return den > 1e-12 ? std::sqrt (num / den) : 1.0;
        };

        double magLS  = evalFilter (lsB0, lsB1, lsB2, lsA1, lsA2);
        double magMid = evalFilter (midB0, midB1, midB2, midA1, midA2);
        double magHS  = evalFilter (hsB0, hsB1, hsB2, hsA1, hsA2);

        double totalLinear = magLS * magMid * magHS;
        return (float) juce::Decibels::gainToDecibels (totalLinear, -60.0);
    }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        return {
            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "lowGain", 1 }, "Low Gain",
                juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f,
                juce::AudioParameterFloatAttributes().withLabel ("dB")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "lowFreq", 1 }, "Low Freq",
                juce::NormalisableRange<float> (40.0f, 600.0f, 1.0f, 0.4f), 150.0f,
                juce::AudioParameterFloatAttributes().withLabel ("Hz")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "midGain", 1 }, "Mid Gain",
                juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f,
                juce::AudioParameterFloatAttributes().withLabel ("dB")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "midFreq", 1 }, "Mid Freq",
                juce::NormalisableRange<float> (200.0f, 6000.0f, 1.0f, 0.4f), 1000.0f,
                juce::AudioParameterFloatAttributes().withLabel ("Hz")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "midQ", 1 }, "Mid Q",
                juce::NormalisableRange<float> (0.3f, 4.0f, 0.05f), 1.0f),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "highGain", 1 }, "High Gain",
                juce::NormalisableRange<float> (-18.0f, 18.0f, 0.1f), 0.0f,
                juce::AudioParameterFloatAttributes().withLabel ("dB")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "highFreq", 1 }, "High Freq",
                juce::NormalisableRange<float> (1500.0f, 18000.0f, 1.0f, 0.4f), 5000.0f,
                juce::AudioParameterFloatAttributes().withLabel ("Hz")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "outputGain", 1 }, "Output Gain",
                juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
                juce::AudioParameterFloatAttributes().withLabel ("dB"))
        };
    }

    void resetFilters()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            lsX1[ch] = lsX2[ch] = lsY1[ch] = lsY2[ch] = 0.0f;
            midX1[ch] = midX2[ch] = midY1[ch] = midY2[ch] = 0.0f;
            hsX1[ch] = hsX2[ch] = hsY1[ch] = hsY2[ch] = 0.0f;
        }
    }

    void updateCoefficients (float lowGain, float lowFreq,
                             float midGain, float midFreq, float midQ,
                             float highGain, float highFreq)
    {
        // 1. Low Shelf (RBJ Cookbook)
        {
            double A = std::pow (10.0, (double) lowGain / 40.0);
            double w0 = 2.0 * juce::MathConstants<double>::pi * (double) lowFreq / sampleRate;
            double cosW0 = std::cos (w0);
            double sinW0 = std::sin (w0);
            double alpha = sinW0 / (2.0 * 0.70710678);
            double twoSqrtAAlpha = 2.0 * std::sqrt (A) * alpha;

            double a0 = (A + 1.0) + (A - 1.0) * cosW0 + twoSqrtAAlpha;
            lsA1 = (float) ((-2.0 * ((A - 1.0) + (A + 1.0) * cosW0)) / a0);
            lsA2 = (float) (((A + 1.0) + (A - 1.0) * cosW0 - twoSqrtAAlpha) / a0);

            lsB0 = (float) ((A * ((A + 1.0) - (A - 1.0) * cosW0 + twoSqrtAAlpha)) / a0);
            lsB1 = (float) ((2.0 * A * ((A - 1.0) - (A + 1.0) * cosW0)) / a0);
            lsB2 = (float) ((A * ((A + 1.0) - (A - 1.0) * cosW0 - twoSqrtAAlpha)) / a0);
        }

        // 2. Mid Peaking Bell (RBJ Cookbook)
        {
            double A = std::pow (10.0, (double) midGain / 40.0);
            double w0 = 2.0 * juce::MathConstants<double>::pi * (double) midFreq / sampleRate;
            double cosW0 = std::cos (w0);
            double sinW0 = std::sin (w0);
            double alpha = sinW0 / (2.0 * (double) midQ);

            double a0 = 1.0 + alpha / A;
            midA1 = (float) ((-2.0 * cosW0) / a0);
            midA2 = (float) ((1.0 - alpha / A) / a0);

            midB0 = (float) ((1.0 + alpha * A) / a0);
            midB1 = (float) ((-2.0 * cosW0) / a0);
            midB2 = (float) ((1.0 - alpha * A) / a0);
        }

        // 3. High Shelf (RBJ Cookbook)
        {
            double A = std::pow (10.0, (double) highGain / 40.0);
            double w0 = 2.0 * juce::MathConstants<double>::pi * (double) highFreq / sampleRate;
            double cosW0 = std::cos (w0);
            double sinW0 = std::sin (w0);
            double alpha = sinW0 / (2.0 * 0.70710678);
            double twoSqrtAAlpha = 2.0 * std::sqrt (A) * alpha;

            double a0 = (A + 1.0) - (A - 1.0) * cosW0 + twoSqrtAAlpha;
            hsA1 = (float) ((2.0 * ((A - 1.0) - (A + 1.0) * cosW0)) / a0);
            hsA2 = (float) (((A + 1.0) - (A - 1.0) * cosW0 - twoSqrtAAlpha) / a0);

            hsB0 = (float) ((A * ((A + 1.0) + (A - 1.0) * cosW0 + twoSqrtAAlpha)) / a0);
            hsB1 = (float) ((-2.0 * A * ((A - 1.0) + (A + 1.0) * cosW0)) / a0);
            hsB2 = (float) ((A * ((A + 1.0) + (A - 1.0) * cosW0 - twoSqrtAAlpha)) / a0);
        }
    }

    inline float processBiquad (float in, float b0, float b1, float b2, float a1, float a2,
                                float& x1, float& x2, float& y1, float& y2)
    {
        float out = b0 * in + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1;
        x1 = in;
        y2 = y1;
        y1 = out;
        return out;
    }

    ParamRef lowGainParam;
    ParamRef lowFreqParam;
    ParamRef midGainParam;
    ParamRef midFreqParam;
    ParamRef midQParam;
    ParamRef highGainParam;
    ParamRef highFreqParam;
    ParamRef outputGainParam;

    double sampleRate = 44100.0;

    float lsB0 = 1, lsB1 = 0, lsB2 = 0, lsA1 = 0, lsA2 = 0;
    float midB0 = 1, midB1 = 0, midB2 = 0, midA1 = 0, midA2 = 0;
    float hsB0 = 1, hsB1 = 0, hsB2 = 0, hsA1 = 0, hsA2 = 0;

    float lsX1[2], lsX2[2], lsY1[2], lsY2[2];
    float midX1[2], midX2[2], midY1[2], midY2[2];
    float hsX1[2], hsX2[2], hsY1[2], hsY2[2];

    std::atomic<float> liveLevel { 0.0f };
};

// ==============================================================================
// 3-Band EQ Custom Graphical Editor
// ==============================================================================
class ThreeBandEQModuleEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    ThreeBandEQModuleEditor (ThreeBandEQModule& p, juce::AudioProcessorValueTreeState& vts)
        : AudioProcessorEditor (&p), module (p)
    {
        auto setupGainKnob = [this] (juce::Slider& s, const juce::String& suffix, juce::Colour col)
        {
            s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
            s.setTextValueSuffix (suffix);
            s.setColour (juce::Slider::rotarySliderFillColourId, col);
            s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff18181c));
            s.setColour (juce::Slider::textBoxOutlineColourId, UITheme::strokeHairline);
            s.setColour (juce::Slider::textBoxTextColourId, UITheme::textPrimary);
            addAndMakeVisible (s);
        };

        auto setupMiniKnob = [this] (juce::Slider& s, const juce::String& suffix, juce::Colour col)
        {
            s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 14);
            s.setTextValueSuffix (suffix);
            s.setColour (juce::Slider::rotarySliderFillColourId, col);
            s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff141417));
            s.setColour (juce::Slider::textBoxOutlineColourId, UITheme::strokeHairline);
            s.setColour (juce::Slider::textBoxTextColourId, UITheme::textSecondary);
            addAndMakeVisible (s);
        };

        // Main Band Gain Knobs
        setupGainKnob (lowGainSlider,  " dB", UITheme::applePurple);
        setupGainKnob (midGainSlider,  " dB", UITheme::appleGreen);
        setupGainKnob (highGainSlider, " dB", UITheme::appleCyan);
        setupMiniKnob (outGainSlider,  " dB", UITheme::appleBlue);

        // Frequency Knobs
        setupMiniKnob (lowFreqSlider,  " Hz", UITheme::applePurple.withAlpha (0.75f));
        setupMiniKnob (midFreqSlider,  " Hz", UITheme::appleGreen.withAlpha (0.75f));
        setupMiniKnob (midQSlider,     " Q",  UITheme::appleGreen.withAlpha (0.75f));
        setupMiniKnob (highFreqSlider, " Hz", UITheme::appleCyan.withAlpha (0.75f));

        lowGainAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "lowGain", lowGainSlider);
        lowFreqAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "lowFreq", lowFreqSlider);
        midGainAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "midGain", midGainSlider);
        midFreqAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "midFreq", midFreqSlider);
        midQAttach     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "midQ", midQSlider);
        highGainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "highGain", highGainSlider);
        highFreqAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "highFreq", highFreqSlider);
        outGainAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "outputGain", outGainSlider);

        flatBtn.setButtonText ("FLAT");
        flatBtn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff202026));
        flatBtn.setColour (juce::TextButton::textColourOffId, UITheme::textSecondary);
        flatBtn.onClick = [&vts] {
            if (auto* p = vts.getParameter ("lowGain"))  p->setValueNotifyingHost (p->getDefaultValue());
            if (auto* p = vts.getParameter ("midGain"))  p->setValueNotifyingHost (p->getDefaultValue());
            if (auto* p = vts.getParameter ("highGain")) p->setValueNotifyingHost (p->getDefaultValue());
        };
        addAndMakeVisible (flatBtn);

        setSize (460, 310);
        startTimerHz (60);
    }

    ~ThreeBandEQModuleEditor() override { stopTimer(); }

    void timerCallback() override { repaint(); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff141417));

        // Header
        auto header = getLocalBounds().removeFromTop (34);
        g.setColour (UITheme::cardHeader);
        g.fillRect (header);
        g.setColour (UITheme::strokeHairline);
        g.drawHorizontalLine (34, 0.0f, (float) getWidth());

        g.setColour (UITheme::textPrimary);
        g.setFont (UITheme::getFont (12.0f, true));
        g.drawText ("3-BAND MUSICAL EQUALIZER", header.withTrimmedLeft (14), juce::Justification::centredLeft);

        // Visual Curve Display Box
        auto curveBounds = juce::Rectangle<float> (14.0f, 42.0f, (float) getWidth() - 28.0f, 96.0f);
        g.setColour (juce::Colour (0xff0e0e11));
        g.fillRoundedRectangle (curveBounds, 4.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (curveBounds, 4.0f, 1.0f);

        // Draw frequency grid lines (100 Hz, 1 kHz, 10 kHz) and dB center line
        float midY = curveBounds.getY() + curveBounds.getHeight() * 0.5f;
        g.setColour (juce::Colour (0x25ffffff));
        g.drawHorizontalLine ((int) midY, curveBounds.getX(), curveBounds.getRight());

        auto getXForFreq = [&curveBounds] (float freq) -> float {
            float minF = 20.0f, maxF = 20000.0f;
            float norm = std::log10 (freq / minF) / std::log10 (maxF / minF);
            return curveBounds.getX() + norm * curveBounds.getWidth();
        };

        for (float f : { 100.0f, 1000.0f, 10000.0f })
        {
            float fx = getXForFreq (f);
            g.drawVerticalLine ((int) fx, curveBounds.getY(), curveBounds.getBottom());
        }

        // Draw EQ Magnitude Response Curve
        juce::Path curvePath;
        const int numPoints = 180;
        float minF = 20.0f, maxF = 20000.0f;

        for (int i = 0; i <= numPoints; ++i)
        {
            float frac = (float) i / (float) numPoints;
            float freq = minF * std::pow (maxF / minF, frac);
            float db = module.getMagnitudeAtFrequency (freq);
            float y = midY - (db / 18.0f) * (curveBounds.getHeight() * 0.44f);
            y = juce::jlimit (curveBounds.getY() + 2.0f, curveBounds.getBottom() - 2.0f, y);

            float x = curveBounds.getX() + frac * curveBounds.getWidth();
            if (i == 0) curvePath.startNewSubPath (x, y);
            else        curvePath.lineTo (x, y);
        }

        // Fill under curve
        juce::Path fillPath = curvePath;
        fillPath.lineTo (curveBounds.getRight(), midY);
        fillPath.lineTo (curveBounds.getX(), midY);
        fillPath.closeSubPath();

        juce::ColourGradient fillGrad (
            UITheme::applePurple.withAlpha (0.18f), curveBounds.getX(), midY,
            UITheme::appleCyan.withAlpha (0.18f), curveBounds.getRight(), midY, false);
        g.setGradientFill (fillGrad);
        g.fillPath (fillPath);

        // Stroke curve
        juce::ColourGradient strokeGrad (
            UITheme::applePurple, curveBounds.getX(), midY,
            UITheme::appleCyan, curveBounds.getRight(), midY, false);
        strokeGrad.addColour (0.5, UITheme::appleGreen);
        g.setGradientFill (strokeGrad);
        g.strokePath (curvePath, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Column Labels
        int colW = (getWidth() - 80) / 3;
        int colY = 148;
        g.setFont (UITheme::getFont (10.0f, true));

        g.setColour (UITheme::applePurple);
        g.drawText ("LOW BAND", 14 + colW * 0, colY, colW, 14, juce::Justification::centred);

        g.setColour (UITheme::appleGreen);
        g.drawText ("MID BAND", 14 + colW * 1, colY, colW, 14, juce::Justification::centred);

        g.setColour (UITheme::appleCyan);
        g.drawText ("HIGH BAND", 14 + colW * 2, colY, colW, 14, juce::Justification::centred);

        g.setColour (UITheme::appleBlue);
        g.drawText ("OUT", getWidth() - 66, colY, 52, 14, juce::Justification::centred);
    }

    void resized() override
    {
        flatBtn.setBounds (getWidth() - 64, 6, 50, 22);

        int colW = (getWidth() - 80) / 3;
        int knobY = 166;
        int knobSize = 54;
        int miniKnobY = 244;
        int miniKnobSize = 42;

        // Low Column
        lowGainSlider.setBounds (14 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 18);
        lowFreqSlider.setBounds (14 + (colW - miniKnobSize) / 2, miniKnobY, miniKnobSize, miniKnobSize + 16);

        // Mid Column
        midGainSlider.setBounds (14 + colW + (colW - knobSize) / 2, knobY, knobSize, knobSize + 18);
        midFreqSlider.setBounds (14 + colW + (colW / 2) - miniKnobSize - 2, miniKnobY, miniKnobSize, miniKnobSize + 16);
        midQSlider.setBounds    (14 + colW + (colW / 2) + 2, miniKnobY, miniKnobSize, miniKnobSize + 16);

        // High Column
        highGainSlider.setBounds (14 + colW * 2 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 18);
        highFreqSlider.setBounds (14 + colW * 2 + (colW - miniKnobSize) / 2, miniKnobY, miniKnobSize, miniKnobSize + 16);

        // Output Trim
        outGainSlider.setBounds (getWidth() - 64, 180, 50, 68);
    }

private:
    ThreeBandEQModule& module;
    juce::Slider lowGainSlider, lowFreqSlider;
    juce::Slider midGainSlider, midFreqSlider, midQSlider;
    juce::Slider highGainSlider, highFreqSlider;
    juce::Slider outGainSlider;
    juce::TextButton flatBtn;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lowGainAttach, lowFreqAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> midGainAttach, midFreqAttach, midQAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> highGainAttach, highFreqAttach, outGainAttach;
};

inline juce::AudioProcessorEditor* ThreeBandEQModule::createEditor()
{
    return new ThreeBandEQModuleEditor (*this, apvts);
}
