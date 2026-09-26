#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Graph/ModuleProcessor.h"
#include "Graph/MixWrapperProcessor.h"
#include "Localization.h"

// =============================================================================
// ChannelDSPProcessor Implementation
// =============================================================================

ChannelDSPProcessor::ChannelDSPProcessor (int channelIdx)
    : channelIndex (channelIdx)
{
    initialiseGraph();
}

void ChannelDSPProcessor::initialiseGraph()
{
    const juce::ScopedLock sl (graph.getCallbackLock());
    graph.clear();

    audioInputNode = graph.addNode (std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor> (
        juce::AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode));
    audioOutputNode = graph.addNode (std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor> (
        juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode));

    audioInputNode->properties.set ("x", 1700);
    audioInputNode->properties.set ("y", 1900);
    audioOutputNode->properties.set ("x", 2100);
    audioOutputNode->properties.set ("y", 1900);

    // Default clean 1:1 direct connection
    for (int ch = 0; ch < 2; ++ch)
        graph.addConnection ({ { audioInputNode->nodeID, ch }, { audioOutputNode->nodeID, ch } });

    updateOutputConnectionFlags();
}

void ChannelDSPProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    graph.setPlayConfigDetails (2, 2, sampleRate, samplesPerBlock);
    graph.prepareToPlay (sampleRate, samplesPerBlock);
    updateOutputConnectionFlags();

    smoothedGain.reset (sampleRate, 0.010); // 10ms ramp
    smoothedGain.setCurrentAndTargetValue (faderGainLinear.load());

    smoothedDuck.reset (sampleRate, 0.010);
    smoothedDuck.setCurrentAndTargetValue (1.0f);

    smoothedBypass.reset (sampleRate, 0.010);
    smoothedBypass.setCurrentAndTargetValue (isBypassed.load() ? 1.0f : 0.0f);

    dryDelayBuffer.setSize (2, 16384);
    dryDelayBuffer.clear();
    dryDelayWritePos = 0;
}

void ChannelDSPProcessor::releaseResources()
{
    graph.releaseResources();
}

void ChannelDSPProcessor::processAudio (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi, float targetGainLinear, float targetDuckLinear)
{
    int numSamples = buffer.getNumSamples();
    if (numSamples <= 0 || buffer.getNumChannels() == 0)
    {
        inLevel.store (0.0f);
        outLevel.store (0.0f);
        return;
    }

    float inPeak = buffer.getMagnitude (0, numSamples);
    float prevIn = inLevel.load();
    inLevel.store (inPeak > prevIn ? inPeak : prevIn * 0.92f);

    bool ch0 = outputChannelConnected[0].load();
    bool ch1 = outputChannelConnected[1].load();

    // 1. Store dry input into dry delay buffer for latency-compensated bypass
    int delayCapacity = dryDelayBuffer.getNumSamples();
    if (delayCapacity > 0)
    {
        for (int s = 0; s < numSamples; ++s)
        {
            int writeIdx = (dryDelayWritePos + s) % delayCapacity;
            for (int ch = 0; ch < juce::jmin (2, buffer.getNumChannels()); ++ch)
            {
                dryDelayBuffer.setSample (ch, writeIdx, buffer.getSample (ch, s));
            }
        }
    }

    // 2. Prepare latency-aligned dry signal
    int latency = graph.getLatencySamples();
    juce::AudioBuffer<float> dryCopy (2, numSamples);
    if (delayCapacity > 0)
    {
        for (int s = 0; s < numSamples; ++s)
        {
            int readIdx = (dryDelayWritePos + s - latency + delayCapacity * 4) % delayCapacity;
            for (int ch = 0; ch < 2; ++ch)
            {
                dryCopy.setSample (ch, s, dryDelayBuffer.getSample (ch, readIdx));
            }
        }
        dryDelayWritePos = (dryDelayWritePos + numSamples) % delayCapacity;
    }

    // 3. Process DSP Graph
    graph.processBlock (buffer, midi);

    // Silence if output is disconnected on canvas
    if (! ch0 && buffer.getNumChannels() > 0)
        buffer.clear (0, 0, numSamples);
    if (! ch1 && buffer.getNumChannels() > 1)
        buffer.clear (1, 0, numSamples);

    // 4. Smooth Bypass Crossfade
    smoothedBypass.setTargetValue (isBypassed.load() ? 1.0f : 0.0f);
    for (int s = 0; s < numSamples; ++s)
    {
        float bypassAmount = smoothedBypass.getNextValue();
        if (bypassAmount > 0.0f)
        {
            for (int ch = 0; ch < juce::jmin (2, buffer.getNumChannels()); ++ch)
            {
                float wetSample = buffer.getSample (ch, s);
                float drySample = dryCopy.getSample (ch, s);
                buffer.setSample (ch, s, wetSample * (1.0f - bypassAmount) + drySample * bypassAmount);
            }
        }
    }

    // 5. Apply Smoothed Gain (Fader & Solo/Mute) & Ducking
    smoothedGain.setTargetValue (targetGainLinear);
    smoothedDuck.setTargetValue (targetDuckLinear);

    for (int s = 0; s < numSamples; ++s)
    {
        float totalGain = smoothedGain.getNextValue() * smoothedDuck.getNextValue();
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            buffer.setSample (ch, s, buffer.getSample (ch, s) * totalGain);
        }
    }

    float outPeak = buffer.getMagnitude (0, numSamples);
    float prevOut = outLevel.load();
    outLevel.store (outPeak > prevOut ? outPeak : prevOut * 0.92f);
}

