#pragma once
#include "../Graph/ModuleProcessor.h"
#include "../UITheme.h"
#include <cmath>
#include <vector>

class LimiterModuleEditor;

class LimiterModule : public ModuleProcessor
{
public:
    static constexpr int maxLookaheadSamples = 1024;
    static constexpr int historyLength = 200;

    LimiterModule()
        : ModuleProcessor ("Limiter", createLayout())
    {
        ceilingParam = getModuleParam ("ceiling", -0.3f);
        gainParam    = getModuleParam ("gain", 0.0f);
        releaseParam = getModuleParam ("release", 80.0f);

        delayBuffer.setSize (2, maxLookaheadSamples);
        delayBuffer.clear();

        for (int i = 0; i < historyLength; ++i)
            history[i] = { -60.0f, 0.0f };
    }

    void prepareToPlay (double sr, int) override
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;
        lookaheadSamples = juce::jlimit (16, maxLookaheadSamples, (int) (0.003 * sampleRate)); // 3ms lookahead
        delayBuffer.setSize (2, maxLookaheadSamples);
        delayBuffer.clear();
        writePos = 0;
        currentGain = 1.0f;
        samplesSincePush = 0;
    }

    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();
        if (numChannels == 0 || numSamples == 0) return;

        float ceilingDb = ceilingParam.get (-0.3f);
        float ceilingLin= juce::Decibels::decibelsToGain (ceilingDb, -90.0f);
        float inputGain = juce::Decibels::decibelsToGain (gainParam.get (0.0f));
        float relMs     = juce::jmax (5.0f, releaseParam.get (80.0f));
        float relCoeff  = std::exp (-1.0f / (float) ((relMs * 0.001f) * sampleRate));

        float blockMaxIn  = 0.0f;
        float blockMaxOut = 0.0f;
        float blockMaxGrDb= 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float peakIn = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float s = buffer.getSample (ch, i) * inputGain;
                peakIn = juce::jmax (peakIn, std::abs (s));
                int chIdx = ch < 2 ? ch : 0;
                delayBuffer.setSample (chIdx, writePos, s);
            }

            blockMaxIn = juce::jmax (blockMaxIn, peakIn);

            // Compute target gain based on upcoming peak
            float targetGain = 1.0f;
            if (peakIn > ceilingLin)
                targetGain = ceilingLin / peakIn;

            if (targetGain < currentGain)
                currentGain = targetGain; // instant attack
            else
                currentGain = relCoeff * currentGain + (1.0f - relCoeff) * targetGain;

            // Read delayed sample from lookahead ring buffer
            int readPos = (writePos - lookaheadSamples + maxLookaheadSamples) % maxLookaheadSamples;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                int chIdx = ch < 2 ? ch : 0;
                float delayed = delayBuffer.getSample (chIdx, readPos);
                float out = delayed * currentGain;
                buffer.setSample (ch, i, out);
                blockMaxOut = juce::jmax (blockMaxOut, std::abs (out));
            }

            writePos = (writePos + 1) % maxLookaheadSamples;

            float grDb = -juce::Decibels::gainToDecibels (currentGain);
            blockMaxGrDb = juce::jmax (blockMaxGrDb, grDb);
        }

        liveGainReduction.store (blockMaxGrDb, std::memory_order_relaxed);
        liveInputPeak.store  (blockMaxIn, std::memory_order_relaxed);
        liveOutputPeak.store (blockMaxOut, std::memory_order_relaxed);

        samplesSincePush += numSamples;
        int interval = (int) (sampleRate / 60.0);
        if (samplesSincePush >= interval)
        {
            samplesSincePush = 0;
            float inDb = juce::Decibels::gainToDecibels (blockMaxOut, -60.0f);
            int idx = historyWriteIndex.load (std::memory_order_relaxed);
            history[idx] = { inDb, blockMaxGrDb };
            historyWriteIndex.store ((idx + 1) % historyLength, std::memory_order_relaxed);
        }
    }

    juce::AudioProcessorEditor* createEditor() override;

    float getLiveGainReduction() const { return liveGainReduction.load (std::memory_order_relaxed); }
    float getLiveInputPeak() const     { return liveInputPeak.load (std::memory_order_relaxed); }
    float getLiveOutputPeak() const    { return liveOutputPeak.load (std::memory_order_relaxed); }
    float getCeilingDb() const         { return ceilingParam.get (-0.3f); }

    struct HistoryPoint { float outDb; float grDb; };
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
                juce::ParameterID { "ceiling", 1 }, "Ceiling",
                juce::NormalisableRange<float> (-12.0f, 0.0f, 0.1f), -0.3f,
                juce::AudioParameterFloatAttributes().withLabel ("dBFS")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "gain", 1 }, "Drive Gain",
                juce::NormalisableRange<float> (0.0f, 24.0f, 0.5f), 0.0f,
                juce::AudioParameterFloatAttributes().withLabel ("dB")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "release", 1 }, "Release",
                juce::NormalisableRange<float> (10.0f, 500.0f, 1.0f, 0.4f), 80.0f,
                juce::AudioParameterFloatAttributes().withLabel ("ms"))
        };
    }

    ParamRef ceilingParam;
    ParamRef gainParam;
    ParamRef releaseParam;

    double sampleRate = 44100.0;
    int lookaheadSamples = 132;
    int writePos = 0;
    float currentGain = 1.0f;
    int samplesSincePush = 0;
    juce::AudioBuffer<float> delayBuffer;

    std::atomic<float> liveGainReduction { 0.0f };
    std::atomic<float> liveInputPeak { 0.0f };
    std::atomic<float> liveOutputPeak { 0.0f };

    HistoryPoint history[historyLength];
    std::atomic<int> historyWriteIndex { 0 };
};

class LimiterModuleEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    LimiterModuleEditor (LimiterModule& p, juce::AudioProcessorValueTreeState& vts)
        : AudioProcessorEditor (&p), module (p)
    {
        auto setupSlider = [this] (juce::Slider& s, const juce::String& suffix, juce::Colour col)
        {
            s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 18);
            s.setTextValueSuffix (suffix);
            s.setColour (juce::Slider::rotarySliderFillColourId, col);
            s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff1e1e24));
            s.setColour (juce::Slider::textBoxOutlineColourId, UITheme::strokeHairline);
            s.setColour (juce::Slider::textBoxTextColourId, UITheme::textPrimary);
            addAndMakeVisible (s);
        };

        setupSlider (ceilingSlider, " dB", UITheme::appleRed);
        setupSlider (gainSlider, " dB", UITheme::appleBlue);
        setupSlider (releaseSlider, " ms", UITheme::textPrimary);

        ceilingAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "ceiling", ceilingSlider);
        gainAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "gain", gainSlider);
        releaseAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "release", releaseSlider);

        setSize (310, 270);
        startTimerHz (60);
    }

    ~LimiterModuleEditor() override { stopTimer(); }

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

        // Title
        g.setColour (UITheme::textPrimary);
        g.setFont (UITheme::getFont (12.0f, true));
        g.drawText ("BRICKWALL PEAK LIMITER", header.withTrimmedLeft (14), juce::Justification::centredLeft);

        // GR readout
        float grVal = module.getLiveGainReduction();
        auto badge = header.removeFromRight (86).toFloat().reduced (6.0f, 8.0f);
        bool limiting = grVal > 0.1f;
        g.setColour (limiting ? UITheme::appleRed.withAlpha (0.30f) : juce::Colour (0x15ffffff));
        g.fillRoundedRectangle (badge, 3.0f);
        g.setColour (limiting ? UITheme::appleRed : UITheme::textTertiary);
        g.drawRoundedRectangle (badge, 3.0f, 1.0f);
        g.setFont (UITheme::getFont (8.0f, true));
        g.drawText (limiting ? "LIMIT -" + juce::String (grVal, 1) + " dB" : "SAFE", badge, juce::Justification::centred);

        // --- REAL-TIME PEAK WAVEFORM WITH MOVABLE CEILING ---
        auto waveRect = juce::Rectangle<float> (16.0f, 44.0f, (float) getWidth() - 56.0f, 76.0f);
        auto meterRect= juce::Rectangle<float> (waveRect.getRight() + 8.0f, 44.0f, 16.0f, 76.0f);

        g.setColour (juce::Colour (0xff111114));
        g.fillRoundedRectangle (waveRect, 4.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (waveRect, 4.0f, 0.8f);

        // Grid lines
        g.setColour (juce::Colour (0x10ffffff));
        for (float db : { -3.0f, -6.0f, -12.0f, -24.0f })
        {
            float norm = 1.0f - (db / -30.0f);
            float gy = waveRect.getY() + (1.0f - norm) * waveRect.getHeight();
            g.drawHorizontalLine ((int) gy, waveRect.getX(), waveRect.getRight());
        }

        // Draw Peak History
        std::vector<LimiterModule::HistoryPoint> pts;
        module.getHistory (pts);

        if (! pts.empty())
        {
            float stepX = waveRect.getWidth() / (float) (pts.size() - 1);
            juce::Path outPath;
            bool started = false;

            for (size_t i = 0; i < pts.size(); ++i)
            {
                float x = waveRect.getX() + (float) i * stepX;
                float norm = juce::jlimit (0.0f, 1.0f, (pts[i].outDb + 30.0f) / 30.0f);
                float y = waveRect.getBottom() - norm * (waveRect.getHeight() - 4.0f) - 2.0f;

                if (! started) { outPath.startNewSubPath (x, y); started = true; }
                else          { outPath.lineTo (x, y); }
            }

            g.setColour (UITheme::appleGreen.withAlpha (0.90f));
            g.strokePath (outPath, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Gain reduction red shading
            if (grVal > 0.1f)
            {
                float grH = juce::jlimit (0.0f, 1.0f, grVal / 12.0f) * waveRect.getHeight();
                g.setColour (UITheme::appleRed.withAlpha (0.28f));
                g.fillRect (waveRect.withHeight (grH));
            }
        }

        // Movable Ceiling Line
        float ceilDb = module.getCeilingDb();
        float ceilNorm = juce::jlimit (0.0f, 1.0f, (ceilDb + 30.0f) / 30.0f);
        float ceilY = waveRect.getBottom() - ceilNorm * (waveRect.getHeight() - 4.0f) - 2.0f;

        g.setColour (UITheme::appleRed);
        float dashes[] = { 4.0f, 2.0f };
        g.drawDashedLine (juce::Line<float> (waveRect.getX(), ceilY, waveRect.getRight(), ceilY), dashes, 2, 1.4f);

        // Ceiling dB readout tag
        g.setFont (UITheme::getFont (7.5f, true));
        g.drawText (juce::String (ceilDb, 1) + " dB",
                    juce::Rectangle<float> (waveRect.getRight() - 38.0f, ceilY - 11.0f, 36.0f, 10.0f),
                    juce::Justification::centredRight);

        // Output Peak Meter
        g.setColour (juce::Colour (0xff111114));
        g.fillRoundedRectangle (meterRect, 3.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (meterRect, 3.0f, 0.8f);

        float outNorm = juce::jlimit (0.0f, 1.0f, module.getLiveOutputPeak());
        float barH = meterRect.getHeight() * outNorm;
        juce::Colour col = outNorm >= 0.99f ? UITheme::appleRed : UITheme::appleGreen;
        g.setColour (col);
        g.fillRect (meterRect.removeFromBottom (barH));

        // Knob labels
        int colW = getWidth() / 3;
        int labelY = 126;
        g.setColour (UITheme::textSecondary);
        g.setFont (UITheme::getFont (9.0f, true));
        g.drawText ("CEILING", 0, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("DRIVE GAIN", colW, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("RELEASE", colW * 2, labelY, colW, 14, juce::Justification::centred);
    }

    void resized() override
    {
        int colW = getWidth() / 3;
        int knobY = 144;
        int knobSize = 74;

        ceilingSlider.setBounds (colW * 0 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        gainSlider.setBounds    (colW * 1 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        releaseSlider.setBounds (colW * 2 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
    }

private:
    LimiterModule& module;
    juce::Slider ceilingSlider, gainSlider, releaseSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> ceilingAttach, gainAttach, releaseAttach;
};

inline juce::AudioProcessorEditor* LimiterModule::createEditor()
{
    return new LimiterModuleEditor (*this, apvts);
}
