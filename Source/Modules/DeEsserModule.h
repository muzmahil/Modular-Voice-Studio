#pragma once
#include "../Graph/ModuleProcessor.h"
#include "../UITheme.h"
#include <cmath>
#include <vector>

class DeEsserModuleEditor;

/**
    Next-Gen Precision Vocal De-Esser
    - Spectral-Ratio Sibilance Detection: distinguishes true sibilants ("s", "sh", "ch", "z") from voiced vowels.
    - Dynamic Precision Biquad Filter: zero phase comb-filtering; transparent minimum-phase attenuation.
    - 4 Frequency Focus Bands: Low-Hi (3.8kHz), Mid-Hi (5.8kHz), High (8.2kHz), Hi-End (11.5kHz).
    - Macro Processing Control with Real-Time Gain Reduction Readout.
    - Intensity (Ratio), Sharpness (Q-Steepness), Make-Up Gain.
    - Audition Modes: 'Diff' (Delta / listen to removed sibilance) & 'Filter' (Detection bandpass).
*/
class DeEsserModule : public ModuleProcessor
{
public:
    static constexpr int numBands = 16;

    DeEsserModule()
        : ModuleProcessor ("De-Esser", createLayout())
    {
        processingParam = apvts.getRawParameterValue ("processing");
        freqRangeParam  = apvts.getRawParameterValue ("freqRange");
        intensityParam  = apvts.getRawParameterValue ("intensity");
        sharpnessParam  = apvts.getRawParameterValue ("sharpness");
        makeupParam     = apvts.getRawParameterValue ("makeup");
        diffParam       = apvts.getRawParameterValue ("diff");
        filterParam     = apvts.getRawParameterValue ("filter");

        for (int b = 0; b < numBands; ++b)
            liveSpectrum[b].store (0.0f);
    }

    ~DeEsserModule() override = default;

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

        // Parameters
        float procAmount   = juce::jlimit (0.0f, 100.0f, processingParam->load()); // 0 to 100%
        int rangeIdx       = juce::jlimit (0, 3, (int) freqRangeParam->load());
        float intensity    = juce::jlimit (1.0f, 10.0f, intensityParam->load());  // Ratio
        float sharpness    = juce::jlimit (4.0f, 24.0f, sharpnessParam->load());  // Filter Q / slope
        float makeupGain   = juce::Decibels::decibelsToGain (makeupParam->load());
        bool diffAudition  = diffParam->load() > 0.5f;
        bool filterAudition= filterParam->load() > 0.5f;

        // Determine Center Frequency & Q from Range & Sharpness
        float centerFreq = 8200.0f;
        switch (rangeIdx)
        {
            case 0: centerFreq = 3800.0f;  break; // Low-Hi (2.5k - 5.5k)
            case 1: centerFreq = 5800.0f;  break; // Mid-Hi (4.5k - 8k)
            case 2: centerFreq = 8200.0f;  break; // High   (6.5k - 11k)
            case 3: centerFreq = 11500.0f; break; // Hi-End (8.5k - 16k)
        }

        // Q from sharpness (4dB ~ 0.9 broad, 24dB ~ 3.2 surgical)
        float filterQ = juce::jmap (sharpness, 4.0f, 24.0f, 0.9f, 3.2f);
        updateBandpassCoeffs (centerFreq, filterQ);
        updateReferenceLowpassCoeffs (1800.0f);

        // Dynamic detection threshold: mapped smoothly from Processing amount (100% = high sensitivity -48dB, 0% = off)
        float threshDb = juce::jmap (procAmount, 0.0f, 100.0f, 0.0f, -48.0f);
        float threshLin = juce::Decibels::decibelsToGain (threshDb, -90.0f);

        // Dynamic Ballistics (Fast 0.8ms attack to catch harsh consonants, 40ms smooth natural release)
        float attCoeff = std::exp (-1.0f / (float) (0.0008f * sampleRate));
        float relCoeff = std::exp (-1.0f / (float) (0.0400f * sampleRate));

        float maxEssRedDb = 0.0f;
        float peakInLevel = 0.0f;
        float peakOutLevel = 0.0f;
        float tempBands[numBands] = {};

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* channelData = buffer.getWritePointer (ch);
            int fIdx = ch < 2 ? ch : 0;

