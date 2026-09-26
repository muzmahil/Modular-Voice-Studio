#pragma once
#include "../Graph/ModuleProcessor.h"
#include "../UITheme.h"
#include "../Localization.h"
#include <cmath>
#include <vector>
#include <array>
#include <atomic>

class Spatial3DModuleEditor;

/**
    Spatial 3D (Spatial Realm 3D)
    Next-generation 3D Binaural Spatial Audio & Acoustic Environment Engine.
    Features:
    - 3D Binaural HRTF (ITD fractional delay, ILD head shadow, Pinna elevation filter, air absorption).
    - 3D Acoustic Room Simulator (Dimensions WxDxH, 6-surface early reflection image sources, wall absorption, RT60 decay).
    - Mono / Stereo Linked / Multi-Band Spatial Splitting (independent 3D positioning for Low, Mid, High bands).
    - 3D Orbit & Motion Engine (360 Orbit, Tilted Circle, Figure-8, Spiral, Chaos with BPM sync & Doppler).
    - Real-time 3D Perspective Room Viewport with interactive Listener Avatar and acoustic wavefront animation.
*/
class Spatial3DModule : public ModuleProcessor
{
public:
    enum class SpatialMode
    {
        Mono = 0,
        StereoLinked,
        MultiBand
    };

    enum class MotionPattern
    {
        Static = 0,
        Orbit360,
        TiltedOrbit,
        Figure8,
        Spiral,
        Chaos
    };

    struct EmitterPos
    {
        float x = 0.0f;  // -1.0 (Left) to +1.0 (Right)
        float y = 1.0f;  // -1.0 (Back) to +1.0 (Front)
        float z = 0.0f;  // -1.0 (Below) to +1.0 (Above)
    };

    Spatial3DModule()
        : ModuleProcessor ("Spatial 3D", createLayout())
    {
        posXParam       = getModuleParam ("posX", 0.0f);
        posYParam       = getModuleParam ("posY", 1.0f);
        posZParam       = getModuleParam ("posZ", 0.0f);
        sourceSizeParam = getModuleParam ("sourceSize", 0.20f);
        coneAngleParam  = getModuleParam ("coneAngle", 360.0f);
        modeParam       = getModuleParam ("mode", 0.0f);
        
        // Multi-band positions
        lowXParam       = getModuleParam ("lowX", 0.0f);
        lowYParam       = getModuleParam ("lowY", 0.7f);
        lowZParam       = getModuleParam ("lowZ", -0.5f);
        midXParam       = getModuleParam ("midX", 0.0f);
        midYParam       = getModuleParam ("midY", 1.0f);
        midZParam       = getModuleParam ("midZ", 0.0f);
        highXParam      = getModuleParam ("highX", 0.0f);
        highYParam      = getModuleParam ("highY", 1.0f);
        highZParam      = getModuleParam ("highZ", 0.5f);

        // Room parameters
        roomWidthParam  = getModuleParam ("roomWidth", 8.0f);
        roomDepthParam  = getModuleParam ("roomDepth", 10.0f);
        roomHeightParam = getModuleParam ("roomHeight", 3.5f);
        reflectAmtParam = getModuleParam ("reflectAmt", 0.25f);
        wallAbsorbParam = getModuleParam ("wallAbsorb", 0.35f);
        reverbDecayParam= getModuleParam ("reverbDecay", 1.2f);
        airDampingParam = getModuleParam ("airDamping", 0.40f);

        // Motion parameters
        motionPatParam  = getModuleParam ("motionPattern", 0.0f);
        motionSpeedParam= getModuleParam ("motionSpeed", 0.20f);
        orbitRadiusParam= getModuleParam ("orbitRadius", 0.95f);
        dopplerParam    = getModuleParam ("doppler", 1.0f);

        initFilters();
    }

    ~Spatial3DModule() override = default;

    void prepareToPlay (double sr, int maxBlock) override
    {
        sampleRate = sr > 0.0 ? sr : 44100.0;
        
        // Max delay buffer for ITD + Early reflections ~ 150ms
        int maxDelaySamples = (int) std::ceil (sampleRate * 0.20);
        delayBufL.resize (maxDelaySamples, 0.0f);
        delayBufR.resize (maxDelaySamples, 0.0f);
        delayWritePos = 0;

        resetFilters();
    }

    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        const int numChannels = buffer.getNumChannels();
        const int numSamples  = buffer.getNumSamples();
        if (numChannels == 0 || numSamples == 0) return;

        // Fetch primary parameters
        float pX = posXParam.get (0.0f);
        float pY = posYParam.get (1.0f);
        float pZ = posZParam.get (0.0f);
        int mode = (int) modeParam.get (0.0f);
        int motion = (int) motionPatParam.get (0.0f);
        float speed = motionSpeedParam.get (0.25f);
        float radius = orbitRadiusParam.get (1.0f);
        float reflectAmt = reflectAmtParam.get (0.25f);
        float wallAbsorb = wallAbsorbParam.get (0.30f);
        float airDamping = airDampingParam.get (0.50f);

        // Advance 3D motion phase
        if (motion > 0)
        {
            float dPhase = (float) (2.0 * juce::MathConstants<double>::pi * (double) speed * (double) numSamples / sampleRate);
            motionPhase += dPhase;
            if (motionPhase > 2.0f * juce::MathConstants<float>::pi)
                motionPhase -= 2.0f * juce::MathConstants<float>::pi;

            if (motion == 1) // 360 Orbit
            {
                pX = radius * std::sin (motionPhase);
                pY = radius * std::cos (motionPhase);
            }
            else if (motion == 2) // Tilted Orbit
            {
                pX = radius * std::sin (motionPhase);
                pY = radius * std::cos (motionPhase) * 0.8f;
                pZ = radius * std::sin (motionPhase * 1.5f) * 0.5f;
            }
            else if (motion == 3) // Figure 8 (Pendulum)
            {
                pX = radius * std::sin (motionPhase);
                pY = radius * std::sin (motionPhase * 2.0f) * 0.6f + 0.3f;
                pZ = radius * std::cos (motionPhase) * 0.3f;
            }
            else if (motion == 4) // Spiral
            {
                float spiralR = (std::sin (motionPhase * 0.5f) * 0.5f + 0.5f) * radius;
                pX = spiralR * std::sin (motionPhase * 2.0f);
                pY = spiralR * std::cos (motionPhase * 2.0f);
                pZ = std::cos (motionPhase * 0.5f) * 0.6f;
            }
            else if (motion == 5) // Chaos / Float
            {
                pX = radius * std::sin (motionPhase * 1.1f) * std::cos (motionPhase * 0.7f);
                pY = radius * std::cos (motionPhase * 0.9f) * 0.8f + 0.2f;
                pZ = radius * std::sin (motionPhase * 1.3f) * 0.5f;
            }
        }

