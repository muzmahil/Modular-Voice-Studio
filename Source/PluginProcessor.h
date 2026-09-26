#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Graph/ModuleFactory.h"
#include "Graph/PresetManager.h"
#include "Graph/GraphHistoryManager.h"
#include "Mixer/AuxAudioCaptureWorker.h"
#include "Mixer/AuxAudioOutputWorker.h"
#include "Modules/PitchShiftModule.h"
#include "Modules/ReverbModule.h"
#include "Modules/VSTPluginModule.h"
#include "Graph/DynamicEffectChain.h"
#include <vector>
#include <mutex>
#include <array>
#include <set>

class ChannelDSPProcessor : public juce::ChangeBroadcaster
{
public:
    using Node = juce::AudioProcessorGraph::Node;
    using NodeID = juce::AudioProcessorGraph::NodeID;
    using Connection = juce::AudioProcessorGraph::Connection;

    explicit ChannelDSPProcessor (int channelIdx);
    ~ChannelDSPProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock);
    void releaseResources();
    void processAudio (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, float targetGainLinear, float targetDuckLinear);

    Node::Ptr addModule (const juce::String& typeId, juce::Point<int> canvasPosition);
    void removeModule (NodeID id);
    void removeModuleAndReconnect (NodeID id);
    bool connect (NodeID sourceNode, int sourceChannel, NodeID destNode, int destChannel);
    void disconnect (const Connection& connection);
    void clearGraphToDefault();

    juce::AudioProcessorGraph& getGraph() { return graph; }
    NodeID getAudioInputNodeID() const  { return audioInputNode != nullptr ? audioInputNode->nodeID : NodeID(); }
    NodeID getAudioOutputNodeID() const { return audioOutputNode != nullptr ? audioOutputNode->nodeID : NodeID(); }
    PresetManager& getPresetManager() { return presetManager; }
    GraphHistoryManager& getHistoryManager() { return historyManager; }
    std::vector<juce::String> getActiveModuleNames() const;

    void getStateInformation (juce::MemoryBlock& destData);
    void setStateInformation (const void* data, int sizeInBytes);

    int channelIndex = 0;
    std::atomic<float> inLevel { 0.0f };
    std::atomic<float> outLevel { 0.0f };
    std::atomic<float> faderGainLinear { 1.0f };
    std::atomic<bool> muted { false };
    std::atomic<bool> isSoloActive { false };
    std::atomic<bool> isBypassed { false };
    std::atomic<bool> isSidechainActive { false };
    std::atomic<bool> isMonitoringActive { false }; // A1 (Headphone Monitor)
    std::atomic<bool> isBusB1Active { true };        // B1 (Virtual Mic)
    std::atomic<bool> isBusB2Active { true };        // B2 (Stream Mix)
    std::atomic<bool> outputChannelConnected[2] { true, true };

    float canvasPanX = -1400.0f;
    float canvasPanY = -1500.0f;
    float canvasZoom = 1.0f;

    void updateOutputConnectionFlags();
    void initialiseGraph();

private:
    juce::AudioProcessorGraph graph;
    Node::Ptr audioInputNode;
    Node::Ptr audioOutputNode;
    PresetManager presetManager;
    GraphHistoryManager historyManager;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedGain;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedDuck;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedBypass;
    juce::AudioBuffer<float> dryDelayBuffer;
    int dryDelayWritePos = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelDSPProcessor)
};

