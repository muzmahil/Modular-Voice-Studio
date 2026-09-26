#pragma once
#include "ModuleProcessor.h"

/**
    Generic placeholder DSP module (unity-gain pass-through with a single
    gain knob) used for every category entry until its real algorithm is
    written. Replace one entry at a time in ModuleFactory.cpp with a real
    ModuleProcessor subclass — the graph/UI code never has to change.
*/
class StubModule : public ModuleProcessor
{
public:
    explicit StubModule (juce::String displayName)
        : ModuleProcessor (std::move (displayName), createLayout())
    {
        gainParam = getRawParam ("gain");
    }

    void prepareToPlay (double, int) override {}
    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        float g = gainParam ? gainParam->load() : 1.0f;
        buffer.applyGain (g);
    }

    using AudioProcessor::processBlock;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        return { std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "gain", 1 }, "Gain",
            juce::NormalisableRange<float> (0.0f, 2.0f, 0.001f), 1.0f) };
    }

    std::atomic<float>* gainParam = nullptr;
};
