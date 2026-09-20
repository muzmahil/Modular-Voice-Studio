#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Graph/ModuleProcessor.h"
#include "Graph/MixWrapperProcessor.h"
#include "Localization.h"

PluginProcessor::PluginProcessor()
    : juce::AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    addParameter (macro1 = new juce::AudioParameterFloat (juce::ParameterID { "macro1", 1 }, "Macro 1", 0.0f, 1.0f, 0.0f));
    addParameter (macro2 = new juce::AudioParameterFloat (juce::ParameterID { "macro2", 1 }, "Macro 2", 0.0f, 1.0f, 0.0f));
    addParameter (macro3 = new juce::AudioParameterFloat (juce::ParameterID { "macro3", 1 }, "Macro 3", 0.0f, 1.0f, 0.0f));
    addParameter (macro4 = new juce::AudioParameterFloat (juce::ParameterID { "macro4", 1 }, "Macro 4", 0.0f, 1.0f, 0.0f));

    initialiseGraph();

    inputWaveform.resize (waveformHistorySize, 0.0f);
    outputWaveform.resize (waveformHistorySize, 0.0f);
}

PluginProcessor::~PluginProcessor() = default;

void PluginProcessor::initialiseGraph()
{
    graph.clear();

    audioInputNode = graph.addNode (std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor> (
        juce::AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode));
    audioOutputNode = graph.addNode (std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor> (
        juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode));

    // Default on-canvas positions; overwritten on state reload if the user moved them.
    audioInputNode->properties.set ("x", 1700); 
    audioInputNode->properties.set ("y", 1900);
    audioOutputNode->properties.set ("x", 2100);
    audioOutputNode->properties.set ("y", 1900);
    // Default empty chain: input wired straight to output.
    // The user rewires this from the canvas as soon as modules are added.
    for (int ch = 0; ch < 2; ++ch)
        graph.addConnection ({ { audioInputNode->nodeID, ch }, { audioOutputNode->nodeID, ch } });

    updateOutputConnectionFlags();
}

void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate.store ((float) sampleRate);
    currentBlockSize.store (samplesPerBlock);

    graph.setPlayConfigDetails (getMainBusNumInputChannels(), getMainBusNumOutputChannels(),
                                 sampleRate, samplesPerBlock);
    graph.prepareToPlay (sampleRate, samplesPerBlock);
    updateOutputConnectionFlags();
}