ChannelDSPProcessor::Node::Ptr ChannelDSPProcessor::addModule (const juce::String& typeId, juce::Point<int> canvasPosition)
{
    try
    {
        auto module = ModuleFactory::instance().create (typeId);
        if (module == nullptr)
            return nullptr;

        auto wrapper = std::make_unique<MixWrapperProcessor> (std::move (module));

        const juce::ScopedLock sl (graph.getCallbackLock());
        auto node = graph.addNode (std::move (wrapper));
        if (node == nullptr)
            return nullptr;

        node->properties.set ("x", canvasPosition.x);
        node->properties.set ("y", canvasPosition.y);
        node->properties.set ("type", typeId);
        node->properties.set ("mix", 1.0f);

        updateOutputConnectionFlags();
        sendChangeMessage();
        return node;
    }
    catch (...)
    {
        return nullptr;
    }
}

void ChannelDSPProcessor::removeModule (NodeID id)
{
    const juce::ScopedLock sl (graph.getCallbackLock());
    graph.removeNode (id);
    updateOutputConnectionFlags();
    sendChangeMessage();
}

void ChannelDSPProcessor::removeModuleAndReconnect (NodeID id)
{
    const juce::ScopedLock sl (graph.getCallbackLock());
    std::vector<Connection> incoming;
    std::vector<Connection> outgoing;

    for (const auto& c : graph.getConnections())
    {
        if (c.destination.nodeID == id)
            incoming.push_back (c);
        else if (c.source.nodeID == id)
            outgoing.push_back (c);
    }

    for (const auto& in : incoming)
    {
        for (const auto& out : outgoing)
        {
            if (in.destination.channelIndex == out.source.channelIndex)
            {
                graph.addConnection ({
                    { in.source.nodeID, in.source.channelIndex },
                    { out.destination.nodeID, out.destination.channelIndex }
                });
            }
        }
    }

    graph.removeNode (id);
    updateOutputConnectionFlags();
    sendChangeMessage();
}

bool ChannelDSPProcessor::connect (NodeID sourceNode, int sourceChannel, NodeID destNode, int destChannel)
{
    const juce::ScopedLock sl (graph.getCallbackLock());
    bool ok = graph.addConnection ({ { sourceNode, sourceChannel }, { destNode, destChannel } });
    if (ok)
    {
        updateOutputConnectionFlags();
        sendChangeMessage();
    }
    return ok;
}

void ChannelDSPProcessor::disconnect (const Connection& connection)
{
    const juce::ScopedLock sl (graph.getCallbackLock());
    graph.removeConnection (connection);
    updateOutputConnectionFlags();
    sendChangeMessage();
}

