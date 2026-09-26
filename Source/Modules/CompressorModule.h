#pragma once
#include "../Graph/ModuleProcessor.h"
#include "../UITheme.h"
#include <cmath>
#include <vector>

class CompressorModuleEditor;

class CompressorModule : public ModuleProcessor
{
public:
    struct HistoryPoint
    {
        float inDb;
        float outDb;
        float grDb;
    };
    static constexpr int historyLength = 200;

    CompressorModule()
        : ModuleProcessor ("Compressor", createLayout())
    {
        threshParam  = getModuleParam ("threshold", -24.0f);
        ratioParam   = getModuleParam ("ratio", 4.0f);
        attackParam  = getModuleParam ("attack", 15.0f);
        releaseParam = getModuleParam ("release", 150.0f);
        kneeParam    = getModuleParam ("knee", 6.0f);
        makeupParam  = getModuleParam ("makeup", 0.0f);

        for (int i = 0; i < historyLength; ++i)
            history[i] = { -80.0f, -80.0f, 0.0f };
    }

    void prepareToPlay (double sr, int) override
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;
        envDb = -80.0f;
        samplesSincePush = 0;
    }

    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();
        if (numChannels == 0 || numSamples == 0) return;

        float threshold = threshParam.get (-24.0f);
        float ratio     = juce::jmax (1.0f, ratioParam.get (4.0f));
        float attMs     = juce::jmax (0.5f, attackParam.get (15.0f));
        float relMs     = juce::jmax (10.0f, releaseParam.get (120.0f));
        float knee      = juce::jmax (0.0f, kneeParam.get (6.0f));
        float makeupDb  = makeupParam.get (0.0f);
        float makeupLin = juce::Decibels::decibelsToGain (makeupDb);

        float attCoeff = std::exp (-1.0f / (float) ((attMs * 0.001f) * sampleRate));
        float relCoeff = std::exp (-1.0f / (float) ((relMs * 0.001f) * sampleRate));
        float kneeHalf = knee * 0.5f;

        float blockInPeak  = 0.0f;
        float blockOutPeak = 0.0f;
        float maxGrDb      = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float peak = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                peak = juce::jmax (peak, std::abs (buffer.getSample (ch, i)));

            blockInPeak = juce::jmax (blockInPeak, peak);

            float inDb = juce::Decibels::gainToDecibels (peak, -90.0f);
            float overshoot = inDb - threshold;
            float targetGrDb = 0.0f;

            if (knee > 0.001f)
            {
                if (overshoot <= -kneeHalf)
                    targetGrDb = 0.0f;
                else if (overshoot >= kneeHalf)
                    targetGrDb = overshoot * (1.0f - 1.0f / ratio);
                else
                {
                    float k = overshoot + kneeHalf;
                    targetGrDb = (k * k / (2.0f * knee)) * (1.0f - 1.0f / ratio);
                }
            }
            else
            {
                if (overshoot > 0.0f)
                    targetGrDb = overshoot * (1.0f - 1.0f / ratio);
            }

            // Smooth envelope detection in dB domain
            if (targetGrDb > envDb)
                envDb = attCoeff * envDb + (1.0f - attCoeff) * targetGrDb;
            else
                envDb = relCoeff * envDb + (1.0f - relCoeff) * targetGrDb;

            float gainReductionLin = juce::Decibels::decibelsToGain (-envDb);
            float finalGain = gainReductionLin * makeupLin;

            for (int ch = 0; ch < numChannels; ++ch)
            {
                float out = buffer.getSample (ch, i) * finalGain;
                buffer.setSample (ch, i, out);
                blockOutPeak = juce::jmax (blockOutPeak, std::abs (out));
            }

            maxGrDb = juce::jmax (maxGrDb, envDb);
        }

        liveGainReduction.store (maxGrDb, std::memory_order_relaxed);
        liveInputPeak.store  (blockInPeak, std::memory_order_relaxed);
        liveOutputPeak.store (blockOutPeak, std::memory_order_relaxed);

        samplesSincePush += numSamples;
        int interval = (int) (sampleRate / 60.0);
        if (samplesSincePush >= interval)
        {
            samplesSincePush = 0;
            float inDb  = juce::Decibels::gainToDecibels (blockInPeak, -80.0f);
            float outDb = juce::Decibels::gainToDecibels (blockOutPeak, -80.0f);
            int idx = historyWriteIndex.load (std::memory_order_relaxed);
            history[idx] = { inDb, outDb, maxGrDb };
            historyWriteIndex.store ((idx + 1) % historyLength, std::memory_order_relaxed);
        }
    }

    juce::AudioProcessorEditor* createEditor() override;

    float getLiveGainReduction() const { return liveGainReduction.load (std::memory_order_relaxed); }
    float getLiveInputPeak() const     { return liveInputPeak.load (std::memory_order_relaxed); }
    float getLiveOutputPeak() const    { return liveOutputPeak.load (std::memory_order_relaxed); }
    float getThresholdDb() const       { return threshParam.get (-24.0f); }
    float getRatio() const             { return ratioParam.get (4.0f); }
    float getKnee() const              { return kneeParam.get (6.0f); }

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
                juce::ParameterID { "threshold", 1 }, "Threshold",
                juce::NormalisableRange<float> (-60.0f, 0.0f, 0.5f), -24.0f,
                juce::AudioParameterFloatAttributes().withLabel ("dB")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "ratio", 1 }, "Ratio",
                juce::NormalisableRange<float> (1.0f, 20.0f, 0.1f, 0.5f), 4.0f,
                juce::AudioParameterFloatAttributes().withLabel (":1")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "attack", 1 }, "Attack",
                juce::NormalisableRange<float> (0.5f, 100.0f, 0.5f, 0.4f), 15.0f,
                juce::AudioParameterFloatAttributes().withLabel ("ms")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "release", 1 }, "Release",
                juce::NormalisableRange<float> (20.0f, 1000.0f, 1.0f, 0.4f), 150.0f,
                juce::AudioParameterFloatAttributes().withLabel ("ms")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "knee", 1 }, "Knee",
                juce::NormalisableRange<float> (0.0f, 18.0f, 0.5f), 6.0f,
                juce::AudioParameterFloatAttributes().withLabel ("dB")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "makeup", 1 }, "Makeup",
                juce::NormalisableRange<float> (0.0f, 24.0f, 0.5f), 0.0f,
                juce::AudioParameterFloatAttributes().withLabel ("dB"))
        };
    }

    ParamRef threshParam;
    ParamRef ratioParam;
    ParamRef attackParam;
    ParamRef releaseParam;
    ParamRef kneeParam;
    ParamRef makeupParam;

    double sampleRate = 44100.0;
    float envDb = -80.0f;
    int samplesSincePush = 0;

    std::atomic<float> liveGainReduction { 0.0f };
    std::atomic<float> liveInputPeak { 0.0f };
    std::atomic<float> liveOutputPeak { 0.0f };

    HistoryPoint history[historyLength];
    std::atomic<int> historyWriteIndex { 0 };
};

class CompressorModuleEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    CompressorModuleEditor (CompressorModule& p, juce::AudioProcessorValueTreeState& vts)
        : AudioProcessorEditor (&p), module (p)
    {
        auto setupSlider = [this] (juce::Slider& s, const juce::String& suffix, juce::Colour col)
        {
            s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 58, 18);
            s.setTextValueSuffix (suffix);
            s.setColour (juce::Slider::rotarySliderFillColourId, col);
            s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff1e1e24));
            s.setColour (juce::Slider::textBoxOutlineColourId, UITheme::strokeHairline);
            s.setColour (juce::Slider::textBoxTextColourId, UITheme::textPrimary);
            addAndMakeVisible (s);
        };

        setupSlider (threshSlider, " dB", UITheme::appleBlue);
        setupSlider (ratioSlider, ":1", UITheme::appleYellow);
        setupSlider (attackSlider, " ms", UITheme::textPrimary);
        setupSlider (releaseSlider, " ms", UITheme::textPrimary);
        setupSlider (kneeSlider, " dB", UITheme::textSecondary);
        setupSlider (makeupSlider, " dB", UITheme::appleGreen);

        threshAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "threshold", threshSlider);
        ratioAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "ratio", ratioSlider);
        attackAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "attack", attackSlider);
        releaseAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "release", releaseSlider);
        kneeAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "knee", kneeSlider);
        makeupAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "makeup", makeupSlider);

        setSize (380, 280);
        startTimerHz (60);
    }

    ~CompressorModuleEditor() override { stopTimer(); }

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
        g.drawText ("VOCAL DYNAMICS COMPRESSOR", header.withTrimmedLeft (14), juce::Justification::centredLeft);

        // Live Gain Reduction readout in header
        float grVal = module.getLiveGainReduction();
        auto grBadge = header.removeFromRight (86).toFloat().reduced (6.0f, 8.0f);
        bool compressing = grVal > 0.3f;
        g.setColour (compressing ? UITheme::appleRed.withAlpha (0.28f) : juce::Colour (0x15ffffff));
        g.fillRoundedRectangle (grBadge, 3.0f);
        g.setColour (compressing ? UITheme::appleRed : UITheme::textTertiary);
        g.drawRoundedRectangle (grBadge, 3.0f, 1.0f);
        g.setFont (UITheme::getFont (8.5f, true));
        g.drawText ("GR -" + juce::String (grVal, 1) + " dB", grBadge, juce::Justification::centred);

        // --- DYNAMIC VISUALIZER ROW (Scrolling Waveform + Soft-Knee Scope + VU) ---
        auto waveRect  = juce::Rectangle<float> (14.0f, 44.0f, 215.0f, 78.0f);
        auto kneeRect  = juce::Rectangle<float> (waveRect.getRight() + 8.0f, 44.0f, 84.0f, 78.0f);
        auto meterRect = juce::Rectangle<float> (kneeRect.getRight() + 8.0f, 44.0f, (float) getWidth() - kneeRect.getRight() - 22.0f, 78.0f);

        // 1. Scrolling Waveform Window
        g.setColour (juce::Colour (0xff111114));
        g.fillRoundedRectangle (waveRect, 4.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (waveRect, 4.0f, 0.8f);

        // dB Grid
        g.setColour (juce::Colour (0x10ffffff));
        for (float db : { -12.0f, -24.0f, -36.0f, -48.0f })
        {
            float norm = 1.0f - (db / -60.0f);
            float gy = waveRect.getY() + (1.0f - norm) * waveRect.getHeight();
            g.drawHorizontalLine ((int) gy, waveRect.getX(), waveRect.getRight());
        }

        // Draw Dual-Trace History (In vs Out)
        std::vector<CompressorModule::HistoryPoint> pts;
        module.getHistory (pts);

        if (! pts.empty())
        {
            float stepX = waveRect.getWidth() / (float) (pts.size() - 1);
            juce::Path inPath, outPath;
            bool started = false;

            for (size_t i = 0; i < pts.size(); ++i)
            {
                float x = waveRect.getX() + (float) i * stepX;
                float inNorm  = juce::jlimit (0.0f, 1.0f, (pts[i].inDb + 60.0f) / 60.0f);
                float outNorm = juce::jlimit (0.0f, 1.0f, (pts[i].outDb + 60.0f) / 60.0f);

                float iy = waveRect.getBottom() - inNorm * (waveRect.getHeight() - 4.0f) - 2.0f;
                float oy = waveRect.getBottom() - outNorm * (waveRect.getHeight() - 4.0f) - 2.0f;

                if (! started)
                {
                    inPath.startNewSubPath (x, iy);
                    outPath.startNewSubPath (x, oy);
                    started = true;
                }
                else
                {
                    inPath.lineTo (x, iy);
                    outPath.lineTo (x, oy);
                }
            }

            // Input trace (dim gray/blue)
            g.setColour (juce::Colour (0x4038bdf8));
            g.strokePath (inPath, juce::PathStrokeType (1.2f));

            // Output trace (green)
            g.setColour (UITheme::appleGreen.withAlpha (0.90f));
            g.strokePath (outPath, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Top-down Gain Reduction Shading
            if (grVal > 0.3f)
            {
                float grNorm = juce::jlimit (0.0f, 1.0f, grVal / 24.0f);
                float grH = grNorm * waveRect.getHeight();
                g.setColour (UITheme::appleRed.withAlpha (0.22f));
                g.fillRect (waveRect.withHeight (grH));
            }
        }

        // Threshold Line on Waveform
        float thDb = module.getThresholdDb();
        float thNorm = juce::jlimit (0.0f, 1.0f, (thDb + 60.0f) / 60.0f);
        float thY = waveRect.getBottom() - thNorm * (waveRect.getHeight() - 4.0f) - 2.0f;
        g.setColour (UITheme::appleBlue.withAlpha (0.80f));
        float dashes[] = { 3.0f, 2.0f };
        g.drawDashedLine (juce::Line<float> (waveRect.getX(), thY, waveRect.getRight(), thY), dashes, 2, 1.0f);

        // 2. Soft-Knee Transfer Function Scope
        g.setColour (juce::Colour (0xff111114));
        g.fillRoundedRectangle (kneeRect, 4.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (kneeRect, 4.0f, 0.8f);

        // Grid diagonal (1:1 line)
        g.setColour (juce::Colour (0x15ffffff));
        g.drawLine (kneeRect.getX(), kneeRect.getBottom(), kneeRect.getRight(), kneeRect.getY(), 1.0f);

        // Transfer curve
        float ratio = module.getRatio();
        float knee = module.getKnee();
        float kneeHalf = knee * 0.5f;

        juce::Path kneeCurve;
        bool kStarted = false;
        for (int s = 0; s <= 30; ++s)
        {
            float inD = -60.0f + 60.0f * ((float) s / 30.0f);
            float overshoot = inD - thDb;
            float outD = inD;

            if (knee > 0.001f)
            {
                if (overshoot > kneeHalf)
                    outD = thDb + kneeHalf + (overshoot - kneeHalf) / ratio;
                else if (overshoot > -kneeHalf)
                {
                    float k = overshoot + kneeHalf;
                    outD = inD - (k * k / (2.0f * knee)) * (1.0f - 1.0f / ratio);
                }
            }
            else
            {
                if (overshoot > 0.0f)
                    outD = thDb + overshoot / ratio;
            }

            float cx = kneeRect.getX() + ((inD + 60.0f) / 60.0f) * kneeRect.getWidth();
            float cy = kneeRect.getBottom() - ((outD + 60.0f) / 60.0f) * kneeRect.getHeight();

            if (! kStarted) { kneeCurve.startNewSubPath (cx, cy); kStarted = true; }
            else            { kneeCurve.lineTo (cx, cy); }
        }

        g.setColour (UITheme::appleYellow);
        g.strokePath (kneeCurve, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Animated Vocal Ball on Knee Curve
        float inPeakLin = module.getLiveInputPeak();
        float inPeakDb = juce::Decibels::gainToDecibels (inPeakLin, -60.0f);
        if (inPeakDb > -58.0f)
        {
            float ballNormX = juce::jlimit (0.0f, 1.0f, (inPeakDb + 60.0f) / 60.0f);
            float bx = kneeRect.getX() + ballNormX * kneeRect.getWidth();
            float by = kneeRect.getBottom() - ballNormX * kneeRect.getHeight(); // approximate on curve
            g.setColour (juce::Colour (0x50ffd60a));
            g.fillEllipse (bx - 4.0f, by - 4.0f, 8.0f, 8.0f);
            g.setColour (juce::Colours::white);
            g.fillEllipse (bx - 2.0f, by - 2.0f, 4.0f, 4.0f);
        }

        // 3. Mini Level & GR Meter Strip
        g.setColour (juce::Colour (0xff111114));
        g.fillRoundedRectangle (meterRect, 4.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (meterRect, 4.0f, 0.8f);

        float barW = (meterRect.getWidth() - 10.0f) / 3.0f;
        auto inBar  = juce::Rectangle<float> (meterRect.getX() + 3.0f, meterRect.getY() + 14.0f, barW, meterRect.getHeight() - 18.0f);
        auto outBar = juce::Rectangle<float> (inBar.getRight() + 2.0f, meterRect.getY() + 14.0f, barW, meterRect.getHeight() - 18.0f);
        auto grBar  = juce::Rectangle<float> (outBar.getRight() + 2.0f, meterRect.getY() + 14.0f, barW, meterRect.getHeight() - 18.0f);

        // Fills
        g.setColour (UITheme::appleBlue);
        g.fillRect (inBar.removeFromBottom (inBar.getHeight() * juce::jlimit (0.0f, 1.0f, module.getLiveInputPeak())));

        g.setColour (UITheme::appleGreen);
        g.fillRect (outBar.removeFromBottom (outBar.getHeight() * juce::jlimit (0.0f, 1.0f, module.getLiveOutputPeak())));

        g.setColour (UITheme::appleRed);
        g.fillRect (grBar.removeFromTop (grBar.getHeight() * juce::jlimit (0.0f, 1.0f, grVal / 20.0f)));

        g.setColour (UITheme::textTertiary);
        g.setFont (UITheme::getFont (6.5f, true));
        g.drawText ("IN", juce::Rectangle<float> (meterRect.getX() + 2.0f, meterRect.getY() + 2.0f, barW + 1.0f, 10.0f), juce::Justification::centred);
        g.drawText ("OUT", juce::Rectangle<float> (meterRect.getX() + 2.0f + barW + 2.0f, meterRect.getY() + 2.0f, barW + 1.0f, 10.0f), juce::Justification::centred);
        g.drawText ("GR", juce::Rectangle<float> (meterRect.getX() + 2.0f + (barW + 2.0f) * 2.0f, meterRect.getY() + 2.0f, barW + 1.0f, 10.0f), juce::Justification::centred);

        // Knob labels
        int colW = getWidth() / 6;
        int labelY = 130;
        g.setColour (UITheme::textSecondary);
        g.setFont (UITheme::getFont (8.5f, true));
        g.drawText ("THRESH", colW * 0, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("RATIO",  colW * 1, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("ATTACK", colW * 2, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("RELEASE",colW * 3, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("KNEE",   colW * 4, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("MAKEUP", colW * 5, labelY, colW, 14, juce::Justification::centred);
    }

    void resized() override
    {
        int colW = getWidth() / 6;
        int knobY = 148;
        int knobSize = 58;

        threshSlider.setBounds  (colW * 0 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        ratioSlider.setBounds   (colW * 1 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        attackSlider.setBounds  (colW * 2 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        releaseSlider.setBounds (colW * 3 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        kneeSlider.setBounds    (colW * 4 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        makeupSlider.setBounds  (colW * 5 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
    }

private:
    CompressorModule& module;
    juce::Slider threshSlider, ratioSlider, attackSlider, releaseSlider, kneeSlider, makeupSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> threshAttach, ratioAttach, attackAttach, releaseAttach, kneeAttach, makeupAttach;
};

inline juce::AudioProcessorEditor* CompressorModule::createEditor()
{
    return new CompressorModuleEditor (*this, apvts);
}
