#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <vector>

class AuxAudioCaptureWorker : public juce::AudioIODeviceCallback
{
public:
    AuxAudioCaptureWorker()
        : fifo (8192)
    {
        fifoBuffer.setSize (2, 8192);
        fifoBuffer.clear();
        scratchIn[0].resize (4096, 0.0f);
        scratchIn[1].resize (4096, 0.0f);
        scratchOut[0].resize (4096, 0.0f);
        scratchOut[1].resize (4096, 0.0f);
    }

    ~AuxAudioCaptureWorker() override
    {
        stopCapture();
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

    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& /*context*/) override
    {
        // Auxiliary capture worker only records input
        if (outputChannelData != nullptr)
        {
            for (int ch = 0; ch < numOutputChannels; ++ch)
                if (outputChannelData[ch] != nullptr)
                    juce::FloatVectorOperations::clear (outputChannelData[ch], numSamples);
        }

        if (numInputChannels <= 0 || inputChannelData == nullptr || numSamples <= 0)
            return;

        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        fifo.prepareToWrite (numSamples, start1, size1, start2, size2);

        if (size1 + size2 < numSamples)
        {
            overruns.fetch_add (1, std::memory_order_relaxed);
        }

        auto writeToFifo = [this, inputChannelData, numInputChannels](int srcOffset, int dstStart, int count)
        {
            if (count <= 0) return;

            const float* inL = inputChannelData[0] != nullptr ? (inputChannelData[0] + srcOffset) : nullptr;
            const float* inR = (numInputChannels > 1 && inputChannelData[1] != nullptr)
                                   ? (inputChannelData[1] + srcOffset)
                                   : inL;

            if (inL != nullptr)
                fifoBuffer.copyFrom (0, dstStart, inL, count);
            else
                fifoBuffer.clear (0, dstStart, count);

            if (inR != nullptr)
                fifoBuffer.copyFrom (1, dstStart, inR, count);
            else
                fifoBuffer.clear (1, dstStart, count);
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
            stopCapture();
            currentDeviceName = "";
            return;
        }

        if (isCapturing.load() && currentDeviceName == deviceName)
            return;

        stopCapture();
        currentDeviceName = deviceName;
        startCapture (deviceName);
    }

    bool startCapture (const juce::String& deviceName)
    {
        deviceManager = std::make_unique<juce::AudioDeviceManager>();
        
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        deviceManager->getAudioDeviceSetup (setup);
        setup.inputDeviceName = deviceName;
        setup.useDefaultInputChannels = true;
        setup.outputDeviceName = ""; // Input-only capture
        setup.useDefaultOutputChannels = false;

        juce::String err = deviceManager->initialise (2, 0, nullptr, true, deviceName, &setup);
        if (err.isNotEmpty())
        {
            lastErrorMessage = err;
            isCapturing.store (false);
            return false;
        }

        auto* currentDevice = deviceManager->getCurrentAudioDevice();
        if (currentDevice != nullptr)
        {
            currentDeviceSampleRate.store (currentDevice->getCurrentSampleRate());
            currentDeviceBlockSize.store (currentDevice->getCurrentBufferSizeSamples());
        }

        deviceManager->addAudioCallback (this);
        isCapturing.store (true);
        lastErrorMessage = "";
        return true;
    }

    void stopCapture()
    {
        isCapturing.store (false);
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

    // Called on the main audio thread in PluginProcessor::processBlock
    void readResampled (juce::AudioBuffer<float>& outBuffer, int startSample, int numSamples, double masterSampleRate)
    {
        if (!isCapturing.load() || masterSampleRate <= 0.0 || numSamples <= 0)
        {
            outBuffer.clear (0, startSample, numSamples);
            if (outBuffer.getNumChannels() > 1)
                outBuffer.clear (1, startSample, numSamples);
            return;
        }

        // Target watermark: max(2 * numSamples, 256 samples (~5ms @ 48kHz))
        const int targetSamples = juce::jmax (2 * numSamples, 256);
        const int ready = fifo.getNumReady();

        if (!hasBuffered.load (std::memory_order_relaxed))
        {
            if (ready < targetSamples)
            {
                outBuffer.clear (0, startSample, numSamples);
                if (outBuffer.getNumChannels() > 1)
                    outBuffer.clear (1, startSample, numSamples);
                return;
            }
            hasBuffered.store (true, std::memory_order_relaxed);
        }

        // Proportional clock drift control
        float err = ((float) ready - (float) targetSamples) / (float) targetSamples;
        errSmoothed = errSmoothed * 0.95f + err * 0.05f;

        double devRate = currentDeviceSampleRate.load (std::memory_order_relaxed);
        if (devRate <= 0.0) devRate = masterSampleRate;

        double nominalRatio = devRate / masterSampleRate;
        double driftFactor = 1.0 + juce::jlimit (-0.005, 0.005, 0.005 * (double) errSmoothed);
        double effectiveRatio = nominalRatio * driftFactor; // input samples per output sample

        int maxInNeeded = (int) std::ceil ((double) numSamples * effectiveRatio) + 16;
        if (ready < maxInNeeded)
        {
            underflows.fetch_add (1, std::memory_order_relaxed);
            if (ready < numSamples)
                hasBuffered.store (false, std::memory_order_relaxed);

            outBuffer.clear (0, startSample, numSamples);
            if (outBuffer.getNumChannels() > 1)
                outBuffer.clear (1, startSample, numSamples);
            return;
        }

        if (maxInNeeded > (int) scratchIn[0].size())
        {
            maxInNeeded = (int) scratchIn[0].size();
        }

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

        int consumed0 = resamplers[0].process (effectiveRatio, scratchIn[0].data(), outBuffer.getWritePointer (0, startSample), numSamples);
        int consumed1 = 0;
        if (outBuffer.getNumChannels() > 1)
        {
            consumed1 = resamplers[1].process (effectiveRatio, scratchIn[1].data(), outBuffer.getWritePointer (1, startSample), numSamples);
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

    bool isActive() const noexcept { return isCapturing.load(); }
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
    std::atomic<bool> isCapturing { false };
    std::atomic<bool> hasBuffered { false };
    std::atomic<double> currentDeviceSampleRate { 48000.0 };
    std::atomic<int> currentDeviceBlockSize { 512 };

    std::atomic<uint32_t> overruns { 0 };
    std::atomic<uint32_t> underflows { 0 };

    juce::LagrangeInterpolator resamplers[2];
    std::vector<float> scratchIn[2];
    std::vector<float> scratchOut[2];
    float errSmoothed = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AuxAudioCaptureWorker)
};