void PluginProcessor::releaseResources()
{
    graph.releaseResources();
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    auto startTime = juce::Time::getHighResolutionTicks();
    int numSamples = buffer.getNumSamples();

    float inPeak = buffer.getMagnitude (0, numSamples);
    float prevIn = inputLevel.load();
    inputLevel.store (inPeak > prevIn ? inPeak : prevIn * 0.92f);

    // 1. Capture incoming dry audio for global waveform monitor
    if (numSamples > 0 && buffer.getNumChannels() > 0)
    {
        const float* inPtr = buffer.getReadPointer (0);
        std::lock_guard<std::mutex> lock (waveformMutex);
        for (int i = 0; i < numSamples; ++i)
        {
            inputWaveform[waveformWriteIndex] = inPtr[i];
            waveformWriteIndex = (waveformWriteIndex + 1) % waveformHistorySize;
        }
    }

    if (pluginBypassed.load())
    {
        // Master plugin bypass: DAW audio passes straight through completely untouched (dry bit-perfect).
        outputLevel.store (inPeak > prevIn ? inPeak : prevIn * 0.92f);
        currentCpuUsage.store (0.0f);
        currentProcessTimeMs.store (0.0f);

        // In bypass, output waveform mirrors input
        if (numSamples > 0 && buffer.getNumChannels() > 0)
        {
            const float* inPtr = buffer.getReadPointer (0);
            std::lock_guard<std::mutex> lock (waveformMutex);
            int writeBack = (waveformWriteIndex - numSamples + waveformHistorySize) % waveformHistorySize;
            for (int i = 0; i < numSamples; ++i)
                outputWaveform[(writeBack + i) % waveformHistorySize] = inPtr[i];
        }
        return;
    }

    bool ch0 = outputChannelConnected[0].load();
    bool ch1 = outputChannelConnected[1].load();

    graph.processBlock (buffer, midi);

    // 2. CRITICAL AUDIO LEAK FIX:
    // If Audio Output node has no connections to channel 0 or 1, silence that channel!
    // Prevents unrouted dry input audio from leaking into the DAW track.
    if (! ch0 && buffer.getNumChannels() > 0)
        buffer.clear (0, 0, numSamples);
    if (! ch1 && buffer.getNumChannels() > 1)
        buffer.clear (1, 0, numSamples);

    // 3. Capture processed output audio for global waveform monitor
    if (numSamples > 0 && buffer.getNumChannels() > 0)
    {
        const float* outPtr = buffer.getReadPointer (0);
        std::lock_guard<std::mutex> lock (waveformMutex);
        int writeBack = (waveformWriteIndex - numSamples + waveformHistorySize) % waveformHistorySize;
        for (int i = 0; i < numSamples; ++i)
            outputWaveform[(writeBack + i) % waveformHistorySize] = outPtr[i];
    }

    float currentPeak = buffer.getMagnitude (0, numSamples);
    float prevLevel = outputLevel.load();
    outputLevel.store (currentPeak > prevLevel ? currentPeak : prevLevel * 0.92f);

    auto endTime = juce::Time::getHighResolutionTicks();
    double elapsedSeconds = juce::Time::highResolutionTicksToSeconds (endTime - startTime);
    float elapsedMs = (float) (elapsedSeconds * 1000.0);

    // Smooth response time (ms) and CPU usage percentage (%)
    float prevProc = currentProcessTimeMs.load();
    currentProcessTimeMs.store (prevProc * 0.92f + elapsedMs * 0.08f);

    double sr = (double) currentSampleRate.load();
    if (sr > 0.0 && numSamples > 0)
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

void PluginProcessor::updateOutputConnectionFlags()
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

void PluginProcessor::getWaveformData (std::vector<float>& inDest, std::vector<float>& outDest)
{
    inDest.resize (waveformHistorySize);
    outDest.resize (waveformHistorySize);

    std::lock_guard<std::mutex> lock (waveformMutex);
    int idx = waveformWriteIndex;
    for (int i = 0; i < waveformHistorySize; ++i)
    {
        inDest[i]  = inputWaveform[(idx + i) % waveformHistorySize];
        outDest[i] = outputWaveform[(idx + i) % waveformHistorySize];
    }
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo()
        && layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

int PluginProcessor::getNumPrograms()
{
    int num = presetManager.getPresetNames().size();
    return num > 0 ? num : 1;
}

int PluginProcessor::getCurrentProgram()
{
    int idx = presetManager.getCurrentPresetIndex();
    return idx >= 0 ? idx : 0;
}

void PluginProcessor::setCurrentProgram (int index)
{
    if (index >= 0 && index < presetManager.getPresetNames().size())
    {
        presetManager.loadPreset (index, *this);
        sendChangeMessage();
    }
}

const juce::String PluginProcessor::getProgramName (int index)
{
    const auto& names = presetManager.getPresetNames();
    if (juce::isPositiveAndBelow (index, names.size()))
        return names[index];
    return "Default Patch";
}

void PluginProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index);
    if (newName.isNotEmpty())
    {
        presetManager.savePreset (newName, *this);
        sendChangeMessage();
    }
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

PluginProcessor::Node::Ptr PluginProcessor::addModule (const juce::String& typeId, juce::Point<int> canvasPosition)
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

        sendChangeMessage();
        historyManager.pushSnapshot (*this);
        return node;
    }
    catch (const std::exception& e)
    {
        juce::Logger::writeToLog ("Error adding module " + typeId + ": " + e.what());
        return nullptr;
    }
    catch (...)
    {
        juce::Logger::writeToLog ("Unknown error adding module " + typeId);
        return nullptr;
    }
}

void PluginProcessor::removeModule (NodeID id)
{
    const juce::ScopedLock sl (graph.getCallbackLock());
    graph.removeNode (id);
    updateOutputConnectionFlags();
    sendChangeMessage();
    historyManager.pushSnapshot (*this);
}

void PluginProcessor::removeModuleAndReconnect (NodeID id)
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

    // Reconnect incoming sources directly to outgoing destinations matching channels
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
    historyManager.pushSnapshot (*this);
}

bool PluginProcessor::connect (NodeID sourceNode, int sourceChannel, NodeID destNode, int destChannel)
{
    const juce::ScopedLock sl (graph.getCallbackLock());
    bool ok = graph.addConnection ({ { sourceNode, sourceChannel }, { destNode, destChannel } });
    if (ok)
    {
        updateOutputConnectionFlags();
        sendChangeMessage();
        historyManager.pushSnapshot (*this);
    }
    return ok;
}

void PluginProcessor::disconnect (const Connection& connection)
{
    const juce::ScopedLock sl (graph.getCallbackLock());
    graph.removeConnection (connection);
    updateOutputConnectionFlags();
    sendChangeMessage();
    historyManager.pushSnapshot (*this);
}