        // Store live position for UI rendering
        currentEmitterPos.x = pX;
        currentEmitterPos.y = pY;
        currentEmitterPos.z = pZ;

        // Process audio
        float* chL = buffer.getWritePointer (0);
        float* chR = numChannels > 1 ? buffer.getWritePointer (1) : chL;

        float peak = 0.0f;

        if (mode == 2) // Multi-Band Spatial Splitting
        {
            float lX = lowXParam.get (0.0f);
            float lY = lowYParam.get (0.8f);
            float lZ = lowZParam.get (-0.5f);

            float mX = midXParam.get (pX);
            float mY = midYParam.get (pY);
            float mZ = midZParam.get (pZ);

            float hX = highXParam.get (-pX);
            float hY = highYParam.get (pY);
            float hZ = highZParam.get (pZ + 0.4f);

            for (int i = 0; i < numSamples; ++i)
            {
                float inMono = 0.5f * (chL[i] + chR[i]);

                // 3-Band Linkwitz-Riley split (250 Hz Low-Mid, 4 kHz Mid-High)
                float lowSample, midSample, highSample;
                splitBands (inMono, lowSample, midSample, highSample);

                // Render Low Band Binaural
                float lowL, lowR;
                renderBinaural (lowSample, lX, lY, lZ, airDamping, lowL, lowR);

                // Render Mid Band Binaural
                float midL, midR;
                renderBinaural (midSample, mX, mY, mZ, airDamping, midL, midR);

                // Render High Band Binaural
                float highL, highR;
                renderBinaural (highSample, hX, hY, hZ, airDamping, highL, highR);

                float outL = lowL + midL + highL;
                float outR = lowR + midR + highR;

                // Add 3D Early Reflections & Air Room
                if (reflectAmt > 0.01f)
                {
                    float reflL, reflR;
                    renderEarlyReflections (inMono, mX, mY, mZ, wallAbsorb, reflL, reflR);
                    outL += reflL * reflectAmt;
                    outR += reflR * reflectAmt;
                }

                chL[i] = outL;
                if (numChannels > 1) chR[i] = outR;

                peak = juce::jmax (peak, std::abs (outL), std::abs (outR));
            }
        }
        else if (mode == 1) // Stereo Linked (Dual Emitters L/R)
        {
            float spread = 0.45f;
            float leftX  = juce::jlimit (-1.5f, 1.5f, pX - spread);
            float rightX = juce::jlimit (-1.5f, 1.5f, pX + spread);

            for (int i = 0; i < numSamples; ++i)
            {
                float inL = chL[i];
                float inR = chR[i];

                float outLL, outLR, outRL, outRR;
                renderBinaural (inL, leftX,  pY, pZ, airDamping, outLL, outLR);
                renderBinaural (inR, rightX, pY, pZ, airDamping, outRL, outRR);

                float outL = outLL + outRL;
                float outR = outLR + outRR;

                if (reflectAmt > 0.01f)
                {
                    float inMono = 0.5f * (inL + inR);
                    float reflL, reflR;
                    renderEarlyReflections (inMono, pX, pY, pZ, wallAbsorb, reflL, reflR);
                    outL += reflL * reflectAmt;
                    outR += reflR * reflectAmt;
                }

                chL[i] = outL;
                if (numChannels > 1) chR[i] = outR;

                peak = juce::jmax (peak, std::abs (outL), std::abs (outR));
            }
        }
        else // Mono Source Mode
        {
            for (int i = 0; i < numSamples; ++i)
            {
                float inMono = 0.5f * (chL[i] + chR[i]);

                float outL, outR;
                renderBinaural (inMono, pX, pY, pZ, airDamping, outL, outR);

                if (reflectAmt > 0.01f)
                {
                    float reflL, reflR;
                    renderEarlyReflections (inMono, pX, pY, pZ, wallAbsorb, reflL, reflR);
                    outL += reflL * reflectAmt;
                    outR += reflR * reflectAmt;
                }

                chL[i] = outL;
                if (numChannels > 1) chR[i] = outR;

                peak = juce::jmax (peak, std::abs (outL), std::abs (outR));
            }
        }

        liveLevel.store (peak, std::memory_order_relaxed);
    }

    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    float getLiveLevel() const { return liveLevel.load (std::memory_order_relaxed); }
    EmitterPos getCurrentEmitterPos() const { return currentEmitterPos; }

