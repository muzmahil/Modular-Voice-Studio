#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <vector>

class AuxAudioOutputWorker : public juce::AudioIODeviceCallback
{
public:
    AuxAudioOutputWorker()
        : fifo (8192)
    {
        fifoBuffer.setSize (2, 8192);
        fifoBuffer.clear();
        scratchIn[0].resize (4096, 0.0f);
        scratchIn[1].resize (4096, 0.0f);
    }

    ~AuxAudioOutputWorker() override
    {
        stopOutput();
    }

    void audioDeviceAboutToStart (juce::AudioIODevice* device) override
    {
        if (device != nullptr)
        {
            currentDeviceSampleRate.store (device->getCurrentSampleRate());
            currentDeviceBlockSize.store (device->getCurrentBufferSizeSamples());
        }
        resamplers[0].reset();
        resamplers[1].reset();
        fifo.reset();
        fifoBuffer.clear();
        hasBuffered.store (false);
        errSmoothed = 0.0f;
    }

    void audioDeviceStopped() override
    {
        resamplers[0].reset();
        resamplers[1].reset();
        fifo.reset();
        fifoBuffer.clear();
        hasBuffered.store (false);
        errSmoothed = 0.0f;
    }

    // Called on the output device audio thread
    void audioDeviceIOCallbackWithContext (const float* const* /*inputChannelData*/,
                                           int /*numInputChannels*/,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& /*context*/) override
    {
        if (numOutputChannels <= 0 || outputChannelData == nullptr || numSamples <= 0)
            return;

        for (int ch = 0; ch < numOutputChannels; ++ch)
        {
            if (outputChannelData[ch] != nullptr)
                juce::FloatVectorOperations::clear (outputChannelData[ch], numSamples);
        }

        if (!isStreaming.load() || masterRate.load() <= 0.0)
            return;

        const double devRate = currentDeviceSampleRate.load (std::memory_order_relaxed);
        const double masterR = masterRate.load (std::memory_order_relaxed);
        // Ultra-low latency anti-pop watermark (approx 2.6ms @ 48kHz)
        const int targetSamples = juce::jmax (numSamples + 32, 128);
        const int ready = fifo.getNumReady();

        if (!hasBuffered.load (std::memory_order_relaxed))
        {
            if (ready < targetSamples)
                return; // Silence while buffering
            hasBuffered.store (true, std::memory_order_relaxed);
        }

        // Smooth proportional drift control to avoid pitch glitches
        float err = ((float) ready - (float) targetSamples) / (float) targetSamples;
        errSmoothed = errSmoothed * 0.95f + err * 0.05f;

        // Effective ratio = (source rate / target rate) * drift (max 0.3% adjustment)
        double nominalRatio = masterR / (devRate > 0.0 ? devRate : masterR);
        double driftFactor = 1.0 + juce::jlimit (-0.003, 0.003, 0.003 * (double) errSmoothed);
        double effectiveRatio = nominalRatio * driftFactor; // input samples from FIFO per output sample

        int maxInNeeded = (int) std::ceil ((double) numSamples * effectiveRatio) + 16;
        if (ready < maxInNeeded)
        {
            underflows.fetch_add (1, std::memory_order_relaxed);
            if (ready < numSamples)
                hasBuffered.store (false, std::memory_order_relaxed);
            return;
        }

        if (maxInNeeded > (int) scratchIn[0].size())
            maxInNeeded = (int) scratchIn[0].size();

        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        fifo.prepareToRead (maxInNeeded, start1, size1, start2, size2);

        auto copyFromFifo = [this](int dstOffset, int srcStart, int count)
        {
            if (count <= 0) return;
            std::memcpy (scratchIn[0].data() + dstOffset, fifoBuffer.getReadPointer (0, srcStart), sizeof (float) * (size_t) count);
            std::memcpy (scratchIn[1].data() + dstOffset, fifoBuffer.getReadPointer (1, srcStart), sizeof (float) * (size_t) count);
        };

        if (size1 > 0)
            copyFromFifo (0, start1, size1);
        if (size2 > 0)
            copyFromFifo (size1, start2, size2);

        float* outL = outputChannelData[0];
        float* outR = (numOutputChannels > 1 && outputChannelData[1] != nullptr) ? outputChannelData[1] : nullptr;

        int consumed0 = resamplers[0].process (effectiveRatio, scratchIn[0].data(), outL, numSamples);
        int consumed1 = 0;
        if (outR != nullptr)
        {
            consumed1 = resamplers[1].process (effectiveRatio, scratchIn[1].data(), outR, numSamples);
        }
        else
        {
            consumed1 = consumed0;
        }

        int consumed = juce::jmax (consumed0, consumed1);
        if (consumed > size1 + size2)
            consumed = size1 + size2;

        fifo.finishedRead (consumed);
    }

