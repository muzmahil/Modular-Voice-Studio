#pragma once
#include "../Graph/ModuleProcessor.h"
#include "../UITheme.h"
#include <cmath>
#include <vector>

class CrossoverJoinerModuleEditor;

/**
    Frequency Crossover Joiner (Modular 3-Way Combiner)
    3 Discrete Stereo Audio INs (LOW, MID, HIGH) -> 1 Summed Stereo Audio OUT.
    Recombines the processed multi-band audio chains with independent gain trims, solo/mute control,
    and zero phase distortion.
*/
class CrossoverJoinerModule : public ModuleProcessor
{
public:
    CrossoverJoinerModule()
        : ModuleProcessor ("Frequency Joiner",
                           BusesProperties()
                               .withInput  ("Low Input",   juce::AudioChannelSet::stereo(), true)
                               .withInput  ("Mid Input",   juce::AudioChannelSet::stereo(), true)
                               .withInput  ("High Input",  juce::AudioChannelSet::stereo(), true)
                               .withOutput ("Output",      juce::AudioChannelSet::stereo(), true),
                           createLayout())
    {
        lowGainParam  = apvts.getRawParameterValue ("lowGain");
        midGainParam  = apvts.getRawParameterValue ("midGain");
        highGainParam = apvts.getRawParameterValue ("highGain");

        lowSoloParam  = apvts.getRawParameterValue ("lowSolo");
        midSoloParam  = apvts.getRawParameterValue ("midSolo");
        highSoloParam = apvts.getRawParameterValue ("highSolo");

        lowMuteParam  = apvts.getRawParameterValue ("lowMute");
        midMuteParam  = apvts.getRawParameterValue ("midMute");
        highMuteParam = apvts.getRawParameterValue ("highMute");
    }

    ~CrossoverJoinerModule() override = default;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        return layouts.getChannelSet (true, 0) == juce::AudioChannelSet::stereo()
            && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    void prepareToPlay (double, int) override {}
    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();
        if (numChannels == 0 || numSamples == 0) return;

        float lowGainLin  = juce::Decibels::decibelsToGain (lowGainParam ? lowGainParam->load() : 0.0f);
        float midGainLin  = juce::Decibels::decibelsToGain (midGainParam ? midGainParam->load() : 0.0f);
        float highGainLin = juce::Decibels::decibelsToGain (highGainParam ? highGainParam->load() : 0.0f);

        bool lowSolo  = lowSoloParam  && lowSoloParam->load() > 0.5f;
        bool midSolo  = midSoloParam  && midSoloParam->load() > 0.5f;
        bool highSolo = highSoloParam && highSoloParam->load() > 0.5f;
        bool anySolo  = lowSolo || midSolo || highSolo;

        bool lowMute  = lowMuteParam  && lowMuteParam->load() > 0.5f;
        bool midMute  = midMuteParam  && midMuteParam->load() > 0.5f;
        bool highMute = highMuteParam && highMuteParam->load() > 0.5f;

        float lowActiveMult  = (anySolo ? (lowSolo ? 1.0f : 0.0f) : (lowMute ? 0.0f : 1.0f));
        float midActiveMult  = (anySolo ? (midSolo ? 1.0f : 0.0f) : (midMute ? 0.0f : 1.0f));
        float highActiveMult = (anySolo ? (highSolo ? 1.0f : 0.0f) : (highMute ? 0.0f : 1.0f));

        if (lowMute && anySolo && lowSolo)   lowActiveMult = 0.0f;
        if (midMute && anySolo && midSolo)   midActiveMult = 0.0f;
        if (highMute && anySolo && highSolo) highActiveMult = 0.0f;

        // Pointers for Low (0,1), Mid (2,3), High (4,5)
        const float* lowL  = numChannels > 0 ? buffer.getReadPointer (0) : nullptr;
        const float* lowR  = numChannels > 1 ? buffer.getReadPointer (1) : lowL;
        const float* midL  = numChannels > 2 ? buffer.getReadPointer (2) : nullptr;
        const float* midR  = numChannels > 3 ? buffer.getReadPointer (3) : midL;
        const float* highL = numChannels > 4 ? buffer.getReadPointer (4) : nullptr;
        const float* highR = numChannels > 5 ? buffer.getReadPointer (5) : highL;

        // Temporary buffers for safe summing into out channels 0, 1
        juce::AudioBuffer<float> sumBuffer (2, numSamples);
        sumBuffer.clear();

        float* outL = sumBuffer.getWritePointer (0);
        float* outR = sumBuffer.getWritePointer (1);

        float blockLowPeak = 0.0f;
        float blockMidPeak = 0.0f;
        float blockHighPeak = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float sLowL = lowL ? lowL[i] : 0.0f;
            float sLowR = lowR ? lowR[i] : 0.0f;
            float sMidL = midL ? midL[i] : 0.0f;
            float sMidR = midR ? midR[i] : 0.0f;
            float sHighL = highL ? highL[i] : 0.0f;
            float sHighR = highR ? highR[i] : 0.0f;

