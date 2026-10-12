#pragma once
#include "../Graph/ModuleProcessor.h"
#include "../UITheme.h"
#include <rnnoise.h>
#include <cmath>
#include <vector>

class NoiseSuppressionModuleEditor;

class NoiseSuppressionModule : public ModuleProcessor
{
public:
    static constexpr int frameSize = 480; // 10ms @ 48kHz (RNNoise native frame size)
    static constexpr int ringCapacity = frameSize * 8;
    static constexpr int historySize = 160;

    struct MonitorPoint
    {
        float dryLevel;
        float cleanLevel;
        float vadProb;
    };

    NoiseSuppressionModule()
        : ModuleProcessor ("Noise Suppression", createLayout())
    {
        amountParam = getModuleParam ("amount", 100.0f);
        vadParam    = getModuleParam ("vadSensitivity", 0.0f); // Default 0% (pure neural suppression, no artificial gating)

        denoiseStates[0] = rnnoise_create (nullptr);
        denoiseStates[1] = rnnoise_create (nullptr);

        for (int ch = 0; ch < 2; ++ch)
        {
            inRing[ch].assign (ringCapacity, 0.0f);
            outRing[ch].assign (ringCapacity, 0.0f);
            resamp48kIn[ch].assign (ringCapacity, 0.0f);
            resamp48kOut[ch].assign (ringCapacity, 0.0f);
        }

        for (int i = 0; i < historySize; ++i)
        {
            history[i].dryLevel.store (0.0f, std::memory_order_relaxed);
            history[i].cleanLevel.store (0.0f, std::memory_order_relaxed);
            history[i].vadProb.store (0.0f, std::memory_order_relaxed);
        }
    }

    ~NoiseSuppressionModule() override
    {
        if (denoiseStates[0] != nullptr) { rnnoise_destroy (denoiseStates[0]); denoiseStates[0] = nullptr; }
        if (denoiseStates[1] != nullptr) { rnnoise_destroy (denoiseStates[1]); denoiseStates[1] = nullptr; }
    }

    void prepareToPlay (double sr, int) override
    {
        sampleRate = sr > 0.0 ? sr : 48000.0;
        isNative48k = std::abs (sampleRate - 48000.0) < 1.0;

        for (int ch = 0; ch < 2; ++ch)
        {
            if (denoiseStates[ch] != nullptr)
                rnnoise_init (denoiseStates[ch], nullptr);

            std::fill (inRing[ch].begin(), inRing[ch].end(), 0.0f);
            std::fill (outRing[ch].begin(), outRing[ch].end(), 0.0f);
            std::fill (resamp48kIn[ch].begin(), resamp48kIn[ch].end(), 0.0f);
            std::fill (resamp48kOut[ch].begin(), resamp48kOut[ch].end(), 0.0f);

            resamplerIn[ch].reset();
            resamplerOut[ch].reset();
        }

        inWritePos = 0;
        outWritePos = frameSize;
        outReadPos = 0;
        samplesInFrame = 0;

        currentVadGain = 1.0f;
        targetVadGain = 1.0f;
        samplesSincePush = 0;
        historyWriteIndex.store (0, std::memory_order_relaxed);

        smoothedAmount.reset (sampleRate, 0.020); // 20ms click-free ramp
        smoothedAmount.setCurrentAndTargetValue (juce::jlimit (0.0f, 1.0f, amountParam.get (100.0f) * 0.01f));

        // Report exact 10ms frame latency to DAW/Host for PDC (Plugin Delay Compensation)
        const int latency = isNative48k ? frameSize : (int) std::round (frameSize * (sampleRate / 48000.0));
        setLatencySamples (latency);
    }

    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();
        if (numChannels == 0 || numSamples == 0) return;

        const int activeChannels = juce::jmin (numChannels, 2);
        const float targetAmount = juce::jlimit (0.0f, 1.0f, amountParam.get (100.0f) * 0.01f);
        smoothedAmount.setTargetValue (targetAmount);

        const float vadThresh = juce::jlimit (0.0f, 1.0f, vadParam.get (0.0f) * 0.01f);
        const float vadSmoothCoeff = std::exp (-1.0f / (float) (0.025f * sampleRate)); // 25ms smooth ramp

