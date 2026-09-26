#pragma once
#include "../Graph/ModuleProcessor.h"
#include "../UITheme.h"
#include <cmath>
#include <vector>

class UpwardCompressorModuleEditor;

/**
    Upward Compressor (Vocal OTT / Detail Maximizer)
    Combines upward expansion of quiet vocal nuances (whispers, breath details, chest resonance)
    with downward compression of loud peaks for an in-your-face modern broadcast sound.
*/
class UpwardCompressorModule : public ModuleProcessor
{
public:
    static constexpr int historySize = 140;

    struct DynamicsPoint
    {
        float inLevel;
        float outLevel;
        float upwardBoostDb;
        float downwardGrDb;
    };

    UpwardCompressorModule()
        : ModuleProcessor ("Upward Compressor", createLayout())
    {
        upwardBoostParam = getModuleParam ("upwardBoost", 9.0f);
        upwardDepthParam = getModuleParam ("upwardDepth", 60.0f);
        downThreshParam  = getModuleParam ("downThresh", -18.0f);
        downRatioParam   = getModuleParam ("downRatio", 3.5f);
        attackParam      = getModuleParam ("attack", 8.0f);
        releaseParam     = getModuleParam ("release", 80.0f);

        for (int i = 0; i < historySize; ++i)
            history[i] = { 0.0f, 0.0f, 0.0f, 0.0f };
    }

    ~UpwardCompressorModule() override = default;

