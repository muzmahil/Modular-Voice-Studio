#pragma once
#include "../Localization.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include "NodeComponent.h"
#include "../PluginProcessor.h"
#include <cmath>
#include <set>

/**
    Pan/zoom-able grid canvas that hosts NodeComponents and draws bezier
    cables between them.
      - Left-drag on empty space  -> pan
      - Mouse wheel               -> zoom (centered on cursor)
      - Right-click empty space   -> categorized "Add Module" menu + view actions
      - Right-click a node        -> node context menu (delete, etc.)
      - Drag from a pin           -> draw/complete a connection
*/

class StickyNote;

class NodeCanvas : public juce::Component,
                   public juce::ChangeListener,
                   public juce::DragAndDropTarget,
                   public LocalizationManager::Listener
{
public:
    explicit NodeCanvas(PluginProcessor &processorToEdit);
    ~NodeCanvas() override;
    bool isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails& details) override;
    void itemDropped (const juce::DragAndDropTarget::SourceDetails& details) override;
    void dropModule (const juce::String& type, juce::Point<int> dropPos);
    void removeStickyNote(StickyNote *note);
    void checkPinning(StickyNote *note);
    void paintOverChildren(juce::Graphics &) override;
    void paint(juce::Graphics &) override;
    bool isPinConnected(juce::AudioProcessorGraph::NodeID id, bool isInput, int channel = -1) const;
    void resized() override;
    void mouseDown(const juce::MouseEvent &) override;
    void mouseDrag(const juce::MouseEvent &) override;
    void mouseUp(const juce::MouseEvent &) override;
    void mouseWheelMove(const juce::MouseEvent &, const juce::MouseWheelDetails &) override;
    bool keyPressed(const juce::KeyPress &key) override;
    void changeListenerCallback(juce::ChangeBroadcaster *) override;
    juce::AudioProcessorGraph::Node *getGraphNode(juce::AudioProcessorGraph::NodeID id) const
    {
        return processor.getGraph().getNodeForId(id);
    }
    void disconnectPin(juce::AudioProcessorGraph::NodeID nodeID, bool isInput, int channel = -1);
    // -- called by NodeComponent --------------------------------------------------------
    void beginCableDrag(NodeComponent &fromNode, bool fromIsInput, int channel = 0);
    void updateCableDrag(juce::Point<int> currentPos);
    void endCableDrag(juce::Point<int> dropPos);
    bool isDraggingCable() const { return dragSourceNode != nullptr; }
    bool startReplugCable(NodeComponent &fromComp, bool isInput, int channel = 0);
    void insertNodeOnCableIfIntersected(juce::AudioProcessorGraph::NodeID newNodeID, juce::Point<int> nodePos);
    void notifyNodeMoved(NodeComponent &, juce::Rectangle<int> oldBounds);
    void showNodeContextMenu(NodeComponent &);

    void centerViewOnNodes();
    void localizationChanged() override;

    void resetView();
    void zoomIn();
    void zoomOut();
    void zoomAt (float newZoom, juce::Point<float> pivotScreen);
    void duplicateSelectedNodes();
    int getZoomPercent() const { return (int) std::lround (zoom * 100.0f); }
    void setSnapToGrid (bool shouldSnap) { snapToGrid = shouldSnap; }
    bool isSnapToGrid() const { return snapToGrid; }
    void setShowCableGlow (bool shouldGlow) { showCableGlow = shouldGlow; repaint(); }
    bool isShowCableGlow() const { return showCableGlow; }
    static constexpr int snapGridSize = 24;

    // Real (not decorative) level readouts, used by NodeComponent for the Audio In/Out meters.
    float getInputLevel() const  { return processor.inputLevel.load(); }
    float getOutputLevel() const { return processor.outputLevel.load(); }

    /** Repaints just the node components (cheap — each is ~150x74) so their live
        parameter/level indicators animate without redrawing the whole 4000x4000 canvas.
        Call this from a timer, not from paint(). */
    void refreshMeters();
    void addModuleAtViewCenter(const juce::String& typeId);

    // Node selection & inspection
    void setSelectedNodeID (juce::AudioProcessorGraph::NodeID id);
    juce::AudioProcessorGraph::NodeID getSelectedNodeID() const { return selectedNodeID; }
    bool isNodeSelected (juce::AudioProcessorGraph::NodeID id) const;
    void addSelectedNode (juce::AudioProcessorGraph::NodeID id, bool deselectOthers);
    void toggleNodeSelection (juce::AudioProcessorGraph::NodeID id);
    void clearSelection();
    const std::set<juce::AudioProcessorGraph::NodeID>& getSelectedNodeIDs() const { return selectedNodeIDs; }
    void packSelectedNodesIntoContainer();
    std::function<void(juce::AudioProcessorGraph::NodeID)> onNodeSelected;

    // Container drop target & adoption
    void setContainerDropTarget (juce::AudioProcessorGraph::NodeID containerID, juce::AudioProcessorGraph::NodeID draggedID = {});
    juce::AudioProcessorGraph::NodeID getContainerDropTarget() const { return containerDropTargetID; }
    juce::AudioProcessorGraph::NodeID getDraggedOverNodeID() const { return draggedOverNodeID; }
    bool adoptNodeIntoContainer (juce::AudioProcessorGraph::NodeID sourceNodeID, juce::AudioProcessorGraph::NodeID targetContainerID);
    void extractModuleFromContainer (juce::AudioProcessorGraph::NodeID containerID, int index);
    void unpackContainer (juce::AudioProcessorGraph::NodeID containerID);
    void requestDeleteNode (juce::AudioProcessorGraph::NodeID nodeID);

    // Viewport navigation & inspection helpers
    juce::Rectangle<float> getVisibleCanvasArea() const;
    void panToLogicalCenter (juce::Point<float> logicalCenter);
    void setPanAndZoom (float panX, float panY, float newZoom)
    {
        pan = { panX, panY };
        zoom = juce::jlimit (0.4f, 2.0f, newZoom);
        applyTransform();
        repaint();
    }
    void rebuildFromGraph();
    void openModuleEditor (juce::AudioProcessorGraph::NodeID id);
    NodeComponent *findComponentFor(juce::AudioProcessorGraph::NodeID) const;