        float blockDryPeak = 0.0f;
        float blockCleanPeak = 0.0f;
        float latestVad = liveVadProb.load (std::memory_order_relaxed);

        for (int i = 0; i < numSamples; ++i)
        {
            float amountNorm = smoothedAmount.getNextValue();

            // 1. Push incoming dry samples into inRing
            for (int ch = 0; ch < activeChannels; ++ch)
            {
                float dry = buffer.getSample (ch, i);
                inRing[ch][inWritePos] = dry;
                blockDryPeak = juce::jmax (blockDryPeak, std::abs (dry));
            }

            samplesInFrame++;

            // 2. Process RNNoise recurrent network on 480-sample frame completion
            if (samplesInFrame >= frameSize)
            {
                samplesInFrame = 0;
                float frameIn[frameSize];
                float frameOut[frameSize];

                for (int ch = 0; ch < activeChannels; ++ch)
                {
                    if (denoiseStates[ch] == nullptr) continue;

                    // Extract previous 480 samples in order from inRing
                    int readStart = (inWritePos - frameSize + 1 + ringCapacity) % ringCapacity;
                    for (int s = 0; s < frameSize; ++s)
                    {
                        int idx = (readStart + s) % ringCapacity;
                        frameIn[s] = inRing[ch][idx] * 32767.0f;
                    }

                    // Process through RNNoise recurrent neural network
                    float vad = rnnoise_process_frame (denoiseStates[ch], frameOut, frameIn);
                    if (ch == 0) latestVad = vad;

                    // Write normalized denoised frame into outRing
                    for (int s = 0; s < frameSize; ++s)
                    {
                        int oIdx = (outWritePos + s) % ringCapacity;
                        outRing[ch][oIdx] = frameOut[s] * (1.0f / 32767.0f);
                    }
                }

                outWritePos = (outWritePos + frameSize) % ringCapacity;

                // 3. Optional VAD secondary gate (only active when vadSensitivity > 0)
                if (vadThresh <= 0.001f)
                {
                    targetVadGain = 1.0f; // 100% natural RNNoise, zero gating/ducking
                }
                else
                {
                    if (latestVad >= vadThresh)
                        targetVadGain = 1.0f;
                    else
                        targetVadGain = juce::jlimit (0.25f, 1.0f, latestVad / vadThresh);
                }
            }

            // Smooth VAD gain with sample-rate aware ballistics
            currentVadGain = vadSmoothCoeff * currentVadGain + (1.0f - vadSmoothCoeff) * targetVadGain;

            // 4. Output with Phase-Coherent Wet/Dry Blend:
            // Crucial fix: dry signal must be delayed by EXACTLY frameSize samples (480)
            // to align precisely with RNNoise's frame processing delay.
            // Eliminates comb filtering / flanging completely!
            int delayedDryPos = (inWritePos - frameSize + ringCapacity) % ringCapacity;

            for (int ch = 0; ch < activeChannels; ++ch)
            {
                float delayedDry = inRing[ch][delayedDryPos];
                float clean = outRing[ch][outReadPos] * currentVadGain;

                float out = (1.0f - amountNorm) * delayedDry + amountNorm * clean;
                buffer.setSample (ch, i, out);
                blockCleanPeak = juce::jmax (blockCleanPeak, std::abs (out));
            }

            // Pass through any extra channels (ch >= 2) untouched
            for (int ch = activeChannels; ch < numChannels; ++ch)
            {
                // Leave buffer[ch][i] as-is
            }

            // Advance positions synchronously
            inWritePos = (inWritePos + 1) % ringCapacity;
            outReadPos = (outReadPos + 1) % ringCapacity;

            // Capture visualizer history at ~40Hz
            samplesSincePush++;
            if (samplesSincePush >= (int) (sampleRate / 40.0))
            {
                samplesSincePush = 0;
                int idx = historyWriteIndex.load (std::memory_order_relaxed);
                history[idx].dryLevel.store (blockDryPeak, std::memory_order_relaxed);
                history[idx].cleanLevel.store (blockCleanPeak, std::memory_order_relaxed);
                history[idx].vadProb.store (latestVad, std::memory_order_relaxed);
                historyWriteIndex.store ((idx + 1) % historySize, std::memory_order_relaxed);

                // Reset block peaks so each slice accurately represents its 25ms window!
                blockDryPeak = 0.0f;
                blockCleanPeak = 0.0f;
            }
        }

