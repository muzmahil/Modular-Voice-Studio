#pragma once
#include "../Graph/ModuleProcessor.h"
#include "../UITheme.h"
#include <cmath>
#include <vector>

class SpectralClarityModuleEditor;

/**
    Spectral Clarity / Resonance Tamer
    Real-time dynamic spectral resonance suppression (inspired by Soothe2 / Smooth Operator).
    Tracks harsh nasal and microphone capsule resonances between 400 Hz and 6 kHz,
    and applies adaptive dynamic notch attenuation without deadening the voice.
*/
class SpectralClarityModule : public ModuleProcessor
{
public:
    static constexpr int numBands = 24;
    static constexpr int fftSize = 1024;
    static constexpr int historySize = 120;

    struct ReductionPoint
    {
        float resonanceProfile[numBands];
        float peakReductionDb;
    };

    SpectralClarityModule()
        : ModuleProcessor ("Spectral Clarity", createLayout())
    {
        amountParam    = apvts.getRawParameterValue ("amount");
        thresholdParam = apvts.getRawParameterValue ("threshold");
        sharpnessParam = apvts.getRawParameterValue ("sharpness");
        speedParam     = apvts.getRawParameterValue ("speed");

        for (int b = 0; b < numBands; ++b)
        {
            bandGains[0][b] = 1.0f;
            bandGains[1][b] = 1.0f;
            bandEnergies[0][b] = 0.0f;
            bandEnergies[1][b] = 0.0f;
        }

        for (int i = 0; i < historySize; ++i)
        {
            for (int b = 0; b < numBands; ++b)
                history[i].resonanceProfile[b] = 0.0f;
            history[i].peakReductionDb = 0.0f;
        }
    }

    ~SpectralClarityModule() override = default;

    void prepareToPlay (double sr, int) override
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;

        // Setup 24 log-spaced band filter center frequencies from 350 Hz to 7000 Hz
        for (int b = 0; b < numBands; ++b)
        {
            float norm = (float) b / (float) (numBands - 1);
            bandFreqs[b] = 350.0f * std::pow (7000.0f / 350.0f, norm);

            // 2nd order bandpass filter coefficients per band
            float q = 3.5f;
            double w0 = 2.0 * juce::MathConstants<double>::pi * (double) bandFreqs[b] / sampleRate;
            double alpha = std::sin (w0) / (2.0 * (double) q);

            double b0 = alpha;
            double b1 = 0.0;
            double b2 = -alpha;
            double a0 = 1.0 + alpha;
            double a1 = -2.0 * std::cos (w0);
            double a2 = 1.0 - alpha;

            bpB0[b] = (float) (b0 / a0);
            bpB1[b] = (float) (b1 / a0);
            bpB2[b] = (float) (b2 / a0);
            bpA1[b] = (float) (a1 / a0);
            bpA2[b] = (float) (a2 / a0);

            for (int ch = 0; ch < 2; ++ch)
            {
                bpX1[ch][b] = bpX2[ch][b] = 0.0f;
                bpY1[ch][b] = bpY2[ch][b] = 0.0f;
                bandGains[ch][b] = 1.0f;
                bandEnergies[ch][b] = 0.0f;
            }
        }

