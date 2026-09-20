#pragma once
#include "../Graph/ModuleProcessor.h"
#include "../UITheme.h"
#include <cmath>
#include <vector>

class GateModuleEditor;

class GateModule : public ModuleProcessor
{
public:
    struct HistoryPoint
    {
        float inputDb;  // in dB (-80..0)
        float gain;     // 0..1
        bool  isOpen;
    };
    static constexpr int historyLength = 200;

    GateModule()
        : ModuleProcessor ("Gate", createLayout())
    {
        thresholdParam = apvts.getRawParameterValue ("threshold");
        releaseParam   = apvts.getRawParameterValue ("release");
        rangeParam     = apvts.getRawParameterValue ("range");
        attackParam    = apvts.getRawParameterValue ("attack");

        for (int i = 0; i < historyLength; ++i)
            history[i] = { -80.0f, 1.0f, false };
    }

    void prepareToPlay (double sr, int) override
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;
        currentGain = 1.0f;
        envelope = 0.0f;
        gateOpenTarget = 1.0f;
        holdCounter = 0;
        dcIn[0] = dcIn[1] = 0.0f;
        dcOut[0] = dcOut[1] = 0.0f;
        samplesSinceHistoryPush = 0;
    }

    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();
        if (numChannels == 0 || numSamples == 0) return;

        float threshDb = thresholdParam->load();
        float threshLinear = juce::Decibels::decibelsToGain (threshDb, -90.0f);
        float hysteresisLinear = juce::Decibels::decibelsToGain (threshDb - 2.5f, -90.0f);

        float relMs = juce::jmax (10.0f, releaseParam->load());
        float attMs = juce::jmax (0.5f, attackParam->load());
        float rangeDb = rangeParam->load();
        float floorGain = juce::Decibels::decibelsToGain (rangeDb, -90.0f);

        float attCoeff = std::exp (-1.0f / (float) ((attMs * 0.001f) * sampleRate));
        float relCoeff = std::exp (-1.0f / (float) ((relMs * 0.001f) * sampleRate));

        float blockPeakIn = 0.0f;
        float blockPeakOut = 0.0f;
        int holdSamples = (int) (0.025f * (float) sampleRate); // 25ms vocal hold time

        for (int i = 0; i < numSamples; ++i)
        {
            float peak = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                int chIdx = ch < 2 ? ch : 0;
                float raw = buffer.getSample (ch, i);

                // 10 Hz DC-blocking filter (eliminates DC pop when opening/closing)
                float filtered = raw - dcIn[chIdx] + 0.9985f * dcOut[chIdx];
                dcIn[chIdx] = raw;
                dcOut[chIdx] = filtered;

                peak = juce::jmax (peak, std::abs (filtered));
            }

            blockPeakIn = juce::jmax (blockPeakIn, peak);

            // Envelope detection with smooth syllabic tracking
            float envCoeff = peak > envelope ? 0.35f : 0.002f;
            envelope = envelope * (1.0f - envCoeff) + peak * envCoeff;

            // Gate target logic with hysteresis & 25ms vocal hold time (prevents syllable chatter)
            if (envelope >= threshLinear)
            {
                gateOpenTarget = 1.0f;
                holdCounter = holdSamples;
            }
            else if (holdCounter > 0)
            {
                holdCounter--;
                gateOpenTarget = 1.0f;
            }
            else if (envelope < hysteresisLinear)
            {
                gateOpenTarget = floorGain;
            }

            // Smooth gain transition
            if (gateOpenTarget > currentGain)
                currentGain = attCoeff * currentGain + (1.0f - attCoeff) * gateOpenTarget;
            else
                currentGain = relCoeff * currentGain + (1.0f - relCoeff) * gateOpenTarget;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                int chIdx = ch < 2 ? ch : 0;
                float processed = dcOut[chIdx] * currentGain;
                buffer.setSample (ch, i, processed);
                blockPeakOut = juce::jmax (blockPeakOut, std::abs (processed));
            }
        }

        liveGainReduction.store (currentGain, std::memory_order_relaxed);
        bool open = gateOpenTarget > floorGain + 0.05f;
        liveGateState.store (open ? 1.0f : 0.0f, std::memory_order_relaxed);
        liveInputPeak.store (blockPeakIn, std::memory_order_relaxed);
        liveOutputPeak.store (blockPeakOut, std::memory_order_relaxed);

        // Record history point (~60 times per second)
        samplesSinceHistoryPush += numSamples;
        int interval = (int) (sampleRate / 60.0);
        if (samplesSinceHistoryPush >= interval)
        {
            samplesSinceHistoryPush = 0;
            float inDb = juce::Decibels::gainToDecibels (blockPeakIn, -80.0f);
            int idx = historyWriteIndex.load (std::memory_order_relaxed);
            history[idx] = { inDb, currentGain, open };
            historyWriteIndex.store ((idx + 1) % historyLength, std::memory_order_relaxed);
        }
    }

    juce::AudioProcessorEditor* createEditor() override;

    float getLiveGainReduction() const { return liveGainReduction.load (std::memory_order_relaxed); }
    bool isGateOpen() const { return liveGateState.load (std::memory_order_relaxed) > 0.5f; }
    float getThresholdDb() const { return thresholdParam ? thresholdParam->load() : -42.0f; }
    float getInputPeak() const { return liveInputPeak.load (std::memory_order_relaxed); }
    float getOutputPeak() const { return liveOutputPeak.load (std::memory_order_relaxed); }

    void getHistory (std::vector<HistoryPoint>& dest) const
    {
        dest.resize (historyLength);
        int head = historyWriteIndex.load (std::memory_order_relaxed);
        for (int i = 0; i < historyLength; ++i)
            dest[i] = history[(head + i) % historyLength];
    }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        return {
            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "threshold", 1 }, "Threshold",
                juce::NormalisableRange<float> (-80.0f, 0.0f, 0.5f), -42.0f,
                juce::AudioParameterFloatAttributes().withLabel ("dB")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "release", 1 }, "Release",
                juce::NormalisableRange<float> (10.0f, 800.0f, 1.0f), 120.0f,
                juce::AudioParameterFloatAttributes().withLabel ("ms")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "range", 1 }, "Floor Range",
                juce::NormalisableRange<float> (-80.0f, 0.0f, 0.5f), -40.0f,
                juce::AudioParameterFloatAttributes().withLabel ("dB")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "attack", 1 }, "Attack",
                juce::NormalisableRange<float> (0.5f, 40.0f, 0.1f), 2.0f,
                juce::AudioParameterFloatAttributes().withLabel ("ms"))
        };
    }

    std::atomic<float>* thresholdParam = nullptr;
    std::atomic<float>* releaseParam   = nullptr;
    std::atomic<float>* rangeParam     = nullptr;
    std::atomic<float>* attackParam    = nullptr;

    double sampleRate = 44100.0;
    float currentGain = 1.0f;
    float envelope = 0.0f;
    float gateOpenTarget = 1.0f;
    int holdCounter = 0;
    float dcIn[2] = { 0.0f, 0.0f };
    float dcOut[2] = { 0.0f, 0.0f };
    int samplesSinceHistoryPush = 0;

    std::atomic<float> liveGainReduction { 1.0f };
    std::atomic<float> liveGateState { 1.0f };
    std::atomic<float> liveInputPeak { 0.0f };
    std::atomic<float> liveOutputPeak { 0.0f };

    HistoryPoint history[historyLength];
    std::atomic<int> historyWriteIndex { 0 };
};

class GateModuleEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    GateModuleEditor (GateModule& p, juce::AudioProcessorValueTreeState& vts)
        : AudioProcessorEditor (&p), module (p)
    {
        auto setupSlider = [this] (juce::Slider& s, const juce::String& suffix)
        {
            s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 18);
            s.setTextValueSuffix (suffix);
            s.setColour (juce::Slider::rotarySliderFillColourId, UITheme::appleBlue);
            s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff1e1e24));
            s.setColour (juce::Slider::textBoxOutlineColourId, UITheme::strokeHairline);
            s.setColour (juce::Slider::textBoxTextColourId, UITheme::textPrimary);
            addAndMakeVisible (s);
        };

        setupSlider (thresholdSlider, " dB");
        setupSlider (releaseSlider, " ms");
        setupSlider (rangeSlider, " dB");

        thresholdAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "threshold", thresholdSlider);
        releaseAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "release", releaseSlider);
        rangeAttach     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "range", rangeSlider);

        setSize (330, 270);
        startTimerHz (60);
    }

    ~GateModuleEditor() override { stopTimer(); }

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
        g.drawText ("VOCAL NOISE GATE", header.withTrimmedLeft (14), juce::Justification::centredLeft);

        // Gate Status LED Indicator
        bool isOpen = module.isGateOpen();
        auto ledArea = header.removeFromRight (76).toFloat().reduced (8.0f, 8.0f);
        g.setColour (isOpen ? UITheme::appleGreen.withAlpha (0.25f) : juce::Colour (0x15ffffff));
        g.fillRoundedRectangle (ledArea, 3.0f);
        g.setColour (isOpen ? UITheme::appleGreen : UITheme::textTertiary);
        g.drawRoundedRectangle (ledArea, 3.0f, 1.0f);
        g.setFont (UITheme::getFont (8.5f, true));
        g.drawText (isOpen ? "OPEN" : "MUTED", ledArea, juce::Justification::centred);

        // --- REAL-TIME DYNAMIC SCROLLING WAVEFORM WINDOW ---
        auto screenRect = juce::Rectangle<float> (16.0f, 44.0f, (float) getWidth() - 44.0f, 74.0f);
        g.setColour (juce::Colour (0xff111114));
        g.fillRoundedRectangle (screenRect, 4.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (screenRect, 4.0f, 0.8f);

        // dB Grid lines
        g.setColour (juce::Colour (0x12ffffff));
        for (float db : { -12.0f, -24.0f, -36.0f, -48.0f, -60.0f })
        {
            float norm = 1.0f - (db / -80.0f);
            float gy = screenRect.getY() + (1.0f - norm) * screenRect.getHeight();
            g.drawHorizontalLine ((int) gy, screenRect.getX(), screenRect.getRight());
        }

        // Fetch circular history points
        std::vector<GateModule::HistoryPoint> pts;
        module.getHistory (pts);

        if (! pts.empty())
        {
            float stepX = screenRect.getWidth() / (float) (pts.size() - 1);

            // Draw scrolling waveform history
            juce::Path wavePath;
            bool pathStarted = false;

            for (size_t i = 0; i < pts.size(); ++i)
            {
                float x = screenRect.getX() + (float) i * stepX;
                float normLevel = juce::jlimit (0.0f, 1.0f, (pts[i].inputDb + 80.0f) / 80.0f);
                float y = screenRect.getBottom() - normLevel * (screenRect.getHeight() - 4.0f) - 2.0f;

                if (! pathStarted)
                {
                    wavePath.startNewSubPath (x, y);
                    pathStarted = true;
                }
                else
                {
                    wavePath.lineTo (x, y);
                }
            }

            // Waveform stroke (green when open, red/gray when muted)
            g.setColour (isOpen ? UITheme::appleGreen.withAlpha (0.90f) : UITheme::appleRed.withAlpha (0.60f));
            g.strokePath (wavePath, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Filled shaded wave body
            juce::Path filledPath = wavePath;
            filledPath.lineTo (screenRect.getRight(), screenRect.getBottom());
            filledPath.lineTo (screenRect.getX(), screenRect.getBottom());
            filledPath.closeSubPath();
            g.setColour (isOpen ? UITheme::appleGreen.withAlpha (0.15f) : juce::Colour (0x10ff453a));
            g.fillPath (filledPath);

            // Real-time Gain Reduction Shading from top
            float grNorm = juce::jlimit (0.0f, 1.0f, 1.0f - module.getLiveGainReduction());
            if (grNorm > 0.02f)
            {
                float grH = grNorm * screenRect.getHeight();
                auto grRect = screenRect.withHeight (grH);
                g.setColour (UITheme::appleRed.withAlpha (0.28f));
                g.fillRect (grRect);
                g.setColour (UITheme::appleRed.withAlpha (0.75f));
                g.drawHorizontalLine ((int) (screenRect.getY() + grH), screenRect.getX(), screenRect.getRight());
            }
        }

        // --- MOVABLE DYNAMIC THRESHOLD LINE ---
        float threshDb = module.getThresholdDb();
        float threshNorm = juce::jlimit (0.0f, 1.0f, (threshDb + 80.0f) / 80.0f);
        float threshY = screenRect.getBottom() - threshNorm * (screenRect.getHeight() - 4.0f) - 2.0f;

        // Dashed Studio Cyan Threshold Line
        g.setColour (juce::Colour (0xff38bdf8));
        float dashes[] = { 4.0f, 3.0f };
        g.drawDashedLine (juce::Line<float> (screenRect.getX(), threshY, screenRect.getRight(), threshY), dashes, 2, 1.4f);

        // Threshold dB Tag
        g.setColour (juce::Colour (0xff38bdf8));
        g.setFont (UITheme::getFont (7.5f, true));
        g.drawText (juce::String ((int) threshDb) + " dB",
                    juce::Rectangle<float> (screenRect.getRight() - 36.0f, threshY - 11.0f, 34.0f, 10.0f),
                    juce::Justification::centredRight);

        // --- DUAL MINI VU METERS (IN & OUT) ---
        float meterX = screenRect.getRight() + 6.0f;
        float meterW = 8.0f;
        auto inMeterRect  = juce::Rectangle<float> (meterX, screenRect.getY(), meterW * 0.5f, screenRect.getHeight());
        auto outMeterRect = juce::Rectangle<float> (meterX + meterW * 0.5f + 2.0f, screenRect.getY(), meterW * 0.5f, screenRect.getHeight());

        // Backgrounds
        g.setColour (juce::Colour (0xff1b1b20));
        g.fillRect (inMeterRect);
        g.fillRect (outMeterRect);

        // Levels
        float inNorm  = juce::jlimit (0.0f, 1.0f, module.getInputPeak() * 1.2f);
        float outNorm = juce::jlimit (0.0f, 1.0f, module.getOutputPeak() * 1.2f);

        g.setColour (UITheme::appleBlue);
        g.fillRect (inMeterRect.removeFromBottom (inMeterRect.getHeight() * inNorm));

        g.setColour (isOpen ? UITheme::appleGreen : UITheme::textTertiary);
        g.fillRect (outMeterRect.removeFromBottom (outMeterRect.getHeight() * outNorm));

        // Knob labels
        int colW = getWidth() / 3;
        int labelY = 126;
        g.setColour (UITheme::textSecondary);
        g.setFont (UITheme::getFont (9.0f, true));
        g.drawText ("THRESHOLD", 0, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("RELEASE", colW, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("RANGE", colW * 2, labelY, colW, 14, juce::Justification::centred);
    }

    void resized() override
    {
        int colW = getWidth() / 3;
        int knobY = 144;
        int knobSize = 74;

        thresholdSlider.setBounds (colW * 0 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        releaseSlider.setBounds   (colW * 1 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        rangeSlider.setBounds     (colW * 2 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
    }

private:
    GateModule& module;
    juce::Slider thresholdSlider, releaseSlider, rangeSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> thresholdAttach, releaseAttach, rangeAttach;
};

inline juce::AudioProcessorEditor* GateModule::createEditor()
{
    return new GateModuleEditor (*this, apvts);
}