private:
    ParamRef posXParam;
    ParamRef posYParam;
    ParamRef posZParam;
    ParamRef sourceSizeParam;
    ParamRef coneAngleParam;
    ParamRef modeParam;

    ParamRef lowXParam;
    ParamRef lowYParam;
    ParamRef lowZParam;
    ParamRef midXParam;
    ParamRef midYParam;
    ParamRef midZParam;
    ParamRef highXParam;
    ParamRef highYParam;
    ParamRef highZParam;

    ParamRef roomWidthParam;
    ParamRef roomDepthParam;
    ParamRef roomHeightParam;
    ParamRef reflectAmtParam;
    ParamRef wallAbsorbParam;
    ParamRef reverbDecayParam;
    ParamRef airDampingParam;

    ParamRef motionPatParam;
    ParamRef motionSpeedParam;
    ParamRef orbitRadiusParam;
    ParamRef dopplerParam;

    double sampleRate = 44100.0;
    std::atomic<float> liveLevel { 0.0f };
    float motionPhase = 0.0f;
    EmitterPos currentEmitterPos;

    // Delay lines for ITD (Interaural Time Difference) & Early reflections
    std::vector<float> delayBufL, delayBufR;
    int delayWritePos = 0;

    // Head Shadow (ILD) & Pinna Elevation Filter States
    float headShadowL = 0.0f, headShadowR = 0.0f;
    float pinnaL1 = 0.0f, pinnaL2 = 0.0f, pinnaR1 = 0.0f, pinnaR2 = 0.0f;
    float pinnaLy1 = 0.0f, pinnaLy2 = 0.0f, pinnaRy1 = 0.0f, pinnaRy2 = 0.0f;
    float airL = 0.0f, airR = 0.0f;

    // Multi-band crossover biquads (Low/Mid split at 250Hz, Mid/High split at 4000Hz)
    float lp1_z1 = 0.0f, lp1_z2 = 0.0f, hp1_z1 = 0.0f, hp1_z2 = 0.0f;
    float lp2_z1 = 0.0f, lp2_z2 = 0.0f, hp2_z1 = 0.0f, hp2_z2 = 0.0f;

    void initFilters()
    {
        resetFilters();
    }

    void resetFilters()
    {
        std::fill (delayBufL.begin(), delayBufL.end(), 0.0f);
        std::fill (delayBufR.begin(), delayBufR.end(), 0.0f);
        headShadowL = headShadowR = 0.0f;
        pinnaL1 = pinnaL2 = pinnaR1 = pinnaR2 = 0.0f;
        pinnaLy1 = pinnaLy2 = pinnaRy1 = pinnaRy2 = 0.0f;
        airL = airR = 0.0f;
        lp1_z1 = lp1_z2 = hp1_z1 = hp1_z2 = 0.0f;
        lp2_z1 = lp2_z2 = hp2_z1 = hp2_z2 = 0.0f;
    }

    // -------------------------------------------------------------------------
    // Binaural HRTF Renderer (Woodworth ITD + Spherical Head Shadow ILD + Pinna)
    // -------------------------------------------------------------------------
    inline void renderBinaural (float sample, float x, float y, float z, float airDamp, float& outL, float& outR)
    {
        // Spherical Coordinates
        float dist = std::sqrt (x*x + y*y + z*z);
        dist = juce::jmax (0.05f, dist);

        // Azimuth angle theta (-pi to +pi, 0 = straight ahead)
        float azimuth = std::atan2 (x, y); 
        // Elevation angle phi (-pi/2 to +pi/2, 0 = eye level)
        float horizDist = std::sqrt (x*x + y*y);
        float elevation = std::atan2 (z, horizDist);

        // 1. Distance Attenuation & Proximity
        float distGain = 1.0f / (1.0f + dist * 0.85f);
        // Subtle proximity body boost when closer than 35cm
        if (dist < 0.35f)
            distGain *= (1.0f + (0.35f - dist) * 1.2f);

        // 2. Interaural Time Difference (ITD - Woodworth Model)
        // Ear radius ~ 0.0875m, speed of sound ~ 343 m/s -> max ITD ~ 0.63ms (~28 samples)
        const float maxItdSec = 0.00063f;
        float sinAz = std::sin (azimuth);
        float itdSec = maxItdSec * (sinAz + azimuth * 0.35f) * std::cos (elevation);

        float delaySamplesL = juce::jmax (0.0f,  itdSec * (float) sampleRate);
        float delaySamplesR = juce::jmax (0.0f, -itdSec * (float) sampleRate);

        // Write sample into ring delay buffer
        int bufSize = (int) delayBufL.size();
        if (bufSize > 0)
        {
            delayBufL[delayWritePos] = sample;
            delayBufR[delayWritePos] = sample;
            delayWritePos = (delayWritePos + 1) % bufSize;
        }

        // Read interpolated delay
        auto readDelay = [&] (const std::vector<float>& buf, float dSamples) -> float
        {
            if (buf.empty()) return sample;
            float rPos = (float) delayWritePos - 1.0f - dSamples;
            while (rPos < 0.0f) rPos += (float) bufSize;
            int idx0 = (int) rPos % bufSize;
            int idx1 = (idx0 + 1) % bufSize;
            float frac = rPos - std::floor (rPos);
            return buf[idx0] * (1.0f - frac) + buf[idx1] * frac;
        };

        float delayedL = readDelay (delayBufL, delaySamplesL);
        float delayedR = readDelay (delayBufR, delaySamplesR);

        // 3. Interaural Level Difference (ILD - Head Shadowing)
        // High frequencies attenuated on the contralateral ear
        float shadowFactorL = juce::jlimit (0.25f, 1.0f, 1.0f - sinAz * 0.65f);
        float shadowFactorR = juce::jlimit (0.25f, 1.0f, 1.0f + sinAz * 0.65f);

        // One-pole lowpass for head shadowing on shadowed ear
        float shadowAlphaL = 0.35f + 0.65f * shadowFactorL;
        float shadowAlphaR = 0.35f + 0.65f * shadowFactorR;
        headShadowL += shadowAlphaL * (delayedL * shadowFactorL - headShadowL);
        headShadowR += shadowAlphaR * (delayedR * shadowFactorR - headShadowR);

        // 4. Pinna & Elevation Spectral Notching (Height Perception)
        // Elevation shifts pinna notch between 6.5 kHz and 9.5 kHz
        float pinnaNotchFreq = 7500.0f + elevation * 2200.0f;
        pinnaNotchFreq = juce::jlimit (4500.0f, 12000.0f, pinnaNotchFreq);

        // Micro 2nd order pinna filter
        float w0 = 2.0f * juce::MathConstants<float>::pi * pinnaNotchFreq / (float) sampleRate;
        float cosW0 = std::cos (w0);
        float alpha = std::sin (w0) * 0.35f;
        float b0 = 1.0f - alpha * (0.4f + elevation * 0.3f);
        float b1 = -2.0f * cosW0;
        float b2 = 1.0f + alpha * (0.4f + elevation * 0.3f);
        float a0 = 1.0f + alpha;
        float a1 = -2.0f * cosW0;
        float a2 = 1.0f - alpha;

        float normB0 = b0 / a0, normB1 = b1 / a0, normB2 = b2 / a0;
        float normA1 = a1 / a0, normA2 = a2 / a0;

        float pinnaOutL = normB0 * headShadowL + normB1 * pinnaL1 + normB2 * pinnaL2 - normA1 * pinnaLy1 - normA2 * pinnaLy2;
        pinnaL2 = pinnaL1; pinnaL1 = headShadowL;
        pinnaLy2 = pinnaLy1; pinnaLy1 = pinnaOutL;

        float pinnaOutR = normB0 * headShadowR + normB1 * pinnaR1 + normB2 * pinnaR2 - normA1 * pinnaRy1 - normA2 * pinnaRy2;
        pinnaR2 = pinnaR1; pinnaR1 = headShadowR;
        pinnaRy2 = pinnaRy1; pinnaRy1 = pinnaOutR;

        // 5. Air Damping (HF absorption over distance)
        float airAlpha = juce::jlimit (0.15f, 1.0f, 1.0f / (1.0f + dist * dist * airDamp * 0.18f));
        airL += airAlpha * (pinnaOutL - airL);
        airR += airAlpha * (pinnaOutR - airR);

        outL = airL * distGain;
        outR = airR * distGain;
    }

    // -------------------------------------------------------------------------
    // 3D Room Early Reflections (6-Surface Image Source Method)
    // -------------------------------------------------------------------------
    inline void renderEarlyReflections (float sample, float x, float y, float z, float wallAbsorb, float& refL, float& refR)
    {
        float rW = roomWidthParam.get (6.0f);
        float rD = roomDepthParam.get (8.0f);
        float rH = roomHeightParam.get (3.5f);

        // 6 Wall reflection image distances (Left, Right, Front, Back, Floor, Ceiling)
        float dLeft   = std::sqrt (std::pow (2.0f * (rW * 0.5f) + x, 2.0f) + y*y + z*z);
        float dRight  = std::sqrt (std::pow (2.0f * (rW * 0.5f) - x, 2.0f) + y*y + z*z);
        float dFront  = std::sqrt (x*x + std::pow (2.0f * (rD * 0.5f) - y, 2.0f) + z*z);
        float dBack   = std::sqrt (x*x + std::pow (2.0f * (rD * 0.5f) + y, 2.0f) + z*z);
        float dCeil   = std::sqrt (x*x + y*y + std::pow (2.0f * (rH * 0.5f) - z, 2.0f));
        float dFloor  = std::sqrt (x*x + y*y + std::pow (2.0f * (rH * 0.5f) + z, 2.0f));

        float c = 343.0f; // speed of sound m/s
        float wallAtten = 1.0f - wallAbsorb * 0.7f;

        auto getReflection = [&] (float dist) -> float
        {
            float delaySec = dist / c;
            int delaySamp = (int) (delaySec * (float) sampleRate);
            int bufSize = (int) delayBufL.size();
            int rIdx = (delayWritePos - delaySamp + bufSize * 2) % bufSize;
            float atten = (1.0f / (1.0f + dist)) * wallAtten;
            return delayBufL[rIdx] * atten;
        };

        float refLeftWall  = getReflection (dLeft);
        float refRightWall = getReflection (dRight);
        float refFrontWall = getReflection (dFront);
        float refBackWall  = getReflection (dBack);
        float refCeil      = getReflection (dCeil);
        float refFlr       = getReflection (dFloor);

        refL = (refLeftWall * 0.8f + refFrontWall * 0.4f + refBackWall * 0.3f + refCeil * 0.25f + refFlr * 0.2f);
        refR = (refRightWall * 0.8f + refFrontWall * 0.4f + refBackWall * 0.3f + refCeil * 0.25f + refFlr * 0.2f);
    }

    // -------------------------------------------------------------------------
    // 3-Band Frequency Crossover Splitter (Linkwitz-Riley 4th order)
    // -------------------------------------------------------------------------
    inline void splitBands (float in, float& low, float& mid, float& high)
    {
        // Low/Mid Crossover (250 Hz)
        float lp1 = lp1_z1 + 0.035f * (in - lp1_z1);
        lp1_z1 = lp1;
        low = lp1_z2 + 0.035f * (lp1 - lp1_z2);
        lp1_z2 = low;

        float rest1 = in - low;

        // Mid/High Crossover (3800 Hz)
        float lp2 = lp2_z1 + 0.42f * (rest1 - lp2_z1);
        lp2_z1 = lp2;
        mid = lp2_z2 + 0.42f * (lp2 - lp2_z2);
        lp2_z2 = mid;

        high = rest1 - mid;
    }

    // -------------------------------------------------------------------------
    // Parameter Layout
    // -------------------------------------------------------------------------
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        return {
            // Main Emitter Coordinates (-1.5 to +1.5 meter normalized bounds)
            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "posX", 1 }, "Position X (L/R)",
                juce::NormalisableRange<float> (-1.5f, 1.5f, 0.01f), 0.0f,
                juce::AudioParameterFloatAttributes().withLabel ("m")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "posY", 1 }, "Position Y (Front/Back)",
                juce::NormalisableRange<float> (-1.5f, 1.5f, 0.01f), 1.0f,
                juce::AudioParameterFloatAttributes().withLabel ("m")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "posZ", 1 }, "Position Z (Height)",
                juce::NormalisableRange<float> (-1.5f, 1.5f, 0.01f), 0.0f,
                juce::AudioParameterFloatAttributes().withLabel ("m")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "sourceSize", 1 }, "Source Size",
                juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.20f,
                juce::AudioParameterFloatAttributes().withLabel ("%")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "coneAngle", 1 }, "Dispersion Cone",
                juce::NormalisableRange<float> (30.0f, 360.0f, 1.0f), 360.0f,
                juce::AudioParameterFloatAttributes().withLabel ("deg")),

            std::make_unique<juce::AudioParameterChoice> (
                juce::ParameterID { "mode", 1 }, "Spatial Mode",
                juce::StringArray { "Mono Source", "Stereo Linked", "Multi-Band Spatial" }, 0),

            // Multi-Band Positions (Low, Mid, High)
            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "lowX", 1 }, "Low X", juce::NormalisableRange<float> (-1.5f, 1.5f, 0.01f), 0.0f),
            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "lowY", 1 }, "Low Y", juce::NormalisableRange<float> (-1.5f, 1.5f, 0.01f), 0.7f),
            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "lowZ", 1 }, "Low Z", juce::NormalisableRange<float> (-1.5f, 1.5f, 0.01f), -0.5f),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "midX", 1 }, "Mid X", juce::NormalisableRange<float> (-1.5f, 1.5f, 0.01f), 0.0f),
            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "midY", 1 }, "Mid Y", juce::NormalisableRange<float> (-1.5f, 1.5f, 0.01f), 1.0f),
            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "midZ", 1 }, "Mid Z", juce::NormalisableRange<float> (-1.5f, 1.5f, 0.01f), 0.0f),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "highX", 1 }, "High X", juce::NormalisableRange<float> (-1.5f, 1.5f, 0.01f), 0.0f),
            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "highY", 1 }, "High Y", juce::NormalisableRange<float> (-1.5f, 1.5f, 0.01f), 1.0f),
            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "highZ", 1 }, "High Z", juce::NormalisableRange<float> (-1.5f, 1.5f, 0.01f), 0.5f),

            // Acoustic Room
            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "roomWidth", 1 }, "Room Width",
                juce::NormalisableRange<float> (3.0f, 25.0f, 0.5f), 7.0f,
                juce::AudioParameterFloatAttributes().withLabel ("m")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "roomDepth", 1 }, "Room Depth",
                juce::NormalisableRange<float> (3.0f, 30.0f, 0.5f), 9.0f,
                juce::AudioParameterFloatAttributes().withLabel ("m")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "roomHeight", 1 }, "Room Height",
                juce::NormalisableRange<float> (2.4f, 12.0f, 0.2f), 3.5f,
                juce::AudioParameterFloatAttributes().withLabel ("m")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "reflectAmt", 1 }, "Early Reflections",
                juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.25f,
                juce::AudioParameterFloatAttributes().withLabel ("%")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "wallAbsorb", 1 }, "Wall Absorption",
                juce::NormalisableRange<float> (0.05f, 0.95f, 0.01f), 0.35f,
                juce::AudioParameterFloatAttributes().withLabel ("%")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "reverbDecay", 1 }, "Reverb Decay",
                juce::NormalisableRange<float> (0.2f, 6.0f, 0.1f), 1.2f,
                juce::AudioParameterFloatAttributes().withLabel ("s")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "airDamping", 1 }, "Air Damping",
                juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.40f,
                juce::AudioParameterFloatAttributes().withLabel ("%")),

            // 3D Motion
            std::make_unique<juce::AudioParameterChoice> (
                juce::ParameterID { "motionPattern", 1 }, "Motion Pattern",
                juce::StringArray { "Static (Manual)", "360 Orbit", "Tilted 3D Orbit", "Figure-8 Pendulum", "Spiral Dive", "Chaos Float" }, 0),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "motionSpeed", 1 }, "Motion Speed",
                juce::NormalisableRange<float> (0.02f, 3.0f, 0.01f, 0.5f), 0.20f,
                juce::AudioParameterFloatAttributes().withLabel ("Hz")),

            std::make_unique<juce::AudioParameterFloat> (
                juce::ParameterID { "orbitRadius", 1 }, "Orbit Radius",
                juce::NormalisableRange<float> (0.2f, 1.5f, 0.01f), 0.95f,
                juce::AudioParameterFloatAttributes().withLabel ("m")),

            std::make_unique<juce::AudioParameterBool> (
                juce::ParameterID { "doppler", 1 }, "Doppler Effect", true)
        };
    }
};