        samplesSincePush = 0;
    }

    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();
        if (numChannels == 0 || numSamples == 0) return;

        float amount    = juce::jlimit (0.0f, 100.0f, amountParam->load()) * 0.01f;
        float threshDb  = thresholdParam->load();
        float sharpness = juce::jlimit (0.5f, 3.0f, sharpnessParam->load());
        float speedNorm = juce::jlimit (0.0f, 1.0f, speedParam->load() * 0.01f);

        // Attack & release smoothing coefficients
        float attCoeff = std::exp (-1.0f / (float) ((0.003f + (1.0f - speedNorm) * 0.008f) * sampleRate));
        float relCoeff = std::exp (-1.0f / (float) ((0.020f + (1.0f - speedNorm) * 0.060f) * sampleRate));

        float maxReductionDb = 0.0f;
        float liveBands[numBands];
        for (int b = 0; b < numBands; ++b) liveBands[b] = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                int chIdx = ch < 2 ? ch : 0;
                float inSample = buffer.getSample (ch, i);
                float processedSample = inSample;

                // 1. Calculate energy across 24 bands and detect local resonant peaks
                float bandSignals[numBands];
                float totalEnergy = 1e-6f;

                for (int b = 0; b < numBands; ++b)
                {
                    // Direct Form II Biquad bandpass filter
                    float bpOut = bpB0[b] * inSample + bpB1[b] * bpX1[chIdx][b] + bpB2[b] * bpX2[chIdx][b]
                                  - bpA1[b] * bpY1[chIdx][b] - bpA2[b] * bpY2[chIdx][b];

                    bpX2[chIdx][b] = bpX1[chIdx][b];
                    bpX1[chIdx][b] = inSample;
                    bpY2[chIdx][b] = bpY1[chIdx][b];
                    bpY1[chIdx][b] = bpOut;

                    bandSignals[b] = bpOut;
                    float absOut = std::abs (bpOut);
                    bandEnergies[chIdx][b] = bandEnergies[chIdx][b] * 0.992f + absOut * 0.008f;
                    totalEnergy += bandEnergies[chIdx][b];
                }

                float avgEnergy = totalEnergy / (float) numBands;

                // 2. Identify local spectral prominence (Resonance Peak-to-Average Ratio)
                for (int b = 0; b < numBands; ++b)
                {
                    // Compare band energy to adjacent neighbor average
                    float neighborAvg = avgEnergy;
                    if (b > 0 && b < numBands - 1)
                        neighborAvg = (bandEnergies[chIdx][b - 1] + bandEnergies[chIdx][b + 1]) * 0.5f;
                    else if (b == 0)
                        neighborAvg = bandEnergies[chIdx][1];
                    else
                        neighborAvg = bandEnergies[chIdx][numBands - 2];

                    neighborAvg = juce::jmax (1e-6f, neighborAvg);
                    float prominence = (bandEnergies[chIdx][b] / neighborAvg);
                    float bandDb = juce::Decibels::gainToDecibels (bandEnergies[chIdx][b], -80.0f);

                    // Dynamic notch gain target
                    float targetGain = 1.0f;
                    if (prominence > 1.25f && bandDb > threshDb)
                    {
                        float excess = std::pow (prominence - 1.25f, sharpness);
                        float cutDb = juce::jmin (18.0f, excess * 4.0f * amount);
                        targetGain = juce::Decibels::decibelsToGain (-cutDb);
                        maxReductionDb = juce::jmax (maxReductionDb, cutDb);
                    }

                    // Smooth gain envelope (fast attack, natural release)
                    if (targetGain < bandGains[chIdx][b])
                        bandGains[chIdx][b] = attCoeff * bandGains[chIdx][b] + (1.0f - attCoeff) * targetGain;
                    else
                        bandGains[chIdx][b] = relCoeff * bandGains[chIdx][b] + (1.0f - relCoeff) * targetGain;

                    // Apply dynamic notch to the isolated resonant band
                    float attenuatedBand = bandSignals[b] * (1.0f - bandGains[chIdx][b]);
                    processedSample -= attenuatedBand * amount;

                    if (ch == 0)
                    {
                        float redDb = -juce::Decibels::gainToDecibels (bandGains[0][b]);
                        liveBands[b] = juce::jmax (liveBands[b], redDb);
                    }
                }

                buffer.setSample (ch, i, processedSample);
            }

            // Capture visualizer points
            samplesSincePush++;
            if (samplesSincePush >= (int) (sampleRate / 30.0))
            {
                samplesSincePush = 0;
                int idx = historyWriteIndex.load (std::memory_order_relaxed);
                for (int b = 0; b < numBands; ++b)
                    history[idx].resonanceProfile[b] = liveBands[b];
                history[idx].peakReductionDb = maxReductionDb;
                historyWriteIndex.store ((idx + 1) % historySize, std::memory_order_relaxed);
            }
        }

        livePeakReductionDb.store (maxReductionDb, std::memory_order_relaxed);
    }

    juce::AudioProcessorEditor* createEditor() override;

    float getLivePeakReduction() const { return livePeakReductionDb.load (std::memory_order_relaxed); }
    static float getBandFreq (int bandIndex)
    {
        float norm = (float) bandIndex / (float) (numBands - 1);
        return 350.0f * std::pow (7000.0f / 350.0f, norm);
    }

    void getHistoryData (std::vector<ReductionPoint>& dest) const
    {
        dest.resize (historySize);
        int head = historyWriteIndex.load (std::memory_order_relaxed);
        for (int i = 0; i < historySize; ++i)
            dest[i] = history[(head + i) % historySize];
    }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        return {
            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "amount", 1 }, "Clarity Amount",
                juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 65.0f,
                juce::AudioParameterFloatAttributes().withLabel ("%")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "threshold", 1 }, "Harshness Threshold",
                juce::NormalisableRange<float> (-60.0f, -10.0f, 0.5f), -36.0f,
                juce::AudioParameterFloatAttributes().withLabel ("dB")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "sharpness", 1 }, "Selectivity",
                juce::NormalisableRange<float> (0.5f, 3.0f, 0.1f), 1.4f,
                juce::AudioParameterFloatAttributes().withLabel ("x")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "speed", 1 }, "Response Speed",
                juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 50.0f,
                juce::AudioParameterFloatAttributes().withLabel ("%"))
        };
    }

    std::atomic<float>* amountParam    = nullptr;
    std::atomic<float>* thresholdParam = nullptr;
    std::atomic<float>* sharpnessParam = nullptr;
    std::atomic<float>* speedParam     = nullptr;

    double sampleRate = 44100.0;

    float bandFreqs[numBands];
    float bpB0[numBands], bpB1[numBands], bpB2[numBands];
    float bpA1[numBands], bpA2[numBands];

    float bpX1[2][numBands], bpX2[2][numBands];
    float bpY1[2][numBands], bpY2[2][numBands];
    float bandGains[2][numBands];
    float bandEnergies[2][numBands];

    std::atomic<float> livePeakReductionDb { 0.0f };

    ReductionPoint history[historySize];
    std::atomic<int> historyWriteIndex { 0 };
    int samplesSincePush = 0;
};

class SpectralClarityModuleEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    SpectralClarityModuleEditor (SpectralClarityModule& p, juce::AudioProcessorValueTreeState& vts)
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

        setupSlider (amountSlider,    " %",  UITheme::appleCyan);
        setupSlider (thresholdSlider, " dB", UITheme::appleBlue);
        setupSlider (sharpnessSlider, " x",  UITheme::applePurple);
        setupSlider (speedSlider,     " %",  UITheme::appleGreen);

        amountAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "amount", amountSlider);
        thresholdAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "threshold", thresholdSlider);
        sharpnessAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "sharpness", sharpnessSlider);
        speedAttach     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "speed", speedSlider);

        setSize (430, 280);
        startTimerHz (60);
    }

    ~SpectralClarityModuleEditor() override { stopTimer(); }

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
        g.drawText ("SPECTRAL CLARITY (RESONANCE TAMER)", header.withTrimmedLeft (14), juce::Justification::centredLeft);

        // Live Peak Reduction Badge
        float peakRed = module.getLivePeakReduction();
        bool active = peakRed > 0.5f;
        auto badge = header.removeFromRight (140).toFloat().reduced (6.0f, 8.0f);
        juce::Colour badgeCol = active ? UITheme::appleCyan : UITheme::textTertiary;

        g.setColour (badgeCol.withAlpha (0.25f));
        g.fillRoundedRectangle (badge, 3.0f);
        g.setColour (badgeCol);
        g.drawRoundedRectangle (badge, 3.0f, 1.0f);
        g.setFont (UITheme::getFont (8.0f, true));
        g.drawText (active ? "TAMING -" + juce::String (peakRed, 1) + " dB" : "CLEAN (NO HARSH)", badge, juce::Justification::centred);

        // --- 24-BAND DYNAMIC RESONANCE SUPPRESSION MONITOR ---
        auto screenRect = juce::Rectangle<float> (16.0f, 44.0f, (float) getWidth() - 32.0f, 82.0f);
        g.setColour (juce::Colour (0xff111114));
        g.fillRoundedRectangle (screenRect, 4.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (screenRect, 4.0f, 0.8f);

        std::vector<SpectralClarityModule::ReductionPoint> pts;
        module.getHistoryData (pts);

        if (! pts.empty())
        {
            auto latest = pts.back();
            float barW = (screenRect.getWidth() - 10.0f) / (float) SpectralClarityModule::numBands;

            for (int b = 0; b < SpectralClarityModule::numBands; ++b)
            {
                float x = screenRect.getX() + 5.0f + (float) b * barW;
                float redDb = juce::jlimit (0.0f, 15.0f, latest.resonanceProfile[b]);
                float barH = (redDb / 15.0f) * (screenRect.getHeight() - 16.0f);

                if (barH > 1.0f)
                {
                    auto barRect = juce::Rectangle<float> (x + 1.0f, screenRect.getY() + 8.0f, barW - 2.0f, barH);
                    g.setColour (UITheme::appleCyan.withAlpha (0.80f));
                    g.fillRoundedRectangle (barRect, 1.5f);
                }
            }

            // Frequency calibration markings
            g.setFont (UITheme::getFont (7.5f));
            g.setColour (UITheme::textTertiary);
            g.drawText ("400Hz", juce::Rectangle<float> (screenRect.getX() + 6.0f, screenRect.getBottom() - 12.0f, 40.0f, 10.0f), juce::Justification::centredLeft);
            g.drawText ("1.5kHz (Nasal)", juce::Rectangle<float> (screenRect.getCentreX() - 35.0f, screenRect.getBottom() - 12.0f, 70.0f, 10.0f), juce::Justification::centred);
            g.drawText ("6kHz", juce::Rectangle<float> (screenRect.getRight() - 46.0f, screenRect.getBottom() - 12.0f, 40.0f, 10.0f), juce::Justification::centredRight);
        }

        // Knob labels
        int colW = getWidth() / 4;
        int labelY = 134;
        g.setColour (UITheme::textSecondary);
        g.setFont (UITheme::getFont (9.0f, true));
        g.drawText ("CLARITY AMOUNT", colW * 0, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("THRESHOLD",      colW * 1, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("SELECTIVITY",    colW * 2, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("RESPONSE SPEED", colW * 3, labelY, colW, 14, juce::Justification::centred);
    }

    void resized() override
    {
        int colW = getWidth() / 4;
        int knobY = 152;
        int knobSize = 62;

        amountSlider.setBounds    (colW * 0 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        thresholdSlider.setBounds (colW * 1 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        sharpnessSlider.setBounds (colW * 2 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        speedSlider.setBounds     (colW * 3 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
    }

private:
    SpectralClarityModule& module;
    juce::Slider amountSlider, thresholdSlider, sharpnessSlider, speedSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttach, thresholdAttach, sharpnessAttach, speedAttach;
};

inline juce::AudioProcessorEditor* SpectralClarityModule::createEditor()
{
    return new SpectralClarityModuleEditor (*this, apvts);
}
