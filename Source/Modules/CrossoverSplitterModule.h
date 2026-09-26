#pragma once
#include "../Graph/ModuleProcessor.h"
#include "../UITheme.h"
#include <cmath>
#include <vector>

class CrossoverSplitterModuleEditor;

/**
    Frequency Crossover Splitter (Modular 3-Way Splitter)
    1 Stereo Audio IN -> 3 Discrete Stereo Audio OUTs (LOW, MID, HIGH).
    Uses 4th-order 24 dB/oct Linkwitz-Riley crossover filters with zero-phase ripple reconstruction.
*/
class CrossoverSplitterModule : public ModuleProcessor
{
public:
    CrossoverSplitterModule()
        : ModuleProcessor ("Frequency Splitter",
                           BusesProperties()
                               .withInput  ("Input",       juce::AudioChannelSet::stereo(), true)
                               .withOutput ("Low Output",  juce::AudioChannelSet::stereo(), true)
                               .withOutput ("Mid Output",  juce::AudioChannelSet::stereo(), true)
                               .withOutput ("High Output", juce::AudioChannelSet::stereo(), true),
                           createLayout())
    {
        lowMidFreqParam  = getRawParam ("lowMidFreq");
        midHighFreqParam = getRawParam ("midHighFreq");
    }

    ~CrossoverSplitterModule() override = default;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        return layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()
            && layouts.getChannelSet (false, 0) == juce::AudioChannelSet::stereo();
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

        float fLowMid  = juce::jlimit (60.0f, 1500.0f, lowMidFreqParam ? lowMidFreqParam->load() : 250.0f);
        float fMidHigh = juce::jlimit (1000.0f, 12000.0f, midHighFreqParam ? midHighFreqParam->load() : 3500.0f);

        if (fLowMid >= fMidHigh)
            fLowMid = fMidHigh * 0.8f;

        updateCoefficients (fLowMid, fMidHigh);

        // Make temporary copies of input channels
        const float* inL = buffer.getReadPointer (0);
        const float* inR = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : inL;

        juce::AudioBuffer<float> inCopy (2, numSamples);
        inCopy.copyFrom (0, 0, inL, numSamples);
        inCopy.copyFrom (1, 0, inR, numSamples);

        // Ensure all output channels exist or clear if needed
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.clear (ch, 0, numSamples);

        float blockLowPeak = 0.0f;
        float blockMidPeak = 0.0f;
        float blockHighPeak = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                float in = inCopy.getSample (ch, i);

                // Stage 1: Low/Mid crossover split (24 dB/oct LR4)
                float lowBand = processLR4Low (in, lp1X1[ch], lp1X2[ch], lp1Y1[ch], lp1Y2[ch],
                                                   lp2X1[ch], lp2X2[ch], lp2Y1[ch], lp2Y2[ch],
                                                   lmB0, lmB1, lmB2, lmA1, lmA2);

                float highPass1 = processLR4High (in, hp1X1[ch], hp1X2[ch], hp1Y1[ch], hp1Y2[ch],
                                                      hp2X1[ch], hp2X2[ch], hp2Y1[ch], hp2Y2[ch],
                                                      lmHpB0, lmHpB1, lmHpB2, lmA1, lmA2);

                // Stage 2: Mid/High crossover split from the highPass1 residual (24 dB/oct LR4)
                float midBand = processLR4Low (highPass1, lp3X1[ch], lp3X2[ch], lp3Y1[ch], lp3Y2[ch],
                                                          lp4X1[ch], lp4X2[ch], lp4Y1[ch], lp4Y2[ch],
                                                          mhB0, mhB1, mhB2, mhA1, mhA2);

                float highBand = processLR4High (highPass1, hp3X1[ch], hp3X2[ch], hp3Y1[ch], hp3Y2[ch],
                                                            hp4X1[ch], hp4X2[ch], hp4Y1[ch], hp4Y2[ch],
                                                            mhHpB0, mhHpB1, mhHpB2, mhA1, mhA2);

                blockLowPeak  = juce::jmax (blockLowPeak, std::abs (lowBand));
                blockMidPeak  = juce::jmax (blockMidPeak, std::abs (midBand));
                blockHighPeak = juce::jmax (blockHighPeak, std::abs (highBand));

                // Route to Discrete Output Buses:
                // Channels 0, 1: LOW
                if (numChannels > ch)
                    buffer.setSample (ch, i, lowBand);

                // Channels 2, 3: MID
                if (numChannels > ch + 2)
                    buffer.setSample (ch + 2, i, midBand);

