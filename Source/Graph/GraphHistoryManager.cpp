#include "GraphHistoryManager.h"
#include "../PluginProcessor.h"

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
