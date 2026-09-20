#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

/**
    Base class for every "mini-plugin" node that can live inside the graph
    (Compressor, Limiter, DeReverb, etc.). Each one is a real juce::AudioProcessor,
    so it can be dropped straight into juce::AudioProcessorGraph as a node.

    Concrete modules just override the DSP + parameter layout; all the
    AudioProcessor boilerplate lives here.
*/
class ModuleProcessor : public juce::AudioProcessor
{
public:
    ModuleProcessor (const juce::String& displayName,
                      juce::AudioProcessorValueTreeState::ParameterLayout layout)
        : ModuleProcessor (displayName,
                           BusesProperties()
                               .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                               .withOutput ("Output", juce::AudioChannelSet::stereo(), true),
                           std::move (layout))
    {
    }

    ModuleProcessor (const juce::String& displayName,
                      BusesProperties buses,
                      juce::AudioProcessorValueTreeState::ParameterLayout layout)
        : juce::AudioProcessor (buses),
          name (displayName),
          apvts (*this, nullptr, "PARAMS", std::move (layout))
    {
    }

    ~ModuleProcessor() override = default;

    // -- identity ----------------------------------------------------
    const juce::String getName() const override { return name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // -- editor --------------------------------------------------------
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override
    {
        return new juce::GenericAudioProcessorEditor (*this);
    }

    // -- programs --------------------------------------------------------
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    // -- state --------------------------------------------------------
    void getStateInformation (juce::MemoryBlock& destData) override
    {
        if (auto state = apvts.copyState().createXml())
            copyXmlToBinary (*state, destData);
    }

    void setStateInformation (const void* data, int sizeInBytes) override
    {
        if (auto xml = getXmlFromBinary (data, sizeInBytes))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
    }

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override
    {
        return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
            && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    const juce::AudioProcessorValueTreeState& getAPVTS() const { return apvts; }

protected:
    juce::String name;
    juce::AudioProcessorValueTreeState apvts;
};
