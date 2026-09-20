#pragma once
#include "../Graph/ModuleProcessor.h"
#include "../UITheme.h"
#include <cmath>

class AuralExciterModuleEditor;

class AuralExciterModule : public ModuleProcessor
{
public:
    static constexpr int numBands = 16;

    AuralExciterModule()
        : ModuleProcessor ("Aural Exciter", createLayout())
    {
        freqParam   = apvts.getRawParameterValue ("frequency");
        driveParam  = apvts.getRawParameterValue ("drive");
        mixParam    = apvts.getRawParameterValue ("mix");
        airHarmParam= apvts.getRawParameterValue ("airHarmonics");

        for (int b = 0; b < numBands; ++b)
            liveAirSpectrum[b].store (0.0f);
    }

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

        float freq      = juce::jlimit (2500.0f, 10000.0f, freqParam->load());
        float driveNorm = juce::jlimit (0.0f, 1.0f, driveParam->load() * 0.01f);
        float mixNorm   = juce::jlimit (0.0f, 1.0f, mixParam->load() * 0.01f);
        float airHarm   = juce::jlimit (0.0f, 1.0f, airHarmParam->load() * 0.01f);

        updateSidechainHighPass (freq);

        float driveGain = 1.0f + driveNorm * 6.0f;
        float maxExciteEnergy = 0.0f;
        float tempAir[numBands] = {};

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* channelData = buffer.getWritePointer (ch);
            int fIdx = ch < 2 ? ch : 0;

            for (int i = 0; i < numSamples; ++i)
            {
                float dry = channelData[i];

                // 1. High-Pass Sidechain (isolate frequencies to excite)
                float hp = hpB0 * dry + hpB1 * hpX1[fIdx] + hpB2 * hpX2[fIdx]
                           - hpA1 * hpY1[fIdx] - hpA2 * hpY2[fIdx];
                hpX2[fIdx] = hpX1[fIdx];
                hpX1[fIdx] = dry;
                hpY2[fIdx] = hpY1[fIdx];
                hpY1[fIdx] = hp;

                // 2. Asymmetric Harmonic Synthesis (generating new air overtones)
                float x = hp * driveGain;
                // Asymmetric non-linear waveshaping generates 2nd + 3rd harmonics
                float harmonics = std::tanh (x) + airHarm * 0.45f * (x * x) / (1.0f + std::abs (x));
                // Remove DC and subtract fundamental to leave pure generated harmonics
                harmonics -= hp * 0.5f;

                float wet = harmonics * 0.8f;
                maxExciteEnergy = juce::jmax (maxExciteEnergy, std::abs (wet));

                // 3. Blend generated airy harmonics back into the original voice
                float out = dry + wet * mixNorm;
                channelData[i] = out;

                // Sample high air spectrum (4 kHz - 18 kHz)
                if (ch == 0 && (i % 2 == 0))
                {
                    float absWet = std::abs (wet);
                    for (int b = 0; b < numBands; ++b)
                    {
                        float f = 3000.0f * std::pow (18000.0f / 3000.0f, (float) b / (float) (numBands - 1));
                        float w = 1.0f / (1.0f + std::abs (f - freq * 1.5f) * 0.001f);
                        tempAir[b] = juce::jmax (tempAir[b], absWet * w);
                    }
                }
            }
        }

        // Decay air spectrum
        for (int b = 0; b < numBands; ++b)
        {
            float prev = liveAirSpectrum[b].load (std::memory_order_relaxed);
            float target = tempAir[b];
            float smoothed = target > prev ? (prev * 0.35f + target * 0.65f) : (prev * 0.88f);
            liveAirSpectrum[b].store (smoothed, std::memory_order_relaxed);
        }

        liveExciterEnergy.store (maxExciteEnergy, std::memory_order_relaxed);
    }

    juce::AudioProcessorEditor* createEditor() override;

    float getLiveExciterEnergy() const { return liveExciterEnergy.load (std::memory_order_relaxed); }
    float getFrequency() const         { return freqParam ? freqParam->load() : 5000.0f; }
    float getAirBandEnergy (int b) const { return liveAirSpectrum[juce::jlimit (0, numBands - 1, b)].load (std::memory_order_relaxed); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        return {
            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "frequency", 1 }, "Tune Freq",
                juce::NormalisableRange<float> (2500.0f, 9500.0f, 50.0f, 0.4f), 4800.0f,
                juce::AudioParameterFloatAttributes().withLabel ("Hz")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "drive", 1 }, "Exciter Drive",
                juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 35.0f,
                juce::AudioParameterFloatAttributes().withLabel ("%")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "airHarmonics", 1 }, "Air Shimmer",
                juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 50.0f,
                juce::AudioParameterFloatAttributes().withLabel ("%")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "mix", 1 }, "Exciter Mix",
                juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 30.0f,
                juce::AudioParameterFloatAttributes().withLabel ("%"))
        };
    }

    void resetFilters()
    {
        for (int ch = 0; ch < 2; ++ch)
            hpX1[ch] = hpX2[ch] = hpY1[ch] = hpY2[ch] = 0.0f;
    }

    void updateSidechainHighPass (float freq)
    {
        float w0 = 2.0f * juce::MathConstants<float>::pi * (freq / (float) sampleRate);
        float cosw0 = std::cos (w0);
        float alpha = std::sin (w0) / (2.0f * 0.7071f);
        float a0 = 1.0f + alpha;
        hpB0 = ((1.0f + cosw0) * 0.5f) / a0;
        hpB1 = (-(1.0f + cosw0)) / a0;
        hpB2 = ((1.0f + cosw0) * 0.5f) / a0;
        hpA1 = (-2.0f * cosw0) / a0;
        hpA2 = (1.0f - alpha) / a0;
    }

    std::atomic<float>* freqParam    = nullptr;
    std::atomic<float>* driveParam   = nullptr;
    std::atomic<float>* mixParam     = nullptr;
    std::atomic<float>* airHarmParam = nullptr;

    double sampleRate = 44100.0;
    float hpB0 = 1.0f, hpB1 = 0.0f, hpB2 = 0.0f, hpA1 = 0.0f, hpA2 = 0.0f;
    float hpX1[2] = {}, hpX2[2] = {}, hpY1[2] = {}, hpY2[2] = {};

    std::atomic<float> liveExciterEnergy { 0.0f };
    std::atomic<float> liveAirSpectrum[numBands];
};

class AuralExciterModuleEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    AuralExciterModuleEditor (AuralExciterModule& p, juce::AudioProcessorValueTreeState& vts)
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

        setupSlider (freqSlider,    " Hz", UITheme::appleBlue);
        setupSlider (driveSlider,   " %",  juce::Colour (0xffff9f0a));
        setupSlider (airHarmSlider, " %",  UITheme::appleYellow);
        setupSlider (mixSlider,     " %",  UITheme::appleGreen);

        freqAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "frequency", freqSlider);
        driveAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "drive", driveSlider);
        airHarmAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "airHarmonics", airHarmSlider);
        mixAttach     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "mix", mixSlider);

        setSize (340, 270);
        startTimerHz (60);
    }

    ~AuralExciterModuleEditor() override { stopTimer(); }

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
        g.drawText ("AURAL EXCITER (AIR & PRESENCE)", header.withTrimmedLeft (14), juce::Justification::centredLeft);

        // Exciter Active Badge
        float energy = module.getLiveExciterEnergy();
        bool active = energy > 0.02f;
        auto badge = header.removeFromRight (95).toFloat().reduced (6.0f, 8.0f);
        g.setColour (active ? UITheme::appleYellow.withAlpha (0.28f) : juce::Colour (0x15ffffff));
        g.fillRoundedRectangle (badge, 3.0f);
        g.setColour (active ? UITheme::appleYellow : UITheme::textTertiary);
        g.drawRoundedRectangle (badge, 3.0f, 1.0f);
        g.setFont (UITheme::getFont (8.0f, true));
        g.drawText (active ? "AIR GLOW" : "IDLE", badge, juce::Justification::centred);

        // --- REAL-TIME AIR SPECTRUM & EXCITER FREQUENCY DISPLAY ---
        auto screenRect = juce::Rectangle<float> (16.0f, 44.0f, (float) getWidth() - 32.0f, 76.0f);
        g.setColour (juce::Colour (0xff111114));
        g.fillRoundedRectangle (screenRect, 4.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (screenRect, 4.0f, 0.8f);

        // Grid lines
        g.setColour (juce::Colour (0x10ffffff));
        for (float gx = screenRect.getX() + 45.0f; gx < screenRect.getRight(); gx += 50.0f)
            g.drawVerticalLine ((int) gx, screenRect.getY(), screenRect.getBottom());
        g.drawHorizontalLine ((int) (screenRect.getY() + screenRect.getHeight() * 0.5f), screenRect.getX(), screenRect.getRight());

        // Air Spectrum Bars (Glowing gold/amber upper harmonics)
        int numBands = AuralExciterModule::numBands;
        float barWidth = (screenRect.getWidth() - 8.0f) / (float) numBands;

        for (int b = 0; b < numBands; ++b)
        {
            float bx = screenRect.getX() + 4.0f + (float) b * barWidth;
            float e = juce::jlimit (0.0f, 1.0f, module.getAirBandEnergy (b) * 3.5f);
            if (e < 0.01f) continue;

            float barH = e * (screenRect.getHeight() - 10.0f);
            auto barRect = juce::Rectangle<float> (bx + 1.0f, screenRect.getBottom() - barH - 4.0f, barWidth - 2.0f, barH);

            g.setColour (juce::Colour (0xffffd60a).withAlpha (0.45f));
            g.fillRoundedRectangle (barRect, 1.5f);
        }

        // Tune Frequency Marker Line
        float tuneFreq = module.getFrequency();
        float normFreq = (std::log10 (tuneFreq) - std::log10 (2500.0f)) / (std::log10 (18000.0f) - std::log10 (2500.0f));
        float markerX = screenRect.getX() + 4.0f + screenRect.getWidth() * juce::jlimit (0.05f, 0.95f, normFreq);

        g.setColour (UITheme::appleBlue);
        float dashes[] = { 3.0f, 2.0f };
        g.drawDashedLine (juce::Line<float> (markerX, screenRect.getY() + 4.0f, markerX, screenRect.getBottom() - 4.0f), dashes, 2, 1.2f);

        // Marker tag
        g.setFont (UITheme::getFont (7.5f, true));
        g.setColour (UITheme::appleBlue);
        g.drawText ("TUNE " + juce::String ((int) tuneFreq) + " Hz",
                    juce::Rectangle<float> (markerX + 4.0f, screenRect.getY() + 4.0f, 65.0f, 10.0f),
                    juce::Justification::centredLeft);

        // Labels
        g.setColour (UITheme::textTertiary);
        g.setFont (UITheme::getFont (7.5f));
        g.drawText ("3 kHz", juce::Rectangle<float> (screenRect.getX() + 6.0f, screenRect.getBottom() - 12.0f, 30.0f, 10.0f), juce::Justification::bottomLeft);
        g.drawText ("18 kHz (AIR)", juce::Rectangle<float> (screenRect.getRight() - 65.0f, screenRect.getBottom() - 12.0f, 60.0f, 10.0f), juce::Justification::bottomRight);

        // Knob labels
        int colW = getWidth() / 4;
        int labelY = 126;
        g.setColour (UITheme::textSecondary);
        g.setFont (UITheme::getFont (9.0f, true));
        g.drawText ("TUNE FREQ", colW * 0, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("DRIVE",     colW * 1, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("SHIMMER",   colW * 2, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("MIX",       colW * 3, labelY, colW, 14, juce::Justification::centred);
    }

    void resized() override
    {
        int colW = getWidth() / 4;
        int knobY = 144;
        int knobSize = 66;

        freqSlider.setBounds    (colW * 0 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        driveSlider.setBounds   (colW * 1 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        airHarmSlider.setBounds (colW * 2 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        mixSlider.setBounds     (colW * 3 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
    }

private:
    AuralExciterModule& module;
    juce::Slider freqSlider, driveSlider, airHarmSlider, mixSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> freqAttach, driveAttach, airHarmAttach, mixAttach;
};

inline juce::AudioProcessorEditor* AuralExciterModule::createEditor()
{
    return new AuralExciterModuleEditor (*this, apvts);
}