            blockLowPeak  = juce::jmax (blockLowPeak, std::abs (sLowL), std::abs (sLowR));
            blockMidPeak  = juce::jmax (blockMidPeak, std::abs (sMidL), std::abs (sMidR));
            blockHighPeak = juce::jmax (blockHighPeak, std::abs (sHighL), std::abs (sHighR));

            outL[i] = (sLowL  * lowGainLin  * lowActiveMult) +
                      (sMidL  * midGainLin  * midActiveMult) +
                      (sHighL * highGainLin * highActiveMult);

            outR[i] = (sLowR  * lowGainLin  * lowActiveMult) +
                      (sMidR  * midGainLin  * midActiveMult) +
                      (sHighR * highGainLin * highActiveMult);
        }

        // Copy summed stereo to output buffer
        if (numChannels > 0)
            buffer.copyFrom (0, 0, sumBuffer, 0, 0, numSamples);
        if (numChannels > 1)
            buffer.copyFrom (1, 0, sumBuffer, 1, 0, numSamples);

        // Clear remaining channels
        for (int ch = 2; ch < numChannels; ++ch)
            buffer.clear (ch, 0, numSamples);

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
                juce::ParameterID { "lowGain", 1 }, "Low Gain",
                juce::NormalisableRange<float> (-24.0f, 12.0f, 0.5f), 0.0f,
                juce::AudioParameterFloatAttributes().withLabel ("dB")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "midGain", 1 }, "Mid Gain",
                juce::NormalisableRange<float> (-24.0f, 12.0f, 0.5f), 0.0f,
                juce::AudioParameterFloatAttributes().withLabel ("dB")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "highGain", 1 }, "High Gain",
                juce::NormalisableRange<float> (-24.0f, 12.0f, 0.5f), 0.0f,
                juce::AudioParameterFloatAttributes().withLabel ("dB")),

            std::make_unique<juce::AudioParameterBool> (
                juce::ParameterID { "lowSolo", 1 }, "Low Solo", false),
            std::make_unique<juce::AudioParameterBool> (
                juce::ParameterID { "midSolo", 1 }, "Mid Solo", false),
            std::make_unique<juce::AudioParameterBool> (
                juce::ParameterID { "highSolo", 1 }, "High Solo", false),

            std::make_unique<juce::AudioParameterBool> (
                juce::ParameterID { "lowMute", 1 }, "Low Mute", false),
            std::make_unique<juce::AudioParameterBool> (
                juce::ParameterID { "midMute", 1 }, "Mid Mute", false),
            std::make_unique<juce::AudioParameterBool> (
                juce::ParameterID { "highMute", 1 }, "High Mute", false)
        };
    }

    std::atomic<float>* lowGainParam  = nullptr;
    std::atomic<float>* midGainParam  = nullptr;
    std::atomic<float>* highGainParam = nullptr;
    std::atomic<float>* lowSoloParam  = nullptr;
    std::atomic<float>* midSoloParam  = nullptr;
    std::atomic<float>* highSoloParam = nullptr;
    std::atomic<float>* lowMuteParam  = nullptr;
    std::atomic<float>* midMuteParam  = nullptr;
    std::atomic<float>* highMuteParam = nullptr;

    std::atomic<float> liveLowLevel { 0.0f };
    std::atomic<float> liveMidLevel { 0.0f };
    std::atomic<float> liveHighLevel { 0.0f };
};

class CrossoverJoinerModuleEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    CrossoverJoinerModuleEditor (CrossoverJoinerModule& p, juce::AudioProcessorValueTreeState& vts)
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

        setupSlider (lowGainSlider,  " dB", UITheme::applePurple);
        setupSlider (midGainSlider,  " dB", UITheme::appleGreen);
        setupSlider (highGainSlider, " dB", UITheme::appleCyan);

        auto setupButton = [this] (juce::ToggleButton& b, const juce::String& text, juce::Colour onCol)
        {
            b.setButtonText (text);
            b.setColour (juce::ToggleButton::textColourId, UITheme::textSecondary);
            b.setColour (juce::ToggleButton::tickColourId, onCol);
            addAndMakeVisible (b);
        };

        setupButton (lowSoloBtn,  "S", UITheme::appleYellow);
        setupButton (midSoloBtn,  "S", UITheme::appleYellow);
        setupButton (highSoloBtn, "S", UITheme::appleYellow);

        setupButton (lowMuteBtn,  "M", UITheme::appleRed);
        setupButton (midMuteBtn,  "M", UITheme::appleRed);
        setupButton (highMuteBtn, "M", UITheme::appleRed);

        lowGainAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "lowGain", lowGainSlider);
        midGainAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "midGain", midGainSlider);
        highGainAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "highGain", highGainSlider);

        lowSoloAttach  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (vts, "lowSolo", lowSoloBtn);
        midSoloAttach  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (vts, "midSolo", midSoloBtn);
        highSoloAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (vts, "highSolo", highSoloBtn);

        lowMuteAttach  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (vts, "lowMute", lowMuteBtn);
        midMuteAttach  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (vts, "midMute", midMuteBtn);
        highMuteAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (vts, "highMute", highMuteBtn);

        setSize (390, 240);
        startTimerHz (60);
    }

    ~CrossoverJoinerModuleEditor() override { stopTimer(); }

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
        g.drawText ("FREQUENCY JOINER (3 IN -> 1 OUT)", header.withTrimmedLeft (14), juce::Justification::centredLeft);

        // Active State Badge
        auto badge = header.removeFromRight (150).toFloat().reduced (6.0f, 8.0f);
        g.setColour (juce::Colour (0x2030d158));
        g.fillRoundedRectangle (badge, 3.0f);
        g.setColour (UITheme::appleGreen);
        g.drawRoundedRectangle (badge, 3.0f, 1.0f);
        g.setFont (UITheme::getFont (8.0f, true));
        g.drawText ("SUMMED MASTER OUT", badge, juce::Justification::centred);

        int colW = getWidth() / 3;

        // Band Cards
        auto drawBandCard = [&] (int colIdx, const juce::String& title, juce::Colour col, float peakLevel, bool muted)
        {
            auto card = juce::Rectangle<float> ((float) (colIdx * colW + 8), 44.0f, (float) colW - 16.0f, 64.0f);
            g.setColour (juce::Colour (0xff19191d));
            g.fillRoundedRectangle (card, 4.0f);
            g.setColour (UITheme::strokeHairline);
            g.drawRoundedRectangle (card, 4.0f, 1.0f);

            // Level Bar
            auto meterTrack = card.reduced (8.0f, 0.0f).withHeight (6.0f).withY (card.getBottom() - 14.0f);
            g.setColour (juce::Colour (0xff101012));
            g.fillRoundedRectangle (meterTrack, 1.5f);

            float norm = juce::jlimit (0.0f, 1.0f, peakLevel);
            if (norm > 0.01f && !muted)
            {
                g.setColour (col);
                g.fillRoundedRectangle (meterTrack.withWidth (meterTrack.getWidth() * norm), 1.5f);
            }

            g.setFont (UITheme::getFont (9.0f, true));
            g.setColour (muted ? UITheme::textTertiary : col);
            g.drawText (title, card.withTrimmedBottom (20.0f), juce::Justification::centred);
        };

        drawBandCard (0, "LOW BAND (IN)",  UITheme::applePurple, module.getLiveLow(),  lowMuteBtn.getToggleState());
        drawBandCard (1, "MID BAND (IN)",  UITheme::appleGreen,  module.getLiveMid(),  midMuteBtn.getToggleState());
        drawBandCard (2, "HIGH BAND (IN)", UITheme::appleCyan,   module.getLiveHigh(), highMuteBtn.getToggleState());

        // Knob labels
        int labelY = 114;
        g.setColour (UITheme::textSecondary);
        g.setFont (UITheme::getFont (8.5f, true));
        g.drawText ("LOW GAIN",  colW * 0, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("MID GAIN",  colW * 1, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("HIGH GAIN", colW * 2, labelY, colW, 14, juce::Justification::centred);
    }

    void resized() override
    {
        int colW = getWidth() / 3;
        int knobY = 130;
        int knobSize = 58;

        lowGainSlider.setBounds  (colW * 0 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        midGainSlider.setBounds  (colW * 1 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        highGainSlider.setBounds (colW * 2 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);

        int btnY = knobY + knobSize + 38;
        int btnW = 22;
        int btnH = 20;

        lowSoloBtn.setBounds  (colW * 0 + (colW / 2) - btnW - 2, btnY, btnW, btnH);
        lowMuteBtn.setBounds  (colW * 0 + (colW / 2) + 2,        btnY, btnW, btnH);

        midSoloBtn.setBounds  (colW * 1 + (colW / 2) - btnW - 2, btnY, btnW, btnH);
        midMuteBtn.setBounds  (colW * 1 + (colW / 2) + 2,        btnY, btnW, btnH);

        highSoloBtn.setBounds (colW * 2 + (colW / 2) - btnW - 2, btnY, btnW, btnH);
        highMuteBtn.setBounds (colW * 2 + (colW / 2) + 2,        btnY, btnW, btnH);
    }

private:
    CrossoverJoinerModule& module;
    juce::Slider lowGainSlider, midGainSlider, highGainSlider;
    juce::ToggleButton lowSoloBtn, midSoloBtn, highSoloBtn, lowMuteBtn, midMuteBtn, highMuteBtn;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> lowGainAttach, midGainAttach, highGainAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> lowSoloAttach, midSoloAttach, highSoloAttach, lowMuteAttach, midMuteAttach, highMuteAttach;
};

inline juce::AudioProcessorEditor* CrossoverJoinerModule::createEditor()
{
    return new CrossoverJoinerModuleEditor (*this, apvts);
}