void ChannelDSPProcessor::clearGraphToDefault()
{
    initialiseGraph();
    sendChangeMessage();
}

void ChannelDSPProcessor::updateOutputConnectionFlags()
{
    bool ch0 = false;
    bool ch1 = false;
    if (audioOutputNode != nullptr)
    {
        for (auto& c : graph.getConnections())
        {
            if (c.destination.nodeID == audioOutputNode->nodeID)
            {
                if (c.destination.channelIndex == 0) ch0 = true;
                if (c.destination.channelIndex == 1) ch1 = true;
            }
        }
    }
    outputChannelConnected[0].store (ch0);
    outputChannelConnected[1].store (ch1);
}

std::vector<juce::String> ChannelDSPProcessor::getActiveModuleNames() const
{
    std::vector<juce::String> names;
    const juce::ScopedLock sl (graph.getCallbackLock());

    auto connections = graph.getConnections();
    std::set<NodeID> connectedNodes;
    for (const auto& c : connections)
    {
        connectedNodes.insert (c.source.nodeID);
        connectedNodes.insert (c.destination.nodeID);
    }

    for (auto* node : graph.getNodes())
    {
        if (node == audioInputNode.get() || node == audioOutputNode.get())
            continue;
        if (connectedNodes.find (node->nodeID) != connectedNodes.end())
        {
            if (auto* proc = node->getProcessor())
                names.push_back (proc->getName());
        }
    }
    return names;
}

void ChannelDSPProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state ("CHANNEL_GRAPH");
    state.setProperty ("inputX",  (int) audioInputNode->properties["x"], nullptr);
    state.setProperty ("inputY",  (int) audioInputNode->properties["y"], nullptr);
    state.setProperty ("outputX", (int) audioOutputNode->properties["x"], nullptr);
    state.setProperty ("outputY", (int) audioOutputNode->properties["y"], nullptr);
    state.setProperty ("inputID",  (int) audioInputNode->nodeID.uid, nullptr);
    state.setProperty ("outputID", (int) audioOutputNode->nodeID.uid, nullptr);

    state.setProperty ("panX", canvasPanX, nullptr);
    state.setProperty ("panY", canvasPanY, nullptr);
    state.setProperty ("zoom", canvasZoom, nullptr);

    juce::ValueTree nodes ("NODES");
    for (auto* node : graph.getNodes())
    {
        if (node == audioInputNode.get() || node == audioOutputNode.get())
            continue;

        juce::ValueTree n ("NODE");
        n.setProperty ("id", (int) node->nodeID.uid, nullptr);
        n.setProperty ("type", node->properties["type"].toString(), nullptr);
        n.setProperty ("x", (int) node->properties["x"], nullptr);
        n.setProperty ("y", (int) node->properties["y"], nullptr);
        n.setProperty ("bypassed", node->isBypassed(), nullptr);
        n.setProperty ("color", node->properties["color"].toString(), nullptr);
        float mixVal = 1.0f;
        if (node->properties.contains ("mix"))
            mixVal = (float) node->properties["mix"];
        n.setProperty ("mix", mixVal, nullptr);

        juce::MemoryBlock nodeState;
        node->getProcessor()->getStateInformation (nodeState);
        n.setProperty ("state", nodeState.toBase64Encoding(), nullptr);

        nodes.appendChild (n, nullptr);
    }
    state.appendChild (nodes, nullptr);

    juce::ValueTree connections ("CONNECTIONS");
    for (auto& c : graph.getConnections())
    {
        juce::ValueTree conn ("CONNECTION");
        conn.setProperty ("srcNode", (int) c.source.nodeID.uid, nullptr);
        conn.setProperty ("srcCh",   c.source.channelIndex, nullptr);
        conn.setProperty ("dstNode", (int) c.destination.nodeID.uid, nullptr);
        conn.setProperty ("dstCh",   c.destination.channelIndex, nullptr);
        connections.appendChild (conn, nullptr);
    }
    state.appendChild (connections, nullptr);

    if (auto xml = state.createXml())
        juce::AudioProcessor::copyXmlToBinary (*xml, destData);
}

void ChannelDSPProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = juce::AudioProcessor::getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    initialiseGraph();

    if (state.hasProperty ("inputX"))
    {
        audioInputNode->properties.set ("x", (int) state["inputX"]);
        audioInputNode->properties.set ("y", (int) state["inputY"]);
        audioOutputNode->properties.set ("x", (int) state["outputX"]);
        audioOutputNode->properties.set ("y", (int) state["outputY"]);
    }

    if (state.hasProperty ("panX"))
    {
        canvasPanX = (float) state["panX"];
        canvasPanY = (float) state["panY"];
        canvasZoom = (float) state["zoom"];
    }

    std::map<int, NodeID> idRemap;
    int savedInID  = (int) state.getProperty ("inputID",  (int) audioInputNode->nodeID.uid);
    int savedOutID = (int) state.getProperty ("outputID", (int) audioOutputNode->nodeID.uid);
    idRemap[savedInID]  = audioInputNode->nodeID;
    idRemap[savedOutID] = audioOutputNode->nodeID;
    idRemap[1] = audioInputNode->nodeID;
    idRemap[2] = audioOutputNode->nodeID;

    auto connections = state.getChildWithName ("CONNECTIONS");
    bool hasValidConnections = (connections.isValid() && connections.getNumChildren() > 0);

    if (auto nodes = state.getChildWithName ("NODES"); nodes.isValid() && hasValidConnections)
    {
        for (auto n : nodes)
        {
            auto typeId = n["type"].toString();
            auto node = addModule (typeId, { (int) n["x"], (int) n["y"] });
            if (node == nullptr)
                continue;

            juce::MemoryBlock nodeState;
            nodeState.fromBase64Encoding (n["state"].toString());
            node->getProcessor()->setStateInformation (nodeState.getData(), (int) nodeState.getSize());

            if (n.hasProperty ("bypassed"))
                node->setBypassed ((bool) n["bypassed"]);

            if (n.hasProperty ("color"))
                node->properties.set ("color", n["color"].toString());

            if (n.hasProperty ("mix"))
            {
                float mixVal = (float) n["mix"];
                node->properties.set ("mix", mixVal);
                if (auto* wrapper = dynamic_cast<MixWrapperProcessor*> (node->getProcessor()))
                    wrapper->setMixLevel (mixVal);
            }

            idRemap[(int) n["id"]] = node->nodeID;
        }
    }

    if (hasValidConnections)
    {
        for (auto& c : graph.getConnections())
            graph.removeConnection (c);

        for (auto c : connections)
        {
            int srcId = (int) c["srcNode"];
            int dstId = (int) c["dstNode"];
            int srcCh = (int) c["srcCh"];
            int dstCh = (int) c["dstCh"];

            auto srcIt = idRemap.find (srcId);
            auto dstIt = idRemap.find (dstId);

            if (srcIt != idRemap.end() && dstIt != idRemap.end())
            {
                connect (srcIt->second, srcCh, dstIt->second, dstCh);
            }
        }
    }
    else
    {
        clearGraphToDefault();
    }

    updateOutputConnectionFlags();
    sendChangeMessage();
}

// =============================================================================
// PluginProcessor Multi-Instance Implementation
// =============================================================================

PluginProcessor::PluginProcessor()
    : juce::AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    addParameter (macro1 = new juce::AudioParameterFloat (juce::ParameterID { "macro1", 1 }, "Macro 1", 0.0f, 1.0f, 0.0f));
    addParameter (macro2 = new juce::AudioParameterFloat (juce::ParameterID { "macro2", 1 }, "Macro 2", 0.0f, 1.0f, 0.0f));
    addParameter (macro3 = new juce::AudioParameterFloat (juce::ParameterID { "macro3", 1 }, "Macro 3", 0.0f, 1.0f, 0.0f));
    addParameter (macro4 = new juce::AudioParameterFloat (juce::ParameterID { "macro4", 1 }, "Macro 4", 0.0f, 1.0f, 0.0f));

    for (int i = 0; i < 4; ++i)
    {
        channels[i] = std::make_unique<ChannelDSPProcessor> (i);
    }
    for (int i = 0; i < 3; ++i)
    {
        auxCaptureWorkers[i] = std::make_unique<AuxAudioCaptureWorker>();
    }
    for (int i = 0; i < 2; ++i)
    {
        auxOutputWorkers[i] = std::make_unique<AuxAudioOutputWorker>();
    }

    inputWaveform.resize (waveformHistorySize, 0.0f);
    outputWaveform.resize (waveformHistorySize, 0.0f);
}