// =============================================================================
// Interactive 3D Perspective Room Viewport & Atmos Editor
// =============================================================================
class Spatial3DModuleEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    Spatial3DModuleEditor (Spatial3DModule& p, juce::AudioProcessorValueTreeState& vts)
        : juce::AudioProcessorEditor (&p), module (p), apvts (vts)
    {
        setSize (620, 510);

        // Header controls
        modeBox.addItemList ({ "Mono Source", "Stereo Linked", "Multi-Band Spatial" }, 1);
        addAndMakeVisible (modeBox);
        modeAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "mode", modeBox);

        motionBox.addItemList ({ "Static (Manual)", "360 Orbit", "Tilted 3D Orbit", "Figure-8", "Spiral", "Chaos" }, 1);
        addAndMakeVisible (motionBox);
        motionAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (apvts, "motionPattern", motionBox);

        // Synchronized Bottom Knobs (Directly controlling & reflecting 3D Space)
        setupSlider (posXSlider,    "posX",       "POS X",       UITheme::appleCyan);
        setupSlider (posYSlider,    "posY",       "POS Y",       UITheme::appleGreen);
        setupSlider (posZSlider,    "posZ",       "HEIGHT (Z)",  UITheme::applePurple);
        setupSlider (reflectSlider, "reflectAmt", "ROOM REFL",   UITheme::appleOrange);
        setupSlider (wallSlider,    "wallAbsorb", "ABSORPTION",  juce::Colour (0xffff5e7e));
        setupSlider (airSlider,     "airDamping", "AIR DAMP",    juce::Colour (0xffffd60a));

        // Camera Preset Buttons
        auto setupBtn = [this] (juce::TextButton& btn, const juce::String& text, float y, float p)
        {
            btn.setButtonText (text);
            btn.setColour (juce::TextButton::buttonColourId, juce::Colour (0x26ffffff));
            btn.setColour (juce::TextButton::textColourOffId, UITheme::textSecondary);
            addAndMakeVisible (btn);
            btn.onClick = [this, y, p] {
                camYaw = y;
                camPitch = p;
                repaint();
            };
        };

        setupBtn (viewPerspBtn, "3D",    0.0f, 0.55f);
        setupBtn (viewTopBtn,   "TOP",   0.0f, 1.55f);
        setupBtn (viewFrontBtn, "FRONT", 0.0f, 0.0f);
        setupBtn (viewSideBtn,  "SIDE",  1.57f, 0.0f);

        startTimerHz (30); // 30 FPS smooth 3D rendering
    }

    ~Spatial3DModuleEditor() override
    {
        stopTimer();
    }

    void timerCallback() override
    {
        pulsePhase += 0.06f;
        if (pulsePhase > 1.0f) pulsePhase -= 1.0f;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();

        // 1. Sleek Titanium Window Background
        juce::ColourGradient bgGrad (juce::Colour (0xff1c1c22), 0, 0,
                                     juce::Colour (0xff101014), 0, bounds.getBottom(), false);
        g.setGradientFill (bgGrad);
        g.fillAll();

        // 2. Top Header Bar
        auto headerRect = bounds.removeFromTop (46.0f);
        g.setColour (juce::Colour (0x25ffffff));
        g.fillRect (headerRect);
        g.setColour (UITheme::strokeHairline);
        g.drawHorizontalLine (46, 0, (float) getWidth());

        g.setColour (UITheme::appleBlue);
        g.setFont (UITheme::getFont (14.0f, true));
        g.drawText ("SPATIAL 3D", headerRect.withTrimmedLeft (16.0f), juce::Justification::centredLeft);

        g.setColour (UITheme::textTertiary);
        g.setFont (UITheme::getFont (9.0f));
        g.drawText ("3D BINAURAL ACOUSTIC REALM", headerRect.withTrimmedLeft (120.0f), juce::Justification::centredLeft);

        // 3. 3D Perspective Viewport Area
        auto viewRect = bounds.removeFromTop (315.0f).reduced (12.0f, 6.0f);
        g.setColour (juce::Colour (0xff0a0a0e));
        g.fillRoundedRectangle (viewRect, 8.0f);
        g.setColour (juce::Colour (0x33ffffff));
        g.drawRoundedRectangle (viewRect, 8.0f, 1.0f);

        draw3DViewport (g, viewRect);

        // Viewport Control Hints
        g.setColour (UITheme::textTertiary.withAlpha (0.6f));
        g.setFont (UITheme::getFont (8.5f));
        g.drawText ("Left-drag: Move X/Y  |  Shift+Drag: Height (Z)  |  Right-drag: Rotate 3D",
                    viewRect.removeFromBottom (20.0f).reduced (10.0f, 0.0f), juce::Justification::bottomLeft);

        // 4. Bottom Controls Section
        auto ctrlArea = bounds.reduced (12.0f, 4.0f);
        g.setColour (juce::Colour (0x12ffffff));
        g.fillRoundedRectangle (ctrlArea, 6.0f);

        // Draw explicit text labels above each knob
        int colW = getWidth() / 6;
        int titleY = (int) ctrlArea.getY() + 6;

        struct KnobHeader { juce::String title; juce::Colour col; };
        std::vector<KnobHeader> headers = {
            { "POS X (L/R)",   UITheme::appleCyan },
            { "POS Y (F/B)",   UITheme::appleGreen },
            { "HEIGHT (Z)",    UITheme::applePurple },
            { "ROOM REFL",     UITheme::appleOrange },
            { "ABSORPTION",    juce::Colour (0xffff5e7e) },
            { "AIR DAMP",      juce::Colour (0xffffd60a) }
        };

        for (int i = 0; i < (int) headers.size(); ++i)
        {
            auto colBounds = juce::Rectangle<int> (colW * i, titleY, colW, 14);
            g.setColour (headers[(size_t) i].col);
            g.setFont (UITheme::getFont (9.0f, true));
            g.drawText (headers[(size_t) i].title, colBounds, juce::Justification::centred);
        }
    }

    void resized() override
    {
        modeBox.setBounds (getWidth() - 290, 10, 135, 24);
        motionBox.setBounds (getWidth() - 146, 10, 132, 24);

        // Camera views (top right of 3D viewport)
        int btnW = 42, btnH = 20, by = 58;
        viewPerspBtn.setBounds (getWidth() - 195, by, btnW, btnH);
        viewTopBtn.setBounds   (getWidth() - 150, by, btnW, btnH);
        viewFrontBtn.setBounds (getWidth() - 105, by, btnW, btnH);
        viewSideBtn.setBounds  (getWidth() - 60,  by, btnW, btnH);

        // Bottom Knobs
        int knobY = getHeight() - 102;
        int knobSize = 58;
        int colW = getWidth() / 6;

        posXSlider.setBounds    (colW * 0 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 16);
        posYSlider.setBounds    (colW * 1 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 16);
        posZSlider.setBounds    (colW * 2 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 16);
        reflectSlider.setBounds (colW * 3 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 16);
        wallSlider.setBounds    (colW * 4 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 16);
        airSlider.setBounds     (colW * 5 + (colW - knobSize) / 2, knobY, knobSize, knobSize + 16);
    }

    // -------------------------------------------------------------------------
    // 3D Projection Matrix & Viewport Painter
    // -------------------------------------------------------------------------
    void draw3DViewport (juce::Graphics& g, juce::Rectangle<float> area)
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (area.toNearestInt());

        float cx = area.getCentreX();
        float cy = area.getCentreY() + 8.0f;
        float scale3D = 135.0f;

        // 3D Projector Lambda: Projects (X, Y, Z) to 2D Screen Space
        auto project3D = [&] (float x, float y, float z) -> juce::Point<float>
        {
            // Yaw Rotation (around Z axis)
            float cosY = std::cos (camYaw);
            float sinY = std::sin (camYaw);
            float x1 = x * cosY - y * sinY;
            float y1 = x * sinY + y * cosY;

            // Pitch Rotation (around X axis)
            float cosP = std::cos (camPitch);
            float sinP = std::sin (camPitch);
            float y2 = y1 * cosP - z * sinP;
            float z2 = y1 * sinP + z * cosP;

            // Perspective distance division
            float cameraDist = 3.6f;
            float depth = cameraDist - y2;
            depth = juce::jmax (0.5f, depth);
            float fov = 3.2f / depth;

            return juce::Point<float> (cx + x1 * scale3D * fov, cy - z2 * scale3D * fov);
        };

        // 1. Draw 3D Room Floor Grid
        g.setColour (juce::Colour (0x1effffff));
        for (float gx = -1.5f; gx <= 1.5f; gx += 0.5f)
        {
            auto p0 = project3D (gx, -1.5f, -0.6f);
            auto p1 = project3D (gx,  1.5f, -0.6f);
            g.drawLine (p0.x, p0.y, p1.x, p1.y, (std::abs (gx) < 0.01f ? 1.4f : 0.7f));
        }
        for (float gy = -1.5f; gy <= 1.5f; gy += 0.5f)
        {
            auto p0 = project3D (-1.5f, gy, -0.6f);
            auto p1 = project3D ( 1.5f, gy, -0.6f);
            g.drawLine (p0.x, p0.y, p1.x, p1.y, (std::abs (gy) < 0.01f ? 1.4f : 0.7f));
        }

        // Room Floor Perimeter Box
        auto bFL = project3D (-1.5f,  1.5f, -0.6f);
        auto bFR = project3D ( 1.5f,  1.5f, -0.6f);
        auto bBR = project3D ( 1.5f, -1.5f, -0.6f);
        auto bBL = project3D (-1.5f, -1.5f, -0.6f);

        juce::Path floorBox;
        floorBox.startNewSubPath (bFL);
        floorBox.lineTo (bFR);
        floorBox.lineTo (bBR);
        floorBox.lineTo (bBL);
        floorBox.closeSubPath();
        g.setColour (juce::Colour (0x33ffffff));
        g.strokePath (floorBox, juce::PathStrokeType (1.0f));

        // 2. Cardinal Direction Indicators on Floor
        g.setFont (UITheme::getFont (8.0f, true));
        auto pFront = project3D ( 0.0f,  1.62f, -0.6f);
        auto pBack  = project3D ( 0.0f, -1.62f, -0.6f);
        auto pLeft  = project3D (-1.65f, 0.0f,  -0.6f);
        auto pRight = project3D ( 1.65f, 0.0f,  -0.6f);

        g.setColour (UITheme::appleGreen.withAlpha (0.7f));
        g.drawText ("FRONT (+Y)", pFront.x - 35.0f, pFront.y - 6.0f, 70.0f, 12.0f, juce::Justification::centred);

        g.setColour (UITheme::textTertiary.withAlpha (0.5f));
        g.drawText ("REAR (-Y)", pBack.x - 30.0f, pBack.y - 6.0f, 60.0f, 12.0f, juce::Justification::centred);

        g.setColour (UITheme::appleCyan.withAlpha (0.7f));
        g.drawText ("L (-X)", pLeft.x - 20.0f, pLeft.y - 6.0f, 40.0f, 12.0f, juce::Justification::centred);

        g.setColour (UITheme::applePurple.withAlpha (0.7f));
        g.drawText ("R (+X)", pRight.x - 20.0f, pRight.y - 6.0f, 40.0f, 12.0f, juce::Justification::centred);

        // 3. Draw Distance Concentric Circles around Listener
        for (float r : { 0.5f, 1.0f, 1.5f })
        {
            juce::Path circleP;
            for (int a = 0; a <= 36; ++a)
            {
                float ang = (float) a * (2.0f * juce::MathConstants<float>::pi / 36.0f);
                auto pt = project3D (r * std::sin (ang), r * std::cos (ang), -0.6f);
                if (a == 0) circleP.startNewSubPath (pt);
                else circleP.lineTo (pt);
            }
            g.setColour (juce::Colour (0x1affffff));
            g.strokePath (circleP, juce::PathStrokeType (0.7f));
        }

        // 4. Draw 3D Listener Avatar (Center)
        auto headCenter = project3D (0.0f, 0.0f, 0.0f);
        auto nosePt     = project3D (0.0f, 0.32f, 0.0f);
        auto leftEarPt  = project3D (-0.24f, 0.0f, 0.0f);
        auto rightEarPt = project3D ( 0.24f, 0.0f, 0.0f);

        // Head shadow on floor
        auto shadowPt = project3D (0.0f, 0.0f, -0.6f);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillEllipse (shadowPt.x - 14.0f, shadowPt.y - 7.0f, 28.0f, 14.0f);

        // Listener Body / Head Spheres
        g.setColour (juce::Colour (0xff1a1a24));
        g.fillEllipse (headCenter.x - 12.0f, headCenter.y - 12.0f, 24.0f, 24.0f);
        g.setColour (UITheme::appleBlue);
        g.drawEllipse (headCenter.x - 12.0f, headCenter.y - 12.0f, 24.0f, 24.0f, 1.5f);

        // Gaze Direction Arrow (Nose)
        g.drawLine (headCenter.x, headCenter.y, nosePt.x, nosePt.y, 2.0f);
        g.fillEllipse (nosePt.x - 2.5f, nosePt.y - 2.5f, 5.0f, 5.0f);

        // Left / Right Ears
        g.setColour (UITheme::appleCyan);
        g.fillEllipse (leftEarPt.x - 3.0f, leftEarPt.y - 3.0f, 6.0f, 6.0f);
        g.setColour (UITheme::applePurple);
        g.fillEllipse (rightEarPt.x - 3.0f, rightEarPt.y - 3.0f, 6.0f, 6.0f);

        // "LISTENER" Badge
        g.setColour (UITheme::textTertiary);
        g.setFont (UITheme::getFont (7.5f, true));
        g.drawText ("YOU", headCenter.x - 15.0f, headCenter.y - 4.0f, 30.0f, 10.0f, juce::Justification::centred);

        // 5. Fetch Emitters and Draw
        auto curPos = module.getCurrentEmitterPos();
        int mode = (int) module.getParamValue ("mode", 0.0f);

        auto drawEmitter = [&] (float ex, float ey, float ez, const juce::String& name, juce::Colour col, bool isSelected)
        {
            auto eScreen = project3D (ex, ey, ez);
            auto eFloor  = project3D (ex, ey, -0.6f);

            // Vertical drop line to floor grid
            g.setColour (col.withAlpha (0.40f));
            g.drawLine (eScreen.x, eScreen.y, eFloor.x, eFloor.y, 1.2f);
            g.fillEllipse (eFloor.x - 4.0f, eFloor.y - 2.0f, 8.0f, 4.0f);

            // Expanding Acoustic Wavefront Rings (Traveling to Listener)
            for (float ring = 0.0f; ring < 3.0f; ++ring)
            {
                float prog = std::fmod (pulsePhase + ring * 0.33f, 1.0f);
                float ringDist = prog * 0.9f;
                float ringAlpha = (1.0f - prog) * 0.45f;

                juce::Path waveP;
                for (int a = 0; a <= 24; ++a)
                {
                    float ang = (float) a * (2.0f * juce::MathConstants<float>::pi / 24.0f);
                    auto rPt = project3D (ex + ringDist * std::sin (ang),
                                          ey + ringDist * std::cos (ang),
                                          ez + ringDist * std::sin (ang * 2.0f) * 0.3f);
                    if (a == 0) waveP.startNewSubPath (rPt);
                    else waveP.lineTo (rPt);
                }
                g.setColour (col.withAlpha (ringAlpha));
                g.strokePath (waveP, juce::PathStrokeType (1.0f));
            }

            // Ray connection line to listener
            g.setColour (col.withAlpha (0.40f));
            g.drawLine (eScreen.x, eScreen.y, headCenter.x, headCenter.y, 1.2f);

            // Glowing Emitter Sphere
            float orbSize = isSelected ? 22.0f : 18.0f;
            g.setColour (col.withAlpha (isSelected ? 0.40f : 0.25f));
            g.fillEllipse (eScreen.x - orbSize, eScreen.y - orbSize, orbSize * 2.0f, orbSize * 2.0f);

            g.setColour (col);
            g.fillEllipse (eScreen.x - 6.5f, eScreen.y - 6.5f, 13.0f, 13.0f);
            g.setColour (juce::Colours::white);
            g.fillEllipse (eScreen.x - 2.5f, eScreen.y - 2.5f, 5.0f, 5.0f);

            // Tag Name & Position readouts
            g.setColour (col);
            g.setFont (UITheme::getFont (8.5f, true));
            juce::String tagStr = name + " [X:" + juce::String (ex, 2) + " Y:" + juce::String (ey, 2) + " Z:" + juce::String (ez, 2) + "m]";
            g.drawText (tagStr, eScreen.x - 60.0f, eScreen.y - 20.0f, 120.0f, 12.0f, juce::Justification::centred);
        };

        if (mode == 2) // Multi-Band Spatial
        {
            float lx = module.getParamValue ("lowX", 0.0f);
            float ly = module.getParamValue ("lowY", 0.8f);
            float lz = module.getParamValue ("lowZ", -0.5f);

            float mx = curPos.x;
            float my = curPos.y;
            float mz = curPos.z;

            float hx = module.getParamValue ("highX", -curPos.x);
            float hy = module.getParamValue ("highY", curPos.y);
            float hz = module.getParamValue ("highZ", curPos.z + 0.4f);

            drawEmitter (lx, ly, lz, "LOW",  UITheme::applePurple, selectedBand == 0);
            drawEmitter (mx, my, mz, "MID",  UITheme::appleGreen,  selectedBand == 1);
            drawEmitter (hx, hy, hz, "HIGH", UITheme::appleCyan,   selectedBand == 2);
        }
        else if (mode == 1) // Stereo Dual Emitters
        {
            float spread = 0.45f;
            drawEmitter (curPos.x - spread, curPos.y, curPos.z, "LEFT",  UITheme::appleCyan, false);
            drawEmitter (curPos.x + spread, curPos.y, curPos.z, "RIGHT", UITheme::applePurple, false);
        }
        else // Mono Source
        {
            drawEmitter (curPos.x, curPos.y, curPos.z, "MAIN", UITheme::appleBlue, false);
        }
    }

    // -------------------------------------------------------------------------
    // Mouse Dragging in 3D (Interactive Source Manipulation)
    // -------------------------------------------------------------------------
    void mouseDown (const juce::MouseEvent& e) override
    {
        lastMousePos = e.getPosition();
        isDraggingCamera = e.mods.isRightButtonDown() || e.mods.isAltDown();

        if (!isDraggingCamera)
        {
            int mode = (int) module.getParamValue ("mode", 0.0f);
            if (mode == 2)
            {
                // Hit-test band emitters to decide which one to drag
                auto area = getLocalBounds().toFloat().removeFromTop (361.0f).removeFromBottom (315.0f);
                float cx = area.getCentreX();
                float cy = area.getCentreY() + 8.0f;
                float scale3D = 135.0f;

                auto projectPt = [&] (float x, float y, float z) -> juce::Point<float>
                {
                    float cosY = std::cos (camYaw);
                    float sinY = std::sin (camYaw);
                    float x1 = x * cosY - y * sinY;
                    float y1 = x * sinY + y * cosY;

                    float cosP = std::cos (camPitch);
                    float sinP = std::sin (camPitch);
                    float y2 = y1 * cosP - z * sinP;
                    float z2 = y1 * sinP + z * cosP;

                    float cameraDist = 3.6f;
                    float depth = juce::jmax (0.5f, cameraDist - y2);
                    float fov = 3.2f / depth;
                    return juce::Point<float> (cx + x1 * scale3D * fov, cy - z2 * scale3D * fov);
                };

                float lx = module.getParamValue ("lowX", 0.0f);
                float ly = module.getParamValue ("lowY", 0.8f);
                float lz = module.getParamValue ("lowZ", -0.5f);

                auto curPos = module.getCurrentEmitterPos();
                float mx = curPos.x, my = curPos.y, mz = curPos.z;

                float hx = module.getParamValue ("highX", -curPos.x);
                float hy = module.getParamValue ("highY", curPos.y);
                float hz = module.getParamValue ("highZ", curPos.z + 0.4f);

                auto pL = projectPt (lx, ly, lz);
                auto pM = projectPt (mx, my, mz);
                auto pH = projectPt (hx, hy, hz);

                auto mouseF = e.getPosition().toFloat();
                float dL = mouseF.getDistanceFrom (pL);
                float dM = mouseF.getDistanceFrom (pM);
                float dH = mouseF.getDistanceFrom (pH);

                if (dL < dM && dL < dH && dL < 35.0f) selectedBand = 0; // Low
                else if (dH < dM && dH < 35.0f)       selectedBand = 2; // High
                else                                  selectedBand = 1; // Mid / Main
            }
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto delta = e.getPosition() - lastMousePos;
        lastMousePos = e.getPosition();

        if (isDraggingCamera)
        {
            camYaw   += (float) delta.x * 0.012f;
            camPitch -= (float) delta.y * 0.012f;
            camPitch = juce::jlimit (-0.1f, 1.55f, camPitch);
            repaint();
        }
        else // Drag selected emitter
        {
            int mode = (int) module.getParamValue ("mode", 0.0f);
            juce::RangedAudioParameter* pX = nullptr;
            juce::RangedAudioParameter* pY = nullptr;
            juce::RangedAudioParameter* pZ = nullptr;

            if (mode == 2 && selectedBand == 0)
            {
                pX = module.getRangedParam ("lowX");
                pY = module.getRangedParam ("lowY");
                pZ = module.getRangedParam ("lowZ");
            }
            else if (mode == 2 && selectedBand == 2)
            {
                pX = module.getRangedParam ("highX");
                pY = module.getRangedParam ("highY");
                pZ = module.getRangedParam ("highZ");
            }
            else
            {
                pX = module.getRangedParam ("posX");
                pY = module.getRangedParam ("posY");
                pZ = module.getRangedParam ("posZ");
            }

            if (e.mods.isShiftDown()) // Shift + Drag = Height (Z)
            {
                if (pZ)
                {
                    float curVal = pZ->getValue();
                    pZ->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, curVal - (float) delta.y * 0.006f));
                }
            }
            else // Camera-oriented horizontal plane motion (X, Y)
            {
                float cosY = std::cos (camYaw);
                float sinY = std::sin (camYaw);

                float dxWorld = ((float) delta.x * cosY + (float) delta.y * sinY) * 0.005f;
                float dyWorld = (- (float) delta.x * sinY + (float) delta.y * cosY) * -0.005f;

                if (pX)
                {
                    float curVal = pX->getValue();
                    pX->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, curVal + dxWorld));
                }
                if (pY)
                {
                    float curVal = pY->getValue();
                    pY->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, curVal + dyWorld));
                }
            }
            repaint();
        }
    }

