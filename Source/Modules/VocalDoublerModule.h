#pragma once
#include "../Graph/ModuleProcessor.h"
#include "../UITheme.h"
#include <cmath>
#include <vector>

class VocalDoublerModuleEditor;

class VocalDoublerModule : public ModuleProcessor
{
public:
    static constexpr int scopePoints = 64;

    struct ScopePoint
    {
        float left;
        float right;
    };

    VocalDoublerModule()
        : ModuleProcessor ("Vocal Doubler", createLayout())
    {
        spreadParam  = getModuleParam ("spread", 75.0f);
        detuneParam  = getModuleParam ("detune", 15.0f);
        delayParam   = getModuleParam ("delayTime", 20.0f);
        levelParam   = getModuleParam ("doubleLevel", 80.0f);

        for (int i = 0; i < scopePoints; ++i)
            scopeHistory[i] = { 0.0f, 0.0f };
    }

    void prepareToPlay (double sr, int) override
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;
        maxDelaySamples = (int) (0.060 * sampleRate); // 60ms buffer
        delayBufferL.assign (maxDelaySamples, 0.0f);
        delayBufferR.assign (maxDelaySamples, 0.0f);
        writeIndex = 0;
        lfoPhaseL = 0.0f;
        lfoPhaseR = juce::MathConstants<float>::pi * 0.5f;
        samplesSincePush = 0;
    }

    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();
        if (numChannels == 0 || numSamples == 0) return;

        float spreadNorm = juce::jlimit (0.0f, 1.0f, spreadParam.get (75.0f) * 0.01f);
        float detuneCts  = juce::jlimit (0.0f, 20.0f, detuneParam.get (15.0f));
        float baseDelayMs= juce::jlimit (5.0f, 40.0f, delayParam.get (20.0f));
        float dblGain    = juce::jlimit (0.0f, 1.0f, levelParam.get (80.0f) * 0.01f);

        // LFO rate for micro-pitch detuning (0.4 Hz to 0.7 Hz)
        float lfoIncL = (2.0f * juce::MathConstants<float>::pi * 0.45f) / (float) sampleRate;
        float lfoIncR = (2.0f * juce::MathConstants<float>::pi * 0.65f) / (float) sampleRate;

        // Detune modulation depth in samples
        float modDepthSamples = (detuneCts * 0.0006f) * (float) sampleRate;
        float baseDelaySamples = (baseDelayMs * 0.001f) * (float) sampleRate;

        float* outL = buffer.getWritePointer (0);
        float* outR = numChannels > 1 ? buffer.getWritePointer (1) : outL;

        for (int i = 0; i < numSamples; ++i)
        {
            float monoIn = (outL[i] + outR[i]) * 0.5f;

            // Write into ring buffer
            delayBufferL[writeIndex] = monoIn;
            delayBufferR[writeIndex] = monoIn;

            // Modulate delay read pointers
            lfoPhaseL += lfoIncL;
            if (lfoPhaseL >= 2.0f * juce::MathConstants<float>::pi) lfoPhaseL -= 2.0f * juce::MathConstants<float>::pi;

            lfoPhaseR += lfoIncR;
            if (lfoPhaseR >= 2.0f * juce::MathConstants<float>::pi) lfoPhaseR -= 2.0f * juce::MathConstants<float>::pi;

            float delayL = baseDelaySamples + std::sin (lfoPhaseL) * modDepthSamples;
            float delayR = (baseDelaySamples * 1.35f) + std::sin (lfoPhaseR) * modDepthSamples;

            // Linear interpolation read
            float voiceL = readDelay (delayBufferL, delayL);
            float voiceR = readDelay (delayBufferR, delayR);

            writeIndex = (writeIndex + 1) % maxDelaySamples;

            // Stereo panning matrix
            float wetL = (voiceL * (0.5f + spreadNorm * 0.5f) + voiceR * (0.5f - spreadNorm * 0.5f)) * dblGain;
            float wetR = (voiceR * (0.5f + spreadNorm * 0.5f) + voiceL * (0.5f - spreadNorm * 0.5f)) * dblGain;

            outL[i] = monoIn + wetL;
            if (numChannels > 1)
                outR[i] = monoIn + wetR;

            // Capture for Lissajous scope
            samplesSincePush++;
            if (samplesSincePush >= (int) (sampleRate / 400.0))
            {
                samplesSincePush = 0;
                int idx = scopeWriteIndex.load (std::memory_order_relaxed);
                scopeHistory[idx] = { outL[i], numChannels > 1 ? outR[i] : outL[i] };
                scopeWriteIndex.store ((idx + 1) % scopePoints, std::memory_order_relaxed);
            }
        }
    }

    juce::AudioProcessorEditor* createEditor() override;

    void getScopeData (std::vector<ScopePoint>& dest) const
    {
        dest.resize (scopePoints);
        int head = scopeWriteIndex.load (std::memory_order_relaxed);
        for (int i = 0; i < scopePoints; ++i)
            dest[i] = scopeHistory[(head + i) % scopePoints];
    }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        return {
            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "spread", 1 }, "Stereo Spread",
                juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 75.0f,
                juce::AudioParameterFloatAttributes().withLabel ("%")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "detune", 1 }, "Detune",
                juce::NormalisableRange<float> (0.0f, 20.0f, 0.5f), 6.0f,
                juce::AudioParameterFloatAttributes().withLabel ("cts")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "delayTime", 1 }, "Delay Spread",
                juce::NormalisableRange<float> (5.0f, 35.0f, 1.0f), 15.0f,
                juce::AudioParameterFloatAttributes().withLabel ("ms")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "doubleLevel", 1 }, "Double Level",
                juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 60.0f,
                juce::AudioParameterFloatAttributes().withLabel ("%"))
        };
    }

    float readDelay (const std::vector<float>& buf, float delaySamples)
    {
        float readPos = (float) writeIndex - delaySamples;
        while (readPos < 0.0f) readPos += (float) maxDelaySamples;

        int i0 = (int) readPos;
        int i1 = (i0 + 1) % maxDelaySamples;
        float frac = readPos - (float) i0;

        return buf[i0] + frac * (buf[i1] - buf[i0]);
    }

    ParamRef spreadParam;
    ParamRef detuneParam;
    ParamRef delayParam;
    ParamRef levelParam;

    double sampleRate = 44100.0;
    int maxDelaySamples = 2646;
    std::vector<float> delayBufferL;
    std::vector<float> delayBufferR;
    int writeIndex = 0;
    float lfoPhaseL = 0.0f;
    float lfoPhaseR = 0.0f;
    int samplesSincePush = 0;

    ScopePoint scopeHistory[scopePoints];
    std::atomic<int> scopeWriteIndex { 0 };
};

class VocalDoublerModuleEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    VocalDoublerModuleEditor (VocalDoublerModule& p, juce::AudioProcessorValueTreeState& vts)
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

        setupSlider (spreadSlider, " %",   UITheme::appleBlue);
        setupSlider (detuneSlider, " cts", juce::Colour (0xffbf5af2));
        setupSlider (delaySlider,  " ms",  UITheme::appleYellow);
        setupSlider (levelSlider,  " %",   UITheme::appleGreen);

        spreadAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "spread", spreadSlider);
        detuneAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "detune", detuneSlider);
        delayAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "delayTime", delaySlider);
        levelAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "doubleLevel", levelSlider);

        setSize (350, 270);
        startTimerHz (60);
    }

    ~VocalDoublerModuleEditor() override { stopTimer(); }

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
        g.drawText ("VOCAL DOUBLER (STEREO SPREAD)", header.withTrimmedLeft (14), juce::Justification::centredLeft);

        // Wide Badge
        auto badge = header.removeFromRight (95).toFloat().reduced (6.0f, 8.0f);
        g.setColour (UITheme::appleBlue.withAlpha (0.28f));
        g.fillRoundedRectangle (badge, 3.0f);
        g.setColour (UITheme::appleBlue);
        g.drawRoundedRectangle (badge, 3.0f, 1.0f);
        g.setFont (UITheme::getFont (8.0f, true));
        g.drawText ("STEREO FIELD", badge, juce::Justification::centred);

        // --- REAL-TIME STEREO LISSAJOUS CORRELATION SCOPE ---
        auto screenRect = juce::Rectangle<float> (16.0f, 44.0f, (float) getWidth() - 32.0f, 76.0f);
        g.setColour (juce::Colour (0xff111114));
        g.fillRoundedRectangle (screenRect, 4.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (screenRect, 4.0f, 0.8f);

        float cx = screenRect.getCentreX();
        float cy = screenRect.getCentreY();

        // 45-degree axis lines
        g.setColour (juce::Colour (0x15ffffff));
        g.drawLine (cx - 30.0f, cy, cx + 30.0f, cy, 1.0f);
        g.drawLine (cx, cy - 30.0f, cx, cy + 30.0f, 1.0f);

        // Draw Lissajous Stereo Field
        std::vector<VocalDoublerModule::ScopePoint> pts;
        module.getScopeData (pts);

        if (! pts.empty())
        {
            g.setColour (UITheme::appleBlue.withAlpha (0.75f));
            for (const auto& p : pts)
            {
                // Rotate 45 degrees: M = (L+R)*0.707, S = (L-R)*0.707
                float m = (p.left + p.right) * 0.7071f;
                float s = (p.left - p.right) * 0.7071f;

                float px = cx + s * 34.0f;
                float py = cy - m * 34.0f;

                g.fillEllipse (px - 1.5f, py - 1.5f, 3.0f, 3.0f);
            }
        }

        // Legends
        g.setFont (UITheme::getFont (7.5f, true));
        g.setColour (UITheme::textTertiary);
        g.drawText ("L", juce::Rectangle<float> (cx - 45.0f, cy - 6.0f, 12.0f, 12.0f), juce::Justification::centred);
        g.drawText ("R", juce::Rectangle<float> (cx + 33.0f, cy - 6.0f, 12.0f, 12.0f), juce::Justification::centred);

        // Knob labels
        int colW = getWidth() / 4;
        int labelY = 126;
        g.setColour (UITheme::textSecondary);
        g.setFont (UITheme::getFont (9.0f, true));
        g.drawText ("SPREAD",  colW * 0, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("DETUNE",  colW * 1, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("DELAY",   colW * 2, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("LEVEL",   colW * 3, labelY, colW, 14, juce::Justification::centred);
    }

    void resized() override
    {
        int colW = getWidth() / 4;
        int knobY = 144;
        int knobSize = 66;

        spreadSlider.setBounds (colW * 0 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        detuneSlider.setBounds (colW * 1 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        delaySlider.setBounds  (colW * 2 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        levelSlider.setBounds  (colW * 3 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
    }

private:
    VocalDoublerModule& module;
    juce::Slider spreadSlider, detuneSlider, delaySlider, levelSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> spreadAttach, detuneAttach, delayAttach, levelAttach;
};

inline juce::AudioProcessorEditor* VocalDoublerModule::createEditor()
{
    return new VocalDoublerModuleEditor (*this, apvts);
}