PluginProcessor::~PluginProcessor() = default;

void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate.store ((float) sampleRate);
    currentBlockSize.store (samplesPerBlock);

    for (int i = 0; i < 4; ++i)
        channels[i]->prepareToPlay (sampleRate, samplesPerBlock);

    effectChain.prepareToPlay (sampleRate, samplesPerBlock);
}

void PluginProcessor::releaseResources()
{
    for (int i = 0; i < 4; ++i)
        channels[i]->releaseResources();

    effectChain.releaseResources();
}

void PluginProcessor::switchChannelGraph (int newChannelIndex)
{
    activeChannelIndex = juce::jlimit (0, 3, newChannelIndex);
    sendChangeMessage();
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    auto startTime = juce::Time::getHighResolutionTicks();
    int numSamples = buffer.getNumSamples();
    int numChannels = buffer.getNumChannels();

    if (numSamples <= 0)
        return;

    // 1. Evaluate Solo Matrix
    bool anySolo = false;
    for (int i = 0; i < 4; ++i)
    {
        if (channels[i]->isSoloActive.load())
        {
            anySolo = true;
            break;
        }
    }

    // 2. Prepare 4 Parallel Channel Audio Buffers
    juce::AudioBuffer<float> chBuffers[4];
    for (int i = 0; i < 4; ++i)
    {
        chBuffers[i].setSize (2, numSamples, false, false, true);
        chBuffers[i].clear();
    }

    // Channel 0 (MIC 1): Direct zero-latency hardware input
    if (numChannels >= 2)
    {
        chBuffers[0].copyFrom (0, 0, buffer, 0, 0, numSamples);
        chBuffers[0].copyFrom (1, 0, buffer, 1, 0, numSamples);
    }
    else if (numChannels == 1)
    {
        chBuffers[0].copyFrom (0, 0, buffer, 0, 0, numSamples);
        chBuffers[0].copyFrom (1, 0, buffer, 0, 0, numSamples);
    }

    // Channels 1..3 (DESKTOP, AUX 3, AUX 4): Read resampled from AuxAudioCaptureWorker
    double masterSr = currentSampleRate.load();
    for (int i = 1; i < 4; ++i)
    {
        if (auxCaptureWorkers[i - 1] != nullptr)
        {
            auxCaptureWorkers[i - 1]->readResampled (chBuffers[i], 0, numSamples, masterSr);
        }
    }

    // 3. Sidechain Hysteresis Ducking on Mic 1 pre-fader RMS
    float mic1Rms = chBuffers[0].getRMSLevel (0, 0, numSamples);
    if (chBuffers[0].getNumChannels() > 1)
        mic1Rms = juce::jmax (mic1Rms, chBuffers[0].getRMSLevel (1, 0, numSamples));
    float mic1Db = juce::Decibels::gainToDecibels (mic1Rms + 1e-6f);

    if (mic1Db > -36.0f)
        isSidechainTriggered = true;
    else if (mic1Db < -40.0f)
        isSidechainTriggered = false;

    // 4. Process all 4 channels simultaneously through DSP matrix
    for (int i = 0; i < 4; ++i)
    {
        bool isMuted = channels[i]->muted.load();
        bool isSolo = channels[i]->isSoloActive.load();

        float effectiveGain = 0.0f;
        if (anySolo)
        {
            effectiveGain = (isSolo && !isMuted) ? channels[i]->faderGainLinear.load() : 0.0f;
        }
        else
        {
            effectiveGain = (!isMuted) ? channels[i]->faderGainLinear.load() : 0.0f;
        }

        float duckGain = 1.0f;
        if (channels[i]->isSidechainActive.load() && isSidechainTriggered)
        {
            duckGain = 0.2512f; // -12 dB ducking
        }

        channels[i]->processAudio (chBuffers[i], midi, effectiveGain, duckGain);
    }

    // Process Modular Voice Studio -> Dynamic Effect Chain (Strip 0 -> Strip 1 -> ... -> Strip N)
    effectChain.processChain (chBuffers[0], midi);

    // 5. Route & Sum to Output Buses
    // Main hardware buffer (Cleared in standalone, used only in DAW plugins)
    buffer.clear();

    if (wrapperType == wrapperType_VST3 || wrapperType == wrapperType_AudioUnit || wrapperType == wrapperType_AAX)
    {
        int activeIdx = activeChannelIndex;
        buffer.copyFrom (0, 0, chBuffers[activeIdx], 0, 0, numSamples);
        if (buffer.getNumChannels() > 1)
            buffer.copyFrom (1, 0, chBuffers[activeIdx], 1, 0, numSamples);
    }
    else
    {
        bool monitorWorkerActive = (auxOutputWorkers[1] != nullptr && auxOutputWorkers[1]->isActive());
        if (! monitorWorkerActive && (channels[0]->isMonitoringActive.load() || anySolo))
        {
            buffer.copyFrom (0, 0, chBuffers[0], 0, 0, numSamples);
            if (buffer.getNumChannels() > 1 && chBuffers[0].getNumChannels() > 1)
                buffer.copyFrom (1, 0, chBuffers[0], 1, 0, numSamples);
        }
    }

    // Bus B1 (VB-CABLE Virtual Mic Output)
    juce::AudioBuffer<float> busB1 (2, numSamples);
    busB1.clear();

    for (int i = 0; i < 4; ++i)
    {
        if (channels[i]->isBusB1Active.load())
        {
            busB1.addFrom (0, 0, chBuffers[i], 0, 0, numSamples);
            busB1.addFrom (1, 0, chBuffers[i], 1, 0, numSamples);
        }
    }

    if (auxOutputWorkers[0] != nullptr)
        auxOutputWorkers[0]->writeStream (busB1, 0, numSamples, masterSr);

    // Bus A1 (Headphone Monitor Output - Only sends audio when isMonitoringActive is true)
    juce::AudioBuffer<float> monitorBus (2, numSamples);
    monitorBus.clear();

    for (int i = 0; i < 4; ++i)
    {
        if (channels[i]->isMonitoringActive.load())
        {
            monitorBus.addFrom (0, 0, chBuffers[i], 0, 0, numSamples);
            if (monitorBus.getNumChannels() > 1 && chBuffers[i].getNumChannels() > 1)
                monitorBus.addFrom (1, 0, chBuffers[i], 1, 0, numSamples);
        }
    }

    if (auxOutputWorkers[1] != nullptr)
        auxOutputWorkers[1]->writeStream (monitorBus, 0, numSamples, masterSr);

    // Waveform & Peak Telemetry
    float rawInPeak = chBuffers[0].getMagnitude (0, numSamples);
    float prevIn = inputLevel.load();
    inputLevel.store (rawInPeak > prevIn ? rawInPeak : prevIn * 0.92f);

    float currentPeak = (numSamples > 0 && buffer.getNumChannels() > 0) ? buffer.getMagnitude (0, numSamples) : 0.0f;
    float prevLevel = outputLevel.load();
    outputLevel.store (currentPeak > prevLevel ? currentPeak : prevLevel * 0.92f);

    if (numSamples > 0 && buffer.getNumChannels() > 0)
    {
        const float* outPtr = buffer.getReadPointer (0);
        std::lock_guard<std::mutex> lock (waveformMutex);
        int writeBack = (waveformWriteIndex - numSamples + waveformHistorySize) % waveformHistorySize;
        for (int i = 0; i < numSamples; ++i)
            outputWaveform[(writeBack + i) % waveformHistorySize] = outPtr[i];
    }

    auto endTime = juce::Time::getHighResolutionTicks();
    double elapsedSeconds = juce::Time::highResolutionTicksToSeconds (endTime - startTime);
    float elapsedMs = (float) (elapsedSeconds * 1000.0);
    float prevProc = currentProcessTimeMs.load();
    currentProcessTimeMs.store (prevProc * 0.92f + elapsedMs * 0.08f);

    double sr = currentSampleRate.load();
    if (sr > 0.0)
    {
        double blockBudget = (double) numSamples / sr;
        if (blockBudget > 0.0)
        {
            float cpuPercent = (float) ((elapsedSeconds / blockBudget) * 100.0);
            cpuPercent = juce::jlimit (0.0f, 100.0f, cpuPercent);
            float prevCpu = currentCpuUsage.load();
            currentCpuUsage.store (prevCpu * 0.92f + cpuPercent * 0.08f);
        }
    }
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PluginProcessor::getWaveformData (std::vector<float>& inDest, std::vector<float>& outDest)
{
    std::lock_guard<std::mutex> lock (waveformMutex);
    inDest.resize (waveformHistorySize);
    outDest.resize (waveformHistorySize);

    for (int i = 0; i < waveformHistorySize; ++i)
    {
        int idx = (waveformWriteIndex + i) % waveformHistorySize;
        inDest[i]  = inputWaveform[idx];
        outDest[i] = outputWaveform[idx];
    }
}

int PluginProcessor::getNumPrograms() { return 1; }
int PluginProcessor::getCurrentProgram() { return getActiveChannel().getPresetManager().getCurrentPresetIndex(); }
void PluginProcessor::setCurrentProgram (int index)
{
    getActiveChannel().getPresetManager().loadPreset (index, *this);
    sendChangeMessage();
}
const juce::String PluginProcessor::getProgramName (int index)
{
    const auto& names = getActiveChannel().getPresetManager().getPresetNames();
    if (juce::isPositiveAndBelow (index, names.size()))
        return names[index];
    return "Default Patch";
}
void PluginProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index);
    if (newName.isNotEmpty())
    {
        getActiveChannel().getPresetManager().savePreset (newName, *this);
        sendChangeMessage();
    }
}

