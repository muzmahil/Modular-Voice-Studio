#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Graph/ModuleFactory.h"
#include "Graph/PresetManager.h"
#include "Graph/GraphHistoryManager.h"
#include <vector>
#include <mutex>

class PluginProcessor : public juce::AudioProcessor,
                         public juce::ChangeBroadcaster // notifies the editor when the graph topology changes
{
public:
    using Node = juce::AudioProcessorGraph::Node;
    using NodeID = juce::AudioProcessorGraph::NodeID;
    using Connection = juce::AudioProcessorGraph::Connection;

    PluginProcessor();
    ~PluginProcessor() override;
    std::atomic<float> outputLevel { 0.0f };
    std::atomic<float> inputLevel { 0.0f };  // peak-in, sampled before the graph runs — feeds the Audio In node's meter
    std::atomic<bool> pluginBypassed { false };
    std::atomic<float> currentCpuUsage { 0.0f };
    std::atomic<float> currentProcessTimeMs { 0.0f };
    std::atomic<float> currentSampleRate { 44100.0f };
    std::atomic<int> currentBlockSize { 512 };

    // Output connection monitoring (silences output when not connected)
    std::atomic<bool> outputChannelConnected[2] { false, false };
    void updateOutputConnectionFlags();

    // Global Dual-Trace Waveform Monitor (Input Dry vs Processed Output)
    static constexpr int waveformHistorySize = 1024;
    void getWaveformData (std::vector<float>& inDest, std::vector<float>& outDest);

    // DAW Host Automatable Macro Parameters
    juce::AudioParameterFloat* macro1 = nullptr;
    juce::AudioParameterFloat* macro2 = nullptr;
    juce::AudioParameterFloat* macro3 = nullptr;
    juce::AudioParameterFloat* macro4 = nullptr;

    float canvasPanX = -1400.0f;
    float canvasPanY = -1500.0f;
    float canvasZoom = 1.0f;
    // -- juce::AudioProcessor --------------------------------------------------------
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Modular Voice Studio"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // -- graph editing API used by the UI --------------------------------------------------------
    Node::Ptr addModule (const juce::String& typeId, juce::Point<int> canvasPosition);
    void removeModule (NodeID id);
    void removeModuleAndReconnect (NodeID id);
    bool connect (NodeID sourceNode, int sourceChannel, NodeID destNode, int destChannel);
    void disconnect (const Connection& connection);
    void clearGraphToDefault();

    juce::AudioProcessorGraph& getGraph() { return graph; }
    NodeID getAudioInputNodeID() const  { return audioInputNode->nodeID; }
    NodeID getAudioOutputNodeID() const { return audioOutputNode->nodeID; }
    PresetManager& getPresetManager() { return presetManager; }
    GraphHistoryManager& getHistoryManager() { return historyManager; }

private:
    juce::AudioProcessorGraph graph;
    Node::Ptr audioInputNode;
    Node::Ptr audioOutputNode;
    PresetManager presetManager;
    GraphHistoryManager historyManager;

    std::vector<float> inputWaveform;
    std::vector<float> outputWaveform;
    int waveformWriteIndex = 0;
    std::mutex waveformMutex;

    void initialiseGraph();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
