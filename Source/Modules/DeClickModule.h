#pragma once
#include "../Graph/ModuleProcessor.h"
#include "../UITheme.h"
#include <cmath>

class DeClickModuleEditor;

class DeClickModule : public ModuleProcessor
{
public:
    DeClickModule()
        : ModuleProcessor ("De-Click", createLayout())
    {
        threshParam = getModuleParam ("sensitivity", 65.0f);
        widthParam  = getModuleParam ("maxClickWidth", 3.0f);
        listenParam = getModuleParam ("listenClicks", 0.0f);
    }

    void prepareToPlay (double sr, int) override
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;
        for (int ch = 0; ch < 2; ++ch)
        {
            prev1[ch] = prev2[ch] = 0.0f;
            smoothEnv[ch] = 0.001f;
        }
        clicksDetectedCount = 0;
        clickCounterTimer = 0;
        liveClicksPerSec = 0;
    }

    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();
        if (numChannels == 0 || numSamples == 0) return;

        float sensNorm = juce::jlimit (0.0f, 1.0f, threshParam.get (65.0f) * 0.01f);
        // Higher sensitivity = lower threshold multiplier
        float spikeThreshold = 18.0f - sensNorm * 14.5f; // Range: 3.5x to 18x local envelope
        bool listenClicks = listenParam.get (0.0f) > 0.5f;
        int maxClickWidth = juce::jlimit (1, 6, (int) std::lround (widthParam.get (3.0f)));

        float envCoeff = std::exp (-1.0f / (float) (0.008f * sampleRate)); // 8ms moving envelope
        int blockClicks = 0;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* channelData = buffer.getWritePointer (ch);
            int chIdx = ch < 2 ? ch : 0;
            int runLength = 0; // consecutive samples currently being repaired, capped by maxClickWidth

            for (int i = 0; i < numSamples; ++i)
            {
                float in = channelData[i];
                float absIn = std::abs (in);

                // Moving baseline envelope
                smoothEnv[chIdx] = envCoeff * smoothEnv[chIdx] + (1.0f - envCoeff) * absIn;

                // 2nd derivative: curvature spike detector
                float delta2 = in - 2.0f * prev1[chIdx] + prev2[chIdx];
                float absDelta2 = std::abs (delta2);

                float threshold = smoothEnv[chIdx] * spikeThreshold + 0.015f;

                // Detect sudden non-musical saliva/mouth click transient — but only
                // repair up to maxClickWidth consecutive samples. Without this cap
                // (the bug: "Click Width" was loaded but never read), a genuine fast
                // transient like a plosive or hard consonant that looks click-like
                // for several samples in a row got flattened into a straight line
                // instead of being left alone once it's clearly not a short click.
                if (absDelta2 > threshold && absIn > 0.02f && runLength < maxClickWidth)
                {
                    runLength++;
                    blockClicks++;
                    // Clean cubic/linear interpolation across the click spike
                    float repaired = (prev1[chIdx] * 2.0f - prev2[chIdx]);
                    repaired = juce::jlimit (-1.0f, 1.0f, repaired);

                    if (listenClicks)
                        channelData[i] = (in - repaired) * 4.0f; // Listen to removed clicks only
                    else
                        channelData[i] = repaired;

                    prev2[chIdx] = prev1[chIdx];
                    prev1[chIdx] = repaired;
                }
                else
                {
                    runLength = 0;
                    prev2[chIdx] = prev1[chIdx];
                    prev1[chIdx] = in;

                    if (listenClicks)
                        channelData[i] = 0.0f;
                }
            }
        }

        // Click rate tracking
        clicksDetectedCount += blockClicks;
        clickCounterTimer += numSamples;
        if (clickCounterTimer >= (int) sampleRate)
        {
            liveClicksPerSec.store (clicksDetectedCount, std::memory_order_relaxed);
            clicksDetectedCount = 0;
            clickCounterTimer = 0;
        }

        if (blockClicks > 0)
            livePulseActivity.store (1.0f, std::memory_order_relaxed);
        else
        {
            float p = livePulseActivity.load (std::memory_order_relaxed);
            livePulseActivity.store (p * 0.92f, std::memory_order_relaxed);
        }
    }

    juce::AudioProcessorEditor* createEditor() override;

    int   getClicksPerSec() const     { return liveClicksPerSec.load (std::memory_order_relaxed); }
    float getLivePulse() const        { return livePulseActivity.load (std::memory_order_relaxed); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        return {
            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "sensitivity", 1 }, "Sensitivity",
                juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 65.0f,
                juce::AudioParameterFloatAttributes().withLabel ("%")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "maxClickWidth", 1 }, "Click Width",
                juce::NormalisableRange<float> (1.0f, 6.0f, 1.0f), 3.0f,
                juce::AudioParameterFloatAttributes().withLabel ("smp")),

            std::make_unique<juce::AudioParameterBool> (
                juce::ParameterID { "listenClicks", 1 }, "Listen Clicks (Diff)", false)
        };
    }

    ParamRef threshParam;
    ParamRef widthParam;
    ParamRef listenParam;

    double sampleRate = 44100.0;
    float prev1[2] = {}, prev2[2] = {};
    float smoothEnv[2] = { 0.001f, 0.001f };

    int clicksDetectedCount = 0;
    int clickCounterTimer = 0;
    std::atomic<int> liveClicksPerSec { 0 };
    std::atomic<float> livePulseActivity { 0.0f };
};

class DeClickModuleEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    DeClickModuleEditor (DeClickModule& p, juce::AudioProcessorValueTreeState& vts)
        : AudioProcessorEditor (&p), module (p)
    {
        auto setupSlider = [this] (juce::Slider& s, const juce::String& suffix, juce::Colour col)
        {
            s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 18);
            s.setTextValueSuffix (suffix);
            s.setColour (juce::Slider::rotarySliderFillColourId, col);
            s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff1e1e24));
            s.setColour (juce::Slider::textBoxOutlineColourId, UITheme::strokeHairline);
            s.setColour (juce::Slider::textBoxTextColourId, UITheme::textPrimary);
            addAndMakeVisible (s);
        };

        setupSlider (sensSlider,  " %",   UITheme::appleBlue);
        setupSlider (widthSlider, " smp", UITheme::appleYellow);

        listenBtn.setButtonText ("Listen Clicks");
        listenBtn.setClickingTogglesState (true);
        listenBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffff9f0a));
        addAndMakeVisible (listenBtn);

        sensAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "sensitivity", sensSlider);
        widthAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "maxClickWidth", widthSlider);
        listenAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (vts, "listenClicks", listenBtn);

        setSize (330, 260);
        startTimerHz (60);
    }

    ~DeClickModuleEditor() override { stopTimer(); }

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
        g.drawText ("DE-CLICK (MOUTH & SALIVA)", header.withTrimmedLeft (14), juce::Justification::centredLeft);

        // Click Rate Badge
        int rate = module.getClicksPerSec();
        auto badge = header.removeFromRight (100).toFloat().reduced (6.0f, 8.0f);
        g.setColour (rate > 0 ? UITheme::appleGreen.withAlpha (0.28f) : juce::Colour (0x15ffffff));
        g.fillRoundedRectangle (badge, 3.0f);
        g.setColour (rate > 0 ? UITheme::appleGreen : UITheme::textTertiary);
        g.drawRoundedRectangle (badge, 3.0f, 1.0f);
        g.setFont (UITheme::getFont (8.0f, true));
        g.drawText (rate > 0 ? juce::String (rate) + " CLICKS/S" : "CLEAN", badge, juce::Justification::centred);

        // --- REAL-TIME CLICK RADAR & PULSE MONITOR ---
        auto screenRect = juce::Rectangle<float> (16.0f, 44.0f, (float) getWidth() - 32.0f, 74.0f);
        g.setColour (juce::Colour (0xff111114));
        g.fillRoundedRectangle (screenRect, 4.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (screenRect, 4.0f, 0.8f);

        float cx = screenRect.getCentreX();
        float cy = screenRect.getCentreY();
        float pulse = module.getLivePulse();

        // Concentric radar rings
        g.setColour (juce::Colour (0x18ffffff));
        g.drawEllipse (cx - 24.0f, cy - 24.0f, 48.0f, 48.0f, 1.0f);
        g.drawEllipse (cx - 42.0f, cy - 42.0f, 84.0f, 84.0f, 0.8f);

        // Animated click pulse ring
        if (pulse > 0.05f)
        {
            float r = 18.0f + (1.0f - pulse) * 28.0f;
            g.setColour (UITheme::appleGreen.withAlpha (pulse * 0.85f));
            g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, 2.0f);
        }

        // Center mic icon indicator
        g.setColour (pulse > 0.05f ? UITheme::appleGreen : UITheme::appleBlue);
        g.fillEllipse (cx - 7.0f, cy - 7.0f, 14.0f, 14.0f);
        g.setColour (juce::Colours::white);
        g.fillEllipse (cx - 3.0f, cy - 3.0f, 6.0f, 6.0f);

        // Text indicator
        g.setFont (UITheme::getFont (8.0f, true));
        g.setColour (pulse > 0.05f ? UITheme::appleGreen : UITheme::textTertiary);
        g.drawText (pulse > 0.05f ? "SALIVA CLICK DETECTED & REMOVED" : "ANALYZING SLEW-RATE...",
                    juce::Rectangle<float> (screenRect.getX(), screenRect.getBottom() - 14.0f, screenRect.getWidth(), 12.0f),
                    juce::Justification::centred);

        // Knob labels
        int colW = getWidth() / 2;
        int labelY = 124;
        g.setColour (UITheme::textSecondary);
        g.setFont (UITheme::getFont (9.0f, true));
        g.drawText ("SENSITIVITY", colW * 0, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("MAX WIDTH",  colW * 1, labelY, colW, 14, juce::Justification::centred);
    }

    void resized() override
    {
        int colW = getWidth() / 2;
        int knobY = 142;
        int knobSize = 64;

        sensSlider.setBounds  (colW * 0 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        widthSlider.setBounds (colW * 1 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);

        listenBtn.setBounds (getWidth() / 2 - 55, getHeight() - 25, 110, 20);
    }

private:
    DeClickModule& module;
    juce::Slider sensSlider, widthSlider;
    juce::TextButton listenBtn;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sensAttach, widthAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> listenAttach;
};

inline juce::AudioProcessorEditor* DeClickModule::createEditor()
{
    return new DeClickModuleEditor (*this, apvts);
}
