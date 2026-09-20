#pragma once
#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

class PluginProcessor;

class GraphHistoryManager
{
public:
    GraphHistoryManager() = default;
    ~GraphHistoryManager() = default;

    void clear()
    {
        undoStack.clear();
        redoStack.clear();
    }

    void pushSnapshot (PluginProcessor& processor);

    bool canUndo() const { return undoStack.size() > 1; }
    bool canRedo() const { return ! redoStack.empty(); }

    void performUndo (PluginProcessor& processor);
    void performRedo (PluginProcessor& processor);

    bool isPerforming() const { return isPerformingUndoRedo; }

private:
    std::vector<juce::MemoryBlock> undoStack;
    std::vector<juce::MemoryBlock> redoStack;
    bool isPerformingUndoRedo = false;
    static constexpr size_t maxHistorySteps = 50;
};
