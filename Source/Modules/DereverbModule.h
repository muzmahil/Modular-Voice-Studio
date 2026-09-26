#pragma once
#include "../Graph/ModuleProcessor.h"
#include "../UITheme.h"
#include <cmath>
#include <vector>

class DereverbModuleEditor;

class DereverbModule : public ModuleProcessor
{
public:
    static constexpr int numBands = 16;
    static constexpr int historySize = 140;

    struct ScopePoint
    {
        float directLevel;
        float reverbLevel;
    };

    DereverbModule()
        : ModuleProcessor ("De-reverb", createLayout())
    {
        reductionParam = getModuleParam ("reduction", 70.0f);
        decayParam     = getModuleParam ("decayTime", 0.45f);
        sizeParam      = getModuleParam ("roomSize", 22.0f);

        for (int b = 0; b < numBands; ++b)
        {
            bandEnergy[b] = 0.0f;
            reverbEnergy[b] = 0.0f;
            bandGains[b] = 1.0f;
        }

        for (int i = 0; i < historySize; ++i)
            history[i] = { 0.0f, 0.0f };
    }

    void prepareToPlay (double sr, int) override
    {
        sampleRate = sr > 0.0 ? sr : 48000.0;
        for (int b = 0; b < numBands; ++b)
        {
            bandEnergy[b] = 0.0f;
            reverbEnergy[b] = 0.0f;
            bandGains[b] = 1.0f;
        }
        delayBuffer.assign ((int) (sampleRate * 0.1), 0.0f);
        delayWritePos = 0;
    }

    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();
        if (numChannels == 0 || numSamples == 0) return;

        float reductionNorm = juce::jlimit (0.0f, 1.0f, reductionParam.get (70.0f) * 0.01f);
        float t60           = juce::jlimit (0.1f, 1.5f, decayParam.get (0.45f));
        float roomDelayMs   = juce::jlimit (5.0f, 50.0f, sizeParam.get (22.0f));

        int delaySamples = juce::jlimit (1, (int) delayBuffer.size() - 1, (int) (roomDelayMs * 0.001f * (float) sampleRate));
        float decayAlpha = std::exp (-6.91f / (t60 * (float) sampleRate * 0.05f));

        float blockDirectPeak = 0.0f;
        float blockReverbPeak = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float monoIn = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                monoIn += buffer.getSample (ch, i);
            monoIn /= (float) numChannels;

            float absIn = std::abs (monoIn);

            // Store in delay line for late reflection modeling
            delayBuffer[delayWritePos] = absIn;
            int lateReadPos = (delayWritePos - delaySamples + (int) delayBuffer.size()) % (int) delayBuffer.size();
            float lateRef = delayBuffer[lateReadPos];
            delayWritePos = (delayWritePos + 1) % (int) delayBuffer.size();

            // Multi-band late reverberation energy estimation
            float totalGain = 0.0f;

            for (int b = 0; b < numBands; ++b)
            {
                float bandWeight = 1.0f / (1.0f + (float) b * 0.15f);
                float bEnergy = absIn * bandWeight;

                bandEnergy[b] = bandEnergy[b] * 0.96f + bEnergy * 0.04f;

                // Reverb floor tracking with exponential decay
                float lateEnergy = lateRef * bandWeight;
                reverbEnergy[b] = reverbEnergy[b] * decayAlpha + lateEnergy * (1.0f - decayAlpha);

                // Compute Dereverb attenuation gain for this sub-band
                float targetGain = 1.0f;
                if (bandEnergy[b] > 1e-4f)
                {
                    float revRatio = reverbEnergy[b] / (bandEnergy[b] + 1e-4f);
                    targetGain = 1.0f - reductionNorm * juce::jlimit (0.0f, 0.88f, revRatio * 0.75f);
                }

                // Asymmetric smoothing: fast attack (preserves direct vocal), gentle release
                if (targetGain < bandGains[b])
                    bandGains[b] = bandGains[b] * 0.85f + targetGain * 0.15f;
                else
                    bandGains[b] = bandGains[b] * 0.992f + targetGain * 0.008f;

                totalGain += bandGains[b];
            }

