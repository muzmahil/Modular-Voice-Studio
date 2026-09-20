#pragma once
#include "../Graph/ModuleProcessor.h"
#include "../UITheme.h"
#include <cmath>
#include <vector>

class DeBreathModuleEditor;

/**
    De-Breath
    Acoustic inhalation / breath suppressor.
    Detects unvoiced breath turbulence and smoothly attenuates inhalation volume
    without making unnatural gating cuts between speech phrases.
*/
class DeBreathModule : public ModuleProcessor
{
public:
    static constexpr int historySize = 140;

    struct MonitorPoint
    {
        float speechLevel;
        float breathActivity;
        float gainReductionDb;
    };

    DeBreathModule()
        : ModuleProcessor ("De-Breath", createLayout())
    {
        reductionParam   = apvts.getRawParameterValue ("reduction");
        sensitivityParam = apvts.getRawParameterValue ("sensitivity");
        thresholdParam   = apvts.getRawParameterValue ("threshold");

        for (int i = 0; i < historySize; ++i)
            history[i] = { 0.0f, 0.0f, 0.0f };
    }

    ~DeBreathModule() override = default;

    void prepareToPlay (double sr, int) override
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;
        currentGain = 1.0f;
        targetGain = 1.0f;

        hpX1[0] = hpX1[1] = 0.0f;
        hpY1[0] = hpY1[1] = 0.0f;
        lpX1[0] = lpX1[1] = 0.0f;
        lpY1[0] = lpY1[1] = 0.0f;

