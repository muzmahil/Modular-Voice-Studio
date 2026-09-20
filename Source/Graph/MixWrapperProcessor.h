#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

/**
    Universal FL Studio-Style Mix Wrapper Processor.
    Wraps any ModuleProcessor node in the AudioProcessorGraph, providing
    seamless, zero-copy Dry/Wet effect mix control from 0% to 100% and dynamic multi-bus support.
*/
class MixWrapperProcessor : public juce::AudioProcessor
{
public:
    explicit MixWrapperProcessor (std::unique_ptr<juce::AudioProcessor> moduleToWrap)
        : juce::AudioProcessor (getBusesPropertiesFor (moduleToWrap.get())),
          inner (std::move (moduleToWrap))
    {
    }

    ~MixWrapperProcessor() override = default;

    static BusesProperties getBusesPropertiesFor (juce::AudioProcessor* proc)
    {
        if (proc == nullptr)
        {
            return BusesProperties()
                .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                .withOutput ("Output", juce::AudioChannelSet::stereo(), true);
        }

        BusesProperties bp;
        int inBuses = proc->getBusCount (true);
        if (inBuses == 0)
        {
            // Inputless processor
        }
        else
        {
            for (int i = 0; i < inBuses; ++i)
            {
                auto* bus = proc->getBus (true, i);
                bp.addBus (true, bus->getName(), bus->getDefaultLayout(), bus->isEnabledByDefault());
            }
        }

        int outBuses = proc->getBusCount (false);
        if (outBuses == 0)
        {
            // Outputless processor
        }
        else
        {
            for (int i = 0; i < outBuses; ++i)
            {
                auto* bus = proc->getBus (false, i);
                bp.addBus (false, bus->getName(), bus->getDefaultLayout(), bus->isEnabledByDefault());
            }
        }
        return bp;
    }

    juce::AudioProcessor* getInnerProcessor() const { return inner.get(); }

    static inline juce::AudioProcessor* getActualProcessor (juce::AudioProcessor* proc)
    {
        if (auto* wrapper = dynamic_cast<MixWrapperProcessor*> (proc))
            return wrapper->getInnerProcessor();
        return proc;
    }

    float getMixLevel() const { return mixLevel.load (std::memory_order_relaxed); }
    void  setMixLevel (float m) { mixLevel.store (juce::jlimit (0.0f, 1.0f, m), std::memory_order_relaxed); }

    // --- AUDIO PROCESSING & DRY/WET MIXING ---
    void prepareToPlay (double sampleRate, int samplesPerBlock) override
    {
        if (inner != nullptr)
            inner->prepareToPlay (sampleRate, samplesPerBlock);
    }

    void releaseResources() override
    {
        if (inner != nullptr)
            inner->releaseResources();
    }

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override
    {
        if (inner == nullptr) return;

        float mix = mixLevel.load (std::memory_order_relaxed);

        // 100% Wet: Pure zero-copy direct execution
        if (mix >= 0.999f || buffer.getNumChannels() != 2)
        {
            inner->processBlock (buffer, midi);
            return;
        }

        // 0% Mix: 100% Dry passthrough (saves CPU)
        if (mix <= 0.001f)
            return;

        // Partial Mix: Capture dry buffer, process through inner module, blend
        juce::AudioBuffer<float> dryCopy;
        dryCopy.makeCopyOf (buffer, true);

        inner->processBlock (buffer, midi);

        float wet = mix;
        float dry = 1.0f - mix;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            buffer.applyGain (ch, 0, buffer.getNumSamples(), wet);
            buffer.addFrom (ch, 0, dryCopy, ch, 0, buffer.getNumSamples(), dry);
        }
    }

    // --- DELEGATION TO INNER PROCESSOR ---
    const juce::String getName() const override
    {
        return inner != nullptr ? inner->getName() : "Module";
    }

    bool acceptsMidi() const override  { return inner != nullptr && inner->acceptsMidi(); }
    bool producesMidi() const override { return inner != nullptr && inner->producesMidi(); }
    double getTailLengthSeconds() const override { return inner != nullptr ? inner->getTailLengthSeconds() : 0.0; }

    bool hasEditor() const override { return inner != nullptr && inner->hasEditor(); }
    juce::AudioProcessorEditor* createEditor() override
    {
        return inner != nullptr ? inner->createEditor() : nullptr;
    }

    int getNumPrograms() override { return inner != nullptr ? inner->getNumPrograms() : 1; }
    int getCurrentProgram() override { return inner != nullptr ? inner->getCurrentProgram() : 0; }
    void setCurrentProgram (int p) override { if (inner != nullptr) inner->setCurrentProgram (p); }
    const juce::String getProgramName (int p) override { return inner != nullptr ? inner->getProgramName (p) : juce::String(); }
    void changeProgramName (int p, const juce::String& n) override { if (inner != nullptr) inner->changeProgramName (p, n); }

    void getStateInformation (juce::MemoryBlock& destData) override
    {
        if (inner != nullptr)
            inner->getStateInformation (destData);
    }

    void setStateInformation (const void* data, int sizeInBytes) override
    {
        if (inner != nullptr)
            inner->setStateInformation (data, sizeInBytes);
    }

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        if (inner != nullptr)
            return inner->checkBusesLayoutSupported (layouts);
        return true;
    }

private:
    std::unique_ptr<juce::AudioProcessor> inner;
    std::atomic<float> mixLevel { 1.0f }; // Default 100%
};
