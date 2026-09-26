#pragma once
#include "../Graph/ModuleProcessor.h"
#include "../UITheme.h"
#include <cmath>

class GainModuleEditor;

class GainModule : public ModuleProcessor
{
public:
    GainModule()
        : ModuleProcessor ("Gain", createLayout())
    {
        gainParam = getModuleParam ("gain", 0.0f);
    }

    void prepareToPlay (double, int) override {}
    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();
        if (numChannels == 0 || numSamples == 0) return;

        float currentDb = gainParam.get (0.0f);
        float linearGain = juce::Decibels::decibelsToGain (currentDb, -100.0f);

        float inPeak = 0.0f;
        for (int ch = 0; ch < numChannels; ++ch)
            inPeak = juce::jmax (inPeak, buffer.getMagnitude (ch, 0, numSamples));

        buffer.applyGain (linearGain);

        float outPeak = inPeak * linearGain;

        float prevIn = liveInPeak.load (std::memory_order_relaxed);
        float prevOut = liveOutPeak.load (std::memory_order_relaxed);
        liveInPeak.store  (inPeak > prevIn ? inPeak : (prevIn * 0.90f), std::memory_order_relaxed);
        liveOutPeak.store (outPeak > prevOut ? outPeak : (prevOut * 0.90f), std::memory_order_relaxed);
    }

    juce::AudioProcessorEditor* createEditor() override;

    float getLiveInPeak() const  { return liveInPeak.load (std::memory_order_relaxed); }
    float getLiveOutPeak() const { return liveOutPeak.load (std::memory_order_relaxed); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        juce::NormalisableRange<float> range (-100.0f, 48.0f, 0.1f);
        range.setSkewForCentre (0.0f); 

        return { std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "gain", 1 }, 
            "Gain",
            range, 
            0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")
        ) };
    }

    ParamRef gainParam;
    std::atomic<float> liveInPeak { 0.0f };
    std::atomic<float> liveOutPeak { 0.0f };
};

class GainModuleEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    GainModuleEditor (GainModule& p, juce::AudioProcessorValueTreeState& vts)
        : AudioProcessorEditor (&p), audioProcessor (p)
    {
        gainSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        gainSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 20);
        gainSlider.setColour (juce::Slider::rotarySliderFillColourId, UITheme::appleBlue);
        gainSlider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff1e1e24));
        gainSlider.setColour (juce::Slider::textBoxOutlineColourId, UITheme::strokeHairline);
        gainSlider.setColour (juce::Slider::textBoxTextColourId, UITheme::textPrimary);
        gainSlider.setTextValueSuffix (" dB"); 
        
        addAndMakeVisible (gainSlider);
        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "gain", gainSlider);
        
        setSize (240, 240);
        startTimerHz (60);
    }

    ~GainModuleEditor() override { stopTimer(); }

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
        g.setFont (UITheme::getFont (12.5f, true));
        g.drawText ("SIGNAL GAIN", header.withTrimmedLeft (14), juce::Justification::centredLeft);

        // Clip Indicator LED
        float outPeak = audioProcessor.getLiveOutPeak();
        bool isClipping = outPeak >= 1.0f;
        auto clipArea = header.removeFromRight (68).toFloat().reduced (8.0f, 8.0f);
        g.setColour (isClipping ? UITheme::appleRed.withAlpha (0.30f) : juce::Colour (0x15ffffff));
        g.fillRoundedRectangle (clipArea, 3.0f);
        g.setColour (isClipping ? UITheme::appleRed : UITheme::textTertiary);
        g.drawRoundedRectangle (clipArea, 3.0f, 1.0f);
        g.setFont (UITheme::getFont (8.0f, true));
        g.drawText (isClipping ? "CLIP" : "0 dBFS", clipArea, juce::Justification::centred);

        // --- DUAL LEVEL METERS (IN & OUT) ---
        auto meterBox = juce::Rectangle<float> ((float) getWidth() - 52.0f, 50.0f, 36.0f, 160.0f);
        g.setColour (juce::Colour (0xff111114));
        g.fillRoundedRectangle (meterBox, 4.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (meterBox, 4.0f, 0.8f);

        float inNorm  = juce::jlimit (0.0f, 1.0f, audioProcessor.getLiveInPeak());
        float outNorm = juce::jlimit (0.0f, 1.0f, outPeak);

        auto inBarArea  = juce::Rectangle<float> (meterBox.getX() + 5.0f, meterBox.getY() + 18.0f, 10.0f, meterBox.getHeight() - 24.0f);
        auto outBarArea = juce::Rectangle<float> (meterBox.getX() + 21.0f, meterBox.getY() + 18.0f, 10.0f, meterBox.getHeight() - 24.0f);

        // Meter Track Backgrounds
        g.setColour (juce::Colour (0xff1c1c22));
        g.fillRoundedRectangle (inBarArea, 1.5f);
        g.fillRoundedRectangle (outBarArea, 1.5f);

        // IN Meter Fill
        if (inNorm > 0.005f)
        {
            float activeH = inBarArea.getHeight() * inNorm;
            g.setColour (UITheme::appleBlue);
            g.fillRoundedRectangle (inBarArea.removeFromBottom (activeH), 1.5f);
        }

        // OUT Meter Fill (Gradient to Red if clipping)
        if (outNorm > 0.005f)
        {
            float activeH = outBarArea.getHeight() * outNorm;
            juce::Colour outCol = outNorm >= 1.0f ? UITheme::appleRed : (outNorm > 0.85f ? UITheme::appleYellow : UITheme::appleGreen);
            g.setColour (outCol);
            g.fillRoundedRectangle (outBarArea.removeFromBottom (activeH), 1.5f);
        }

        // Meter Labels
        g.setColour (UITheme::textTertiary);
        g.setFont (UITheme::getFont (7.5f, true));
        g.drawText ("IN", juce::Rectangle<float> (meterBox.getX() + 3.0f, meterBox.getY() + 4.0f, 14.0f, 10.0f), juce::Justification::centred);
        g.drawText ("OUT", juce::Rectangle<float> (meterBox.getX() + 19.0f, meterBox.getY() + 4.0f, 14.0f, 10.0f), juce::Justification::centred);
    }

    void resized() override
    {
        gainSlider.setBounds (20, 50, 150, 160);
    }

private:
    GainModule& audioProcessor;
    juce::Slider gainSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

inline juce::AudioProcessorEditor* GainModule::createEditor()
{
    return new GainModuleEditor (*this, apvts);
}