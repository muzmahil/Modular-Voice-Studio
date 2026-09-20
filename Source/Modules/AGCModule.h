#pragma once
#include "../Graph/ModuleProcessor.h"
#include "../UITheme.h"
#include <cmath>
#include <vector>

class AGCModuleEditor;

class AGCModule : public ModuleProcessor
{
public:
    static constexpr int historyLength = 160;

    struct LevelPoint
    {
        float inRmsDb;
        float gainAppliedDb;
    };

    AGCModule()
        : ModuleProcessor ("AGC", createLayout())
    {
        targetParam   = apvts.getRawParameterValue ("target");
        maxBoostParam = apvts.getRawParameterValue ("maxBoost");
        maxCutParam   = apvts.getRawParameterValue ("maxCut");
        speedParam    = apvts.getRawParameterValue ("speed");
        gateParam     = apvts.getRawParameterValue ("gateThresh");

        for (int i = 0; i < historyLength; ++i)
            history[i] = { -60.0f, 0.0f };
    }

    void prepareToPlay (double sr, int) override
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;
        currentGainLin = 1.0f;
        rmsSum = 0.0f;
        samplesSincePush = 0;
    }

    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();
        if (numChannels == 0 || numSamples == 0) return;

        float targetDb   = targetParam->load();
        float maxBoostDb = maxBoostParam->load();
        float maxCutDb   = -maxCutParam->load();
        float speedNorm  = juce::jlimit (0.0f, 1.0f, speedParam->load() * 0.01f);
        float gateDb     = gateParam->load();

        // Speed translates to smoothing time: 50ms (fast) to 600ms (slow transparent)
        float smoothTimeSec = 0.60f - speedNorm * 0.52f;
        float smoothCoeff = std::exp (-1.0f / (float) (smoothTimeSec * sampleRate));

        float blockRms = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float mag = buffer.getRMSLevel (ch, 0, numSamples);
            blockRms = juce::jmax (blockRms, mag);
        }

        float inDb = juce::Decibels::gainToDecibels (blockRms, -70.0f);
        // Gate: if speech is too quiet (silence/pause), freeze fader at last position!
        if (inDb > gateDb)
        {
            float errorDb = targetDb - inDb;
            lastTargetGainDb = juce::jlimit (maxCutDb, maxBoostDb, errorDb);
        }
        float targetGainDb = lastTargetGainDb;
        float targetGainLin = juce::Decibels::decibelsToGain (targetGainDb);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* channelData = buffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
            {
                currentGainLin = smoothCoeff * currentGainLin + (1.0f - smoothCoeff) * targetGainLin;
                channelData[i] *= currentGainLin;
            }
        }

        float liveGainDb = juce::Decibels::gainToDecibels (currentGainLin);
        liveCurrentGainDb.store (liveGainDb, std::memory_order_relaxed);
        liveInputRmsDb.store (inDb, std::memory_order_relaxed);

        samplesSincePush += numSamples;
        if (samplesSincePush >= (int) (sampleRate / 30.0)) // 30 FPS history
        {
            samplesSincePush = 0;
            int idx = historyWriteIndex.load (std::memory_order_relaxed);
            history[idx] = { inDb, liveGainDb };
            historyWriteIndex.store ((idx + 1) % historyLength, std::memory_order_relaxed);
        }
    }

    juce::AudioProcessorEditor* createEditor() override;

    float getLiveGainDb() const    { return liveCurrentGainDb.load (std::memory_order_relaxed); }
    float getLiveInputRms() const  { return liveInputRmsDb.load (std::memory_order_relaxed); }
    float getTargetDb() const      { return targetParam ? targetParam->load() : -16.0f; }
    float getGateDb() const        { return gateParam ? gateParam->load() : -42.0f; }

    void getHistoryData (std::vector<LevelPoint>& dest) const
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
                juce::ParameterID { "target", 1 }, "Target Loudness",
                juce::NormalisableRange<float> (-28.0f, -10.0f, 0.5f), -16.0f,
                juce::AudioParameterFloatAttributes().withLabel ("dBFS")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "maxBoost", 1 }, "Max Boost",
                juce::NormalisableRange<float> (0.0f, 18.0f, 0.5f), 12.0f,
                juce::AudioParameterFloatAttributes().withLabel ("dB")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "maxCut", 1 }, "Max Cut",
                juce::NormalisableRange<float> (0.0f, 18.0f, 0.5f), 12.0f,
                juce::AudioParameterFloatAttributes().withLabel ("dB")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "speed", 1 }, "Fader Speed",
                juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 50.0f,
                juce::AudioParameterFloatAttributes().withLabel ("%")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "gateThresh", 1 }, "Silence Freeze",
                juce::NormalisableRange<float> (-60.0f, -25.0f, 1.0f), -42.0f,
                juce::AudioParameterFloatAttributes().withLabel ("dBFS"))
        };
    }

    std::atomic<float>* targetParam   = nullptr;
    std::atomic<float>* maxBoostParam = nullptr;
    std::atomic<float>* maxCutParam   = nullptr;
    std::atomic<float>* speedParam    = nullptr;
    std::atomic<float>* gateParam     = nullptr;

    double sampleRate = 44100.0;
    float currentGainLin = 1.0f;
    float lastTargetGainDb = 0.0f;
    float rmsSum = 0.0f;
    int samplesSincePush = 0;

    std::atomic<float> liveCurrentGainDb { 0.0f };
    std::atomic<float> liveInputRmsDb { -60.0f };

    LevelPoint history[historyLength];
    std::atomic<int> historyWriteIndex { 0 };
};

class AGCModuleEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    AGCModuleEditor (AGCModule& p, juce::AudioProcessorValueTreeState& vts)
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

        setupSlider (targetSlider,   " dB", UITheme::appleBlue);
        setupSlider (maxBoostSlider, " dB", UITheme::appleGreen);
        setupSlider (maxCutSlider,   " dB", UITheme::appleRed);
        setupSlider (speedSlider,    " %",  juce::Colour (0xffff9f0a));
        setupSlider (gateSlider,     " dB", UITheme::appleYellow);

        targetAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "target", targetSlider);
        maxBoostAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "maxBoost", maxBoostSlider);
        maxCutAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "maxCut", maxCutSlider);
        speedAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "speed", speedSlider);
        gateAttach     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "gateThresh", gateSlider);

        setSize (390, 280);
        startTimerHz (60);
    }

    ~AGCModuleEditor() override { stopTimer(); }

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
        g.drawText ("AGC (SPEECH LEVELLER)", header.withTrimmedLeft (14), juce::Justification::centredLeft);

        // Fader Ride Badge
        float gain = module.getLiveGainDb();
        bool isRiding = std::abs (gain) > 0.4f;
        auto badge = header.removeFromRight (95).toFloat().reduced (6.0f, 8.0f);
        juce::Colour badgeCol = gain >= 0.0f ? UITheme::appleGreen : juce::Colour (0xffff9f0a);
        g.setColour (isRiding ? badgeCol.withAlpha (0.28f) : juce::Colour (0x15ffffff));
        g.fillRoundedRectangle (badge, 3.0f);
        g.setColour (isRiding ? badgeCol : UITheme::textTertiary);
        g.drawRoundedRectangle (badge, 3.0f, 1.0f);
        g.setFont (UITheme::getFont (8.0f, true));
        g.drawText (gain >= 0.0f ? "+" + juce::String (gain, 1) + " dB" : juce::String (gain, 1) + " dB", badge, juce::Justification::centred);

        // --- REAL-TIME LOUDNESS RIDE STREAM DISPLAY ---
        auto screenRect = juce::Rectangle<float> (16.0f, 44.0f, (float) getWidth() - 32.0f, 82.0f);
        g.setColour (juce::Colour (0xff111114));
        g.fillRoundedRectangle (screenRect, 4.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (screenRect, 4.0f, 0.8f);

        // Target loudness horizontal reference line
        float targetDb = module.getTargetDb();
        float normTarget = (targetDb - (-48.0f)) / (0.0f - (-48.0f));
        float targetY = screenRect.getBottom() - normTarget * screenRect.getHeight();

        g.setColour (UITheme::appleBlue);
        float dashes[] = { 4.0f, 3.0f };
        g.drawDashedLine (juce::Line<float> (screenRect.getX(), targetY, screenRect.getRight(), targetY), dashes, 2, 1.2f);
        g.setFont (UITheme::getFont (7.5f, true));
        g.drawText ("TARGET " + juce::String ((int) targetDb) + " dB",
                    juce::Rectangle<float> (screenRect.getX() + 6.0f, targetY - 11.0f, 75.0f, 10.0f),
                    juce::Justification::centredLeft);

        // Draw Loudness History Curve
        std::vector<AGCModule::LevelPoint> pts;
        module.getHistoryData (pts);

        if (! pts.empty())
        {
            float stepX = screenRect.getWidth() / (float) (pts.size() - 1);
            juce::Path streamPath;
            bool started = false;

            for (size_t i = 0; i < pts.size(); ++i)
            {
                float x = screenRect.getX() + (float) i * stepX;
                float db = juce::jlimit (-48.0f, 0.0f, pts[i].inRmsDb + pts[i].gainAppliedDb);
                float normLvl = (db - (-48.0f)) / 48.0f;
                float y = screenRect.getBottom() - normLvl * (screenRect.getHeight() - 4.0f) - 2.0f;

                if (! started) { streamPath.startNewSubPath (x, y); started = true; }
                else           { streamPath.lineTo (x, y); }
            }

            g.setColour (UITheme::appleGreen);
            g.strokePath (streamPath, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Knob labels
        int colW = getWidth() / 5;
        int labelY = 134;
        g.setColour (UITheme::textSecondary);
        g.setFont (UITheme::getFont (8.5f, true));
        g.drawText ("TARGET",    colW * 0, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("MAX BOOST", colW * 1, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("MAX CUT",   colW * 2, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("SPEED",     colW * 3, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("FREEZE",    colW * 4, labelY, colW, 14, juce::Justification::centred);
    }

    void resized() override
    {
        int colW = getWidth() / 5;
        int knobY = 152;
        int knobSize = 60;

        targetSlider.setBounds   (colW * 0 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        maxBoostSlider.setBounds (colW * 1 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        maxCutSlider.setBounds   (colW * 2 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        speedSlider.setBounds    (colW * 3 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        gateSlider.setBounds     (colW * 4 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
    }

private:
    AGCModule& module;
    juce::Slider targetSlider, maxBoostSlider, maxCutSlider, speedSlider, gateSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> targetAttach, maxBoostAttach, maxCutAttach, speedAttach, gateAttach;
};

inline juce::AudioProcessorEditor* AGCModule::createEditor()
{
    return new AGCModuleEditor (*this, apvts);
}