    // Called on the master plugin audio thread
    void writeStream (const juce::AudioBuffer<float>& inBuffer, int startSample, int numSamples, double currentMasterSampleRate)
    {
        if (!isStreaming.load() || numSamples <= 0)
            return;

        masterRate.store (currentMasterSampleRate, std::memory_order_relaxed);

        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        fifo.prepareToWrite (numSamples, start1, size1, start2, size2);

        if (size1 + size2 < numSamples)
        {
            overruns.fetch_add (1, std::memory_order_relaxed);
        }

        auto writeToFifo = [this, &inBuffer, startSample](int srcOffset, int dstStart, int count)
        {
            if (count <= 0) return;

            const float* inL = inBuffer.getReadPointer (0, startSample + srcOffset);
            const float* inR = inBuffer.getNumChannels() > 1 ? inBuffer.getReadPointer (1, startSample + srcOffset) : inL;

            fifoBuffer.copyFrom (0, dstStart, inL, count);
            fifoBuffer.copyFrom (1, dstStart, inR, count);
        };

        if (size1 > 0)
            writeToFifo (0, start1, size1);
        if (size2 > 0)
            writeToFifo (size1, start2, size2);

        fifo.finishedWrite (size1 + size2);
    }

    void setDevice (const juce::String& deviceName)
    {
        if (deviceName.isEmpty() || deviceName == "None" || deviceName == "Yok")
        {
            stopOutput();
            currentDeviceName = "";
            return;
        }

        if (isStreaming.load() && currentDeviceName == deviceName)
            return;

        stopOutput();
        currentDeviceName = deviceName;
        startOutput (deviceName);
    }

    bool startOutput (const juce::String& deviceName)
    {
        deviceManager = std::make_unique<juce::AudioDeviceManager>();

        juce::AudioDeviceManager::AudioDeviceSetup setup;
        deviceManager->getAudioDeviceSetup (setup);
        setup.inputDeviceName = ""; // Output-only
        setup.useDefaultInputChannels = false;
        setup.outputDeviceName = deviceName;
        setup.useDefaultOutputChannels = true;
        setup.sampleRate = 48000.0;
        setup.bufferSize = 256; // Low latency buffer

        juce::String err = deviceManager->initialise (0, 2, nullptr, true, deviceName, &setup);
        if (err.isNotEmpty())
        {
            lastErrorMessage = err;
            isStreaming.store (false);
            return false;
        }

        // Enforce 256 buffer if supported
        setup = deviceManager->getAudioDeviceSetup();
        setup.bufferSize = 256;
        deviceManager->setAudioDeviceSetup (setup, true);

        auto* currentDevice = deviceManager->getCurrentAudioDevice();
        if (currentDevice != nullptr)
        {
            currentDeviceSampleRate.store (currentDevice->getCurrentSampleRate());
            currentDeviceBlockSize.store (currentDevice->getCurrentBufferSizeSamples());
        }

        deviceManager->addAudioCallback (this);
        isStreaming.store (true);
        lastErrorMessage = "";
        return true;
    }

    void stopOutput()
    {
        isStreaming.store (false);
        if (deviceManager != nullptr)
        {
            deviceManager->removeAudioCallback (this);
            deviceManager->closeAudioDevice();
            deviceManager = nullptr;
        }
        fifo.reset();
        fifoBuffer.clear();
        hasBuffered.store (false);
        errSmoothed = 0.0f;
    }

    bool isActive() const noexcept { return isStreaming.load(); }
    juce::String getDeviceName() const noexcept { return currentDeviceName; }
    juce::String getLastError() const noexcept { return lastErrorMessage; }
    uint32_t getOverruns() const noexcept { return overruns.load (std::memory_order_relaxed); }
    uint32_t getUnderflows() const noexcept { return underflows.load (std::memory_order_relaxed); }

private:
    std::unique_ptr<juce::AudioDeviceManager> deviceManager;
    juce::String currentDeviceName;
    juce::String lastErrorMessage;

    juce::AudioBuffer<float> fifoBuffer;
    juce::AbstractFifo fifo;
    std::atomic<bool> isStreaming { false };
    std::atomic<bool> hasBuffered { false };
    std::atomic<double> masterRate { 48000.0 };
    std::atomic<double> currentDeviceSampleRate { 48000.0 };
    std::atomic<int> currentDeviceBlockSize { 512 };

    std::atomic<uint32_t> overruns { 0 };
    std::atomic<uint32_t> underflows { 0 };

    juce::LagrangeInterpolator resamplers[2];
    std::vector<float> scratchIn[2];
    float errSmoothed = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuxAudioOutputWorker)
};