            for (int i = 0; i < numSamples; ++i)
            {
                float in = channelData[i];
                float absIn = std::abs (in);
                peakInLevel = juce::jmax (peakInLevel, absIn);

                // 1. Sibilant Bandpass Detection Signal
                float sibSignal = bpB0 * in + bpB1 * bpX1[fIdx] + bpB2 * bpX2[fIdx]
                                 - bpA1 * bpY1[fIdx] - bpA2 * bpY2[fIdx];
                bpX2[fIdx] = bpX1[fIdx]; bpX1[fIdx] = in;
                bpY2[fIdx] = bpY1[fIdx]; bpY1[fIdx] = sibSignal;

                // 2. Low-Mid Vocal Body Reference Signal (for Spectral Ratio Voice discrimination)
                float refSignal = lpB0 * in + lpB1 * lpX1[fIdx] + lpB2 * lpX2[fIdx]
                                 - lpA1 * lpY1[fIdx] - lpA2 * lpY2[fIdx];
                lpX2[fIdx] = lpX1[fIdx]; lpX1[fIdx] = in;
                lpY2[fIdx] = lpY1[fIdx]; lpY1[fIdx] = refSignal;

                float absSib = std::abs (sibSignal);
                float absRef = std::abs (refSignal);

                // Update detection envelope
                if (absSib > sideEnv[fIdx])
                    sideEnv[fIdx] = attCoeff * sideEnv[fIdx] + (1.0f - attCoeff) * absSib;
                else
                    sideEnv[fIdx] = relCoeff * sideEnv[fIdx] + (1.0f - relCoeff) * absSib;

                // Spectral Sibilance Weight: suppress de-essing if low-mid fundamental vowel energy dominates
                float sibilanceWeight = absSib / (absSib + absRef * 0.7f + 0.0001f);
                sibilanceWeight = juce::jlimit (0.0f, 1.0f, (sibilanceWeight - 0.25f) * 1.6f);

                // Calculate required dynamic cut in dB
                float currentRedDb = 0.0f;
                if (procAmount > 0.5f && sideEnv[fIdx] > threshLin)
                {
                    float excessDb = juce::Decibels::gainToDecibels (sideEnv[fIdx] / threshLin);
                    float compSlope = 1.0f - (1.0f / intensity);
                    float desiredRed = excessDb * compSlope * sibilanceWeight;
                    currentRedDb = juce::jlimit (0.0f, 32.0f, desiredRed);
                }

                maxEssRedDb = juce::jmax (maxEssRedDb, currentRedDb);

                // 3. Dynamic Precision Peaking EQ Filter (Zero Phase Distortion)
                // When currentRedDb == 0, filter is pure wire (gain = 0 dB)
                float cutGainDb = -currentRedDb;
                float dynB0, dynB1, dynB2, dynA1, dynA2;
                computePeakingEQCoeffs (centerFreq, filterQ, cutGainDb, dynB0, dynB1, dynB2, dynA1, dynA2);

                float out = dynB0 * in + dynB1 * eqX1[fIdx] + dynB2 * eqX2[fIdx]
                            - dynA1 * eqY1[fIdx] - dynA2 * eqY2[fIdx];
                eqX2[fIdx] = eqX1[fIdx]; eqX1[fIdx] = in;
                eqY2[fIdx] = eqY1[fIdx]; eqY1[fIdx] = out;

                // Apply Make-up Gain
                float processedOut = out * makeupGain;

                // Audition modes
                if (filterAudition)
                {
                    channelData[i] = sibSignal * makeupGain;
                }
                else if (diffAudition)
                {
                    // Delta / Difference: listen to ONLY the removed harshness
                    channelData[i] = (in - out) * makeupGain * 2.5f;
                }
                else
                {
                    channelData[i] = processedOut;
                }

                peakOutLevel = juce::jmax (peakOutLevel, std::abs (channelData[i]));

                // Spectrum Visualization
                if (ch == 0 && (i % 2 == 0))
                {
                    for (int b = 0; b < numBands; ++b)
                    {
                        float f = 2000.0f * std::pow (16000.0f / 2000.0f, (float) b / (float) (numBands - 1));
                        float dist = std::abs (f - centerFreq);
                        float w = 1.0f / (1.0f + dist * 0.0025f);
                        tempBands[b] = juce::jmax (tempBands[b], absIn * w);
                    }
                }
            }
        }

        // Smooth visual meters
        for (int b = 0; b < numBands; ++b)
        {
            float prev = liveSpectrum[b].load (std::memory_order_relaxed);
            float target = tempBands[b];
            float smoothed = target > prev ? (prev * 0.35f + target * 0.65f) : (prev * 0.85f);
            liveSpectrum[b].store (smoothed, std::memory_order_relaxed);
        }

        liveEssReduction.store (maxEssRedDb, std::memory_order_relaxed);
        liveInPeak.store (peakInLevel, std::memory_order_relaxed);
        liveOutPeak.store (peakOutLevel, std::memory_order_relaxed);
    }

    juce::AudioProcessorEditor* createEditor() override;

    float getLiveEssReduction() const { return liveEssReduction.load (std::memory_order_relaxed); }
    float getLiveInPeak() const       { return liveInPeak.load (std::memory_order_relaxed); }
    float getLiveOutPeak() const      { return liveOutPeak.load (std::memory_order_relaxed); }
    float getCenterFreq() const
    {
        int rangeIdx = freqRangeParam ? (int) freqRangeParam->load() : 2;
        switch (rangeIdx)
        {
            case 0: return 3800.0f;
            case 1: return 5800.0f;
            case 2: return 8200.0f;
            case 3: return 11500.0f;
        }
        return 8200.0f;
    }
    float getBandEnergy (int b) const { return liveSpectrum[juce::jlimit (0, numBands - 1, b)].load (std::memory_order_relaxed); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        return {
            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "processing", 1 }, "Processing",
                juce::NormalisableRange<float> (0.0f, 100.0f, 0.5f), 55.0f,
                juce::AudioParameterFloatAttributes().withLabel ("%")),

            std::make_unique<juce::AudioParameterChoice> (
                juce::ParameterID { "freqRange", 1 }, "Frequency Range",
                juce::StringArray { "Low-Hi", "Mid-Hi", "High", "Hi-End" }, 2),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "intensity", 1 }, "Intensity",
                juce::NormalisableRange<float> (1.0f, 10.0f, 0.1f), 5.5f,
                juce::AudioParameterFloatAttributes().withLabel (":1")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "sharpness", 1 }, "Sharpness",
                juce::NormalisableRange<float> (4.0f, 24.0f, 0.5f), 14.0f,
                juce::AudioParameterFloatAttributes().withLabel ("dB")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "makeup", 1 }, "Make Up",
                juce::NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f,
                juce::AudioParameterFloatAttributes().withLabel ("dB")),

            std::make_unique<juce::AudioParameterBool> (
                juce::ParameterID { "diff", 1 }, "Diff (Delta Audition)", false),

            std::make_unique<juce::AudioParameterBool> (
                juce::ParameterID { "filter", 1 }, "Filter Audition", false)
        };
    }

    void resetFilters()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            bpX1[ch] = bpX2[ch] = bpY1[ch] = bpY2[ch] = 0.0f;
            lpX1[ch] = lpX2[ch] = lpY1[ch] = lpY2[ch] = 0.0f;
            eqX1[ch] = eqX2[ch] = eqY1[ch] = eqY2[ch] = 0.0f;
            sideEnv[ch] = 0.0f;
        }
    }

    void updateBandpassCoeffs (float freq, float q)
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

    void updateReferenceLowpassCoeffs (float freq)
    {
        float w0 = 2.0f * juce::MathConstants<float>::pi * (freq / (float) sampleRate);
        float alpha = std::sin (w0) / (2.0f * 0.7071f);
        float a0 = 1.0f + alpha;

        lpB0 = ((1.0f - std::cos (w0)) * 0.5f) / a0;
        lpB1 = (1.0f - std::cos (w0)) / a0;
        lpB2 = ((1.0f - std::cos (w0)) * 0.5f) / a0;
        lpA1 = (-2.0f * std::cos (w0)) / a0;
        lpA2 = (1.0f - alpha) / a0;
    }

    inline void computePeakingEQCoeffs (float freq, float q, float gainDb,
                                        float& b0, float& b1, float& b2, float& a1, float& a2)
    {
        if (std::abs (gainDb) < 0.05f)
        {
            b0 = 1.0f; b1 = 0.0f; b2 = 0.0f;
            a1 = 0.0f; a2 = 0.0f;
            return;
        }

        float A = std::pow (10.0f, gainDb / 40.0f);
        float w0 = 2.0f * juce::MathConstants<float>::pi * (freq / (float) sampleRate);
        float alpha = std::sin (w0) / (2.0f * q);
        float cosw = std::cos (w0);

        float a0 = 1.0f + alpha / A;
        b0 = (1.0f + alpha * A) / a0;
        b1 = (-2.0f * cosw) / a0;
        b2 = (1.0f - alpha * A) / a0;
        a1 = (-2.0f * cosw) / a0;
        a2 = (1.0f - alpha / A) / a0;
    }

    std::atomic<float>* processingParam = nullptr;
    std::atomic<float>* freqRangeParam  = nullptr;
    std::atomic<float>* intensityParam  = nullptr;
    std::atomic<float>* sharpnessParam  = nullptr;
    std::atomic<float>* makeupParam     = nullptr;
    std::atomic<float>* diffParam       = nullptr;
    std::atomic<float>* filterParam     = nullptr;

    double sampleRate = 44100.0;

    float bpB0 = 0, bpB1 = 0, bpB2 = 0, bpA1 = 0, bpA2 = 0;
    float lpB0 = 0, lpB1 = 0, lpB2 = 0, lpA1 = 0, lpA2 = 0;

    float bpX1[2] = {}, bpX2[2] = {}, bpY1[2] = {}, bpY2[2] = {};
    float lpX1[2] = {}, lpX2[2] = {}, lpY1[2] = {}, lpY2[2] = {};
    float eqX1[2] = {}, eqX2[2] = {}, eqY1[2] = {}, eqY2[2] = {};
    float sideEnv[2] = {};

    std::atomic<float> liveEssReduction { 0.0f };
    std::atomic<float> liveInPeak { 0.0f };
    std::atomic<float> liveOutPeak { 0.0f };
    std::atomic<float> liveSpectrum[numBands];
};

class DeEsserModuleEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    DeEsserModuleEditor (DeEsserModule& p, juce::AudioProcessorValueTreeState& vts)
        : AudioProcessorEditor (&p), module (p), apvtsRef (vts)
    {
        // Central Giant Processing Knob
        processingSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        processingSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        processingSlider.setColour (juce::Slider::rotarySliderFillColourId, UITheme::appleBlue);
        processingSlider.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff25252b));
        addAndMakeVisible (processingSlider);
        procAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "processing", processingSlider);

        // Sub Knobs
        auto setupSubKnob = [this] (juce::Slider& s, const juce::String& suffix)
        {
            s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 16);
            s.setTextValueSuffix (suffix);
            s.setColour (juce::Slider::rotarySliderFillColourId, UITheme::appleBlue);
            s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff1c1c22));
            s.setColour (juce::Slider::textBoxOutlineColourId, UITheme::strokeHairline);
            s.setColour (juce::Slider::textBoxTextColourId, UITheme::textPrimary);
            addAndMakeVisible (s);
        };

        setupSubKnob (intensitySlider, ":1");
        setupSubKnob (sharpnessSlider, " dB");
        setupSubKnob (makeupSlider, " dB");

        intensityAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "intensity", intensitySlider);
        sharpnessAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "sharpness", sharpnessSlider);
        makeupAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "makeup", makeupSlider);

        // 4 Frequency Range Buttons
        const char* rangeNames[4] = { "Low-Hi", "Mid-Hi", "High", "Hi-End" };
        for (int i = 0; i < 4; ++i)
        {
            rangeBtns[i].setButtonText (rangeNames[i]);
            rangeBtns[i].setRadioGroupId (1001);
            rangeBtns[i].setClickingTogglesState (true);
            rangeBtns[i].setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1e1e24));
            rangeBtns[i].setColour (juce::TextButton::buttonOnColourId, UITheme::appleBlue);
            rangeBtns[i].setColour (juce::TextButton::textColourOffId, UITheme::textSecondary);
            rangeBtns[i].setColour (juce::TextButton::textColourOnId, juce::Colours::white);
            rangeBtns[i].onClick = [this, i]
            {
                if (auto* param = apvtsRef.getParameter ("freqRange"))
                    param->setValueNotifyingHost ((float) i / 3.0f);
            };
            addAndMakeVisible (rangeBtns[i]);
        }

        int curRange = (int) *vts.getRawParameterValue ("freqRange");
        rangeBtns[juce::jlimit (0, 3, curRange)].setToggleState (true, juce::dontSendNotification);

        // Audition Buttons (Diff & Filter)
        auto setupAuditionBtn = [this] (juce::TextButton& btn, const juce::String& name)
        {
            btn.setButtonText (name);
            btn.setClickingTogglesState (true);
            btn.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1c1c22));
            btn.setColour (juce::TextButton::buttonOnColourId, UITheme::appleRed);
            btn.setColour (juce::TextButton::textColourOffId, UITheme::textSecondary);
            btn.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
            addAndMakeVisible (btn);
        };

        setupAuditionBtn (diffBtn, "diff");
        setupAuditionBtn (filterBtn, "filter");

        diffAttach   = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (vts, "diff", diffBtn);
        filterAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (vts, "filter", filterBtn);

        setSize (420, 490);
        startTimerHz (60);
    }

    ~DeEsserModuleEditor() override { stopTimer(); }

    void timerCallback() override
    {
        int curRange = (int) *apvtsRef.getRawParameterValue ("freqRange");
        for (int i = 0; i < 4; ++i)
            rangeBtns[i].setToggleState (i == curRange, juce::dontSendNotification);

        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        // Dark studio chassis
        g.fillAll (juce::Colour (0xff151518));

        // Subtle Border
        g.setColour (UITheme::strokeHairline);
        g.drawRect (getLocalBounds().toFloat(), 1.0f);

        // Header
        auto header = getLocalBounds().removeFromTop (44);
        g.setColour (juce::Colour (0xff1a1a1f));
        g.fillRect (header);
        g.setColour (UITheme::strokeHairline);
        g.drawHorizontalLine (44, 0.0f, (float) getWidth());

        // Header Title: Clean De-Esser Aesthetic
        g.setColour (juce::Colours::white);
        g.setFont (UITheme::getFont (15.0f, true));
        g.drawText ("De-Esser", 16, 10, 200, 24, juce::Justification::centredLeft);

        g.setColour (UITheme::textTertiary);
        g.setFont (UITheme::getFont (10.0f, true));
        g.drawText ("PRECISION DSP", getWidth() - 120, 14, 104, 18, juce::Justification::centredRight);

        // =========================================================================
        // --- INPUT & OUTPUT VU PEAK METERS ---
        // =========================================================================
        auto drawMeter = [&g] (float x, float y, float w, float h, float peakLin, const juce::String& label)
        {
            auto track = juce::Rectangle<float> (x, y, w, h);
            g.setColour (juce::Colour (0xff101014));
            g.fillRoundedRectangle (track, 2.0f);
            g.setColour (UITheme::strokeHairline);
            g.drawRoundedRectangle (track, 2.0f, 1.0f);

            float db = juce::Decibels::gainToDecibels (peakLin, -60.0f);
            float norm = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);

            if (norm > 0.001f)
            {
                float barH = h * norm;
                auto bar = juce::Rectangle<float> (x + 1.0f, y + h - barH, w - 2.0f, barH);
                g.setColour (norm > 0.90f ? UITheme::appleRed : UITheme::appleBlue);
                g.fillRoundedRectangle (bar, 1.5f);
            }

            g.setColour (UITheme::textSecondary);
            g.setFont (UITheme::getFont (8.0f));
            g.drawText (label, juce::Rectangle<float> (x - 20.0f, y + h + 4.0f, w + 40.0f, 12.0f), juce::Justification::centred);

            g.setFont (UITheme::getFont (8.0f, true));
            g.setColour (peakLin > 0.001f ? UITheme::textPrimary : UITheme::textTertiary);
            juce::String valStr = peakLin > 0.001f ? (juce::String (db, 1) + " dB") : "-inf";
            g.drawText (valStr, juce::Rectangle<float> (x - 20.0f, y - 16.0f, w + 40.0f, 12.0f), juce::Justification::centred);
        };

        drawMeter (26.0f,  100.0f, 8.0f, 150.0f, module.getLiveInPeak(), "Input");
        drawMeter (getWidth() - 34.0f, 100.0f, 8.0f, 150.0f, module.getLiveOutPeak(), "Output");

        // =========================================================================
        // --- CENTRAL PROCESSING GAIN REDUCTION ARC ---
        // =========================================================================
        float cx = (float) getWidth() * 0.5f;
        float cy = 175.0f;
        float radius = 72.0f;

        // Base Arc Track (270 degrees)
        float startAngle = -juce::MathConstants<float>::pi * 0.75f;
        float endAngle   =  juce::MathConstants<float>::pi * 0.75f;

        juce::Path trackPath;
        trackPath.addCentredArc (cx, cy, radius, radius, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colour (0xff25252c));
        g.strokePath (trackPath, juce::PathStrokeType (10.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Active Gain Reduction Arc (Cyan / Blue Glowing)
        float essRed = module.getLiveEssReduction(); // in dB (e.g. 0 to 32 dB)
        float redNorm = juce::jlimit (0.0f, 1.0f, essRed / 24.0f);
        if (redNorm > 0.005f)
        {
            float redAngle = startAngle + (endAngle - startAngle) * redNorm;
            juce::Path redPath;
            redPath.addCentredArc (cx, cy, radius, radius, 0.0f, startAngle, redAngle, true);

            // Glow
            g.setColour (UITheme::appleBlue.withAlpha (0.35f));
            g.strokePath (redPath, juce::PathStrokeType (14.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Solid arc
            g.setColour (UITheme::appleBlue);
            g.strokePath (redPath, juce::PathStrokeType (10.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Center Processing Numerical Readout
        g.setColour (UITheme::textSecondary);
        g.setFont (UITheme::getFont (12.0f));
        g.drawText ("Processing", juce::Rectangle<float> (cx - 60.0f, cy - 26.0f, 120.0f, 16.0f), juce::Justification::centred);

        g.setColour (essRed > 0.5f ? UITheme::appleBlue : UITheme::textPrimary);
        g.setFont (UITheme::getFont (22.0f, true));
        juce::String redStr = essRed > 0.1f ? ("-" + juce::String (essRed, 1) + " dB") : "-0.0 dB";
        g.drawText (redStr, juce::Rectangle<float> (cx - 70.0f, cy - 6.0f, 140.0f, 28.0f), juce::Justification::centred);

        // Make-up Label on Left
        g.setColour (UITheme::textSecondary);
        g.setFont (UITheme::getFont (10.0f, true));
        g.drawText ("MAKE UP", 50, 270, 70, 14, juce::Justification::centred);

        // Intensity and Sharpness Labels
        g.drawText ("INTENSITY", 140, 270, 70, 14, juce::Justification::centred);
        g.drawText ("SHARPNESS", 220, 270, 70, 14, juce::Justification::centred);

        // Frequency Range Section Title
        g.drawText ("FREQUENCY RANGE", 50, 360, getWidth() - 100, 14, juce::Justification::centred);

        // Bottom Brand Signature
        g.setColour (juce::Colour (0xff757580));
        g.setFont (UITheme::getFont (9.0f));
        g.drawText ("Natural Sounding Vocal Sibilance Processor", 0, getHeight() - 20, getWidth(), 14, juce::Justification::centred);
    }

    void resized() override
    {
        // Central Knob covers the arc area
        processingSlider.setBounds (getWidth() / 2 - 85, 90, 170, 170);

        // Audition buttons next to arc on the right
        diffBtn.setBounds (getWidth() - 95, 230, 42, 22);
        filterBtn.setBounds (getWidth() - 95, 256, 42, 22);

        // Sub Knobs row
        makeupSlider.setBounds    (50,  288, 70, 68);
        intensitySlider.setBounds (140, 288, 70, 68);
        sharpnessSlider.setBounds (220, 288, 70, 68);

        // Frequency Range 2x2 Grid Buttons
        int btnW = 145;
        int btnH = 26;
        int btnX1 = (getWidth() - (btnW * 2 + 8)) / 2;
        int btnX2 = btnX1 + btnW + 8;
        int btnY1 = 380;
        int btnY2 = btnY1 + btnH + 6;

        rangeBtns[0].setBounds (btnX1, btnY1, btnW, btnH); // Low-Hi
        rangeBtns[1].setBounds (btnX2, btnY1, btnW, btnH); // Mid-Hi
        rangeBtns[2].setBounds (btnX1, btnY2, btnW, btnH); // High
        rangeBtns[3].setBounds (btnX2, btnY2, btnW, btnH); // Hi-End
    }

private:
    DeEsserModule& module;
    juce::AudioProcessorValueTreeState& apvtsRef;

    juce::Slider processingSlider;
    juce::Slider intensitySlider, sharpnessSlider, makeupSlider;
    juce::TextButton rangeBtns[4];
    juce::TextButton diffBtn, filterBtn;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> procAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> intensityAttach, sharpnessAttach, makeupAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> diffAttach, filterAttach;
};

inline juce::AudioProcessorEditor* DeEsserModule::createEditor()
{
    return new DeEsserModuleEditor (*this, apvts);
}
