#pragma once
#include "../Graph/ModuleProcessor.h"
#include "../UITheme.h"
#include <cmath>
#include <vector>

class PhantomSubModuleEditor;

class PhantomSubModule : public ModuleProcessor
{
public:
    static constexpr int historyLength = 200;

    struct WavePoint
    {
        float drySample;
        float subSample;
    };

    PhantomSubModule()
        : ModuleProcessor ("Phantom Sub", createLayout())
    {
        amountParam    = getModuleParam ("amount", 45.0f);
        freqParam      = getModuleParam ("freq", 90.0f);
        warmthParam    = getModuleParam ("warmth", 40.0f);
        lowCutParam    = getModuleParam ("lowCut", 80.0f);
        autoTrackParam = getModuleParam ("autoTrack", 1.0f);

        for (int i = 0; i < historyLength; ++i)
            history[i] = { 0.0f, 0.0f };
    }

    void prepareToPlay (double sr, int) override
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;
        resetFilters();
        subPhase = 0.0f;
        subEnv = 0.0f;
        trackedFreq = 100.0f;
        lastMid = 0.0f;
        lastZeroCrossSample = 0;
        sampleCount = 0;
        samplesSincePush = 0;
    }

    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();
        if (numChannels == 0 || numSamples == 0) return;

        float subAmountNorm = juce::jlimit (0.0f, 1.0f, amountParam.get (45.0f) * 0.01f);
        float manualFreq    = juce::jlimit (50.0f, 160.0f, freqParam.get (90.0f));
        float warmthNorm    = juce::jlimit (0.0f, 1.0f, warmthParam.get (40.0f) * 0.01f);
        float lowCutFreq    = juce::jlimit (20.0f, 180.0f, lowCutParam.get (80.0f));
        bool autoTrack      = autoTrackParam.get (1.0f) > 0.5f;

        updateLowCutCoeffs (lowCutFreq);
        updateBandpassCoeffs (350.0f, 1.2f);
        updateSubLpCoeffs (140.0f);

        // Fast envelope attack (2ms), vocal-matched release (45ms)
        float attCoeff = std::exp (-1.0f / (float) (0.002f * sampleRate));
        float relCoeff = std::exp (-1.0f / (float) (0.045f * sampleRate));

        float blockSubMax = 0.0f;
        float blockDryMax = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            sampleCount++;
            float drySum = 0.0f;
            for (int ch = 0; ch < numChannels; ++ch)
                drySum += buffer.getSample (ch, i);
            drySum /= (float) numChannels;

            blockDryMax = juce::jmax (blockDryMax, std::abs (drySum));

            // 1. Mid-Band Formant Filter for Pitch Tracking (200 Hz - 800 Hz)
            float mid = bpB0 * drySum + bpB1 * bpX1 + bpB2 * bpX2
                        - bpA1 * bpY1 - bpA2 * bpY2;
            bpX2 = bpX1;
            bpX1 = drySum;
            bpY2 = bpY1;
            bpY1 = mid;

            // Zero-crossing pitch tracking on mid formants — compares the freshly
            // computed sample to the PREVIOUS filtered sample (lastMid), not to
            // bpX1: bpX1 was already overwritten with the raw input two lines above,
            // so the old check ("mid > 0 && bpX1 <= 0") was comparing a filtered
            // sample against raw input — not a zero-crossing test at all. That made
            // pitch tracking fire on essentially arbitrary sample pairs, producing an
            // erratic tracked frequency and an out-of-tune, warbling synthesized sub.
            if (mid > 0.0f && lastMid <= 0.0f)
            {
                int periodSamples = sampleCount - lastZeroCrossSample;
                lastZeroCrossSample = sampleCount;
                if (periodSamples > 15 && periodSamples < 500)
                {
                    float instantFreq = (float) sampleRate / (float) periodSamples;
                    // Sub-octave division: vocal fundamental is typically between 80 Hz and 240 Hz
                    while (instantFreq > 140.0f) instantFreq *= 0.5f;
                    if (instantFreq >= 50.0f && instantFreq <= 140.0f)
                    {
                        trackedFreq = trackedFreq * 0.92f + instantFreq * 0.08f;
                    }
                }
            }
            lastMid = mid;

            // 2. Voiced Speech Detection & Envelope Follower
            float absMid = std::abs (mid);
            if (absMid > subEnv)
                subEnv = attCoeff * subEnv + (1.0f - attCoeff) * absMid;
            else
                subEnv = relCoeff * subEnv + (1.0f - relCoeff) * absMid;

            // Gate out unvoiced hiss and background noise (threshold at 0.015)
            float voicedWeight = juce::jlimit (0.0f, 1.0f, (subEnv - 0.015f) * 18.0f);

            // 3. Phase-Locked Sub-Bass Synthesis
            float currentSubFreq = autoTrack ? trackedFreq : manualFreq;
            float phaseDelta = (2.0f * juce::MathConstants<float>::pi * currentSubFreq) / (float) sampleRate;
            subPhase += phaseDelta;
            if (subPhase >= 2.0f * juce::MathConstants<float>::pi)
                subPhase -= 2.0f * juce::MathConstants<float>::pi;

            float pureSubSine = std::sin (subPhase);

            // Add warm 2nd-harmonic saturation for warmth
            float warmSub = pureSubSine;
            if (warmthNorm > 0.01f)
            {
                float saturated = pureSubSine + 0.35f * warmthNorm * (pureSubSine * pureSubSine - 0.5f);
                warmSub = (1.0f - warmthNorm * 0.5f) * pureSubSine + warmthNorm * 0.5f * saturated;
            }

            // Amplitude modulate with voiced vocal envelope
            float rawSub = warmSub * subEnv * voicedWeight * subAmountNorm * 2.8f;

            // 4. Steep Low-Pass Filter on synthesized sub (keeps it strictly sub-bass)
            float filteredSub = lpB0 * rawSub + lpB1 * lpX1 + lpB2 * lpX2
                                - lpA1 * lpY1 - lpA2 * lpY2;
            lpX2 = lpX1;
            lpX1 = rawSub;
            lpY2 = lpY1;
            lpY1 = filteredSub;

            blockSubMax = juce::jmax (blockSubMax, std::abs (filteredSub));

            // 5. Clean Dry Low-Cut and Mix
            for (int ch = 0; ch < numChannels; ++ch)
            {
                int fIdx = ch < 2 ? ch : 0;
                float in = buffer.getSample (ch, i);
                float cleanedDry = lcB0 * in + lcB1 * lcX1[fIdx] + lcB2 * lcX2[fIdx]
                                   - lcA1 * lcY1[fIdx] - lcA2 * lcY2[fIdx];
                lcX2[fIdx] = lcX1[fIdx];
                lcX1[fIdx] = in;
                lcY2[fIdx] = lcY1[fIdx];
                lcY1[fIdx] = cleanedDry;

                buffer.setSample (ch, i, cleanedDry + filteredSub);
            }

            // Oscilloscope buffer capture
            samplesSincePush++;
            if (samplesSincePush >= (int) (sampleRate / 600.0)) // 600 Hz capture rate for smooth wave
            {
                samplesSincePush = 0;
                int idx = historyWriteIndex.load (std::memory_order_relaxed);
                history[idx] = { drySum, filteredSub };
                historyWriteIndex.store ((idx + 1) % historyLength, std::memory_order_relaxed);
            }
        }

        liveSubLevel.store (blockSubMax, std::memory_order_relaxed);
        liveDryLevel.store (blockDryMax, std::memory_order_relaxed);
        liveTrackedPitch.store (trackedFreq, std::memory_order_relaxed);
    }

    juce::AudioProcessorEditor* createEditor() override;

    float getLiveSubLevel() const    { return liveSubLevel.load (std::memory_order_relaxed); }
    float getLiveDryLevel() const    { return liveDryLevel.load (std::memory_order_relaxed); }
    float getTrackedPitch() const    { return liveTrackedPitch.load (std::memory_order_relaxed); }
    float getTargetFreq() const      { return freqParam.get (90.0f); }
    bool  isAutoTrackEnabled() const { return autoTrackParam.get (1.0f) > 0.5f; }

    void getWaveHistory (std::vector<WavePoint>& dest) const
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
                juce::ParameterID { "amount", 1 }, "Sub Amount",
                juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 45.0f,
                juce::AudioParameterFloatAttributes().withLabel ("%")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "freq", 1 }, "Target Pitch",
                juce::NormalisableRange<float> (50.0f, 150.0f, 1.0f, 0.5f), 90.0f,
                juce::AudioParameterFloatAttributes().withLabel ("Hz")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "warmth", 1 }, "Sub Warmth",
                juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 40.0f,
                juce::AudioParameterFloatAttributes().withLabel ("%")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "lowCut", 1 }, "Clean Low Cut",
                juce::NormalisableRange<float> (20.0f, 160.0f, 1.0f, 0.4f), 80.0f,
                juce::AudioParameterFloatAttributes().withLabel ("Hz")),

            std::make_unique<juce::AudioParameterBool> (
                juce::ParameterID { "autoTrack", 1 }, "Auto Pitch Track", true)
        };
    }

    void resetFilters()
    {
        bpX1 = bpX2 = bpY1 = bpY2 = 0.0f;
        lpX1 = lpX2 = lpY1 = lpY2 = 0.0f;
        for (int ch = 0; ch < 2; ++ch)
            lcX1[ch] = lcX2[ch] = lcY1[ch] = lcY2[ch] = 0.0f;
    }

    void updateBandpassCoeffs (float freq, float q)
    {
        float w0 = 2.0f * juce::MathConstants<float>::pi * (freq / (float) sampleRate);
        float alpha = std::sin (w0) / (2.0f * q);
        float a0 = 1.0f + alpha;
        bpB0 = (alpha) / a0;
        bpB1 = 0.0f;
        bpB2 = (-alpha) / a0;
        bpA1 = (-2.0f * std::cos (w0)) / a0;
        bpA2 = (1.0f - alpha) / a0;
    }

    void updateSubLpCoeffs (float freq)
    {
        float w0 = 2.0f * juce::MathConstants<float>::pi * (freq / (float) sampleRate);
        float cosw0 = std::cos (w0);
        float alpha = std::sin (w0) / (2.0f * 0.7071f);
        float a0 = 1.0f + alpha;
        lpB0 = ((1.0f - cosw0) * 0.5f) / a0;
        lpB1 = (1.0f - cosw0) / a0;
        lpB2 = ((1.0f - cosw0) * 0.5f) / a0;
        lpA1 = (-2.0f * cosw0) / a0;
        lpA2 = (1.0f - alpha) / a0;
    }

    void updateLowCutCoeffs (float freq)
    {
        float w0 = 2.0f * juce::MathConstants<float>::pi * (freq / (float) sampleRate);
        float cosw0 = std::cos (w0);
        float alpha = std::sin (w0) / (2.0f * 0.7071f);
        float a0 = 1.0f + alpha;
        lcB0 = ((1.0f + cosw0) * 0.5f) / a0;
        lcB1 = (-(1.0f + cosw0)) / a0;
        lcB2 = ((1.0f + cosw0) * 0.5f) / a0;
        lcA1 = (-2.0f * cosw0) / a0;
        lcA2 = (1.0f - alpha) / a0;
    }

    ParamRef amountParam;
    ParamRef freqParam;
    ParamRef warmthParam;
    ParamRef lowCutParam;
    ParamRef autoTrackParam;

    double sampleRate = 44100.0;

    // Filters
    float bpB0 = 0.0f, bpB1 = 0.0f, bpB2 = 0.0f, bpA1 = 0.0f, bpA2 = 0.0f;
    float bpX1 = 0.0f, bpX2 = 0.0f, bpY1 = 0.0f, bpY2 = 0.0f;

    float lpB0 = 0.0f, lpB1 = 0.0f, lpB2 = 0.0f, lpA1 = 0.0f, lpA2 = 0.0f;
    float lpX1 = 0.0f, lpX2 = 0.0f, lpY1 = 0.0f, lpY2 = 0.0f;

    float lcB0 = 1.0f, lcB1 = 0.0f, lcB2 = 0.0f, lcA1 = 0.0f, lcA2 = 0.0f;
    float lcX1[2] = {}, lcX2[2] = {}, lcY1[2] = {}, lcY2[2] = {};

    float subPhase = 0.0f;
    float subEnv = 0.0f;
    float trackedFreq = 100.0f;
    float lastMid = 0.0f;
    int lastZeroCrossSample = 0;
    int sampleCount = 0;
    int samplesSincePush = 0;

    std::atomic<float> liveSubLevel { 0.0f };
    std::atomic<float> liveDryLevel { 0.0f };
    std::atomic<float> liveTrackedPitch { 100.0f };

    WavePoint history[historyLength];
    std::atomic<int> historyWriteIndex { 0 };
};

class PhantomSubModuleEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    PhantomSubModuleEditor (PhantomSubModule& p, juce::AudioProcessorValueTreeState& vts)
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

        setupSlider (amountSlider, " %", juce::Colour (0xffbf5af2)); // Studio Purple
        setupSlider (freqSlider,   " Hz", UITheme::appleYellow);
        setupSlider (warmthSlider, " %", juce::Colour (0xffff9f0a));
        setupSlider (lowCutSlider, " Hz", UITheme::appleBlue);

        autoTrackBtn.setButtonText ("Auto Pitch Track");
        autoTrackBtn.setClickingTogglesState (true);
        autoTrackBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffbf5af2));
        addAndMakeVisible (autoTrackBtn);

        amountAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "amount", amountSlider);
        freqAttach      = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "freq", freqSlider);
        warmthAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "warmth", warmthSlider);
        lowCutAttach    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "lowCut", lowCutSlider);
        autoTrackAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (vts, "autoTrack", autoTrackBtn);

        setSize (370, 290);
        startTimerHz (60);
    }

    ~PhantomSubModuleEditor() override { stopTimer(); }

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
        g.drawText ("PHANTOM SUB (BASS RECONSTRUCTOR)", header.withTrimmedLeft (14), juce::Justification::centredLeft);

        // Tracked Pitch Badge
        float pitch = module.getTrackedPitch();
        float subLvl = module.getLiveSubLevel();
        bool active = subLvl > 0.04f;
        auto badge = header.removeFromRight (105).toFloat().reduced (6.0f, 8.0f);
        g.setColour (active ? juce::Colour (0xffbf5af2).withAlpha (0.28f) : juce::Colour (0x15ffffff));
        g.fillRoundedRectangle (badge, 3.0f);
        g.setColour (active ? juce::Colour (0xffbf5af2) : UITheme::textTertiary);
        g.drawRoundedRectangle (badge, 3.0f, 1.0f);
        g.setFont (UITheme::getFont (8.0f, true));

        juce::String pitchText = module.isAutoTrackEnabled() ? "TRACK " + juce::String ((int) pitch) + " Hz" : "FIXED " + juce::String ((int) module.getTargetFreq()) + " Hz";
        g.drawText (pitchText, badge, juce::Justification::centred);

        // --- REAL-TIME DUAL-WAVE OSCILLOSCOPE WINDOW ---
        auto screenRect = juce::Rectangle<float> (16.0f, 44.0f, (float) getWidth() - 54.0f, 82.0f);
        auto meterRect  = juce::Rectangle<float> (screenRect.getRight() + 8.0f, 44.0f, 14.0f, 82.0f);

        g.setColour (juce::Colour (0xff111114));
        g.fillRoundedRectangle (screenRect, 4.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (screenRect, 4.0f, 0.8f);

        // Center zero lines
        float midY = screenRect.getCentreY();
        g.setColour (juce::Colour (0x15ffffff));
        g.drawHorizontalLine ((int) midY, screenRect.getX(), screenRect.getRight());

        std::vector<PhantomSubModule::WavePoint> pts;
        module.getWaveHistory (pts);

        if (! pts.empty())
        {
            float stepX = screenRect.getWidth() / (float) (pts.size() - 1);
            juce::Path dryPath, subPath;
            bool started = false;

            for (size_t i = 0; i < pts.size(); ++i)
            {
                float x = screenRect.getX() + (float) i * stepX;
                float dryVal = juce::jlimit (-1.0f, 1.0f, pts[i].drySample * 1.5f);
                float subVal = juce::jlimit (-1.0f, 1.0f, pts[i].subSample * 2.2f);

                float dy = midY - dryVal * (screenRect.getHeight() * 0.38f);
                float sy = midY - subVal * (screenRect.getHeight() * 0.42f);

                if (! started)
                {
                    dryPath.startNewSubPath (x, dy);
                    subPath.startNewSubPath (x, sy);
                    started = true;
                }
                else
                {
                    dryPath.lineTo (x, dy);
                    subPath.lineTo (x, sy);
                }
            }

            // 1. Synthesized Sub-bass wave (Neon Purple, glowing)
            g.setColour (juce::Colour (0xffbf5af2).withAlpha (0.90f));
            g.strokePath (subPath, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Shaded sub foundation glow
            juce::Path filledSub = subPath;
            filledSub.lineTo (screenRect.getRight(), midY);
            filledSub.lineTo (screenRect.getX(), midY);
            filledSub.closeSubPath();
            g.setColour (juce::Colour (0x20bf5af2));
            g.fillPath (filledSub);

            // 2. Incoming thin microphone voice wave (Cyan, thinner)
            g.setColour (UITheme::appleBlue.withAlpha (0.65f));
            g.strokePath (dryPath, juce::PathStrokeType (1.2f));
        }

        // Oscilloscope Legend
        g.setFont (UITheme::getFont (7.5f, true));
        g.setColour (UITheme::appleBlue);
        g.drawText ("MIC VOICE", juce::Rectangle<float> (screenRect.getX() + 6.0f, screenRect.getY() + 4.0f, 60.0f, 10.0f), juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xffbf5af2));
        g.drawText ("SYNTHESIZED SUB", juce::Rectangle<float> (screenRect.getX() + 70.0f, screenRect.getY() + 4.0f, 90.0f, 10.0f), juce::Justification::centredLeft);

        // Real-Time Sub Level Meter
        g.setColour (juce::Colour (0xff111114));
        g.fillRoundedRectangle (meterRect, 3.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (meterRect, 3.0f, 0.8f);

        float subNorm = juce::jlimit (0.0f, 1.0f, subLvl * 1.8f);
        float meterH = meterRect.getHeight() * subNorm;
        g.setColour (juce::Colour (0xffbf5af2));
        g.fillRect (meterRect.removeFromBottom (meterH));

        // Knob labels
        int colW = getWidth() / 4;
        int labelY = 132;
        g.setColour (UITheme::textSecondary);
        g.setFont (UITheme::getFont (8.5f, true));
        g.drawText ("SUB AMOUNT", colW * 0, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("TARGET PITCH",colW * 1, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("WARMTH",    colW * 2, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("LOW CUT",   colW * 3, labelY, colW, 14, juce::Justification::centred);
    }

    void resized() override
    {
        int colW = getWidth() / 4;
        int knobY = 150;
        int knobSize = 64;

        amountSlider.setBounds (colW * 0 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        freqSlider.setBounds   (colW * 1 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        warmthSlider.setBounds (colW * 2 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        lowCutSlider.setBounds (colW * 3 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);

        autoTrackBtn.setBounds (getWidth() / 2 - 60, getHeight() - 26, 120, 20);
    }

private:
    PhantomSubModule& module;
    juce::Slider amountSlider, freqSlider, warmthSlider, lowCutSlider;
    juce::TextButton autoTrackBtn;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttach, freqAttach, warmthAttach, lowCutAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoTrackAttach;
};

inline juce::AudioProcessorEditor* PhantomSubModule::createEditor()
{
    return new PhantomSubModuleEditor (*this, apvts);
}
