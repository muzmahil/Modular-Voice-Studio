#pragma once
#include "../Graph/ModuleProcessor.h"
#include "../UITheme.h"
#include <cmath>
#include <vector>

class AECModuleEditor;

class AECModule : public ModuleProcessor
{
public:
    static constexpr int filterTaps = 256;
    static constexpr int erleHistorySize = 120;

    enum Status
    {
        Status_NoReference = 0,
        Status_Cancelling,
        Status_DoubleTalk,
        Status_Adapting
    };

    AECModule()
        : ModuleProcessor ("AEC", createLayout())
    {
        depthParam = getModuleParam ("depth", 100.0f);
        speedParam = getModuleParam ("speed", 55.0f);
        dtdParam   = getModuleParam ("dtdSensitivity", 45.0f);

        weights.assign (filterTaps, 0.0f);
        refBuffer.assign (filterTaps * 2, 0.0f);

        for (int i = 0; i < erleHistorySize; ++i)
            erleHistory[i] = 0.0f;
    }

    void prepareToPlay (double sr, int) override
    {
        sampleRate = sr > 0.0 ? sr : 48000.0;
        refWritePos = 0;
        std::fill (weights.begin(), weights.end(), 0.0f);
        std::fill (refBuffer.begin(), refBuffer.end(), 0.0f);
        refPower = 1e-4f;
    }

    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();
        if (numChannels == 0 || numSamples == 0) return;

        float depthNorm = juce::jlimit (0.0f, 1.0f, depthParam.get (100.0f) * 0.01f);
        float muBase    = juce::jlimit (0.005f, 0.25f, (speedParam.get (55.0f) * 0.01f) * 0.20f + 0.005f);
        float dtdSens   = juce::jlimit (0.1f, 2.5f, dtdParam.get (45.0f) * 0.02f + 0.1f);

        auto* micChannel = buffer.getWritePointer (0);
        const float* refChannel = (numChannels > 1) ? buffer.getReadPointer (1) : nullptr;

        float blockMicEnergy = 0.0f;
        float blockEchoEnergy = 0.0f;
        float blockRefEnergy = 0.0f;
        int doubleTalkCount = 0;

        for (int i = 0; i < numSamples; ++i)
        {
            float d = micChannel[i]; // Near-end mic signal (speech + room echo)
            float x = refChannel ? refChannel[i] : 0.0f; // Far-end speaker reference

            blockMicEnergy += d * d;
            blockRefEnergy += x * x;

            // Push reference into circular buffer
            refBuffer[refWritePos] = x;
            refBuffer[refWritePos + filterTaps] = x;

            // Update running reference power for normalization
            refPower = refPower * 0.999f + (x * x) * 0.001f;
            float normFactor = 1.0f / (refPower * (float) filterTaps + 1e-4f);

            // Compute estimated echo d_hat = sum(w_k * x[n-k])
            float d_hat = 0.0f;
            int rIdx = refWritePos + filterTaps;

            for (int k = 0; k < filterTaps; ++k)
                d_hat += weights[k] * refBuffer[rIdx - k];

            // Error signal (echo cancelled mic audio)
            float e = d - d_hat;

            // Geigel Double-Talk Detection (compare |d| vs max(|x|))
            float maxRef = std::abs (x);
            for (int k = 0; k < 64; k += 8)
                maxRef = juce::jmax (maxRef, std::abs (refBuffer[rIdx - k]));

            bool isDoubleTalk = (std::abs (d) > maxRef * dtdSens && std::abs (d) > 0.015f);
            if (isDoubleTalk) doubleTalkCount++;

            // Adaptation update (freeze weights during double-talk)
            if (! isDoubleTalk && maxRef > 0.001f)
            {
                float step = muBase * normFactor * e;
                for (int k = 0; k < filterTaps; ++k)
                {
                    weights[k] += step * refBuffer[rIdx - k];
                    // Leakage to prevent weight explosion
                    weights[k] *= 0.99998f;
                }
            }

            // Apply Echo Cancellation Depth
            float cleanMic = (1.0f - depthNorm) * d + depthNorm * e;
            micChannel[i] = cleanMic;
            blockEchoEnergy += cleanMic * cleanMic;

            // Mirror cancelled mic to Ch 2 if requested
            if (numChannels > 1)
                buffer.setSample (1, i, cleanMic);

            refWritePos = (refWritePos + 1) % filterTaps;
        }

