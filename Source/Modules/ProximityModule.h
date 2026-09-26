#pragma once
#include "../Graph/ModuleProcessor.h"
#include "../UITheme.h"
#include <cmath>

class ProximityModuleEditor;

class ProximityModule : public ModuleProcessor
{
public:
    ProximityModule()
        : ModuleProcessor ("Proximity", createLayout())
    {
        distParam      = getModuleParam ("distance", 12.0f);
        bodyParam      = getModuleParam ("body", 60.0f);
        dynamicParam   = getModuleParam ("dynamic", 45.0f);
        airLossParam   = getModuleParam ("airLoss", 35.0f);
        autoLevelParam = getModuleParam ("autoLevel", 1.0f);
    }

    void prepareToPlay (double sr, int) override
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;
        resetFilters();
        lowEnv = 0.0f;
    }

    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();
        if (numChannels == 0 || numSamples == 0) return;

        float distanceCm = juce::jlimit (2.0f, 50.0f, distParam.get (12.0f));
        float bodyAmt    = juce::jlimit (0.0f, 1.0f, bodyParam.get (60.0f) * 0.01f);
        float dynAmt     = juce::jlimit (0.0f, 1.0f, dynamicParam.get (45.0f) * 0.01f);
        float airAmt     = juce::jlimit (0.0f, 1.0f, airLossParam.get (35.0f) * 0.01f);
        bool autoLevel   = autoLevelParam.get (1.0f) > 0.5f;

        // Acoustic modeling:
        // Reference distance d0 = 15 cm.
        // Proximity low-frequency boost scales inversely with distance below d0.
        float distRatio = 15.0f / distanceCm;
        float baseLowDb = 20.0f * std::log10 (juce::jlimit (0.4f, 4.5f, distRatio)) * bodyAmt;
        baseLowDb = juce::jlimit (-6.0f, 14.0f, baseLowDb);

        // Air absorption roll-off above 8.5 kHz over distance
        float airDb = -juce::jlimit (0.0f, 9.0f, (distanceCm - 8.0f) * 0.18f * airAmt);

        // Auto-gain compensation: keeps loudness consistent as distance changes
        float compGainLin = 1.0f;
        if (autoLevel)
        {
            float normDist = (distanceCm - 2.0f) / 48.0f;
            float compDb = -baseLowDb * 0.35f + (normDist * 3.0f);
            compGainLin = juce::Decibels::decibelsToGain (compDb);
        }

        // Fast attack / medium release for low-end envelope detector
        float attCoeff = std::exp (-1.0f / (float) (0.005f * sampleRate)); // 5ms
        float relCoeff = std::exp (-1.0f / (float) (0.080f * sampleRate)); // 80ms
        float gainSmoothCoeff = std::exp (-1.0f / (float) (0.003f * sampleRate)); // keeps the low-shelf's coefficients from stepping

        // Dedicated low-pass just for the Dynamic Mud Guard's detector (~300 Hz, gentle
        // Q) — separate from the actual low-shelf signal path, so the detector reflects
        // real bass content instead of the whole broadband signal.
        updateDetectorLpCoeffs (300.0f);

        // airDb (and therefore the high-shelf coefficients) is constant for the whole
        // block — it was being recomputed every single sample for no reason.
        updateHighShelfCoeffs (8500.0f, airDb);

        float blockPeak = 0.0f;
        float maxDynamicClampDb = 0.0f;

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* channelData = buffer.getWritePointer (ch);
            int chIdx = ch < 2 ? ch : 0;

            for (int i = 0; i < numSamples; ++i)
            {
                float in = channelData[i];
                blockPeak = juce::jmax (blockPeak, std::abs (in));

                // 1. Measure LOW-FREQUENCY energy for dynamic proximity — previously this
                // tracked abs(in), the full broadband signal, so a loud sibilant "s" or a
                // shouted high note ducked the bass boost just as much as real bass
                // content did (unrelated, unwanted pumping). Detecting off a dedicated
                // low-passed version of the input instead means the clamp only reacts
                // to what it's actually meant to guard against.
                float lowOnly = lpDetB0 * in + lpDetB1 * lpDetX1[chIdx] + lpDetB2 * lpDetX2[chIdx]
                                - lpDetA1 * lpDetY1[chIdx] - lpDetA2 * lpDetY2[chIdx];
                lpDetX2[chIdx] = lpDetX1[chIdx];
                lpDetX1[chIdx] = in;
                lpDetY2[chIdx] = lpDetY1[chIdx];
                lpDetY1[chIdx] = lowOnly;

                float lowEnergy = std::abs (lowOnly);
                if (lowEnergy > lowEnv)
                    lowEnv = attCoeff * lowEnv + (1.0f - attCoeff) * lowEnergy;
                else
                    lowEnv = relCoeff * lowEnv + (1.0f - relCoeff) * lowEnergy;

                // Dynamic Anti-Boom: if voice gets loud and baseLowDb is high, dynamically clamp the boost!
                float dynamicClampDb = 0.0f;
                if (baseLowDb > 0.5f && dynAmt > 0.01f)
                {
                    float envDb = juce::Decibels::gainToDecibels (lowEnv, -60.0f);
                    float excess = juce::jmax (0.0f, envDb + 22.0f); // threshold at -22 dBFS
                    dynamicClampDb = juce::jmin (baseLowDb * 0.85f, excess * 0.45f * dynAmt);
                    maxDynamicClampDb = juce::jmax (maxDynamicClampDb, dynamicClampDb);
                }

                float effectiveLowDb = baseLowDb - dynamicClampDb;

                // Smoothed before feeding the filter design — same fix as DynamicEQModule:
                // effectiveLowDb can now move faster since the mud-guard detector above is
                // more responsive, and recomputing shelf sin/cos coefficients from an
                // abruptly-stepped gain every sample is what causes zipper noise.
                smoothedLowDb = smoothedLowDb * gainSmoothCoeff + effectiveLowDb * (1.0f - gainSmoothCoeff);
                updateLowShelfCoeffs (180.0f, smoothedLowDb);

                // Low shelf stage
                float ls = lsB0 * in + lsB1 * lsX1[chIdx] + lsB2 * lsX2[chIdx]
                           - lsA1 * lsY1[chIdx] - lsA2 * lsY2[chIdx];
                lsX2[chIdx] = lsX1[chIdx];
                lsX1[chIdx] = in;
                lsY2[chIdx] = lsY1[chIdx];
                lsY1[chIdx] = ls;

                // High shelf air absorption stage (8.5 kHz) — coefficients computed
                // once per block above now, since airDb doesn't change mid-block.
                float hs = hsB0 * ls + hsB1 * hsX1[chIdx] + hsB2 * hsX2[chIdx]
                           - hsA1 * hsY1[chIdx] - hsA2 * hsY2[chIdx];
                hsX2[chIdx] = hsX1[chIdx];
                hsX1[chIdx] = ls;
                hsY2[chIdx] = hsY1[chIdx];
                hsY1[chIdx] = hs;

                channelData[i] = hs * compGainLin;
            }
        }

        // Smooth visual tracker
        float prevPeak = liveVocalLevel.load (std::memory_order_relaxed);
        liveVocalLevel.store (blockPeak > prevPeak ? blockPeak : (prevPeak * 0.90f), std::memory_order_relaxed);
        liveDynamicClamp.store (maxDynamicClampDb, std::memory_order_relaxed);
    }

    juce::AudioProcessorEditor* createEditor() override;

    float getDistanceCm() const        { return distParam.get (12.0f); }
    void  setDistanceCm (float d)      { distParam.set (d); }
    float getBody() const              { return bodyParam.get (70.0f); }
    float getDynamic() const           { return dynamicParam.get (50.0f); }
    float getAirLoss() const           { return airLossParam.get (40.0f); }
    float getLiveVocalLevel() const    { return liveVocalLevel.load (std::memory_order_relaxed); }
    float getLiveDynamicClamp() const  { return liveDynamicClamp.load (std::memory_order_relaxed); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        return {
            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "distance", 1 }, "Distance",
                juce::NormalisableRange<float> (2.0f, 50.0f, 0.5f, 0.4f), 12.0f,
                juce::AudioParameterFloatAttributes().withLabel ("cm")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "body", 1 }, "Body Warmth",
                juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 70.0f,
                juce::AudioParameterFloatAttributes().withLabel ("%")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "dynamic", 1 }, "Dynamic Mud Guard",
                juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 50.0f,
                juce::AudioParameterFloatAttributes().withLabel ("%")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "airLoss", 1 }, "Air Loss",
                juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 40.0f,
                juce::AudioParameterFloatAttributes().withLabel ("%")),

            std::make_unique<juce::AudioParameterBool> (
                juce::ParameterID { "autoLevel", 1 }, "Auto Level", true)
        };
    }

    void resetFilters()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            lsX1[ch] = lsX2[ch] = lsY1[ch] = lsY2[ch] = 0.0f;
            hsX1[ch] = hsX2[ch] = hsY1[ch] = hsY2[ch] = 0.0f;
            lpDetX1[ch] = lpDetX2[ch] = lpDetY1[ch] = lpDetY2[ch] = 0.0f;
        }
        smoothedLowDb = 0.0f;
    }

    // Simple Butterworth-Q low-pass, used only to feed the Dynamic Mud Guard's envelope
    // detector — kept separate from the low-shelf signal path.
    void updateDetectorLpCoeffs (float freq)
    {
        float w0 = 2.0f * juce::MathConstants<float>::pi * (freq / (float) sampleRate);
        float cosw0 = std::cos (w0);
        float alpha = std::sin (w0) / (2.0f * 0.7071f);
        float a0 = 1.0f + alpha;
        lpDetB0 = ((1.0f - cosw0) * 0.5f) / a0;
        lpDetB1 = (1.0f - cosw0) / a0;
        lpDetB2 = ((1.0f - cosw0) * 0.5f) / a0;
        lpDetA1 = (-2.0f * cosw0) / a0;
        lpDetA2 = (1.0f - alpha) / a0;
    }

    void updateLowShelfCoeffs (float freq, float gainDb)
    {
        float A = std::pow (10.0f, gainDb / 40.0f);
        float w0 = 2.0f * juce::MathConstants<float>::pi * (freq / (float) sampleRate);
        float cosw0 = std::cos (w0);
        float sinw0 = std::sin (w0);
        float alpha = sinw0 * 0.5f * std::sqrt (2.0f);

        float a0 = (A + 1.0f) + (A - 1.0f) * cosw0 + 2.0f * std::sqrt (A) * alpha;
        lsB0 = (A * ((A + 1.0f) - (A - 1.0f) * cosw0 + 2.0f * std::sqrt (A) * alpha)) / a0;
        lsB1 = (2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosw0)) / a0;
        lsB2 = (A * ((A + 1.0f) - (A - 1.0f) * cosw0 - 2.0f * std::sqrt (A) * alpha)) / a0;
        lsA1 = (-2.0f * ((A - 1.0f) + (A + 1.0f) * cosw0)) / a0;
        lsA2 = ((A + 1.0f) + (A - 1.0f) * cosw0 - 2.0f * std::sqrt (A) * alpha) / a0;
    }

    void updateHighShelfCoeffs (float freq, float gainDb)
    {
        float A = std::pow (10.0f, gainDb / 40.0f);
        float w0 = 2.0f * juce::MathConstants<float>::pi * (freq / (float) sampleRate);
        float cosw0 = std::cos (w0);
        float sinw0 = std::sin (w0);
        float alpha = sinw0 * 0.5f * std::sqrt (2.0f);

        float a0 = (A + 1.0f) - (A - 1.0f) * cosw0 + 2.0f * std::sqrt (A) * alpha;
        hsB0 = (A * ((A + 1.0f) + (A - 1.0f) * cosw0 + 2.0f * std::sqrt (A) * alpha)) / a0;
        hsB1 = (-2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosw0)) / a0;
        hsB2 = (A * ((A + 1.0f) + (A - 1.0f) * cosw0 - 2.0f * std::sqrt (A) * alpha)) / a0;
        hsA1 = (2.0f * ((A - 1.0f) - (A + 1.0f) * cosw0)) / a0;
        hsA2 = ((A + 1.0f) - (A - 1.0f) * cosw0 - 2.0f * std::sqrt (A) * alpha) / a0;
    }

    ParamRef distParam;
    ParamRef bodyParam;
    ParamRef dynamicParam;
    ParamRef airLossParam;
    ParamRef autoLevelParam;

    double sampleRate = 44100.0;
    float lsB0 = 1.0f, lsB1 = 0.0f, lsB2 = 0.0f, lsA1 = 0.0f, lsA2 = 0.0f;
    float hsB0 = 1.0f, hsB1 = 0.0f, hsB2 = 0.0f, hsA1 = 0.0f, hsA2 = 0.0f;
    float lsX1[2] = {}, lsX2[2] = {}, lsY1[2] = {}, lsY2[2] = {};
    float hsX1[2] = {}, hsX2[2] = {}, hsY1[2] = {}, hsY2[2] = {};
    float lowEnv = 0.0f;
    float smoothedLowDb = 0.0f;

    // Dedicated low-pass for the Dynamic Mud Guard's detector (see updateDetectorLpCoeffs)
    float lpDetB0 = 0.0f, lpDetB1 = 0.0f, lpDetB2 = 0.0f, lpDetA1 = 0.0f, lpDetA2 = 0.0f;
    float lpDetX1[2] = {}, lpDetX2[2] = {}, lpDetY1[2] = {}, lpDetY2[2] = {};

    std::atomic<float> liveVocalLevel { 0.0f };
    std::atomic<float> liveDynamicClamp { 0.0f };
};

class ProximityModuleEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    ProximityModuleEditor (ProximityModule& p, juce::AudioProcessorValueTreeState& vts)
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

        setupSlider (distSlider, " cm", juce::Colour (0xffff9f0a));
        setupSlider (bodySlider, " %", juce::Colour (0xffff6934));
        setupSlider (dynamicSlider, " %", UITheme::appleGreen);
        setupSlider (airLossSlider, " %", UITheme::appleBlue);

        autoLevelBtn.setButtonText ("Auto Level");
        autoLevelBtn.setClickingTogglesState (true);
        autoLevelBtn.setColour (juce::TextButton::buttonOnColourId, UITheme::appleBlue);
        addAndMakeVisible (autoLevelBtn);

        distAttach     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "distance", distSlider);
        bodyAttach     = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "body", bodySlider);
        dynamicAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "dynamic", dynamicSlider);
        airLossAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "airLoss", airLossSlider);
        autoLevelAttach= std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (vts, "autoLevel", autoLevelBtn);

        setSize (360, 280);
        startTimerHz (60);
    }

    ~ProximityModuleEditor() override { stopTimer(); }

    void timerCallback() override { repaint(); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        // Check if clicked in 2D stage
        auto stageRect = getStageBounds();
        if (stageRect.contains (e.position))
        {
            isDraggingStage = true;
            updateDistanceByMouse (e.position.x, stageRect);
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (isDraggingStage)
        {
            auto stageRect = getStageBounds();
            updateDistanceByMouse (e.position.x, stageRect);
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        isDraggingStage = false;
    }

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
        g.drawText ("PROXIMITY (ACOUSTIC DEPTH)", header.withTrimmedLeft (14), juce::Justification::centredLeft);

        // Dynamic Mud Guard Clamp Badge
        float clampDb = module.getLiveDynamicClamp();
        bool isClamping = clampDb > 0.4f;
        auto badge = header.removeFromRight (94).toFloat().reduced (6.0f, 8.0f);
        g.setColour (isClamping ? UITheme::appleGreen.withAlpha (0.28f) : juce::Colour (0x15ffffff));
        g.fillRoundedRectangle (badge, 3.0f);
        g.setColour (isClamping ? UITheme::appleGreen : UITheme::textTertiary);
        g.drawRoundedRectangle (badge, 3.0f, 1.0f);
        g.setFont (UITheme::getFont (8.0f, true));
        g.drawText (isClamping ? "DYN -" + juce::String (clampDb, 1) + " dB" : "DYN ACTIVE", badge, juce::Justification::centred);

        // --- INTERACTIVE 2D ACOUSTIC STUDIO STAGE ---
        auto stageRect = getStageBounds();
        g.setColour (juce::Colour (0xff111114));
        g.fillRoundedRectangle (stageRect, 4.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (stageRect, 4.0f, 0.8f);

        // Microphone on the right side of the stage
        float micX = stageRect.getRight() - 28.0f;
        float micY = stageRect.getCentreY();

        // Mic capsule vector drawing
        auto micCapsule = juce::Rectangle<float> (micX - 7.0f, micY - 14.0f, 14.0f, 28.0f);
        g.setColour (juce::Colour (0xff25252b));
        g.fillRoundedRectangle (micCapsule, 4.0f);
        g.setColour (UITheme::textSecondary);
        g.drawRoundedRectangle (micCapsule, 4.0f, 1.0f);
        // Mic diaphragm mesh
        g.setColour (juce::Colour (0x40ffffff));
        g.drawHorizontalLine ((int) (micY - 4.0f), micCapsule.getX() + 2.0f, micCapsule.getRight() - 2.0f);
        g.drawHorizontalLine ((int) micY, micCapsule.getX() + 2.0f, micCapsule.getRight() - 2.0f);
        g.drawHorizontalLine ((int) (micY + 4.0f), micCapsule.getX() + 2.0f, micCapsule.getRight() - 2.0f);

        // Map Distance to X coordinate: 2 cm (near mic) to 50 cm (far left)
        float distCm = module.getDistanceCm();
        float normDist = juce::jlimit (0.0f, 1.0f, (distCm - 2.0f) / 48.0f);
        float voiceX = (micX - 25.0f) - normDist * (stageRect.getWidth() - 75.0f);
        float voiceY = micY;

        // Color tone interpolation: Warm amber when close, cool blue when far
        juce::Colour waveCol = juce::Colour (0xffff9f0a).interpolatedWith (juce::Colour (0xff38bdf8), normDist);

        // Draw Pulsing Soundwave Rings emitting from voice to mic
        float vocalLevel = module.getLiveVocalLevel();
        float basePulse = 0.25f + vocalLevel * 0.75f;

        for (int r = 1; r <= 3; ++r)
        {
            float ringRadius = (float) r * 14.0f + (vocalLevel * 8.0f);
            float alpha = juce::jlimit (0.10f, 0.90f, (1.0f - (float) r * 0.25f) * basePulse);

            juce::Path ringArc;
            // Draw arc facing towards the mic (angle from -45 deg to +45 deg)
            ringArc.addCentredArc (voiceX, voiceY, ringRadius, ringRadius, 0.0f,
                                   -juce::MathConstants<float>::pi * 0.35f,
                                   juce::MathConstants<float>::pi * 0.35f, true);

            g.setColour (waveCol.withAlpha (alpha));
            g.strokePath (ringArc, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // Draggable Vocal Source Emitter
        g.setColour (waveCol.withAlpha (0.45f));
        g.fillEllipse (voiceX - 8.0f, voiceY - 8.0f, 16.0f, 16.0f);
        g.setColour (juce::Colours::white);
        g.fillEllipse (voiceX - 4.0f, voiceY - 4.0f, 8.0f, 8.0f);

        // Distance Text tag
        g.setFont (UITheme::getFont (8.0f, true));
        g.setColour (waveCol);
        juce::String distDesc = distCm < 6.0f ? " (RADIO / LIPS)" : (distCm < 16.0f ? " (INTIMATE)" : " (ROOM DEPTH)");
        g.drawText (juce::String (distCm, 1) + " cm" + distDesc,
                    juce::Rectangle<float> (stageRect.getX() + 8.0f, stageRect.getY() + 4.0f, 180.0f, 12.0f),
                    juce::Justification::centredLeft);

        g.setColour (UITheme::textTertiary);
        g.setFont (UITheme::getFont (7.5f));
        g.drawText ("DRAG VOICE TO POSITION",
                    juce::Rectangle<float> (stageRect.getRight() - 140.0f, stageRect.getBottom() - 14.0f, 132.0f, 10.0f),
                    juce::Justification::centredRight);

        // Knob labels
        int colW = getWidth() / 4;
        int labelY = 126;
        g.setColour (UITheme::textSecondary);
        g.setFont (UITheme::getFont (8.5f, true));
        g.drawText ("DISTANCE", colW * 0, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("BODY",     colW * 1, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("DYNAMIC",  colW * 2, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("AIR LOSS", colW * 3, labelY, colW, 14, juce::Justification::centred);
    }

    void resized() override
    {
        int colW = getWidth() / 4;
        int knobY = 144;
        int knobSize = 68;

        distSlider.setBounds    (colW * 0 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        bodySlider.setBounds    (colW * 1 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        dynamicSlider.setBounds (colW * 2 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        airLossSlider.setBounds (colW * 3 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);

        autoLevelBtn.setBounds (getWidth() / 2 - 45, getHeight() - 26, 90, 20);
    }

private:
    juce::Rectangle<float> getStageBounds() const
    {
        return juce::Rectangle<float> (16.0f, 44.0f, (float) getWidth() - 32.0f, 76.0f);
    }

    void updateDistanceByMouse (float mouseX, juce::Rectangle<float> stageRect)
    {
        float micX = stageRect.getRight() - 28.0f;
        float trackW = stageRect.getWidth() - 75.0f;
        float offsetFromMic = (micX - 25.0f) - mouseX;
        float norm = juce::jlimit (0.0f, 1.0f, offsetFromMic / trackW);
        float newDist = 2.0f + norm * 48.0f;
        module.setDistanceCm (newDist);
        repaint();
    }

    ProximityModule& module;
    juce::Slider distSlider, bodySlider, dynamicSlider, airLossSlider;
    juce::TextButton autoLevelBtn;
    bool isDraggingStage = false;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> distAttach, bodyAttach, dynamicAttach, airLossAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoLevelAttach;
};

inline juce::AudioProcessorEditor* ProximityModule::createEditor()
{
    return new ProximityModuleEditor (*this, apvts);
}