private:
    PluginProcessor &processor;
    juce::AudioProcessorGraph::NodeID selectedNodeID;
    std::set<juce::AudioProcessorGraph::NodeID> selectedNodeIDs;
    juce::AudioProcessorGraph::NodeID containerDropTargetID;
    juce::AudioProcessorGraph::NodeID draggedOverNodeID;
    bool isBoxSelecting = false;
    juce::Point<int> boxSelectStart;
    juce::Rectangle<int> boxSelectRect;
    juce::OwnedArray<NodeComponent> nodeComponents;
    // pan / zoom
    float zoom = 1.0f;
    juce::Point<float> pan{0.0f, 0.0f};
    bool isPanning = false;
    bool isRightClickCandidate = false;
    juce::Point<int> panStartMouse, panStartOffset;
    juce::OwnedArray<StickyNote> stickyNotes;
    // in-progress cable drag state
    NodeComponent *dragSourceNode = nullptr;
    bool dragSourceIsInput = false;
    int dragSourceChannel = 0;
    juce::Point<int> dragCurrentPos;
    std::map<juce::String, juce::Point<float>> cableOffsets;
    juce::String activeCableDragId;
    bool isDraggingCableRoute = false;
    bool snapToGrid = false;
    bool showCableGlow = true;
    bool isReplugging = false;
    juce::AudioProcessorGraph::NodeID replugOldSrcID;
    juce::AudioProcessorGraph::NodeID replugOldDstID;
    juce::Point<float> activeCableDragStartOffset;
    juce::Point<int> activeCableDragMouseStart;
    void applyTransform();
    void showAddModuleMenu(juce::Point<int> position);
    bool hitTestCable (juce::Point<float> pos, juce::AudioProcessorGraph::Connection &outConn, juce::String &outCableId) const;
    void showCableContextMenu (const juce::AudioProcessorGraph::Connection &conn, const juce::String &cableId, juce::Point<int> clickPos);
    juce::Rectangle<int> boundsForCable(juce::Point<float> a, juce::Point<float> b) const;
    static juce::Path makeCablePath(juce::Point<float> start, juce::Point<float> end, juce::Point<float> offset = {});
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NodeCanvas)
};