                // Channels 4, 5: HIGH
                if (numChannels > ch + 4)
                    buffer.setSample (ch + 4, i, highBand);
            }
        }

        liveLowLevel.store (blockLowPeak, std::memory_order_relaxed);
        liveMidLevel.store (blockMidPeak, std::memory_order_relaxed);
        liveHighLevel.store (blockHighPeak, std::memory_order_relaxed);
    }

    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    float getLiveLow() const  { return liveLowLevel.load (std::memory_order_relaxed); }
    float getLiveMid() const  { return liveMidLevel.load (std::memory_order_relaxed); }
    float getLiveHigh() const { return liveHighLevel.load (std::memory_order_relaxed); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        return {
            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "lowMidFreq", 1 }, "Low/Mid Split",
                juce::NormalisableRange<float> (60.0f, 1500.0f, 1.0f, 0.4f), 250.0f,
                juce::AudioParameterFloatAttributes().withLabel ("Hz")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "midHighFreq", 1 }, "Mid/High Split",
                juce::NormalisableRange<float> (1000.0f, 12000.0f, 1.0f, 0.4f), 3500.0f,
                juce::AudioParameterFloatAttributes().withLabel ("Hz"))
        };
    }

    void resetFilters()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            lp1X1[ch] = lp1X2[ch] = lp1Y1[ch] = lp1Y2[ch] = 0.0f;
            lp2X1[ch] = lp2X2[ch] = lp2Y1[ch] = lp2Y2[ch] = 0.0f;
            hp1X1[ch] = hp1X2[ch] = hp1Y1[ch] = hp1Y2[ch] = 0.0f;
            hp2X1[ch] = hp2X2[ch] = hp2Y1[ch] = hp2Y2[ch] = 0.0f;

            lp3X1[ch] = lp3X2[ch] = lp3Y1[ch] = lp3Y2[ch] = 0.0f;
            lp4X1[ch] = lp4X2[ch] = lp4Y1[ch] = lp4Y2[ch] = 0.0f;
            hp3X1[ch] = hp3X2[ch] = hp3Y1[ch] = hp3Y2[ch] = 0.0f;
            hp4X1[ch] = hp4X2[ch] = hp4Y1[ch] = hp4Y2[ch] = 0.0f;
        }
    }

    void updateCoefficients (float fLowMid, float fMidHigh)
    {
        computeBiquadCoeffs (fLowMid, lmB0, lmB1, lmB2, lmHpB0, lmHpB1, lmHpB2, lmA1, lmA2);
        computeBiquadCoeffs (fMidHigh, mhB0, mhB1, mhB2, mhHpB0, mhHpB1, mhHpB2, mhA1, mhA2);
    }

    void computeBiquadCoeffs (float freq, float& lpB0, float& lpB1, float& lpB2,
                              float& hpB0, float& hpB1, float& hpB2, float& a1, float& a2)
    {
        double omega = 2.0 * juce::MathConstants<double>::pi * (double) freq / sampleRate;
        double cosw = std::cos (omega);
        double sinw = std::sin (omega);
        double alpha = sinw / (2.0 * 0.70710678);

        double a0 = 1.0 + alpha;
        a1 = (float) ((-2.0 * cosw) / a0);
        a2 = (float) ((1.0 - alpha) / a0);

        lpB0 = (float) (((1.0 - cosw) * 0.5) / a0);
        lpB1 = (float) ((1.0 - cosw) / a0);
        lpB2 = (float) (((1.0 - cosw) * 0.5) / a0);

        hpB0 = (float) (((1.0 + cosw) * 0.5) / a0);
        hpB1 = (float) ((-(1.0 + cosw)) / a0);
        hpB2 = (float) (((1.0 + cosw) * 0.5) / a0);
    }

    inline float processLR4Low (float in, float& x1a, float& x2a, float& y1a, float& y2a,
                                          float& x1b, float& x2b, float& y1b, float& y2b,
                                          float b0, float b1, float b2, float a1, float a2)
    {
        float yA = b0 * in + b1 * x1a + b2 * x2a - a1 * y1a - a2 * y2a;
        x2a = x1a; x1a = in; y2a = y1a; y1a = yA;

        float yB = b0 * yA + b1 * x1b + b2 * x2b - a1 * y1b - a2 * y2b;
        x2b = x1b; x1b = yA; y2b = y1b; y1b = yB;
        return yB;
    }

    inline float processLR4High (float in, float& x1a, float& x2a, float& y1a, float& y2a,
                                           float& x1b, float& x2b, float& y1b, float& y2b,
                                           float b0, float b1, float b2, float a1, float a2)
    {
        float yA = b0 * in + b1 * x1a + b2 * x2a - a1 * y1a - a2 * y2a;
        x2a = x1a; x1a = in; y2a = y1a; y1a = yA;

        float yB = b0 * yA + b1 * x1b + b2 * x2b - a1 * y1b - a2 * y2b;
        x2b = x1b; x1b = yA; y2b = y1b; y1b = yB;
        return yB;
    }

    std::atomic<float>* lowMidFreqParam  = nullptr;
    std::atomic<float>* midHighFreqParam = nullptr;

    double sampleRate = 44100.0;

    float lmB0 = 0, lmB1 = 0, lmB2 = 0, lmHpB0 = 0, lmHpB1 = 0, lmHpB2 = 0, lmA1 = 0, lmA2 = 0;
    float mhB0 = 0, mhB1 = 0, mhB2 = 0, mhHpB0 = 0, mhHpB1 = 0, mhHpB2 = 0, mhA1 = 0, mhA2 = 0;

    float lp1X1[2], lp1X2[2], lp1Y1[2], lp1Y2[2];
    float lp2X1[2], lp2X2[2], lp2Y1[2], lp2Y2[2];
    float hp1X1[2], hp1X2[2], hp1Y1[2], hp1Y2[2];
    float hp2X1[2], hp2X2[2], hp2Y1[2], hp2Y2[2];

    float lp3X1[2], lp3X2[2], lp3Y1[2], lp3Y2[2];
    float lp4X1[2], lp4X2[2], lp4Y1[2], lp4Y2[2];
    float hp3X1[2], hp3X2[2], hp3Y1[2], hp3Y2[2];
    float hp4X1[2], hp4X2[2], hp4Y1[2], hp4Y2[2];

    std::atomic<float> liveLowLevel { 0.0f };
    std::atomic<float> liveMidLevel { 0.0f };
    std::atomic<float> liveHighLevel { 0.0f };
};

class CrossoverSplitterModuleEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    CrossoverSplitterModuleEditor (CrossoverSplitterModule& p, juce::AudioProcessorValueTreeState& vts)
        : AudioProcessorEditor (&p), module (p)
    {
        auto setupSlider = [this] (juce::Slider& s, const juce::String& suffix, juce::Colour col)
        {
            s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 18);
            s.setTextValueSuffix (suffix);
            s.setColour (juce::Slider::rotarySliderFillColourId, col);
            s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff1e1e24));
            s.setColour (juce::Slider::textBoxOutlineColourId, UITheme::strokeHairline);
            s.setColour (juce::Slider::textBoxTextColourId, UITheme::textPrimary);
            addAndMakeVisible (s);
        };

        setupSlider (lowMidSlider,  " Hz", UITheme::appleBlue);
        setupSlider (midHighSlider, " Hz", UITheme::appleCyan);

        lowMidAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "lowMidFreq", lowMidSlider);
        midHighAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "midHighFreq", midHighSlider);

        setSize (420, 220);
        startTimerHz (60);
    }

    ~CrossoverSplitterModuleEditor() override { stopTimer(); }

    void timerCallback() override { repaint(); }

    juce::Rectangle<float> getVisualizerBounds() const
    {
        return juce::Rectangle<float> (16.0f, 44.0f, (float) getWidth() - 32.0f, 76.0f);
    }

    float freqToX (float f, const juce::Rectangle<float>& bounds) const
    {
        float norm = std::log10 (juce::jlimit (20.0f, 20000.0f, f) / 20.0f) / std::log10 (20000.0f / 20.0f);
        return bounds.getX() + norm * bounds.getWidth();
    }

    float xToFreq (float x, const juce::Rectangle<float>& bounds) const
    {
        float norm = juce::jlimit (0.0f, 1.0f, (x - bounds.getX()) / bounds.getWidth());
        return 20.0f * std::pow (1000.0f, norm);
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        auto bounds = getVisualizerBounds();
        if (bounds.contains (e.position))
        {
            float xLM = freqToX ((float) lowMidSlider.getValue(), bounds);
            float xMH = freqToX ((float) midHighSlider.getValue(), bounds);

            if (std::abs (e.position.x - xLM) <= 6.0f || std::abs (e.position.x - xMH) <= 6.0f)
            {
                setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
                return;
            }
        }
        setMouseCursor (juce::MouseCursor::NormalCursor);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        auto bounds = getVisualizerBounds();
        if (bounds.contains (e.position))
        {
            float xLM = freqToX ((float) lowMidSlider.getValue(), bounds);
            float xMH = freqToX ((float) midHighSlider.getValue(), bounds);

            float dLM = std::abs (e.position.x - xLM);
            float dMH = std::abs (e.position.x - xMH);

            if (dLM <= 10.0f && dLM <= dMH)
            {
                draggingLowMid = true;
                draggingMidHigh = false;
            }
            else if (dMH <= 10.0f)
            {
                draggingMidHigh = true;
                draggingLowMid = false;
            }
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto bounds = getVisualizerBounds();
        if (draggingLowMid)
        {
            float freq = xToFreq (e.position.x, bounds);
            float maxFreq = (float) midHighSlider.getValue() - 50.0f;
            lowMidSlider.setValue (juce::jlimit (60.0f, maxFreq, freq), juce::sendNotificationSync);
        }
        else if (draggingMidHigh)
        {
            float freq = xToFreq (e.position.x, bounds);
            float minFreq = (float) lowMidSlider.getValue() + 50.0f;
            midHighSlider.setValue (juce::jlimit (minFreq, 12000.0f, freq), juce::sendNotificationSync);
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        draggingLowMid = false;
        draggingMidHigh = false;
    }

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
        g.drawText ("FREKANS AYIRICI (1 IN -> 3 OUT)", header.withTrimmedLeft (14), juce::Justification::centredLeft);

        // Active State Badge
        auto badge = header.removeFromRight (180).toFloat().reduced (6.0f, 8.0f);
        g.setColour (juce::Colour (0x200a84ff));
        g.fillRoundedRectangle (badge, 3.0f);
        g.setColour (UITheme::appleBlue);
        g.drawRoundedRectangle (badge, 3.0f, 1.0f);
        g.setFont (UITheme::getFont (8.0f, true));
        g.drawText ("LOW | MID | HIGH SPLIT", badge, juce::Justification::centred);

        // --- 3-BAND FREQUENCY SPLIT VISUALIZER ---
        auto screenRect = getVisualizerBounds();
        g.setColour (juce::Colour (0xff111114));
        g.fillRoundedRectangle (screenRect, 4.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (screenRect, 4.0f, 0.8f);

        float fLM = (float) lowMidSlider.getValue();
        float fMH = (float) midHighSlider.getValue();

        float xLM = freqToX (fLM, screenRect);
        float xMH = freqToX (fMH, screenRect);

        // Low Band Region (Purple fill)
        auto lowRect = juce::Rectangle<float> (screenRect.getX(), screenRect.getY(), xLM - screenRect.getX(), screenRect.getHeight());
        g.setColour (UITheme::applePurple.withAlpha (0.24f));
        g.fillRect (lowRect);

        // Mid Band Region (Green fill)
        auto midRect = juce::Rectangle<float> (xLM, screenRect.getY(), xMH - xLM, screenRect.getHeight());
        g.setColour (UITheme::appleGreen.withAlpha (0.24f));
        g.fillRect (midRect);

        // High Band Region (Cyan fill)
        auto highRect = juce::Rectangle<float> (xMH, screenRect.getY(), screenRect.getRight() - xMH, screenRect.getHeight());
        g.setColour (UITheme::appleCyan.withAlpha (0.24f));
        g.fillRect (highRect);

        // Split Draggable Divider Lines
        g.setColour (UITheme::appleBlue);
        g.drawLine (xLM, screenRect.getY(), xLM, screenRect.getBottom(), 2.0f);
        g.fillEllipse (xLM - 4.0f, screenRect.getCentreY() - 4.0f, 8.0f, 8.0f);

        g.setColour (UITheme::appleCyan);
        g.drawLine (xMH, screenRect.getY(), xMH, screenRect.getBottom(), 2.0f);
        g.fillEllipse (xMH - 4.0f, screenRect.getCentreY() - 4.0f, 8.0f, 8.0f);

        // Labels inside visualizer bands
        g.setFont (UITheme::getFont (9.0f, true));
        g.setColour (UITheme::applePurple);
        g.drawText ("LOW OUT", lowRect, juce::Justification::centred);

        g.setColour (UITheme::appleGreen);
        g.drawText ("MID OUT", midRect, juce::Justification::centred);

        g.setColour (UITheme::appleCyan);
        g.drawText ("HIGH OUT", highRect, juce::Justification::centred);

        // Knob headers
        int colW = getWidth() / 2;
        int labelY = 126;
        g.setColour (UITheme::textSecondary);
        g.setFont (UITheme::getFont (9.0f, true));
        g.drawText ("LOW / MID CROSSOVER",  0,    labelY, colW, 14, juce::Justification::centred);
        g.drawText ("MID / HIGH CROSSOVER", colW, labelY, colW, 14, juce::Justification::centred);
    }

    void resized() override
    {
        int colW = getWidth() / 2;
        int knobY = 142;
        int knobSize = 58;

        lowMidSlider.setBounds  ((colW - knobSize) / 2,        knobY, knobSize, knobSize + 20);
        midHighSlider.setBounds (colW + (colW - knobSize) / 2, knobY, knobSize, knobSize + 20);
    }

private:
    CrossoverSplitterModule& module;
    juce::Slider lowMidSlider, midHighSlider;
    bool draggingLowMid = false;
    bool draggingMidHigh = false;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lowMidAttach, midHighAttach;
};

inline juce::AudioProcessorEditor* CrossoverSplitterModule::createEditor()
{
    return new CrossoverSplitterModuleEditor (*this, apvts);
}