void PluginProcessor::clearGraphToDefault()
{
    const juce::ScopedLock sl (graph.getCallbackLock());
    initialiseGraph();
    updateOutputConnectionFlags();
    sendChangeMessage();
    historyManager.pushSnapshot (*this);
}

void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ValueTree state ("MODULAR_AUDIO_OS");

    // I/O node positions & IDs are stored explicitly
    state.setProperty ("inputX",  (int) audioInputNode->properties["x"], nullptr);
    state.setProperty ("inputY",  (int) audioInputNode->properties["y"], nullptr);
    state.setProperty ("outputX", (int) audioOutputNode->properties["x"], nullptr);
    state.setProperty ("outputY", (int) audioOutputNode->properties["y"], nullptr);
    state.setProperty ("inputID",  (int) audioInputNode->nodeID.uid, nullptr);
    state.setProperty ("outputID", (int) audioOutputNode->nodeID.uid, nullptr);

    // Persist Canvas camera position across sessions
    state.setProperty ("panX", canvasPanX, nullptr);
    state.setProperty ("panY", canvasPanY, nullptr);
    state.setProperty ("zoom", canvasZoom, nullptr);
    state.setProperty ("pluginBypassed", pluginBypassed.load(), nullptr);
    state.setProperty ("language", LocalizationManager::instance().getLanguageCode(), nullptr);
    state.setProperty ("macro1", macro1->get(), nullptr);
    state.setProperty ("macro2", macro2->get(), nullptr);
    state.setProperty ("macro3", macro3->get(), nullptr);
    state.setProperty ("macro4", macro4->get(), nullptr);

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
        copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
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

    if (state.hasProperty ("pluginBypassed"))
        pluginBypassed.store ((bool) state["pluginBypassed"]);

    if (state.hasProperty ("language"))
        LocalizationManager::instance().setLanguageFromCode (state["language"].toString());

    if (state.hasProperty ("macro1") && macro1 != nullptr) *macro1 = (float) state["macro1"];
    if (state.hasProperty ("macro2") && macro2 != nullptr) *macro2 = (float) state["macro2"];
    if (state.hasProperty ("macro3") && macro3 != nullptr) *macro3 = (float) state["macro3"];
    if (state.hasProperty ("macro4") && macro4 != nullptr) *macro4 = (float) state["macro4"];

    std::map<int, NodeID> idRemap;
    int savedInID  = (int) state.getProperty ("inputID",  (int) audioInputNode->nodeID.uid);
    int savedOutID = (int) state.getProperty ("outputID", (int) audioOutputNode->nodeID.uid);
    idRemap[savedInID]  = audioInputNode->nodeID;
    idRemap[savedOutID] = audioOutputNode->nodeID;
    idRemap[1] = audioInputNode->nodeID;
    idRemap[2] = audioOutputNode->nodeID;

    if (auto nodes = state.getChildWithName ("NODES"); nodes.isValid())
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

    if (auto connections = state.getChildWithName ("CONNECTIONS"); connections.isValid() && connections.getNumChildren() > 0)
    {
        // Clear the default direct 1:1 input->output connection so only saved user cables are active
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

    updateOutputConnectionFlags();
    sendChangeMessage();
}

// =============================================================================
// GraphHistoryManager Implementation
// =============================================================================
void GraphHistoryManager::pushSnapshot (PluginProcessor& processor)
{
    if (isPerformingUndoRedo)
        return;

    juce::MemoryBlock state;
    processor.getStateInformation (state);

    if (state.getSize() == 0)
        return;

    if (! undoStack.empty() && undoStack.back() == state)
        return;

    undoStack.push_back (state);
    redoStack.clear();

    if (undoStack.size() > maxHistorySteps)
        undoStack.erase (undoStack.begin());
}

void GraphHistoryManager::performUndo (PluginProcessor& processor)
{
    if (! canUndo() || isPerformingUndoRedo)
        return;

    isPerformingUndoRedo = true;

    auto current = undoStack.back();
    undoStack.pop_back();
    redoStack.push_back (current);

    auto prev = undoStack.back();
    processor.setStateInformation (prev.getData(), (int) prev.getSize());

    isPerformingUndoRedo = false;
}

void GraphHistoryManager::performRedo (PluginProcessor& processor)
{
    if (! canRedo() || isPerformingUndoRedo)
        return;

    isPerformingUndoRedo = true;

    auto next = redoStack.back();
    redoStack.pop_back();
    undoStack.push_back (next);

    processor.setStateInformation (next.getData(), (int) next.getSize());

    isPerformingUndoRedo = false;
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