            float overallGain = totalGain / (float) numBands;
            overallGain = juce::jlimit (0.12f, 1.0f, overallGain);

            // Apply dereverberation gain to all channels
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float s = buffer.getSample (ch, i);
                float clean = s * overallGain;
                buffer.setSample (ch, i, clean);
            }

            float cleanPeak = absIn * overallGain;
            float reverbTail = absIn * (1.0f - overallGain);

            blockDirectPeak = juce::jmax (blockDirectPeak, cleanPeak);
            blockReverbPeak = juce::jmax (blockReverbPeak, reverbTail);
        }

        // Live Reverb Reduction dB
        float reductionDb = 0.0f;
        if (blockDirectPeak > 1e-4f && blockReverbPeak > 1e-5f)
            reductionDb = 20.0f * std::log10 ((blockDirectPeak + blockReverbPeak) / blockDirectPeak);
        reductionDb = juce::jlimit (0.0f, 30.0f, reductionDb);

        liveReductionDb.store (reductionDb, std::memory_order_relaxed);

        // Update visualizer stream
        samplesSincePush += numSamples;
        if (samplesSincePush >= (int) (sampleRate / 40.0))
        {
            samplesSincePush = 0;
            int idx = historyWritePos.load (std::memory_order_relaxed);
            history[idx] = { blockDirectPeak, blockReverbPeak };
            historyWritePos.store ((idx + 1) % historySize, std::memory_order_relaxed);
        }
    }

    juce::AudioProcessorEditor* createEditor() override;

    float getLiveReductionDb() const { return liveReductionDb.load (std::memory_order_relaxed); }

    void getHistory (std::vector<ScopePoint>& dest) const
    {
        dest.resize (historySize);
        int head = historyWritePos.load (std::memory_order_relaxed);
        for (int i = 0; i < historySize; ++i)
            dest[i] = history[(head + i) % historySize];
    }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        return {
            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "reduction", 1 }, "Reduction Depth",
                juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 70.0f,
                juce::AudioParameterFloatAttributes().withLabel ("%")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "decayTime", 1 }, "Room Decay T60",
                juce::NormalisableRange<float> (0.1f, 1.5f, 0.01f), 0.45f,
                juce::AudioParameterFloatAttributes().withLabel ("s")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "roomSize", 1 }, "Early Delay",
                juce::NormalisableRange<float> (5.0f, 50.0f, 1.0f), 22.0f,
                juce::AudioParameterFloatAttributes().withLabel ("ms"))
        };
    }

    ParamRef reductionParam;
    ParamRef decayParam;
    ParamRef sizeParam;

    double sampleRate = 48000.0;
    float bandEnergy[numBands];
    float reverbEnergy[numBands];
    float bandGains[numBands];

    std::vector<float> delayBuffer;
    int delayWritePos = 0;

    std::atomic<float> liveReductionDb { 0.0f };
    ScopePoint history[historySize];
    std::atomic<int> historyWritePos { 0 };
    int samplesSincePush = 0;
};

class DereverbModuleEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    DereverbModuleEditor (DereverbModule& p, juce::AudioProcessorValueTreeState& vts)
        : AudioProcessorEditor (&p), module (p)
    {
        auto setupSlider = [this] (juce::Slider& s, const juce::String& suffix, juce::Colour col)
        {
            s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 55, 18);
            s.setTextValueSuffix (suffix);
            s.setColour (juce::Slider::rotarySliderFillColourId, col);
            s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff1e1e24));
            s.setColour (juce::Slider::textBoxOutlineColourId, UITheme::strokeHairline);
            s.setColour (juce::Slider::textBoxTextColourId, UITheme::textPrimary);
            addAndMakeVisible (s);
        };

        setupSlider (reducSlider, " %",  UITheme::applePurple);
        setupSlider (decaySlider, " s",  UITheme::appleCyan);
        setupSlider (sizeSlider,  " ms", UITheme::appleGreen);

        reducAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "reduction", reducSlider);
        decayAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "decayTime", decaySlider);
        sizeAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "roomSize", sizeSlider);

        setSize (420, 280);
        startTimerHz (60);
    }

    ~DereverbModuleEditor() override { stopTimer(); }

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
        g.drawText ("DE-REVERB (ROOM REFLECTION FILTER)", header.withTrimmedLeft (14), juce::Justification::centredLeft);

        // Reverb Reduction Badge
        float redDb = module.getLiveReductionDb();
        auto badge = header.removeFromRight (125).toFloat().reduced (6.0f, 8.0f);
        g.setColour (UITheme::applePurple.withAlpha (0.28f));
        g.fillRoundedRectangle (badge, 3.0f);
        g.setColour (UITheme::applePurple);
        g.drawRoundedRectangle (badge, 3.0f, 1.0f);
        g.setFont (UITheme::getFont (8.0f, true));
        g.drawText ("REVERB -" + juce::String (redDb, 1) + " dB", badge, juce::Justification::centred);

        // --- REAL-TIME DUAL-TRACE ROOM DECAY MONITOR ---
        auto screenRect = juce::Rectangle<float> (16.0f, 44.0f, (float) getWidth() - 32.0f, 82.0f);
        g.setColour (juce::Colour (0xff111114));
        g.fillRoundedRectangle (screenRect, 4.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (screenRect, 4.0f, 0.8f);

        std::vector<DereverbModule::ScopePoint> pts;
        module.getHistory (pts);

        if (! pts.empty())
        {
            float stepX = screenRect.getWidth() / (float) (pts.size() - 1);
            juce::Path dirPath, revPath;
            bool started = false;

            for (size_t i = 0; i < pts.size(); ++i)
            {
                float x = screenRect.getX() + (float) i * stepX;
                float dirH = juce::jlimit (0.0f, 1.0f, pts[i].directLevel * 1.6f) * (screenRect.getHeight() - 10.0f);
                float revH = juce::jlimit (0.0f, 1.0f, (pts[i].directLevel + pts[i].reverbLevel) * 1.6f) * (screenRect.getHeight() - 10.0f);

                float dirY = screenRect.getBottom() - dirH - 5.0f;
                float revY = screenRect.getBottom() - revH - 5.0f;

                if (! started)
                {
                    dirPath.startNewSubPath (x, dirY);
                    revPath.startNewSubPath (x, revY);
                    started = true;
                }
                else
                {
                    dirPath.lineTo (x, dirY);
                    revPath.lineTo (x, revY);
                }
            }

            // Suppressed Reverb Tail (Purple)
            g.setColour (UITheme::applePurple.withAlpha (0.45f));
            g.strokePath (revPath, juce::PathStrokeType (1.2f));

            // Clean Direct Speech (Green)
            g.setColour (UITheme::appleGreen);
            g.strokePath (dirPath, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Legend
        g.setFont (UITheme::getFont (7.5f, true));
        g.setColour (UITheme::appleGreen);
        g.drawText ("DIRECT SPEECH", juce::Rectangle<float> (screenRect.getX() + 8.0f, screenRect.getY() + 5.0f, 100.0f, 12.0f), juce::Justification::centredLeft);
        g.setColour (UITheme::applePurple);
        g.drawText ("SUPPRESSED ROOM REVERB", juce::Rectangle<float> (screenRect.getRight() - 140.0f, screenRect.getY() + 5.0f, 132.0f, 12.0f), juce::Justification::centredRight);

        // Knob labels
        int colW = getWidth() / 3;
        int labelY = 136;
        g.setColour (UITheme::textSecondary);
        g.setFont (UITheme::getFont (9.0f, true));
        g.drawText ("REVERB REDUCTION", colW * 0, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("ROOM DECAY T60",   colW * 1, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("EARLY DELAY",      colW * 2, labelY, colW, 14, juce::Justification::centred);
    }

    void resized() override
    {
        int colW = getWidth() / 3;
        int knobY = 156;
        int knobSize = 64;

        reducSlider.setBounds (colW * 0 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        decaySlider.setBounds (colW * 1 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        sizeSlider.setBounds  (colW * 2 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
    }

private:
    DereverbModule& module;
    juce::Slider reducSlider, decaySlider, sizeSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> reducAttach, decayAttach, sizeAttach;
};

inline juce::AudioProcessorEditor* DereverbModule::createEditor()
{
    return new DereverbModuleEditor (*this, apvts);
}