        liveVadProb.store (latestVad, std::memory_order_relaxed);
        liveDryLevel.store (blockDryPeak, std::memory_order_relaxed);
        liveCleanLevel.store (blockCleanPeak, std::memory_order_relaxed);
    }

    juce::AudioProcessorEditor* createEditor() override;

    float getLiveVad() const        { return liveVadProb.load (std::memory_order_relaxed); }
    float getLiveDryLevel() const   { return liveDryLevel.load (std::memory_order_relaxed); }
    float getLiveCleanLevel() const { return liveCleanLevel.load (std::memory_order_relaxed); }

    void getHistoryData (std::vector<MonitorPoint>& dest) const
    {
        dest.resize (historySize);
        int head = historyWriteIndex.load (std::memory_order_relaxed);
        for (int i = 0; i < historySize; ++i)
        {
            int idx = (head + i) % historySize;
            dest[i].dryLevel   = history[idx].dryLevel.load (std::memory_order_relaxed);
            dest[i].cleanLevel = history[idx].cleanLevel.load (std::memory_order_relaxed);
            dest[i].vadProb    = history[idx].vadProb.load (std::memory_order_relaxed);
        }
    }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        return {
            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "amount", 1 }, "Denoise Amount",
                juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f,
                juce::AudioParameterFloatAttributes().withLabel ("%")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "vadSensitivity", 1 }, "VAD Threshold",
                juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 0.0f,
                juce::AudioParameterFloatAttributes().withLabel ("%"))
        };
    }

    struct MonitorPointAtomic
    {
        std::atomic<float> dryLevel { 0.0f };
        std::atomic<float> cleanLevel { 0.0f };
        std::atomic<float> vadProb { 0.0f };
    };

    ParamRef amountParam;
    ParamRef vadParam;

    double sampleRate = 48000.0;
    bool isNative48k = true;
    DenoiseState* denoiseStates[2] = { nullptr, nullptr };

    juce::CatmullRomInterpolator resamplerIn[2];
    juce::CatmullRomInterpolator resamplerOut[2];

    std::vector<float> inRing[2];
    std::vector<float> outRing[2];
    std::vector<float> resamp48kIn[2];
    std::vector<float> resamp48kOut[2];

    int inWritePos = 0;
    int outWritePos = frameSize;
    int outReadPos = 0;
    int samplesInFrame = 0;

    float currentVadGain = 1.0f;
    float targetVadGain = 1.0f;

    juce::SmoothedValue<float> smoothedAmount;

    std::atomic<float> liveVadProb { 0.0f };
    std::atomic<float> liveDryLevel { 0.0f };
    std::atomic<float> liveCleanLevel { 0.0f };

    MonitorPointAtomic history[historySize];
    std::atomic<int> historyWriteIndex { 0 };
    int samplesSincePush = 0;
};

class NoiseSuppressionModuleEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    NoiseSuppressionModuleEditor (NoiseSuppressionModule& p, juce::AudioProcessorValueTreeState& vts)
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

        setupSlider (amountSlider, " %", UITheme::appleBlue);
        setupSlider (vadSlider,    " %", UITheme::appleGreen);

        amountAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "amount", amountSlider);
        vadAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "vadSensitivity", vadSlider);

        setSize (350, 270);
        startTimerHz (60);
    }

    ~NoiseSuppressionModuleEditor() override { stopTimer(); }

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
        g.drawText ("NOISE SUPPRESSION (RNNOISE)", header.withTrimmedLeft (14), juce::Justification::centredLeft);

        // VAD Speech Probability Badge
        float vad = module.getLiveVad();
        bool isSpeech = vad > 0.40f;
        auto badge = header.removeFromRight (115).toFloat().reduced (6.0f, 8.0f);
        juce::Colour vadCol = isSpeech ? UITheme::appleGreen : juce::Colour (0xffff9f0a);

        g.setColour (vadCol.withAlpha (0.28f));
        g.fillRoundedRectangle (badge, 3.0f);
        g.setColour (vadCol);
        g.drawRoundedRectangle (badge, 3.0f, 1.0f);
        g.setFont (UITheme::getFont (8.0f, true));
        g.drawText (isSpeech ? "VOICE " + juce::String ((int) (vad * 100.0f)) + "%" : "NOISE (MUTED)", badge, juce::Justification::centred);

        // --- REAL-TIME DUAL-STREAM MONITOR (RAW NOISE vs CLEAN VOICE) ---
        auto screenRect = juce::Rectangle<float> (16.0f, 44.0f, (float) getWidth() - 32.0f, 76.0f);
        g.setColour (juce::Colour (0xff111114));
        g.fillRoundedRectangle (screenRect, 4.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (screenRect, 4.0f, 0.8f);

        std::vector<NoiseSuppressionModule::MonitorPoint> pts;
        module.getHistoryData (pts);

        if (! pts.empty())
        {
            float stepX = screenRect.getWidth() / (float) (pts.size() - 1);
            juce::Path rawPath, cleanPath;
            bool started = false;

            for (size_t i = 0; i < pts.size(); ++i)
            {
                float x = screenRect.getX() + (float) i * stepX;
                float rawH = juce::jlimit (0.0f, 1.0f, pts[i].dryLevel * 1.5f) * (screenRect.getHeight() - 8.0f);
                float cleanH = juce::jlimit (0.0f, 1.0f, pts[i].cleanLevel * 1.5f) * (screenRect.getHeight() - 8.0f);

                float rawY = screenRect.getBottom() - rawH - 4.0f;
                float cleanY = screenRect.getBottom() - cleanH - 4.0f;

                if (! started)
                {
                    rawPath.startNewSubPath (x, rawY);
                    cleanPath.startNewSubPath (x, cleanY);
                    started = true;
                }
                else
                {
                    rawPath.lineTo (x, rawY);
                    cleanPath.lineTo (x, cleanY);
                }
            }

            // Raw input with noise (Red/Orange background)
            g.setColour (juce::Colour (0xffff453a).withAlpha (0.45f));
            g.strokePath (rawPath, juce::PathStrokeType (1.2f));

            // Clean denoised voice (Green foreground)
            g.setColour (UITheme::appleGreen);
            g.strokePath (cleanPath, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Legend
        g.setFont (UITheme::getFont (7.5f, true));
        g.setColour (juce::Colour (0xffff453a));
        g.drawText ("RAW IN (WITH NOISE)", juce::Rectangle<float> (screenRect.getX() + 6.0f, screenRect.getY() + 4.0f, 110.0f, 10.0f), juce::Justification::centredLeft);
        g.setColour (UITheme::appleGreen);
        g.drawText ("DENOISED SPEECH", juce::Rectangle<float> (screenRect.getRight() - 100.0f, screenRect.getY() + 4.0f, 94.0f, 10.0f), juce::Justification::centredRight);

        // Knob labels
        int colW = getWidth() / 2;
        int labelY = 126;
        g.setColour (UITheme::textSecondary);
        g.setFont (UITheme::getFont (9.0f, true));
        g.drawText ("SUPPRESSION AMOUNT", colW * 0, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("VAD SENSITIVITY",    colW * 1, labelY, colW, 14, juce::Justification::centred);
    }

    void resized() override
    {
        int colW = getWidth() / 2;
        int knobY = 144;
        int knobSize = 66;

        amountSlider.setBounds (colW * 0 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        vadSlider.setBounds    (colW * 1 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
    }

private:
    NoiseSuppressionModule& module;
    juce::Slider amountSlider, vadSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttach, vadAttach;
};

inline juce::AudioProcessorEditor* NoiseSuppressionModule::createEditor()
{
    return new NoiseSuppressionModuleEditor (*this, apvts);
}