    void prepareToPlay (double sr, int) override
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;
        currentEnvelope = 0.0f;
        currentGain = 1.0f;
        samplesSincePush = 0;
    }

    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();
        if (numChannels == 0 || numSamples == 0) return;

        float upMaxBoostDb  = upwardBoostParam.get (9.0f);
        float upDepth       = juce::jlimit (0.0f, 100.0f, upwardDepthParam.get (60.0f)) * 0.01f;
        float downThreshDb  = downThreshParam.get (-18.0f);
        float downRatio     = downRatioParam.get (3.5f);
        float attMs         = attackParam.get (8.0f);
        float relMs         = releaseParam.get (80.0f);

        float upThreshDb = downThreshDb - 14.0f; // Upward knee threshold

        float attCoeff = std::exp (-1.0f / (float) ((attMs * 0.001f) * sampleRate));
        float relCoeff = std::exp (-1.0f / (float) ((relMs * 0.001f) * sampleRate));

        float blockMaxIn = 0.0f;
        float blockMaxOut = 0.0f;
        float blockMaxUpward = 0.0f;
        float blockMaxDownward = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float peak = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                peak = juce::jmax (peak, std::abs (buffer.getSample (ch, i)));

            blockMaxIn = juce::jmax (blockMaxIn, peak);

            // Envelope follower
            if (peak > currentEnvelope)
                currentEnvelope = attCoeff * currentEnvelope + (1.0f - attCoeff) * peak;
            else
                currentEnvelope = relCoeff * currentEnvelope + (1.0f - relCoeff) * peak;

            float envDb = juce::Decibels::gainToDecibels (currentEnvelope, -80.0f);

            // 1. Calculate Target Gain from Dual-Stage Dynamics
            float targetGainDb = 0.0f;
            float currentUpBoost = 0.0f;
            float currentDownGr = 0.0f;

            // Gate noise floor protection (below -60 dB, don't boost room noise)
            if (envDb > -60.0f)
            {
                // Upward Compression: Lift levels between -60 dB and Upward Threshold
                if (envDb < upThreshDb)
                {
                    float deficit = (upThreshDb - envDb);
                    float lift = (deficit / (upThreshDb - (-60.0f))) * upMaxBoostDb * upDepth;
                    currentUpBoost = juce::jlimit (0.0f, upMaxBoostDb, lift);
                    targetGainDb += currentUpBoost;
                }

                // Downward Compression: Clamp peaks above Downward Threshold
                if (envDb > downThreshDb)
                {
                    float overshoot = envDb - downThreshDb;
                    currentDownGr = overshoot * (1.0f - 1.0f / downRatio);
                    targetGainDb -= currentDownGr;
                }
            }

            blockMaxUpward   = juce::jmax (blockMaxUpward, currentUpBoost);
            blockMaxDownward = juce::jmax (blockMaxDownward, currentDownGr);

            float targetGainLin = juce::Decibels::decibelsToGain (targetGainDb);
            currentGain = currentGain * 0.995f + targetGainLin * 0.005f;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float out = buffer.getSample (ch, i) * currentGain;
                buffer.setSample (ch, i, out);
                blockMaxOut = juce::jmax (blockMaxOut, std::abs (out));
            }

            // Capture visualizer history
            samplesSincePush++;
            if (samplesSincePush >= (int) (sampleRate / 35.0))
            {
                samplesSincePush = 0;
                int idx = historyWriteIndex.load (std::memory_order_relaxed);
                history[idx] = { blockMaxIn, blockMaxOut, blockMaxUpward, blockMaxDownward };
                historyWriteIndex.store ((idx + 1) % historySize, std::memory_order_relaxed);
            }
        }

        liveUpwardBoostDb.store (blockMaxUpward, std::memory_order_relaxed);
        liveDownwardGrDb.store (blockMaxDownward, std::memory_order_relaxed);
    }

    juce::AudioProcessorEditor* createEditor() override;

    float getLiveUpwardBoost() const { return liveUpwardBoostDb.load (std::memory_order_relaxed); }
    float getLiveDownwardGr() const  { return liveDownwardGrDb.load (std::memory_order_relaxed); }

    void getHistoryData (std::vector<DynamicsPoint>& dest) const
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
                juce::ParameterID { "upwardBoost", 1 }, "Upward Boost",
                juce::NormalisableRange<float> (0.0f, 18.0f, 0.5f), 8.0f,
                juce::AudioParameterFloatAttributes().withLabel ("dB")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "upwardDepth", 1 }, "Upward Depth",
                juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 75.0f,
                juce::AudioParameterFloatAttributes().withLabel ("%")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "downThresh", 1 }, "Down Threshold",
                juce::NormalisableRange<float> (-40.0f, 0.0f, 0.5f), -16.0f,
                juce::AudioParameterFloatAttributes().withLabel ("dB")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "downRatio", 1 }, "Down Ratio",
                juce::NormalisableRange<float> (1.5f, 12.0f, 0.1f), 4.0f,
                juce::AudioParameterFloatAttributes().withLabel (":1")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "attack", 1 }, "Attack",
                juce::NormalisableRange<float> (0.5f, 50.0f, 0.5f), 10.0f,
                juce::AudioParameterFloatAttributes().withLabel ("ms")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "release", 1 }, "Release",
                juce::NormalisableRange<float> (10.0f, 300.0f, 1.0f), 80.0f,
                juce::AudioParameterFloatAttributes().withLabel ("ms"))
        };
    }

    ParamRef upwardBoostParam;
    ParamRef upwardDepthParam;
    ParamRef downThreshParam;
    ParamRef downRatioParam;
    ParamRef attackParam;
    ParamRef releaseParam;

    double sampleRate = 44100.0;
    float currentEnvelope = 0.0f;
    float currentGain = 1.0f;

    std::atomic<float> liveUpwardBoostDb { 0.0f };
    std::atomic<float> liveDownwardGrDb  { 0.0f };

    DynamicsPoint history[historySize];
    std::atomic<int> historyWriteIndex { 0 };
    int samplesSincePush = 0;
};

class UpwardCompressorModuleEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    UpwardCompressorModuleEditor (UpwardCompressorModule& p, juce::AudioProcessorValueTreeState& vts)
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

        setupSlider (upBoostSlider,    " dB", UITheme::appleGreen);
        setupSlider (upDepthSlider,    " %",  UITheme::appleCyan);
        setupSlider (downThreshSlider, " dB", UITheme::appleOrange);
        setupSlider (downRatioSlider,  ":1",  UITheme::appleRed);
        setupSlider (attackSlider,     " ms", UITheme::appleBlue);
        setupSlider (releaseSlider,    " ms", UITheme::applePurple);

        upBoostAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "upwardBoost", upBoostSlider);
        upDepthAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "upwardDepth", upDepthSlider);
        downThreshAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "downThresh", downThreshSlider);
        downRatioAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "downRatio", downRatioSlider);
        attackAttach     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "attack", attackSlider);
        releaseAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "release", releaseSlider);

        setSize (480, 290);
        startTimerHz (60);
    }

    ~UpwardCompressorModuleEditor() override { stopTimer(); }

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
        g.drawText ("UPWARD COMPRESSOR (VOCAL OTT)", header.withTrimmedLeft (14), juce::Justification::centredLeft);

        // Status Dual Badge
        float upBoost = module.getLiveUpwardBoost();
        float downGr  = module.getLiveDownwardGr();
        auto badge = header.removeFromRight (170).toFloat().reduced (6.0f, 8.0f);

        g.setColour (juce::Colour (0x2530d158));
        g.fillRoundedRectangle (badge, 3.0f);
        g.setColour (UITheme::appleGreen);
        g.drawRoundedRectangle (badge, 3.0f, 1.0f);
        g.setFont (UITheme::getFont (8.0f, true));
        g.drawText ("LIFT +" + juce::String (upBoost, 1) + "dB | GR -" + juce::String (downGr, 1) + "dB", badge, juce::Justification::centred);

        // --- REAL-TIME DYNAMICS MONITOR ---
        auto screenRect = juce::Rectangle<float> (16.0f, 44.0f, (float) getWidth() - 32.0f, 82.0f);
        g.setColour (juce::Colour (0xff111114));
        g.fillRoundedRectangle (screenRect, 4.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (screenRect, 4.0f, 0.8f);

        std::vector<UpwardCompressorModule::DynamicsPoint> pts;
        module.getHistoryData (pts);

        if (! pts.empty())
        {
            float stepX = screenRect.getWidth() / (float) (pts.size() - 1);
            juce::Path inPath, outPath;
            bool started = false;

            for (size_t i = 0; i < pts.size(); ++i)
            {
                float x = screenRect.getX() + (float) i * stepX;
                float inH  = juce::jlimit (0.0f, 1.0f, pts[i].inLevel * 1.5f) * (screenRect.getHeight() - 8.0f);
                float outH = juce::jlimit (0.0f, 1.0f, pts[i].outLevel * 1.5f) * (screenRect.getHeight() - 8.0f);

                float inY  = screenRect.getBottom() - inH - 4.0f;
                float outY = screenRect.getBottom() - outH - 4.0f;

                if (! started)
                {
                    inPath.startNewSubPath (x, inY);
                    outPath.startNewSubPath (x, outY);
                    started = true;
                }
                else
                {
                    inPath.lineTo (x, inY);
                    outPath.lineTo (x, outY);
                }
            }

            g.setColour (UITheme::textTertiary.withAlpha (0.45f));
            g.strokePath (inPath, juce::PathStrokeType (1.2f));

            g.setColour (UITheme::appleGreen);
            g.strokePath (outPath, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Legend
        g.setFont (UITheme::getFont (7.5f, true));
        g.setColour (UITheme::textSecondary);
        g.drawText ("INPUT", juce::Rectangle<float> (screenRect.getX() + 6.0f, screenRect.getY() + 4.0f, 50.0f, 10.0f), juce::Justification::centredLeft);
        g.setColour (UITheme::appleGreen);
        g.drawText ("UPWARD EXPANDED VOCAL", juce::Rectangle<float> (screenRect.getRight() - 130.0f, screenRect.getY() + 4.0f, 124.0f, 10.0f), juce::Justification::centredRight);

        // Knob labels
        int colW = getWidth() / 6;
        int labelY = 134;
        g.setColour (UITheme::textSecondary);
        g.setFont (UITheme::getFont (8.5f, true));
        g.drawText ("UPWARD BOOST", colW * 0, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("UPWARD DEPTH", colW * 1, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("DOWN THRESH",  colW * 2, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("DOWN RATIO",   colW * 3, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("ATTACK",       colW * 4, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("RELEASE",      colW * 5, labelY, colW, 14, juce::Justification::centred);
    }

    void resized() override
    {
        int colW = getWidth() / 6;
        int knobY = 152;
        int knobSize = 58;

        upBoostSlider.setBounds    (colW * 0 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        upDepthSlider.setBounds    (colW * 1 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        downThreshSlider.setBounds (colW * 2 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        downRatioSlider.setBounds  (colW * 3 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        attackSlider.setBounds     (colW * 4 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        releaseSlider.setBounds    (colW * 5 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
    }

private:
    UpwardCompressorModule& module;
    juce::Slider upBoostSlider, upDepthSlider, downThreshSlider, downRatioSlider, attackSlider, releaseSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> upBoostAttach, upDepthAttach, downThreshAttach, downRatioAttach, attackAttach, releaseAttach;
};

inline juce::AudioProcessorEditor* UpwardCompressorModule::createEditor()
{
    return new UpwardCompressorModuleEditor (*this, apvts);
}