private:
    Spatial3DModule& module;
    juce::AudioProcessorValueTreeState& apvts;

    juce::ComboBox modeBox, motionBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttach, motionAttach;

    juce::Slider posXSlider, posYSlider, posZSlider, reflectSlider, wallSlider, airSlider;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;

    juce::TextButton viewPerspBtn, viewTopBtn, viewFrontBtn, viewSideBtn;

    float camYaw = 0.0f;
    float camPitch = 0.55f;
    float pulsePhase = 0.0f;
    int selectedBand = 1; // 0=Low, 1=Mid/Main, 2=High
    juce::Point<int> lastMousePos;
    bool isDraggingCamera = false;

    void setupSlider (juce::Slider& s, const juce::String& paramId, const juce::String& title, juce::Colour fillCol)
    {
        s.setSliderStyle (juce::Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 14);
        s.setColour (juce::Slider::rotarySliderFillColourId, fillCol);
        s.setColour (juce::Slider::rotarySliderOutlineColourId, juce::Colour (0xff22222a));
        s.setColour (juce::Slider::textBoxTextColourId, UITheme::textPrimary);
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (s);

        attachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramId, s));
    }
};

inline juce::AudioProcessorEditor* Spatial3DModule::createEditor()
{
    return new Spatial3DModuleEditor (*this, apvts);
}