        // Determine live status
        float avgRef = std::sqrt (blockRefEnergy / (float) numSamples);
        float erleDb = 0.0f;

        if (avgRef < 0.002f)
        {
            liveStatus.store (Status_NoReference, std::memory_order_relaxed);
            erleDb = 0.0f;
        }
        else if (doubleTalkCount > (numSamples / 4))
        {
            liveStatus.store (Status_DoubleTalk, std::memory_order_relaxed);
            erleDb = 12.0f;
        }
        else
        {
            liveStatus.store (Status_Cancelling, std::memory_order_relaxed);
            if (blockEchoEnergy > 1e-7f)
                erleDb = 10.0f * std::log10 ((blockMicEnergy + 1e-6f) / (blockEchoEnergy + 1e-6f));
            erleDb = juce::jlimit (0.0f, 45.0f, erleDb);
        }

        liveErle.store (erleDb, std::memory_order_relaxed);

        // Update visualizer stream
        samplesSincePush += numSamples;
        if (samplesSincePush >= (int) (sampleRate / 40.0))
        {
            samplesSincePush = 0;
            int idx = historyWritePos.load (std::memory_order_relaxed);
            erleHistory[idx] = erleDb;
            historyWritePos.store ((idx + 1) % erleHistorySize, std::memory_order_relaxed);
        }
    }

    juce::AudioProcessorEditor* createEditor() override;

    float getLiveErle() const { return liveErle.load (std::memory_order_relaxed); }
    Status getLiveStatus() const { return (Status) liveStatus.load (std::memory_order_relaxed); }

    void getWeightsSnapshot (std::vector<float>& dest) const
    {
        dest.resize (64);
        for (int i = 0; i < 64; ++i)
            dest[i] = weights[i * 4];
    }

    void getErleHistory (std::vector<float>& dest) const
    {
        dest.resize (erleHistorySize);
        int head = historyWritePos.load (std::memory_order_relaxed);
        for (int i = 0; i < erleHistorySize; ++i)
            dest[i] = erleHistory[(head + i) % erleHistorySize];
    }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        return {
            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "depth", 1 }, "Echo Reduction",
                juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f,
                juce::AudioParameterFloatAttributes().withLabel ("%")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "speed", 1 }, "Adaptation Speed",
                juce::NormalisableRange<float> (10.0f, 100.0f, 1.0f), 55.0f,
                juce::AudioParameterFloatAttributes().withLabel ("%")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "dtdSensitivity", 1 }, "DTD Sensitivity",
                juce::NormalisableRange<float> (10.0f, 100.0f, 1.0f), 45.0f,
                juce::AudioParameterFloatAttributes().withLabel ("%"))
        };
    }

    ParamRef depthParam;
    ParamRef speedParam;
    ParamRef dtdParam;

    double sampleRate = 48000.0;
    std::vector<float> weights;
    std::vector<float> refBuffer;
    int refWritePos = 0;
    float refPower = 1e-4f;

    std::atomic<float> liveErle { 0.0f };
    std::atomic<int> liveStatus { Status_NoReference };

    float erleHistory[erleHistorySize];
    std::atomic<int> historyWritePos { 0 };
    int samplesSincePush = 0;
};

class AECModuleEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    AECModuleEditor (AECModule& p, juce::AudioProcessorValueTreeState& vts)
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

        setupSlider (depthSlider, " %", UITheme::appleCyan);
        setupSlider (speedSlider, " %", UITheme::appleBlue);
        setupSlider (dtdSlider,   " %", UITheme::appleYellow);

        depthAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "depth", depthSlider);
        speedAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "speed", speedSlider);
        dtdAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "dtdSensitivity", dtdSlider);

        setSize (420, 280);
        startTimerHz (60);
    }

    ~AECModuleEditor() override { stopTimer(); }

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
        g.drawText ("ACOUSTIC ECHO CANCELLER (AEC)", header.withTrimmedLeft (14), juce::Justification::centredLeft);

        // Live Status Badge
        auto status = module.getLiveStatus();
        juce::Colour badgeCol = UITheme::textTertiary;
        juce::String badgeText = "NO REF (CH 2)";

        if (status == AECModule::Status_Cancelling)
        {
            badgeCol = UITheme::appleGreen;
            badgeText = "ECHO CANCEL ACTIVE";
        }
        else if (status == AECModule::Status_DoubleTalk)
        {
            badgeCol = juce::Colour (0xffff9f0a);
            badgeText = "DOUBLE-TALK (SAFE)";
        }
        else if (status == AECModule::Status_Adapting)
        {
            badgeCol = UITheme::appleBlue;
            badgeText = "ADAPTING ROOM...";
        }

        auto badge = header.removeFromRight (140).toFloat().reduced (6.0f, 8.0f);
        g.setColour (badgeCol.withAlpha (0.24f));
        g.fillRoundedRectangle (badge, 3.0f);
        g.setColour (badgeCol);
        g.drawRoundedRectangle (badge, 3.0f, 1.0f);
        g.setFont (UITheme::getFont (8.0f, true));
        g.drawText (badgeText, badge, juce::Justification::centred);

        // --- REAL-TIME ERLE & ACOUSTIC COUPLING SCOPE ---
        auto screenRect = juce::Rectangle<float> (16.0f, 44.0f, (float) getWidth() - 32.0f, 82.0f);
        g.setColour (juce::Colour (0xff111114));
        g.fillRoundedRectangle (screenRect, 4.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (screenRect, 4.0f, 0.8f);

        // Draw 64-tap Acoustic Coupling Impulse Response
        std::vector<float> w;
        module.getWeightsSnapshot (w);

        if (! w.empty())
        {
            float stepX = screenRect.getWidth() / (float) w.size();
            float midY = screenRect.getCentreY();
            juce::Path wave;
            bool started = false;

            for (size_t i = 0; i < w.size(); ++i)
            {
                float x = screenRect.getX() + (float) i * stepX;
                float y = midY - juce::jlimit (-1.0f, 1.0f, w[i] * 3.5f) * (screenRect.getHeight() * 0.40f);

                if (! started) { wave.startNewSubPath (x, y); started = true; }
                else           { wave.lineTo (x, y); }
            }

            g.setColour (UITheme::appleCyan.withAlpha (0.85f));
            g.strokePath (wave, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Live ERLE dB Readout
        float erle = module.getLiveErle();
        g.setColour (UITheme::appleGreen);
        g.setFont (UITheme::getFont (11.0f, true));
        juce::String erleStr = "-" + juce::String (erle, 1) + " dB ECHO SUPPRESSED";
        g.drawText (erleStr, juce::Rectangle<float> (screenRect.getX() + 8.0f, screenRect.getY() + 6.0f, 220.0f, 14.0f), juce::Justification::centredLeft);

        g.setColour (UITheme::textTertiary);
        g.setFont (UITheme::getFont (7.5f));
        g.drawText ("ROOM IMPULSE RESPONSE (NLMS 256-TAP)",
                    juce::Rectangle<float> (screenRect.getRight() - 190.0f, screenRect.getY() + 6.0f, 180.0f, 14.0f),
                    juce::Justification::centredRight);

        // Knob labels
        int colW = getWidth() / 3;
        int labelY = 136;
        g.setColour (UITheme::textSecondary);
        g.setFont (UITheme::getFont (9.0f, true));
        g.drawText ("ECHO REDUCTION", colW * 0, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("ADAPTATION SPEED", colW * 1, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("DTD SENSITIVITY", colW * 2, labelY, colW, 14, juce::Justification::centred);
    }

    void resized() override
    {
        int colW = getWidth() / 3;
        int knobY = 156;
        int knobSize = 64;

        depthSlider.setBounds (colW * 0 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        speedSlider.setBounds (colW * 1 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        dtdSlider.setBounds   (colW * 2 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
    }

private:
    AECModule& module;
    juce::Slider depthSlider, speedSlider, dtdSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> depthAttach, speedAttach, dtdAttach;
};

inline juce::AudioProcessorEditor* AECModule::createEditor()
{
    return new AECModuleEditor (*this, apvts);
}
