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
        processingParam = getModuleParam ("processing", 55.0f);
        freqRangeParam  = getModuleParam ("freqRange", 2.0f);
        intensityParam  = getModuleParam ("intensity", 5.5f);
        sharpnessParam  = getModuleParam ("sharpness", 14.0f);
        makeupParam     = getModuleParam ("makeup", 0.0f);
        diffParam       = getModuleParam ("diff", 0.0f);
        filterParam     = getModuleParam ("filter", 0.0f);

        for (int b = 0; b < numBands; ++b)
            liveSpectrum[b].store (0.0f);
    }

    ~DeEsserModule() override = default;

    struct Biquad
    {
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float a1 = 0.0f, a2 = 0.0f;
        float z1 = 0.0f, z2 = 0.0f;

        inline float process (float x) noexcept
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }

        void setCoeffs (float inB0, float inB1, float inB2, float inA1, float inA2) noexcept
        {
            b0 = inB0; b1 = inB1; b2 = inB2;
            a1 = inA1; a2 = inA2;
        }

        void reset() noexcept
        {
            z1 = 0.0f;
            z2 = 0.0f;
        }
    };

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
        const float procAmount    = juce::jlimit (0.0f, 100.0f, processingParam.get (55.0f));
        const int rangeIdx        = juce::jlimit (0, 3, (int) freqRangeParam.get (2.0f));
        const float intensity     = juce::jlimit (1.0f, 10.0f, intensityParam.get (5.5f));  // Ratio
        const float sharpness     = juce::jlimit (4.0f, 24.0f, sharpnessParam.get (14.0f));
        const float makeupGain    = juce::Decibels::decibelsToGain (makeupParam.get (0.0f));
        const bool diffAudition   = diffParam.get (0.0f) > 0.5f;
        const bool filterAudition = filterParam.get (0.0f) > 0.5f;

        // Determine Center Frequency & Q
        float centerFreq = 8200.0f;
        switch (rangeIdx)
        {
            case 0: centerFreq = 3800.0f;  break; // Low-Hi
            case 1: centerFreq = 5800.0f;  break; // Mid-Hi
            case 2: centerFreq = 8200.0f;  break; // High
            case 3: centerFreq = 11500.0f; break; // Hi-End
        }

        // Q from sharpness (4dB ~ 0.7 broad natural, 24dB ~ 2.5 surgical)
        const float filterQ = juce::jmap (sharpness, 4.0f, 24.0f, 0.7f, 2.5f);
        updateBandpassCoeffs (centerFreq, filterQ);
        updateReferenceLowpassCoeffs (1800.0f);

        // Ballistics constants
        const float sr    = (float) sampleRate;
        const float attC  = std::exp (-1.0f / (0.0005f * sr));  // Detector attack: 0.5 ms
        const float relC  = std::exp (-1.0f / (0.0080f * sr));  // Detector release: 8.0 ms
        const float grAtt = std::exp (-1.0f / (0.0010f * sr));  // Gain Reduction attack: 1.0 ms
        const float grRel = std::exp (-1.0f / (0.0450f * sr));  // Gain Reduction release: 45.0 ms

        const float slope = 1.0f - (1.0f / intensity);
        const float maxRedDb = 24.0f;
        const int nCh = juce::jmin (numChannels, 2);

        // Threshold in dB: mapped smoothly from Processing amount (0% -> 0 dB, 100% -> -54 dB)
        const float threshDb = juce::jmap (procAmount, 0.0f, 100.0f, 0.0f, -54.0f);

        auto follow = [&] (float& env, float in) {
            const float c = in > env ? attC : relC;
            env = c * env + (1.0f - c) * in;
            if (env < 1e-9f) env = 0.0f;
        };

        float maxEssRedDb = 0.0f;
        float peakInLevel = 0.0f;
        float peakOutLevel = 0.0f;
        float tempBands[numBands] = {};

        for (int i = 0; i < numSamples; ++i)
        {
            float x[2]   = {};
            float sib[2] = {};
            float sibPk  = 0.0f;
            float refPk  = 0.0f;

            for (int c = 0; c < nCh; ++c)
            {
                x[c]   = buffer.getReadPointer (c)[i];
                sib[c] = bp[c].process (x[c]);
                float refVal = lp[c].process (x[c]);

                sibPk = juce::jmax (sibPk, std::abs (sib[c]));
                refPk = juce::jmax (refPk, std::abs (refVal));
                peakInLevel = juce::jmax (peakInLevel, std::abs (x[c]));
            }

            // Stereo-linked envelope followers (prevents stereo image shift)
            follow (sibEnv, sibPk);
            follow (refEnv, refPk);

            const float sibDb = juce::Decibels::gainToDecibels (sibEnv, -100.0f);
            const float refDb = juce::Decibels::gainToDecibels (refEnv, -100.0f);

            // Spectral Sibilance Weight: smooth transition in -12 dB .. 0 dB range
            // (Only active when high-frequency energy is dominant compared to low-mid body)
            const float weight = juce::jlimit (0.0f, 1.0f, (sibDb - refDb + 12.0f) / 12.0f);

            // Over-threshold excess in dB
            float target = 0.0f;
            if (procAmount > 0.5f && sibDb > threshDb)
            {
                const float over = juce::jmax (0.0f, sibDb - threshDb);
                target = juce::jmin (maxRedDb, over * slope * weight);
            }

            // Smooth gain reduction in dB domain
            const float c = target > grDb ? grAtt : grRel;
            grDb = c * grDb + (1.0f - c) * target;
            if (grDb < 1e-4f) grDb = 0.0f;

            maxEssRedDb = juce::jmax (maxEssRedDb, grDb);

            // Convert reduction dB to linear reduction gain
            const float g = juce::Decibels::decibelsToGain (-grDb);

            for (int c = 0; c < nCh; ++c)
            {
                // Parallel Bandpass Subtractive Reduction:
                // out = x[c] + (g - 1.0) * sib[c]
                // When g == 1 (no reduction), out == x[c] with bit-perfect transparent phase.
                const float cleanOut = x[c] + (g - 1.0f) * sib[c];

                float y = filterAudition ? sib[c]
                        : diffAudition   ? (x[c] - cleanOut) * 2.0f
                        :                  cleanOut;

                y *= makeupGain;
                buffer.getWritePointer (c)[i] = y;
                peakOutLevel = juce::jmax (peakOutLevel, std::abs (y));
            }

            // Spectrum Visualization on channel 0
            if (i % 2 == 0)
            {
                float absIn0 = std::abs (x[0]);
                for (int b = 0; b < numBands; ++b)
                {
                    float f = 2000.0f * std::pow (16000.0f / 2000.0f, (float) b / (float) (numBands - 1));
                    float dist = std::abs (f - centerFreq);
                    float w = 1.0f / (1.0f + dist * 0.0025f);
                    tempBands[b] = juce::jmax (tempBands[b], absIn0 * w);
                }
            }
        }

        // If there are more than 2 channels, pass through untouched (or apply makeup)
        for (int c = nCh; c < numChannels; ++c)
        {
            auto* writePtr = buffer.getWritePointer (c);
            for (int i = 0; i < numSamples; ++i)
                writePtr[i] *= makeupGain;
        }

        // Smooth visual meters
        for (int b = 0; b < numBands; ++b)
        {
            float prev = liveSpectrum[b].load (std::memory_order_relaxed);
            float trg  = tempBands[b];
            float smoothed = trg > prev ? (prev * 0.35f + trg * 0.65f) : (prev * 0.85f);
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
        int rangeIdx = (int) freqRangeParam.get (2.0f);
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
            bp[ch].reset();
            lp[ch].reset();
        }
        sibEnv = 0.0f;
        refEnv = 0.0f;
        grDb   = 0.0f;
    }

    void updateBandpassCoeffs (float freq, float q)
    {
        float w0 = 2.0f * juce::MathConstants<float>::pi * (freq / (float) sampleRate);
        float alpha = std::sin (w0) / (2.0f * q);
        float a0 = 1.0f + alpha;

        float b0 = alpha / a0;
        float b1 = 0.0f;
        float b2 = -alpha / a0;
        float a1 = (-2.0f * std::cos (w0)) / a0;
        float a2 = (1.0f - alpha) / a0;

        for (int ch = 0; ch < 2; ++ch)
            bp[ch].setCoeffs (b0, b1, b2, a1, a2);
    }

    void updateReferenceLowpassCoeffs (float freq)
    {
        float w0 = 2.0f * juce::MathConstants<float>::pi * (freq / (float) sampleRate);
        float alpha = std::sin (w0) / (2.0f * 0.7071f);
        float a0 = 1.0f + alpha;

        float b0 = ((1.0f - std::cos (w0)) * 0.5f) / a0;
        float b1 = (1.0f - std::cos (w0)) / a0;
        float b2 = ((1.0f - std::cos (w0)) * 0.5f) / a0;
        float a1 = (-2.0f * std::cos (w0)) / a0;
        float a2 = (1.0f - alpha) / a0;

        for (int ch = 0; ch < 2; ++ch)
            lp[ch].setCoeffs (b0, b1, b2, a1, a2);
    }

    ParamRef processingParam;
    ParamRef freqRangeParam;
    ParamRef intensityParam;
    ParamRef sharpnessParam;
    ParamRef makeupParam;
    ParamRef diffParam;
    ParamRef filterParam;

    double sampleRate = 44100.0;

    Biquad bp[2];
    Biquad lp[2];
    float sibEnv = 0.0f;
    float refEnv = 0.0f;
    float grDb   = 0.0f;

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