class PluginProcessor : public juce::AudioProcessor,
                         public juce::ChangeBroadcaster // notifies the editor when the graph topology changes
{
public:
    using Node = juce::AudioProcessorGraph::Node;
    using NodeID = juce::AudioProcessorGraph::NodeID;
    using Connection = juce::AudioProcessorGraph::Connection;

    PluginProcessor();
    ~PluginProcessor() override;

    ChannelDSPProcessor& getChannel (int index) { return *channels[juce::jlimit (0, 3, index)]; }
    const ChannelDSPProcessor& getChannel (int index) const { return *channels[juce::jlimit (0, 3, index)]; }
    ChannelDSPProcessor& getActiveChannel() { return *channels[activeChannelIndex]; }
    const ChannelDSPProcessor& getActiveChannel() const { return *channels[activeChannelIndex]; }
    int getActiveChannelIndex() const { return activeChannelIndex; }
    void switchChannelGraph (int newChannelIndex);

    std::vector<juce::String> getChannelModuleNames (int channelIndex) const
    {
        if (channelIndex >= 0 && channelIndex < 4)
            return channels[channelIndex]->getActiveModuleNames();
        return {};
    }

    AuxAudioCaptureWorker* getCaptureWorker (int channelIndex)
    {
        if (channelIndex >= 1 && channelIndex <= 3)
            return auxCaptureWorkers[channelIndex - 1].get();
        return nullptr;
    }

    AuxAudioOutputWorker* getOutputWorker (int busIndex)
    {
        if (busIndex >= 0 && busIndex < 2)
            return auxOutputWorkers[busIndex].get();
        return nullptr;
    }

    // Direct forwarding to active channel for canvas/sidebar UI
    Node::Ptr addModule (const juce::String& typeId, juce::Point<int> canvasPosition) { return getActiveChannel().addModule (typeId, canvasPosition); }
    void removeModule (NodeID id) { getActiveChannel().removeModule (id); }
    void removeModuleAndReconnect (NodeID id) { getActiveChannel().removeModuleAndReconnect (id); }
    bool connect (NodeID srcNode, int srcCh, NodeID dstNode, int dstCh) { return getActiveChannel().connect (srcNode, srcCh, dstNode, dstCh); }
    void disconnect (const Connection& c) { getActiveChannel().disconnect (c); }
    void clearGraphToDefault() { getActiveChannel().clearGraphToDefault(); }
    void resetAllToFactoryDefaults();

    juce::AudioProcessorGraph& getGraph() { return getActiveChannel().getGraph(); }
    NodeID getAudioInputNodeID() const { return getActiveChannel().getAudioInputNodeID(); }
    NodeID getAudioOutputNodeID() const { return getActiveChannel().getAudioOutputNodeID(); }
    PresetManager& getPresetManager() { return getActiveChannel().getPresetManager(); }
    GraphHistoryManager& getHistoryManager() { return getActiveChannel().getHistoryManager(); }
    std::vector<juce::String> getActiveModuleNames() const { return getActiveChannel().getActiveModuleNames(); }

    std::atomic<float> outputLevel { 0.0f };
    std::atomic<float> inputLevel { 0.0f };
    std::atomic<bool> pluginBypassed { false };
    std::atomic<float> currentCpuUsage { 0.0f };
    std::atomic<float> currentProcessTimeMs { 0.0f };
    std::atomic<float> currentSampleRate { 44100.0f };
    std::atomic<int> currentBlockSize { 512 };

    // Realtime channel strip mixing controls (legacy compatibility aliases)
    std::atomic<float> channelGainLinear { 1.0f };
    std::atomic<float> channelFaderLinear { 1.0f };
    std::atomic<bool> channelMuted { false };
    std::atomic<bool> isMonitoringActive { false };
    std::atomic<bool> isCanvasAuditionActive { false };
    std::atomic<bool> outputChannelConnected[2] { true, true };
    void updateOutputConnectionFlags() { getActiveChannel().updateOutputConnectionFlags(); }

    // Waveform telemetry
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

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    bool isBusesLayoutSupported (const BusesLayout&) const override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Modular Voice Studio"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    std::array<std::unique_ptr<ChannelDSPProcessor>, 4> channels;
    std::array<std::unique_ptr<AuxAudioCaptureWorker>, 3> auxCaptureWorkers;
    std::array<std::unique_ptr<AuxAudioOutputWorker>, 2> auxOutputWorkers;
    int activeChannelIndex = 0;

    PitchShiftModule& getQuickPitch() { return quickPitch; }
    ReverbModule& getQuickReverb() { return quickReverb; }
    VSTPluginModule& getQuickVST() { return quickVST; }
    DynamicEffectChain& getEffectChain() { return effectChain; }

private:
    PitchShiftModule quickPitch;
    ReverbModule quickReverb;
    VSTPluginModule quickVST;
    DynamicEffectChain effectChain;

    std::vector<float> inputWaveform;
    std::vector<float> outputWaveform;
    int waveformWriteIndex = 0;
    std::mutex waveformMutex;

    bool isSidechainTriggered = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
