#pragma once
#include "../Graph/ModuleProcessor.h"
#include "../UITheme.h"
#include <cmath>

class BroadcastMorphModuleEditor;

class BroadcastMorphModule : public ModuleProcessor
{
public:
    enum ProfileIndex
    {
        SM7B_Broadcast = 0,
        Neumann_U87,
        EV_RE20,
        Vintage_Tube_1950s,
        Tactical_Comm,
        Megaphone_Horn,
        NumProfiles
    };

    static constexpr int numBands = 16;

    BroadcastMorphModule()
        : ModuleProcessor ("Broadcast Morph", createLayout())
    {
        profileParam = apvts.getRawParameterValue ("profile");
        morphParam   = apvts.getRawParameterValue ("morph");
        driveParam   = apvts.getRawParameterValue ("drive");
        tiltParam    = apvts.getRawParameterValue ("tilt");
        outputParam  = apvts.getRawParameterValue ("output");

        for (int b = 0; b < numBands; ++b)
            liveSpectrum[b].store (0.0f);
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

        int profile = juce::jlimit (0, (int) NumProfiles - 1, (int) profileParam->load());
        float morphNorm = juce::jlimit (0.0f, 1.0f, morphParam->load() * 0.01f);
        float driveNorm = juce::jlimit (0.0f, 1.0f, driveParam->load() * 0.01f);
        float tiltDb    = tiltParam->load();
        float outputDb  = outputParam->load();
        float outGainLin= juce::Decibels::decibelsToGain (outputDb);

        updateProfileCoefficients (profile, tiltDb);

        float blockPeakIn  = 0.0f;
        float blockPeakOut = 0.0f;
        float tempBands[numBands] = {};

        for (int ch = 0; ch < numChannels; ++ch)
        {
            auto* channelData = buffer.getWritePointer (ch);
            int fIdx = ch < 2 ? ch : 0;

            for (int i = 0; i < numSamples; ++i)
            {
                float dry = channelData[i];
                blockPeakIn = juce::jmax (blockPeakIn, std::abs (dry));

                // 1. Stage 1: High-Pass / Low Cut
                float hp = b_hp0 * dry + b_hp1 * x_hp1[fIdx] + b_hp2 * x_hp2[fIdx]
                           - a_hp1 * y_hp1[fIdx] - a_hp2 * y_hp2[fIdx];
                x_hp2[fIdx] = x_hp1[fIdx];
                x_hp1[fIdx] = dry;
                y_hp2[fIdx] = y_hp1[fIdx];
                y_hp1[fIdx] = hp;

                // 2. Stage 2: Low-Mid Warmth Bell / Shelf
                float lm = b_lm0 * hp + b_lm1 * x_lm1[fIdx] + b_lm2 * x_lm2[fIdx]
                           - a_lm1 * y_lm1[fIdx] - a_lm2 * y_lm2[fIdx];
                x_lm2[fIdx] = x_lm1[fIdx];
                x_lm1[fIdx] = hp;
                y_lm2[fIdx] = y_lm1[fIdx];
                y_lm1[fIdx] = lm;

                // 3. Stage 3: Presence & Articulation Bell
                float pr = b_pr0 * lm + b_pr1 * x_pr1[fIdx] + b_pr2 * x_pr2[fIdx]
                           - a_pr1 * y_pr1[fIdx] - a_pr2 * y_pr2[fIdx];
                x_pr2[fIdx] = x_pr1[fIdx];
                x_pr1[fIdx] = lm;
                y_pr2[fIdx] = y_pr1[fIdx];
                y_pr1[fIdx] = pr;

                // 4. Stage 4: High Shelf Air / Lowpass Roll-Off
                float hs = b_hs0 * pr + b_hs1 * x_hs1[fIdx] + b_hs2 * x_hs2[fIdx]
                           - a_hs1 * y_hs1[fIdx] - a_hs2 * y_hs2[fIdx];
                x_hs2[fIdx] = x_hs1[fIdx];
                x_hs1[fIdx] = pr;
                y_hs2[fIdx] = y_hs1[fIdx];
                y_hs1[fIdx] = hs;

                // 5. Capsule Saturation & Non-Linearity
                float wet = hs;
                wet = applyProfileNonLinearity (wet, profile, driveNorm);

                // 6. Morph crossfade (Dry to Wet) + Output Trim
                float out = (1.0f - morphNorm) * dry + morphNorm * (wet * profileLevelCompensation[profile]);
                out *= outGainLin;

                channelData[i] = out;
                blockPeakOut = juce::jmax (blockPeakOut, std::abs (out));

                // Sample frequency spectrum for dynamic display
                if (ch == 0 && (i % 2 == 0))
                {
                    float absOut = std::abs (out);
                    for (int b = 0; b < numBands; ++b)
                    {
                        float f = 50.0f * std::pow (14000.0f / 50.0f, (float) b / (float) (numBands - 1));
                        float w = 1.0f / (1.0f + std::abs (f - profilePresenceFreq[profile]) * 0.002f);
                        tempBands[b] = juce::jmax (tempBands[b], absOut * w);
                    }
                }
            }
        }

        // Decay spectrum
        for (int b = 0; b < numBands; ++b)
        {
            float prev = liveSpectrum[b].load (std::memory_order_relaxed);
            float target = tempBands[b];
            float smoothed = target > prev ? (prev * 0.35f + target * 0.65f) : (prev * 0.88f);
            liveSpectrum[b].store (smoothed, std::memory_order_relaxed);
        }

        liveInputPeak.store  (blockPeakIn, std::memory_order_relaxed);
        liveOutputPeak.store (blockPeakOut, std::memory_order_relaxed);
    }