        highEnergy[0] = highEnergy[1] = 0.0f;
        lowEnergy[0]  = lowEnergy[1]  = 0.0f;
        breathConfidence = 0.0f;
        samplesSincePush = 0;
    }

    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();
        if (numChannels == 0 || numSamples == 0) return;

        float reductionNorm = juce::jlimit (0.0f, 100.0f, reductionParam->load()) * 0.01f;
        float sensitivity   = juce::jlimit (0.0f, 100.0f, sensitivityParam->load()) * 0.01f;
        float threshDb      = thresholdParam->load();
        float threshLin     = juce::Decibels::decibelsToGain (threshDb);

        // Maximum attenuation in linear gain (e.g. -24 dB max reduction)
        float maxAttenuationGain = juce::Decibels::decibelsToGain (-24.0f * reductionNorm);

        // Attack & release smoothing coefficients (15ms attack, 35ms release)
        float attCoeff = std::exp (-1.0f / (float) (0.015f * sampleRate));
        float relCoeff = std::exp (-1.0f / (float) (0.035f * sampleRate));

        float blockMaxIn = 0.0f;
        float blockMaxBreath = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float peak = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                int chIdx = ch < 2 ? ch : 0;
                float in = buffer.getSample (ch, i);
                float absIn = std::abs (in);
                peak = juce::jmax (peak, absIn);

                // 1. High-pass filter (2.5 kHz) to isolate breath turbulence / air rush
                float hpOut = in - hpX1[chIdx] + 0.965f * hpY1[chIdx];
                hpX1[chIdx] = in;
                hpY1[chIdx] = hpOut;

                // 2. Low-pass filter (800 Hz) to isolate voiced pitch energy
                float lpOut = lpY1[chIdx] + 0.12f * (in - lpY1[chIdx]);
                lpY1[chIdx] = lpOut;

                highEnergy[chIdx] = highEnergy[chIdx] * 0.994f + std::abs (hpOut) * 0.006f;
                lowEnergy[chIdx]  = lowEnergy[chIdx]  * 0.994f + std::abs (lpOut) * 0.006f;
            }

            blockMaxIn = juce::jmax (blockMaxIn, peak);

            // Breath Detection: High air turbulence with low vocal fold pitch harmonics
            float hEnergy = (highEnergy[0] + highEnergy[1]) * 0.5f;
            float lEnergy = (lowEnergy[0] + lowEnergy[1]) * 0.5f;
            float totalEnergy = hEnergy + lEnergy + 1e-6f;

            float turbulenceRatio = hEnergy / (totalEnergy);
            bool levelInRange = (peak > threshLin * 0.3f) && (peak < threshLin * 3.5f);

            // Breath confidence score (0.0 to 1.0)
            float instantScore = 0.0f;
            if (levelInRange && turbulenceRatio > (0.65f - sensitivity * 0.25f))
            {
                instantScore = juce::jlimit (0.0f, 1.0f, (turbulenceRatio - 0.40f) * 2.5f);
            }

            breathConfidence = breathConfidence * 0.992f + instantScore * 0.008f;
            blockMaxBreath = juce::jmax (blockMaxBreath, breathConfidence);

            // Smooth gain target
            if (breathConfidence > 0.35f)
            {
                float breathDepth = (breathConfidence - 0.35f) / 0.65f;
                targetGain = (1.0f - breathDepth) * 1.0f + breathDepth * maxAttenuationGain;
            }
            else
            {
                targetGain = 1.0f;
            }

            if (targetGain < currentGain)
                currentGain = attCoeff * currentGain + (1.0f - attCoeff) * targetGain;
            else
                currentGain = relCoeff * currentGain + (1.0f - relCoeff) * targetGain;

            // Apply smooth attenuation
            for (int ch = 0; ch < numChannels; ++ch)
            {
                buffer.setSample (ch, i, buffer.getSample (ch, i) * currentGain);
            }

            // Capture visualizer history
            samplesSincePush++;
            if (samplesSincePush >= (int) (sampleRate / 35.0))
            {
                samplesSincePush = 0;
                int idx = historyWriteIndex.load (std::memory_order_relaxed);
                float redDb = -juce::Decibels::gainToDecibels (currentGain);
                history[idx] = { blockMaxIn, blockMaxBreath, redDb };
                historyWriteIndex.store ((idx + 1) % historySize, std::memory_order_relaxed);
            }
        }

        liveBreathLevel.store (blockMaxBreath, std::memory_order_relaxed);
        liveGainReductionDb.store (-juce::Decibels::gainToDecibels (currentGain), std::memory_order_relaxed);
    }

    juce::AudioProcessorEditor* createEditor() override;

    float getLiveBreath() const        { return liveBreathLevel.load (std::memory_order_relaxed); }
    float getLiveReductionDb() const   { return liveGainReductionDb.load (std::memory_order_relaxed); }

    void getHistoryData (std::vector<MonitorPoint>& dest) const
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
                juce::ParameterID { "reduction", 1 }, "Breath Reduction",
                juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 70.0f,
                juce::AudioParameterFloatAttributes().withLabel ("%")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "sensitivity", 1 }, "Sensitivity",
                juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 55.0f,
                juce::AudioParameterFloatAttributes().withLabel ("%")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "threshold", 1 }, "Inhale Threshold",
                juce::NormalisableRange<float> (-60.0f, -15.0f, 0.5f), -38.0f,
                juce::AudioParameterFloatAttributes().withLabel ("dB"))
        };
    }

    std::atomic<float>* reductionParam   = nullptr;
    std::atomic<float>* sensitivityParam = nullptr;
    std::atomic<float>* thresholdParam   = nullptr;

    double sampleRate = 44100.0;
    float currentGain = 1.0f;
    float targetGain = 1.0f;

    float hpX1[2] = { 0.0f, 0.0f };
    float hpY1[2] = { 0.0f, 0.0f };
    float lpX1[2] = { 0.0f, 0.0f };
    float lpY1[2] = { 0.0f, 0.0f };

    float highEnergy[2] = { 0.0f, 0.0f };
    float lowEnergy[2]  = { 0.0f, 0.0f };
    float breathConfidence = 0.0f;

    std::atomic<float> liveBreathLevel { 0.0f };
    std::atomic<float> liveGainReductionDb { 0.0f };

    MonitorPoint history[historySize];
    std::atomic<int> historyWriteIndex { 0 };
    int samplesSincePush = 0;
};

class DeBreathModuleEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    DeBreathModuleEditor (DeBreathModule& p, juce::AudioProcessorValueTreeState& vts)
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

        setupSlider (reductionSlider,   " %",  UITheme::appleCyan);
        setupSlider (sensitivitySlider, " %",  UITheme::appleGreen);
        setupSlider (thresholdSlider,   " dB", UITheme::appleBlue);

        reductionAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "reduction", reductionSlider);
        sensitivityAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "sensitivity", sensitivitySlider);
        thresholdAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "threshold", thresholdSlider);

        setSize (360, 270);
        startTimerHz (60);
    }

    ~DeBreathModuleEditor() override { stopTimer(); }

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
        g.drawText ("DE-BREATH (INHALATION TAMER)", header.withTrimmedLeft (14), juce::Justification::centredLeft);

        // Breath Status Badge
        float breath = module.getLiveBreath();
        float redDb = module.getLiveReductionDb();
        bool isBreath = breath > 0.40f;
        auto badge = header.removeFromRight (130).toFloat().reduced (6.0f, 8.0f);
        juce::Colour badgeCol = isBreath ? UITheme::appleCyan : UITheme::textTertiary;

        g.setColour (badgeCol.withAlpha (0.25f));
        g.fillRoundedRectangle (badge, 3.0f);
        g.setColour (badgeCol);
        g.drawRoundedRectangle (badge, 3.0f, 1.0f);
        g.setFont (UITheme::getFont (8.0f, true));
        g.drawText (isBreath ? "BREATH -" + juce::String (redDb, 1) + " dB" : "VOICE ACTIVE", badge, juce::Justification::centred);

        // --- REAL-TIME BREATH ACTIVITY MONITOR ---
        auto screenRect = juce::Rectangle<float> (16.0f, 44.0f, (float) getWidth() - 32.0f, 76.0f);
        g.setColour (juce::Colour (0xff111114));
        g.fillRoundedRectangle (screenRect, 4.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (screenRect, 4.0f, 0.8f);

        std::vector<DeBreathModule::MonitorPoint> pts;
        module.getHistoryData (pts);

        if (! pts.empty())
        {
            float stepX = screenRect.getWidth() / (float) (pts.size() - 1);
            juce::Path speechPath, breathPath;
            bool started = false;

            for (size_t i = 0; i < pts.size(); ++i)
            {
                float x = screenRect.getX() + (float) i * stepX;
                float spH = juce::jlimit (0.0f, 1.0f, pts[i].speechLevel * 1.6f) * (screenRect.getHeight() - 8.0f);
                float brH = juce::jlimit (0.0f, 1.0f, pts[i].breathActivity) * (screenRect.getHeight() - 8.0f);

                float spY = screenRect.getBottom() - spH - 4.0f;
                float brY = screenRect.getBottom() - brH - 4.0f;

                if (! started)
                {
                    speechPath.startNewSubPath (x, spY);
                    breathPath.startNewSubPath (x, brY);
                    started = true;
                }
                else
                {
                    speechPath.lineTo (x, spY);
                    breathPath.lineTo (x, brY);
                }
            }

            // Normal voice waveform (Green)
            g.setColour (UITheme::appleGreen.withAlpha (0.45f));
            g.strokePath (speechPath, juce::PathStrokeType (1.2f));

            // Detected Inhalation Breath (Cyan)
            g.setColour (UITheme::appleCyan);
            g.strokePath (breathPath, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Legend
        g.setFont (UITheme::getFont (7.5f, true));
        g.setColour (UITheme::appleGreen);
        g.drawText ("SPEECH LEVEL", juce::Rectangle<float> (screenRect.getX() + 6.0f, screenRect.getY() + 4.0f, 90.0f, 10.0f), juce::Justification::centredLeft);
        g.setColour (UITheme::appleCyan);
        g.drawText ("BREATH ATTENUATION", juce::Rectangle<float> (screenRect.getRight() - 110.0f, screenRect.getY() + 4.0f, 104.0f, 10.0f), juce::Justification::centredRight);

        // Knob labels
        int colW = getWidth() / 3;
        int labelY = 126;
        g.setColour (UITheme::textSecondary);
        g.setFont (UITheme::getFont (9.0f, true));
        g.drawText ("BREATH REDUCTION", colW * 0, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("SENSITIVITY",      colW * 1, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("INHALE THRESHOLD", colW * 2, labelY, colW, 14, juce::Justification::centred);
    }

    void resized() override
    {
        int colW = getWidth() / 3;
        int knobY = 144;
        int knobSize = 64;

        reductionSlider.setBounds   (colW * 0 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        sensitivitySlider.setBounds (colW * 1 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        thresholdSlider.setBounds   (colW * 2 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
    }

private:
    DeBreathModule& module;
    juce::Slider reductionSlider, sensitivitySlider, thresholdSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reductionAttach, sensitivityAttach, thresholdAttach;
};

inline juce::AudioProcessorEditor* DeBreathModule::createEditor()
{
    return new DeBreathModuleEditor (*this, apvts);
}