void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state ("MODULAR_AUDIO_OS_MULTI");
    state.setProperty ("activeChannel", activeChannelIndex, nullptr);
    state.setProperty ("pluginBypassed", pluginBypassed.load(), nullptr);
    state.setProperty ("language", LocalizationManager::instance().getLanguageCode(), nullptr);

    for (int i = 0; i < 4; ++i)
    {
        juce::MemoryBlock chData;
        channels[i]->getStateInformation (chData);
        juce::ValueTree chTree ("CHANNEL");
        chTree.setProperty ("index", i, nullptr);
        chTree.setProperty ("data", chData.toBase64Encoding(), nullptr);
        state.appendChild (chTree, nullptr);
    }

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr)
        return;

    auto state = juce::ValueTree::fromXml (*xml);
    if (state.hasType ("MODULAR_AUDIO_OS_MULTI"))
    {
        if (state.hasProperty ("activeChannel"))
            activeChannelIndex = (int) state["activeChannel"];

        for (auto chTree : state)
        {
            if (chTree.hasType ("CHANNEL"))
            {
                int idx = (int) chTree["index"];
                if (idx >= 0 && idx < 4)
                {
                    juce::MemoryBlock chData;
                    chData.fromBase64Encoding (chTree["data"].toString());
                    channels[idx]->setStateInformation (chData.getData(), (int) chData.getSize());
                }
            }
        }
    }
    else
    {
        // Legacy single-graph state backward compatibility
        channels[0]->setStateInformation (data, sizeInBytes);
    }

    sendChangeMessage();
}

void PluginProcessor::resetAllToFactoryDefaults()
{
    for (int i = 0; i < 4; ++i)
    {
        channels[i]->clearGraphToDefault();
        channels[i]->faderGainLinear.store (1.0f);
        channels[i]->muted.store (false);
        channels[i]->isSoloActive.store (false);
        channels[i]->isBypassed.store (false);
        channels[i]->isMonitoringActive.store (false);
        channels[i]->isBusB1Active.store (true);
    }
    sendChangeMessage();
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