    juce::AudioProcessorEditor* createEditor() override;

    int   getProfileIndex() const     { return profileParam ? (int) profileParam->load() : 0; }
    void  setProfileIndex (int idx)   { if (auto* p = apvts.getParameter ("profile")) p->setValueNotifyingHost (p->convertTo0to1 ((float) idx)); }
    float getLiveInputPeak() const    { return liveInputPeak.load (std::memory_order_relaxed); }
    float getLiveOutputPeak() const   { return liveOutputPeak.load (std::memory_order_relaxed); }
    float getBandEnergy (int b) const { return liveSpectrum[juce::jlimit (0, numBands - 1, b)].load (std::memory_order_relaxed); }
    float getProfilePresenceFreq() const { return profilePresenceFreq[getProfileIndex()]; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        return {
            std::make_unique<juce::AudioParameterChoice> (
                juce::ParameterID { "profile", 1 }, "Microphone Profile",
                juce::StringArray { "SM7B Broadcast", "Neumann U87", "EV RE20", "1950s Tube", "Tactical Comm", "Megaphone" }, 0),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "morph", 1 }, "Morph Strength",
                juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 100.0f,
                juce::AudioParameterFloatAttributes().withLabel ("%")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "drive", 1 }, "Capsule Drive",
                juce::NormalisableRange<float> (0.0f, 100.0f, 1.0f), 25.0f,
                juce::AudioParameterFloatAttributes().withLabel ("%")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "tilt", 1 }, "Tone Tilt",
                juce::NormalisableRange<float> (-8.0f, 8.0f, 0.5f), 0.0f,
                juce::AudioParameterFloatAttributes().withLabel ("dB")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "output", 1 }, "Output Trim",
                juce::NormalisableRange<float> (-12.0f, 12.0f, 0.5f), 0.0f,
                juce::AudioParameterFloatAttributes().withLabel ("dB"))
        };
    }

    void resetFilters()
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            x_hp1[ch] = x_hp2[ch] = y_hp1[ch] = y_hp2[ch] = 0.0f;
            x_lm1[ch] = x_lm2[ch] = y_lm1[ch] = y_lm2[ch] = 0.0f;
            x_pr1[ch] = x_pr2[ch] = y_pr1[ch] = y_pr2[ch] = 0.0f;
            x_hs1[ch] = x_hs2[ch] = y_hs1[ch] = y_hs2[ch] = 0.0f;
        }
    }

    float applyProfileNonLinearity (float in, int profile, float drive)
    {
        switch (profile)
        {
            case SM7B_Broadcast:
            {
                // Dynamic inductive compression with subtle smoothing
                float driveGain = 1.0f + drive * 1.5f;
                return std::tanh (in * driveGain) / driveGain;
            }
            case Neumann_U87:
            {
                // Ultra-linear condenser capsule with minimal harmonic warmth
                float driveGain = 1.0f + drive * 0.4f;
                return (in * driveGain) / (1.0f + 0.1f * std::abs (in * driveGain));
            }
            case EV_RE20:
            {
                // American broadcast punchy speech saturation
                float driveGain = 1.0f + drive * 1.8f;
                return std::tanh (in * driveGain) * 0.95f;
            }
            case Vintage_Tube_1950s:
            {
                // Rich 2nd-order transformer & tube asymmetry
                float driveGain = 1.0f + drive * 3.2f;
                float x = in * driveGain;
                float tube = std::tanh (x) + 0.18f * (x * x) / (1.0f + std::abs (x));
                return tube / std::sqrt (1.0f + drive * 2.0f);
            }
            case Tactical_Comm:
            {
                // Crunchy pilot / military radio squelch & diode clipping
                float driveGain = 1.0f + drive * 4.0f;
                float x = in * driveGain;
                float clipped = juce::jlimit (-0.65f, 0.65f, x * 1.5f);
                return clipped * 0.85f;
            }
            case Megaphone_Horn:
            {
                // Hard-driven horn speaker distortion
                float driveGain = 1.5f + drive * 6.0f;
                float x = in * driveGain;
                float dist = std::tanh (x * 1.8f);
                return dist * 0.80f;
            }
            default:
                return in;
        }
    }

    void updateProfileCoefficients (int profile, float tilt)
    {
        // 1. Stage 1: High-Pass Frequency
        float hpFreq = 40.0f;
        if      (profile == SM7B_Broadcast)     hpFreq = 75.0f;   // Tight radio bottom
        else if (profile == Neumann_U87)        hpFreq = 32.0f;   // Deep extended condenser sub
        else if (profile == EV_RE20)            hpFreq = 65.0f;   // Clean speech cut
        else if (profile == Vintage_Tube_1950s) hpFreq = 50.0f;   // Warm vintage roll-off
        else if (profile == Tactical_Comm)      hpFreq = 420.0f;  // Aggressive radio bandpass cut!
        else if (profile == Megaphone_Horn)     hpFreq = 580.0f;  // Sharp megaphone horn cut!
        designHP (hpFreq, 0.7071f);

        // 2. Stage 2: Low-Mid Warmth (Chest Resonance / Body)
        float lmFreq = 200.0f;
        float lmGain = 0.0f;
        if      (profile == SM7B_Broadcast)     { lmFreq = 180.0f; lmGain = 3.5f - tilt * 0.4f; } // Warm radio body
        else if (profile == Neumann_U87)        { lmFreq = 120.0f; lmGain = 1.2f - tilt * 0.3f; } // Flat, linear low
        else if (profile == EV_RE20)            { lmFreq = 220.0f; lmGain = 0.5f - tilt * 0.3f; } // Non-boomy flat speech
        else if (profile == Vintage_Tube_1950s) { lmFreq = 220.0f; lmGain = 5.5f - tilt * 0.5f; } // Heavy velvety tube chest
        else if (profile == Tactical_Comm)      { lmFreq = 650.0f; lmGain = -4.0f; }              // Hollow radio
        else if (profile == Megaphone_Horn)     { lmFreq = 800.0f; lmGain = -6.0f; }              // Tinny horn
        designPeak (lmFreq, lmGain, 1.2f, b_lm0, b_lm1, b_lm2, a_lm1, a_lm2);

        // 3. Stage 3: Presence & Articulation Peak
        float prFreq = 3000.0f;
        float prGain = 0.0f;
        float prQ    = 1.4f;
        if      (profile == SM7B_Broadcast)     { prFreq = 4200.0f; prGain = 5.5f + tilt * 0.5f; prQ = 1.4f; } // Iconic SM7B presence bump!
        else if (profile == Neumann_U87)        { prFreq = 3200.0f; prGain = 2.0f + tilt * 0.4f; prQ = 1.0f; } // Open speech clarity
        else if (profile == EV_RE20)            { prFreq = 2800.0f; prGain = 4.8f + tilt * 0.4f; prQ = 1.6f; } // Punchy talk-radio bite
        else if (profile == Vintage_Tube_1950s) { prFreq = 2200.0f; prGain = 2.5f;              prQ = 1.1f; } // Smooth golden-age mid
        else if (profile == Tactical_Comm)      { prFreq = 1600.0f; prGain = 8.5f;              prQ = 2.4f; } // Nasal ATC radio honk!
        else if (profile == Megaphone_Horn)     { prFreq = 1850.0f; prGain = 11.0f;             prQ = 2.8f; } // Ear-splitting megaphone resonance!
        designPeak (prFreq, prGain, prQ, b_pr0, b_pr1, b_pr2, a_pr1, a_pr2);

        // 4. Stage 4: High Shelf Air / Lowpass Cut
        float hsFreq = 10000.0f;
        float hsGain = 0.0f;
        if      (profile == SM7B_Broadcast)     { hsFreq = 11000.0f; hsGain = -3.5f + tilt * 0.5f; } // Thick windscreen air attenuation
        else if (profile == Neumann_U87)        { hsFreq = 10500.0f; hsGain = 4.8f  + tilt * 0.6f; } // Shimmering German air sheen!
        else if (profile == EV_RE20)            { hsFreq = 9500.0f;  hsGain = -1.5f + tilt * 0.4f; } // Controlled broadcast top
        else if (profile == Vintage_Tube_1950s) { hsFreq = 6800.0f;  hsGain = -7.0f + tilt * 0.3f; } // Dark vintage ribbon top
        else if (profile == Tactical_Comm)      { hsFreq = 3400.0f;  hsGain = -18.0f; }              // Steep military telecom lowpass cut!
        else if (profile == Megaphone_Horn)     { hsFreq = 3200.0f;  hsGain = -16.0f; }              // Narrow horn dispersion cut!
        designHighShelf (hsFreq, hsGain);
    }

    void designHP (float freq, float q)
    {
        float w0 = 2.0f * juce::MathConstants<float>::pi * (freq / (float) sampleRate);
        float cosw0 = std::cos (w0);
        float alpha = std::sin (w0) / (2.0f * q);
        float a0 = 1.0f + alpha;
        b_hp0 = ((1.0f + cosw0) * 0.5f) / a0;
        b_hp1 = (-(1.0f + cosw0)) / a0;
        b_hp2 = ((1.0f + cosw0) * 0.5f) / a0;
        a_hp1 = (-2.0f * cosw0) / a0;
        a_hp2 = (1.0f - alpha) / a0;
    }

    void designPeak (float freq, float gainDb, float q, float& b0, float& b1, float& b2, float& a1, float& a2)
    {
        float A = std::pow (10.0f, gainDb / 40.0f);
        float w0 = 2.0f * juce::MathConstants<float>::pi * (freq / (float) sampleRate);
        float cosw0 = std::cos (w0);
        float alpha = std::sin (w0) / (2.0f * q);
        float a0 = 1.0f + alpha / A;
        b0 = (1.0f + alpha * A) / a0;
        b1 = (-2.0f * cosw0) / a0;
        b2 = (1.0f - alpha * A) / a0;
        a1 = (-2.0f * cosw0) / a0;
        a2 = (1.0f - alpha / A) / a0;
    }

    void designHighShelf (float freq, float gainDb)
    {
        float A = std::pow (10.0f, gainDb / 40.0f);
        float w0 = 2.0f * juce::MathConstants<float>::pi * (freq / (float) sampleRate);
        float cosw0 = std::cos (w0);
        float sinw0 = std::sin (w0);
        float alpha = sinw0 * 0.5f * std::sqrt (2.0f);

        float a0 = (A + 1.0f) - (A - 1.0f) * cosw0 + 2.0f * std::sqrt (A) * alpha;
        b_hs0 = (A * ((A + 1.0f) + (A - 1.0f) * cosw0 + 2.0f * std::sqrt (A) * alpha)) / a0;
        b_hs1 = (-2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosw0)) / a0;
        b_hs2 = (A * ((A + 1.0f) + (A - 1.0f) * cosw0 - 2.0f * std::sqrt (A) * alpha)) / a0;
        a_hs1 = (2.0f * ((A - 1.0f) - (A + 1.0f) * cosw0)) / a0;
        a_hs2 = ((A + 1.0f) - (A - 1.0f) * cosw0 - 2.0f * std::sqrt (A) * alpha) / a0;
    }

    std::atomic<float>* profileParam = nullptr;
    std::atomic<float>* morphParam   = nullptr;
    std::atomic<float>* driveParam   = nullptr;
    std::atomic<float>* tiltParam    = nullptr;
    std::atomic<float>* outputParam  = nullptr;

    double sampleRate = 44100.0;

    // Filter coefficients
    float b_hp0 = 1.0f, b_hp1 = 0.0f, b_hp2 = 0.0f, a_hp1 = 0.0f, a_hp2 = 0.0f;
    float b_lm0 = 1.0f, b_lm1 = 0.0f, b_lm2 = 0.0f, a_lm1 = 0.0f, a_lm2 = 0.0f;
    float b_pr0 = 1.0f, b_pr1 = 0.0f, b_pr2 = 0.0f, a_pr1 = 0.0f, a_pr2 = 0.0f;
    float b_hs0 = 1.0f, b_hs1 = 0.0f, b_hs2 = 0.0f, a_hs1 = 0.0f, a_hs2 = 0.0f;

    // Filter states
    float x_hp1[2] = {}, x_hp2[2] = {}, y_hp1[2] = {}, y_hp2[2] = {};
    float x_lm1[2] = {}, x_lm2[2] = {}, y_lm1[2] = {}, y_lm2[2] = {};
    float x_pr1[2] = {}, x_pr2[2] = {}, y_pr1[2] = {}, y_pr2[2] = {};
    float x_hs1[2] = {}, x_hs2[2] = {}, y_hs1[2] = {}, y_hs2[2] = {};

    const float profileLevelCompensation[NumProfiles] = { 1.05f, 0.92f, 1.02f, 1.08f, 0.72f, 0.65f };
    const float profilePresenceFreq[NumProfiles] = { 4200.0f, 10500.0f, 2800.0f, 2200.0f, 1600.0f, 1850.0f };

    std::atomic<float> liveInputPeak { 0.0f };
    std::atomic<float> liveOutputPeak { 0.0f };
    std::atomic<float> liveSpectrum[numBands];
};

class BroadcastMorphModuleEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    BroadcastMorphModuleEditor (BroadcastMorphModule& p, juce::AudioProcessorValueTreeState& vts)
        : AudioProcessorEditor (&p), module (p)
    {
        // 6 Profile Selection Buttons
        const juce::String names[] = { "SM7B", "U87", "RE20", "1950s TUBE", "COMM", "MEGAPHONE" };
        for (int i = 0; i < 6; ++i)
        {
            auto* btn = profileButtons.add (new juce::TextButton (names[i]));
            btn->setRadioGroupId (1001);
            btn->setClickingTogglesState (true);
            btn->setColour (juce::TextButton::buttonOnColourId, UITheme::appleBlue);
            btn->setColour (juce::TextButton::textColourOnId, juce::Colours::white);
            btn->onClick = [this, i]() { module.setProfileIndex (i); repaint(); };
            addAndMakeVisible (btn);
        }
        profileButtons[module.getProfileIndex()]->setToggleState (true, juce::dontSendNotification);

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

        setupSlider (morphSlider,  " %", UITheme::appleBlue);
        setupSlider (driveSlider,  " %", juce::Colour (0xffff9f0a));
        setupSlider (tiltSlider,   " dB", UITheme::appleYellow);
        setupSlider (outputSlider, " dB", UITheme::appleGreen);

        morphAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "morph", morphSlider);
        driveAttach  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "drive", driveSlider);
        tiltAttach   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "tilt", tiltSlider);
        outputAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (vts, "output", outputSlider);

        setSize (390, 310);
        startTimerHz (60);
    }

    ~BroadcastMorphModuleEditor() override { stopTimer(); }

    void timerCallback() override
    {
        int curIdx = module.getProfileIndex();
        for (int i = 0; i < 6; ++i)
        {
            if (profileButtons[i]->getToggleState() != (i == curIdx))
                profileButtons[i]->setToggleState (i == curIdx, juce::dontSendNotification);
        }
        repaint();
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
        g.drawText ("BROADCAST MORPH (MIC CLONER)", header.withTrimmedLeft (14), juce::Justification::centredLeft);

        // Profile Badge in header
        int prof = module.getProfileIndex();
        const juce::String profTag[] = { "SM7B PODCAST", "U87 GERMAN", "RE20 RADIO", "1950s RETRO", "ATC COMM", "HORN MEGAPHONE" };
        auto badge = header.removeFromRight (110).toFloat().reduced (6.0f, 8.0f);
        g.setColour (UITheme::appleBlue.withAlpha (0.25f));
        g.fillRoundedRectangle (badge, 3.0f);
        g.setColour (UITheme::appleBlue);
        g.drawRoundedRectangle (badge, 3.0f, 1.0f);
        g.setFont (UITheme::getFont (8.0f, true));
        g.drawText (profTag[prof], badge, juce::Justification::centred);

        // --- STAGE 1: VECTOR MICROPHONE DRAWING & REAL-TIME ACOUSTIC CURVE ---
        auto micBox   = juce::Rectangle<float> (14.0f, 68.0f, 86.0f, 82.0f);
        auto curveBox = juce::Rectangle<float> (micBox.getRight() + 8.0f, 68.0f, (float) getWidth() - micBox.getRight() - 22.0f, 82.0f);

        // 1. Microphone Vector Display Box
        g.setColour (juce::Colour (0xff111114));
        g.fillRoundedRectangle (micBox, 4.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (micBox, 4.0f, 0.8f);

        drawMicrophoneVector (g, micBox, prof);

        // 2. Real-Time Acoustic EQ Curve & Spectrum Display Box
        g.setColour (juce::Colour (0xff111114));
        g.fillRoundedRectangle (curveBox, 4.0f);
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (curveBox, 4.0f, 0.8f);

        // Center line (0 dB)
        float midY = curveBox.getCentreY();
        g.setColour (juce::Colour (0x18ffffff));
        g.drawHorizontalLine ((int) midY, curveBox.getX(), curveBox.getRight());

        // Draw Real-time Spectrum Bins
        int numBands = BroadcastMorphModule::numBands;
        float barWidth = (curveBox.getWidth() - 6.0f) / (float) numBands;

        for (int b = 0; b < numBands; ++b)
        {
            float bx = curveBox.getX() + 3.0f + (float) b * barWidth;
            float energy = juce::jlimit (0.0f, 1.0f, module.getBandEnergy (b) * 2.6f);
            if (energy < 0.01f) continue;

            float barH = energy * (curveBox.getHeight() * 0.45f);
            auto barRect = juce::Rectangle<float> (bx + 1.0f, midY - barH, barWidth - 2.0f, barH * 2.0f);

            g.setColour (UITheme::appleBlue.withAlpha (0.28f));
            g.fillRoundedRectangle (barRect, 1.0f);
        }

        // Draw Characteristic Target Frequency Curve
        drawAcousticCurve (g, curveBox, prof, midY);

        // Knob labels
        int colW = getWidth() / 4;
        int labelY = 158;
        g.setColour (UITheme::textSecondary);
        g.setFont (UITheme::getFont (8.5f, true));
        g.drawText ("MORPH",   colW * 0, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("DRIVE",   colW * 1, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("TILT",    colW * 2, labelY, colW, 14, juce::Justification::centred);
        g.drawText ("OUTPUT",  colW * 3, labelY, colW, 14, juce::Justification::centred);
    }

    void resized() override
    {
        // Profile buttons row
        int btnW = (getWidth() - 28) / 6;
        for (int i = 0; i < 6; ++i)
            profileButtons[i]->setBounds (14 + i * btnW, 42, btnW - 2, 22);

        // Knobs row
        int colW = getWidth() / 4;
        int knobY = 176;
        int knobSize = 64;

        morphSlider.setBounds  (colW * 0 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        driveSlider.setBounds  (colW * 1 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        tiltSlider.setBounds   (colW * 2 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
        outputSlider.setBounds (colW * 3 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 36);
    }

private:
    void drawMicrophoneVector (juce::Graphics& g, juce::Rectangle<float> box, int profile)
    {
        float cx = box.getCentreX();
        float cy = box.getCentreY() + 4.0f;
        float vocalPulse = module.getLiveOutputPeak();

        switch (profile)
        {
            case BroadcastMorphModule::SM7B_Broadcast:
            {
                // SM7B: Matte black body + thick foam windscreen + yoke
                auto micBody = juce::Rectangle<float> (cx - 11.0f, cy - 24.0f, 22.0f, 38.0f);
                g.setColour (juce::Colour (0xff1f1f24));
                g.fillRoundedRectangle (micBody, 6.0f);
                g.setColour (juce::Colour (0xff3a3a42));
                g.drawRoundedRectangle (micBody, 6.0f, 1.2f);

                // Foam Windscreen cap
                auto foam = juce::Rectangle<float> (cx - 10.0f, cy - 23.0f, 20.0f, 16.0f);
                g.setColour (juce::Colour (0xff161619));
                g.fillRoundedRectangle (foam, 4.0f);

                // Gold SM7B ring
                g.setColour (juce::Colour (0xffc99738));
                g.drawHorizontalLine ((int) (cy - 7.0f), cx - 10.0f, cx + 10.0f);

                // Yoke mount U-bracket
                juce::Path yoke;
                yoke.addCentredArc (cx, cy + 2.0f, 16.0f, 15.0f, 0.0f, -juce::MathConstants<float>::pi * 0.45f, juce::MathConstants<float>::pi * 0.45f, true);
                g.setColour (juce::Colour (0xff44444c));
                g.strokePath (yoke, juce::PathStrokeType (2.0f));
                break;
            }
            case BroadcastMorphModule::Neumann_U87:
            {
                // U87: German Silver/Nickel tapered condenser body
                auto body = juce::Rectangle<float> (cx - 10.0f, cy - 26.0f, 20.0f, 44.0f);
                g.setColour (juce::Colour (0xffb8b8c2));
                g.fillRoundedRectangle (body, 3.0f);
                g.setColour (juce::Colour (0xffe5e5ec));
                g.drawRoundedRectangle (body, 3.0f, 1.0f);

                // Dual wire mesh grill (upper half)
                auto grill = juce::Rectangle<float> (cx - 9.0f, cy - 25.0f, 18.0f, 20.0f);
                g.setColour (juce::Colour (0xff90909c));
                g.fillRoundedRectangle (grill, 3.0f);
                g.setColour (juce::Colour (0xff4fa3e0)); // Blue Neumann diamond badge
                g.fillRect (cx - 2.5f, cy + 2.0f, 5.0f, 5.0f);
                break;
            }
            case BroadcastMorphModule::EV_RE20:
            {
                // RE20: Ribbed broadcast fawn beige/gray cylinder
                auto body = juce::Rectangle<float> (cx - 12.0f, cy - 25.0f, 24.0f, 42.0f);
                g.setColour (juce::Colour (0xff82828b));
                g.fillRoundedRectangle (body, 5.0f);

                // Continuous Variable-D acoustic vent ribs
                g.setColour (juce::Colour (0xff2c2c34));
                for (float y = cy - 18.0f; y < cy + 12.0f; y += 5.0f)
                    g.drawHorizontalLine ((int) y, cx - 8.0f, cx + 8.0f);
                break;
            }
            case BroadcastMorphModule::Vintage_Tube_1950s:
            {
                // 1950s Retro Chrome Bullet / Ribbon Mic
                auto head = juce::Rectangle<float> (cx - 13.0f, cy - 24.0f, 26.0f, 32.0f);
                g.setColour (juce::Colour (0xff484852));
                g.fillRoundedRectangle (head, 12.0f);
                g.setColour (juce::Colour (0xffc0c0d0));
                g.drawRoundedRectangle (head, 12.0f, 1.5f);

                // Glowing internal vacuum tube
                float glow = juce::jlimit (0.35f, 0.95f, 0.40f + vocalPulse * 0.60f);
                g.setColour (juce::Colour (0xffff7a00).withAlpha (glow));
                g.fillEllipse (cx - 4.0f, cy - 12.0f, 8.0f, 16.0f);

                // Chrome vertical ribs
                g.setColour (juce::Colour (0xffa0a0b0));
                g.drawVerticalLine ((int) cx, head.getY() + 4.0f, head.getBottom() - 4.0f);
                g.drawVerticalLine ((int) (cx - 6.0f), head.getY() + 7.0f, head.getBottom() - 7.0f);
                g.drawVerticalLine ((int) (cx + 6.0f), head.getY() + 7.0f, head.getBottom() - 7.0f);
                break;
            }
            case BroadcastMorphModule::Tactical_Comm:
            {
                // Aviation / Military Headset with boom mic
                g.setColour (juce::Colour (0xff2a4530)); // Military olive drab
                g.fillEllipse (cx - 16.0f, cy - 20.0f, 32.0f, 28.0f);
                g.setColour (juce::Colour (0xff406848));
                g.drawEllipse (cx - 16.0f, cy - 20.0f, 32.0f, 28.0f, 1.5f);

                // Boom arm & small electret capsule
                g.setColour (juce::Colour (0xff111115));
                g.drawLine (cx, cy, cx + 18.0f, cy + 14.0f, 2.0f);
                g.fillRoundedRectangle (cx + 14.0f, cy + 10.0f, 8.0f, 10.0f, 2.0f);
                break;
            }
            case BroadcastMorphModule::Megaphone_Horn:
            {
                // Classic megaphone horn cone
                juce::Path horn;
                horn.startNewSubPath (cx - 14.0f, cy - 16.0f);
                horn.lineTo (cx + 16.0f, cy - 26.0f);
                horn.lineTo (cx + 16.0f, cy + 22.0f);
                horn.lineTo (cx - 14.0f, cy + 12.0f);
                horn.closeSubPath();

                g.setColour (juce::Colour (0xffc53030)); // Classic Red megaphone
                g.fillPath (horn);
                g.setColour (juce::Colours::white.withAlpha (0.75f));
                g.strokePath (horn, juce::PathStrokeType (1.4f));

                // Handle
                g.setColour (juce::Colour (0xff252528));
                g.fillRect (cx - 12.0f, cy + 10.0f, 6.0f, 14.0f);
                break;
            }
        }

        // Microphone Name below icon
        const juce::String shortNames[] = { "SHURE SM7B", "NEUMANN U87", "EV RE20", "VINTAGE TUBE", "TACTICAL COMM", "MEGAPHONE" };
        g.setColour (UITheme::textTertiary);
        g.setFont (UITheme::getFont (7.0f, true));
        g.drawText (shortNames[profile], juce::Rectangle<float> (box.getX() + 2.0f, box.getBottom() - 13.0f, box.getWidth() - 4.0f, 10.0f), juce::Justification::centred);
    }

    void drawAcousticCurve (juce::Graphics& g, juce::Rectangle<float> box, int profile, float midY)
    {
        juce::Path curve;
        bool started = false;
        int steps = 50;

        float w = box.getWidth() - 16.0f;
        float startX = box.getX() + 8.0f;

        for (int s = 0; s <= steps; ++s)
        {
            float normX = (float) s / (float) steps;
            float x = startX + normX * w;

            // Frequency from 50 Hz to 14000 Hz
            float f = 50.0f * std::pow (14000.0f / 50.0f, normX);
            float db = 0.0f;

            switch (profile)
            {
                case BroadcastMorphModule::SM7B_Broadcast:
                    // Sub cut, 180Hz body, 4.2kHz presence bump, 11kHz air roll-off
                    if (f < 80.0f) db = -8.0f * (1.0f - f / 80.0f);
                    else if (f < 400.0f) db = 2.5f * std::exp (-std::pow ((f - 180.0f) / 140.0f, 2.0f));
                    else if (f > 2500.0f && f < 7500.0f) db = 5.2f * std::exp (-std::pow ((f - 4200.0f) / 1600.0f, 2.0f));
                    else if (f >= 7500.0f) db = -3.5f * ((f - 7500.0f) / 6500.0f);
                    break;

                case BroadcastMorphModule::Neumann_U87:
                    // Linear open low, 3.2kHz clarity, 10.5kHz shimmering air sheen
                    if (f < 50.0f) db = -1.0f;
                    else if (f > 2000.0f && f < 5000.0f) db = 2.0f * std::exp (-std::pow ((f - 3200.0f) / 1200.0f, 2.0f));
                    else if (f >= 5000.0f) db = 5.0f * ((f - 5000.0f) / 7000.0f);
                    break;

                case BroadcastMorphModule::EV_RE20:
                    // Tight flat low, strong 2.8kHz speech articulation
                    if (f < 70.0f) db = -4.0f;
                    else if (f > 1800.0f && f < 5500.0f) db = 4.8f * std::exp (-std::pow ((f - 2800.0f) / 1100.0f, 2.0f));
                    else if (f >= 8000.0f) db = -2.0f;
                    break;

                case BroadcastMorphModule::Vintage_Tube_1950s:
                    // Heavy warm 220Hz chest, rolled off highs above 6kHz
                    if (f < 500.0f) db = 5.5f * std::exp (-std::pow ((f - 220.0f) / 180.0f, 2.0f));
                    else if (f >= 4000.0f) db = -8.0f * ((f - 4000.0f) / 8000.0f);
                    break;

                case BroadcastMorphModule::Tactical_Comm:
                    // Steep bandpass: cut below 400Hz and above 3.5kHz, sharp 1.6kHz peak
                    if (f < 400.0f) db = -18.0f;
                    else if (f > 3600.0f) db = -22.0f;
                    else db = 8.5f * std::exp (-std::pow ((f - 1600.0f) / 600.0f, 2.0f));
                    break;

                case BroadcastMorphModule::Megaphone_Horn:
                    // Harsh megaphone horn resonance
                    if (f < 550.0f) db = -20.0f;
                    else if (f > 3400.0f) db = -18.0f;
                    else db = 11.0f * std::exp (-std::pow ((f - 1850.0f) / 500.0f, 2.0f));
                    break;
            }

            // Map dB (-18 to +14) to Y coordinate
            float y = midY - (db / 16.0f) * (box.getHeight() * 0.40f);

            if (! started) { curve.startNewSubPath (x, y); started = true; }
            else           { curve.lineTo (x, y); }
        }

        // Shaded under curve
        juce::Path filled = curve;
        filled.lineTo (box.getRight() - 8.0f, midY);
        filled.lineTo (startX, midY);
        filled.closeSubPath();

        juce::Colour curveCol = profile == BroadcastMorphModule::Neumann_U87 ? UITheme::appleBlue
                              : (profile == BroadcastMorphModule::Tactical_Comm ? UITheme::appleGreen
                              : (profile == BroadcastMorphModule::Megaphone_Horn ? UITheme::appleRed
                              : juce::Colour (0xffff9f0a)));

        g.setColour (curveCol.withAlpha (0.15f));
        g.fillPath (filled);

        g.setColour (curveCol);
        g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Frequency Labels
        g.setColour (UITheme::textTertiary);
        g.setFont (UITheme::getFont (7.5f));
        g.drawText ("50 Hz", juce::Rectangle<float> (box.getX() + 6.0f, box.getBottom() - 13.0f, 40.0f, 10.0f), juce::Justification::bottomLeft);
        g.drawText ("14 kHz", juce::Rectangle<float> (box.getRight() - 46.0f, box.getBottom() - 13.0f, 40.0f, 10.0f), juce::Justification::bottomRight);
    }

    BroadcastMorphModule& module;
    juce::OwnedArray<juce::TextButton> profileButtons;
    juce::Slider morphSlider, driveSlider, tiltSlider, outputSlider;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> morphAttach, driveAttach, tiltAttach, outputAttach;
};

inline juce::AudioProcessorEditor* BroadcastMorphModule::createEditor()
{
    return new BroadcastMorphModuleEditor (*this, apvts);
}
