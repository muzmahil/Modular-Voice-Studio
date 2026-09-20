#include "NodeCanvas.h"
#include "../Graph/ModuleFactory.h"
#include "../Graph/MixWrapperProcessor.h"
#include "../Modules/ContainerModule.h"
#include "../UITheme.h"
#include "../Localization.h"
#include <cmath>

static constexpr int kCanvasLogicalSize = 4000;
static constexpr int kGridSize = 24;





class StickyNote : public juce::Component, public juce::TextEditor::Listener {
public:
    StickyNote(NodeCanvas& owner) : canvas(owner) {
        setOpaque(false);

        editor.addListener(this);
        editor.setMultiLine(true, true); // Word wrap enabled!
        editor.setReturnKeyStartsNewLine(true);
        editor.setScrollbarsShown(true);
        
        editor.setOpaque(false);
        editor.setBorder(juce::BorderSize<int>(0)); 
        editor.setIndents(4, 4);
        
        editor.setColour(juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
        editor.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
        editor.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
        editor.setColour(juce::TextEditor::textColourId, juce::Colour (0xffececf2)); 
        editor.setTextToShowWhenEmpty ("Write notes, signal chain details, vocal settings...", juce::Colour (0xff70707d));
        editor.setFont (UITheme::getFont (12.5f));

        addAndMakeVisible(editor);
        setSize(220, 130); 
    }

    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);
        
        // 3D Soft Drop Shadow
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (bounds.translated(2.0f, 3.0f), 6.0f);

        // Apple Notes Dark Sheet Background
        juce::ColourGradient bgGrad (juce::Colour (0xff28282d), 0, bounds.getY(),
                                     juce::Colour (0xff202024), 0, bounds.getBottom(), false);
        g.setGradientFill (bgGrad);
        g.fillRoundedRectangle (bounds, 8.0f);
        
        // Header Bar
        auto headerRect = bounds.withHeight (22.0f);
        g.setColour (juce::Colour (0xff2f2f35));
        g.fillRoundedRectangle (headerRect, 8.0f);
        g.fillRect (headerRect.getX(), headerRect.getY() + 12.0f, headerRect.getWidth(), 10.0f);

        // Apple Notes Amber Indicator
        g.setColour (UITheme::appleOrange);
        g.fillRoundedRectangle (bounds.getX() + 6.0f, bounds.getY() + 4.0f, 14.0f, 2.5f, 1.0f);

        // Title: "Note"
        g.setColour (UITheme::textPrimary);
        g.setFont (UITheme::getFont (10.5f));
        g.drawText ("Note", juce::Rectangle<float> (bounds.getX() + 26.0f, bounds.getY(), 60.0f, 22.0f), juce::Justification::centredLeft);

        // Close '✕' Button
        float closeBtnX = bounds.getRight() - 18.0f;
        float closeBtnY = bounds.getY() + 11.0f;
        g.setColour (isHoveringClose ? UITheme::appleRed : UITheme::textSecondary);
        g.drawLine (closeBtnX - 3.5f, closeBtnY - 3.5f, closeBtnX + 3.5f, closeBtnY + 3.5f, 1.4f);
        g.drawLine (closeBtnX + 3.5f, closeBtnY - 3.5f, closeBtnX - 3.5f, closeBtnY + 3.5f, 1.4f);

        // Pin Icon
        if (pinnedNodeID.uid != 0) {
            g.setColour (UITheme::appleBlue);
            g.fillEllipse (bounds.getX() + 64.0f, bounds.getY() + 7.0f, 7.0f, 7.0f);
            g.drawLine (bounds.getX() + 67.5f, bounds.getY() + 14.0f, bounds.getX() + 67.5f, bounds.getY() + 19.0f, 1.5f);
        }

        // Resize Grip
        float rx = bounds.getRight() - 4.0f;
        float ry = bounds.getBottom() - 4.0f;
        g.setColour (UITheme::textTertiary);
        g.drawLine (rx - 8.0f, ry, rx, ry - 8.0f, 1.2f);
        g.drawLine (rx - 4.0f, ry, rx, ry - 4.0f, 1.2f);

        // Apple Keyline Outline
        g.setColour (UITheme::strokeHairline);
        g.drawRoundedRectangle (bounds, 8.0f, 1.0f);
    }

    void resized() override {
        editor.setBounds(getLocalBounds().withTrimmedTop(24).reduced(8, 6)); 
    }

    void textEditorTextChanged(juce::TextEditor& ed) override {
        // Otomatik satır boyu taşma kontrolü (genişliği bozmadan dikey genişlet)
        auto text = ed.getText();
        int numLines = 1;
        for (int i = 0; i < text.length(); ++i) 
            if (text[i] == '\n') numLines++;

        float textW = (float) ed.getFont().getStringWidth (text);
        float availW = (float) juce::jmax (40, ed.getWidth() - 10);
        int estimatedLines = juce::jmax (numLines, (int) std::ceil (textW / availW));
        int neededHeight = estimatedLines * 20 + 46;

        if (neededHeight > getHeight())
            setSize(getWidth(), juce::jmin (500, neededHeight));
    }

    void mouseMove (const juce::MouseEvent& e) override {
        bool inResizeCorner = (e.x >= getWidth() - 16 && e.y >= getHeight() - 16);
        bool inCloseBtn = (e.x >= getWidth() - 24 && e.x <= getWidth() - 4 && e.y >= 2 && e.y <= 22);

        if (inCloseBtn != isHoveringClose) {
            isHoveringClose = inCloseBtn;
            repaint();
        }

        if (inResizeCorner)
            setMouseCursor (juce::MouseCursor::BottomRightCornerResizeCursor);
        else if (e.y < 24)
            setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        else
            setMouseCursor (juce::MouseCursor::NormalCursor);
    }

    void mouseDown(const juce::MouseEvent& e) override {
        toFront(true);

        // Kapatma 'X' butonuna tıklandıysa sil
        if (e.x >= getWidth() - 24 && e.x <= getWidth() - 4 && e.y >= 2 && e.y <= 22) {
            canvas.removeStickyNote(this);
            return;
        }

        if (e.mods.isRightButtonDown()) {
            juce::PopupMenu m;
            m.addItem(1, "Notu Sil");
            m.addItem(2, pinnedNodeID.uid != 0 ? "Node'dan Ayir" : "Pinle: Node uzerine surukle", pinnedNodeID.uid != 0, false);
            
            m.showMenuAsync(juce::PopupMenu::Options(), [this](int result){
                if (result == 1) canvas.removeStickyNote(this);
                else if (result == 2) { pinnedNodeID.uid = 0; repaint(); } 
            });
            return;
        }

        // Yeniden boyutlandırma köşesi mi?
        if (e.x >= getWidth() - 16 && e.y >= getHeight() - 16) {
            isResizing = true;
            resizeStartSize = juce::Point<int> (getWidth(), getHeight());
            resizeStartPos = e.getScreenPosition();
        } else {
            isResizing = false;
            dragger.startDraggingComponent(this, e);
        }
    }

    void mouseDrag(const juce::MouseEvent& e) override {
        if (isResizing) {
            auto delta = e.getScreenPosition() - resizeStartPos;
            int newW = juce::jlimit (160, 600, resizeStartSize.x + delta.x);
            int newH = juce::jlimit (90, 600, resizeStartSize.y + delta.y);
            setSize (newW, newH);
        } else {
            dragger.dragComponent(this, e, nullptr);
        }
    }
    
    void mouseUp(const juce::MouseEvent& e) override {
        isResizing = false;
        if (!e.mods.isRightButtonDown()) {
            canvas.checkPinning(this);
        }
    }

    juce::AudioProcessorGraph::NodeID pinnedNodeID { 0 };
    juce::Point<int> pinOffset;

private:
    NodeCanvas& canvas;
    juce::TextEditor editor;
    juce::ComponentDragger dragger;
    bool isResizing = false;
    bool isHoveringClose = false;
    juce::Point<int> resizeStartSize;
    juce::Point<int> resizeStartPos;
};

void NodeCanvas::removeStickyNote(StickyNote *note)
{
    stickyNotes.removeObject(note); // Listeden ve ekrandan kalıcı olarak siler
}

void NodeCanvas::checkPinning(StickyNote *note)
{
    // Sürükleme bittiğinde, not herhangi bir Node'un üzerine bırakıldıysa ona iğnele
    for (auto *comp : nodeComponents)
    {
        if (comp->getBounds().intersects(note->getBounds()))
        {
            note->pinnedNodeID = comp->getNodeID();
            // Node hareket ettiğinde aradaki mesafeyi korumak için offset hesaplıyoruz
            note->pinOffset = note->getPosition() - comp->getPosition();
            note->repaint(); // Kırmızı raptiyeyi çizdir
            return;
        }
    }
    // Boşluğa bırakıldıysa pinlemeyi kaldır
    note->pinnedNodeID.uid = 0;
    note->repaint();
}

NodeCanvas::NodeCanvas(PluginProcessor &processorToEdit)
    : processor(processorToEdit)
{
    setSize(kCanvasLogicalSize, kCanvasLogicalSize);
    setWantsKeyboardFocus(true);
    processor.addChangeListener(this);
    rebuildFromGraph();

    pan = { processor.canvasPanX, processor.canvasPanY };
    zoom = processor.canvasZoom;
    applyTransform();
    LocalizationManager::instance().addListener (this);
}

NodeCanvas::~NodeCanvas()
{
    LocalizationManager::instance().removeListener (this);
    processor.removeChangeListener(this);
}

void NodeCanvas::resized() {}

void NodeCanvas::applyTransform()
{
    processor.canvasPanX = pan.x;
    processor.canvasPanY = pan.y;
    processor.canvasZoom = zoom;
    setTransform(juce::AffineTransform::scale(zoom).translated(pan.x, pan.y));
}

void NodeCanvas::localizationChanged()
{
    for (auto* comp : nodeComponents)
    {
        if (comp != nullptr)
            comp->repaint();
    }
    repaint();
}

void NodeCanvas::centerViewOnNodes()
{
    auto nodes = processor.getGraph().getNodes();
    if (nodes.isEmpty())
    {
        zoom = 1.0f;
        pan = {-1400.0f, -1500.0f};
        applyTransform();
        repaint();
        return;
    }

    float minX = 4000.0f, maxX = 0.0f;
    float minY = 4000.0f, maxY = 0.0f;

    for (auto* n : nodes)
    {
        float x = n->properties.contains("x") ? (float) n->properties["x"] : 2000.0f;
        float y = n->properties.contains("y") ? (float) n->properties["y"] : 2000.0f;
        
        minX = juce::jmin (minX, x);
        maxX = juce::jmax (maxX, x + 120.0f);
        minY = juce::jmin (minY, y);
        maxY = juce::jmax (maxY, y + 70.0f);
    }

    juce::Point<float> logicalCenter ((minX + maxX) * 0.5f, (minY + maxY) * 0.5f);
    
    auto* parent = getParentComponent();
    auto viewBounds = parent != nullptr ? parent->getLocalBounds().toFloat() : getLocalBounds().toFloat();
    if (viewBounds.getWidth() > 0 && viewBounds.getHeight() > 0)
    {
        pan.x = (viewBounds.getWidth() * 0.5f) - (logicalCenter.x * zoom);
        pan.y = (viewBounds.getHeight() * 0.5f) - (logicalCenter.y * zoom);
        applyTransform();
        repaint();
    }
}

void NodeCanvas::resetView()
{
    zoom = 1.0f;
    centerViewOnNodes();
}

void NodeCanvas::zoomAt (float newZoom, juce::Point<float> pivotScreen)
{
    newZoom = juce::jlimit (0.25f, 3.0f, newZoom);
    if (std::abs (newZoom - zoom) < 0.001f)
        return;

    pan += pivotScreen * (zoom - newZoom);
    zoom = newZoom;
    applyTransform();
    repaint();
}

void NodeCanvas::zoomIn()
{
    auto parent = getParentComponent();
    auto viewBounds = parent != nullptr ? parent->getLocalBounds().toFloat() : getLocalBounds().toFloat();
    juce::Point<float> center (viewBounds.getWidth() * 0.5f, viewBounds.getHeight() * 0.5f);
    zoomAt (zoom * 1.20f, center);
}

void NodeCanvas::zoomOut()
{
    auto parent = getParentComponent();
    auto viewBounds = parent != nullptr ? parent->getLocalBounds().toFloat() : getLocalBounds().toFloat();
    juce::Point<float> center (viewBounds.getWidth() * 0.5f, viewBounds.getHeight() * 0.5f);
    zoomAt (zoom / 1.20f, center);
}

void NodeCanvas::duplicateSelectedNodes()
{
    auto toDuplicate = selectedNodeIDs;
    if (toDuplicate.empty() && selectedNodeID != juce::AudioProcessorGraph::NodeID())
        toDuplicate.insert (selectedNodeID);

    std::vector<juce::AudioProcessorGraph::NodeID> newlyCreated;

    for (auto id : toDuplicate)
    {
        // Don't duplicate system I/O nodes
        if (id == processor.getAudioInputNodeID() || id == processor.getAudioOutputNodeID())
            continue;

        auto* origNode = getGraphNode (id);
        if (origNode == nullptr)
            continue;

        juce::String typeId = origNode->properties["type"].toString();
        if (typeId.isEmpty())
            continue;

        int posX = (int) origNode->properties["x"] + 35;
        int posY = (int) origNode->properties["y"] + 35;

        auto newNode = processor.addModule (typeId, { posX, posY });
        if (newNode != nullptr)
        {
            // Copy color property
            if (origNode->properties.contains ("color"))
                newNode->properties.set ("color", origNode->properties["color"].toString());

            // Copy processor parameter state
            juce::MemoryBlock stateBlock;
            origNode->getProcessor()->getStateInformation (stateBlock);
            if (stateBlock.getSize() > 0)
                newNode->getProcessor()->setStateInformation (stateBlock.getData(), (int) stateBlock.getSize());

            newlyCreated.push_back (newNode->nodeID);
        }
    }

    if (! newlyCreated.empty())
    {
        clearSelection();
        for (auto id : newlyCreated)
            addSelectedNode (id, false);
        setSelectedNodeID (newlyCreated.front());
    }
}

void NodeCanvas::refreshMeters()
{
    for (auto* c : nodeComponents)
        c->repaint();
}

// -------------------------------------------------------------------------
// Painting: everything below is bounded by g.getClipBounds() so panning/
// zooming a large graph never costs more than what's actually on screen.
// -------------------------------------------------------------------------
void NodeCanvas::paint(juce::Graphics &g)
{
    auto clip = g.getClipBounds();

    // 1. macOS Dark Canvas Background
    g.fillAll (UITheme::bgCanvas);

    // 2. macOS Subtle Design Grid (Soft dots and hairline guides)
    int startX = (clip.getX() / kGridSize) * kGridSize;
    int startY = (clip.getY() / kGridSize) * kGridSize;

    g.setColour (juce::Colour (0xff212126));
    for (int x = startX; x < clip.getRight(); x += kGridSize * 4)
        g.drawVerticalLine (x, (float) clip.getY(), (float) clip.getBottom());
    for (int y = startY; y < clip.getBottom(); y += kGridSize * 4)
        g.drawHorizontalLine (y, (float) clip.getX(), (float) clip.getRight());

    g.setColour (juce::Colour (0xff2c2c34));
    for (int x = startX; x < clip.getRight(); x += kGridSize)
        for (int y = startY; y < clip.getBottom(); y += kGridSize)
            g.fillRect (x, y, 2, 2);
}

bool NodeCanvas::hitTestCable (juce::Point<float> pos, juce::AudioProcessorGraph::Connection &outConn, juce::String &outCableId) const
{
    // Tolerance is kept in *screen* pixels by dividing by zoom, so cables stay just as
    // easy to grab whether you're zoomed in or zoomed way out.
    const float grabRadius = 15.0f / juce::jmax (zoom, 0.05f);

    for (auto& c : processor.getGraph().getConnections())
    {
        auto* srcComp = findComponentFor (c.source.nodeID);
        auto* dstComp = findComponentFor (c.destination.nodeID);
        if (!srcComp || !dstComp) continue;

        auto start = srcComp->getPinPosition (false, c.source.channelIndex);
        auto end   = dstComp->getPinPosition (true, c.destination.channelIndex);
        juce::String cableId = juce::String(c.source.nodeID.uid) + "_" + juce::String(c.destination.nodeID.uid);

        juce::Path p = makeCablePath (start, end, cableOffsets.count (cableId) ? cableOffsets.at (cableId) : juce::Point<float>{});
        juce::Path strokedPath;
        juce::PathStrokeType (grabRadius).createStrokedPath (strokedPath, p);

        if (strokedPath.contains (pos))
        {
            outConn = c;
            outCableId = cableId;
            return true;
        }
    }
    return false;
}

void NodeCanvas::showCableContextMenu (const juce::AudioProcessorGraph::Connection &conn, const juce::String &cableId, juce::Point<int> clickPos)
{
    juce::PopupMenu menu;
    auto &factory = ModuleFactory::instance();

    juce::PopupMenu addModule;
    int itemId = 100;
    std::vector<juce::String> idToType;
    idToType.resize (100);

    for (auto &category : factory.getCategories())
    {
        juce::PopupMenu sub;
        for (auto &type : factory.getTypesInCategory (category))
        {
            sub.addItem (itemId, tr (type));
            idToType.push_back (type);
            itemId++;
        }

        juce::String catDisplay = category;
        if (category.equalsIgnoreCase ("Utility")) catDisplay = tr ("CAT_UTILITY");
        else if (category.equalsIgnoreCase ("Dynamics")) catDisplay = tr ("CAT_DYNAMICS");
        else if (category.equalsIgnoreCase ("Frequency")) catDisplay = tr ("CAT_FREQUENCY");
        else if (category.equalsIgnoreCase ("Cleanup")) catDisplay = tr ("CAT_CLEANUP");
        else if (category.equalsIgnoreCase ("Vocal Tone") || category.equalsIgnoreCase ("Radio Tone")) catDisplay = tr ("CAT_VOCAL_TONE");

        addModule.addSubMenu (catDisplay, sub);
    }

    menu.addSubMenu (tr ("INSERT_MODULE"), addModule);
    menu.addSeparator();
    menu.addItem (1, tr ("DISCONNECT_CABLE"));
    menu.addItem (2, tr ("STRAIGHTEN_CABLE"));

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [this, conn, cableId, clickPos, idToType] (int result)
    {
        if (result == 1)
        {
            // Disconnect all stereo channels between source and destination in a single click
            std::vector<juce::AudioProcessorGraph::Connection> toDisconnect;
            for (auto& c : processor.getGraph().getConnections())
            {
                if (c.source.nodeID == conn.source.nodeID && c.destination.nodeID == conn.destination.nodeID)
                    toDisconnect.push_back (c);
            }
            for (auto& c : toDisconnect)
                processor.disconnect (c);

            cableOffsets.erase (cableId);
            repaint();
        }
        else if (result == 2)
        {
            cableOffsets.erase (cableId);
            repaint();
        }
        else if (result >= 100 && (size_t) result < idToType.size())
        {
            auto typeId = idToType[(size_t) result];
            juce::Point<int> pos = clickPos - juce::Point<int>(60, 35);
            if (snapToGrid)
            {
                pos.x = (pos.x / snapGridSize) * snapGridSize;
                pos.y = (pos.y / snapGridSize) * snapGridSize;
            }

            auto newNode = processor.addModule (typeId, pos);
            if (newNode != nullptr)
            {
                auto srcID = conn.source.nodeID;
                auto dstID = conn.destination.nodeID;

                // Disconnect existing cable(s) between src and dst
                juce::Array<juce::AudioProcessorGraph::Connection> toRemove;
                for (auto& c : processor.getGraph().getConnections())
                {
                    if (c.source.nodeID == srcID && c.destination.nodeID == dstID)
                        toRemove.add (c);
                }
                for (auto& c : toRemove)
                    processor.disconnect (c);

                // Splice the new node directly into the chain!
                processor.connect (srcID, 0, newNode->nodeID, 0);
                processor.connect (srcID, 1, newNode->nodeID, 1);
                processor.connect (newNode->nodeID, 0, dstID, 0);
                processor.connect (newNode->nodeID, 1, dstID, 1);

                repaint();
            }
        }
    });
}

// -------------------------------------------------------------------------
// Navigation: left-drag empty space = pan, wheel = zoom to cursor.
// -------------------------------------------------------------------------
void NodeCanvas::mouseDown(const juce::MouseEvent &e)
{
    if (e.mods.isRightButtonDown())
    {
        isRightClickCandidate = true;
        return;
    }
    if (e.mods.isMiddleButtonDown() || e.mods.isAltDown())
    {
        juce::AudioProcessorGraph::Connection hitConn;
        juce::String hitCableId;
        if (e.mods.isMiddleButtonDown() && hitTestCable (e.getPosition().toFloat(), hitConn, hitCableId))
        {
            activeCableDragId = hitCableId;
            isDraggingCableRoute = true;
            activeCableDragStartOffset = cableOffsets[hitCableId];
            activeCableDragMouseStart = e.getScreenPosition();
            return;
        }

        // Panning with Middle-click or Alt+click
        isPanning = true;
        panStartMouse = e.getScreenPosition();
        panStartOffset = pan.toInt();
        return;
    }

    // Left-click on empty canvas starts Rubber-Band Box Selection!
    if (! e.mods.isShiftDown())
        clearSelection();

    isBoxSelecting = true;
    boxSelectStart = e.getPosition();
    boxSelectRect = juce::Rectangle<int> (boxSelectStart, boxSelectStart);
    repaint();
}

void NodeCanvas::mouseDrag(const juce::MouseEvent &e)
{
    if (isDraggingCableRoute && activeCableDragId.isNotEmpty())
    {
        auto delta = e.getScreenPosition() - activeCableDragMouseStart;
        cableOffsets[activeCableDragId] = activeCableDragStartOffset + (delta.toFloat() / zoom);
        repaint();
        return;
    }

    if (isPanning)
    {
        auto delta = e.getScreenPosition() - panStartMouse;
        pan = (panStartOffset + delta).toFloat();
        applyTransform();
        repaint();
        return;
    }

    if (isBoxSelecting)
    {
        boxSelectRect = juce::Rectangle<int>::leftTopRightBottom (
            std::min (boxSelectStart.x, e.getPosition().x),
            std::min (boxSelectStart.y, e.getPosition().y),
            std::max (boxSelectStart.x, e.getPosition().x),
            std::max (boxSelectStart.y, e.getPosition().y)
        );

        for (auto* comp : nodeComponents)
        {
            if (boxSelectRect.intersects (comp->getBounds()))
                selectedNodeIDs.insert (comp->getNodeID());
            else if (! e.mods.isShiftDown())
                selectedNodeIDs.erase (comp->getNodeID());
        }

        selectedNodeID = selectedNodeIDs.empty() ? juce::AudioProcessorGraph::NodeID() : *selectedNodeIDs.rbegin();

        for (auto* c : nodeComponents)
            c->repaint();

        if (onNodeSelected)
            onNodeSelected (selectedNodeID);

        repaint();
    }
}

void NodeCanvas::mouseUp(const juce::MouseEvent &e)
{
    isPanning = false;
    isDraggingCableRoute = false;
    activeCableDragId = "";

    if (isRightClickCandidate || e.mods.isPopupMenu())
    {
        bool wasRightClick = isRightClickCandidate;
        isRightClickCandidate = false;

        if (wasRightClick && e.getDistanceFromDragStart() < 6)
        {
            juce::AudioProcessorGraph::Connection hitConn;
            juce::String hitCableId;
            if (hitTestCable (e.getPosition().toFloat(), hitConn, hitCableId))
            {
                showCableContextMenu (hitConn, hitCableId, e.getPosition());
                return;
            }
            showAddModuleMenu (e.getPosition());
            return;
        }
    }

    if (isBoxSelecting)
    {
        isBoxSelecting = false;
        repaint();
    }
}

void NodeCanvas::mouseWheelMove(const juce::MouseEvent &e, const juce::MouseWheelDetails &wheel)
{
    auto local = e.getPosition().toFloat(); // already in canvas-local space (pre-zoom-change)

    float factor = wheel.deltaY > 0 ? 1.1f : (1.0f / 1.1f);
    float newZoom = juce::jlimit(0.25f, 3.0f, zoom * factor);
    if (newZoom == zoom)
        return;

    pan += local * (zoom - newZoom);
    zoom = newZoom;
    applyTransform();
    repaint();
}

bool NodeCanvas::keyPressed (const juce::KeyPress &key)
{
    // Ctrl+D / Cmd+D: Duplicate Selected Modules
    if (key == juce::KeyPress ('d', juce::ModifierKeys::commandModifier, 0) ||
        key == juce::KeyPress ('D', juce::ModifierKeys::commandModifier, 0))
    {
        duplicateSelectedNodes();
        return true;
    }

    if (key.getKeyCode() == juce::KeyPress::deleteKey || key.getKeyCode() == juce::KeyPress::backspaceKey)
    {
        bool shift = key.getModifiers().isShiftDown();
        auto toDelete = selectedNodeIDs;
        if (toDelete.empty() && selectedNodeID != juce::AudioProcessorGraph::NodeID())
            toDelete.insert (selectedNodeID);

        bool didDelete = false;
        for (auto id : toDelete)
        {
            if (id != processor.getAudioInputNodeID() && id != processor.getAudioOutputNodeID())
            {
                if (shift)
                    processor.removeModuleAndReconnect (id);
                else
                    requestDeleteNode (id);

                didDelete = true;
            }
        }

        if (didDelete)
        {
            clearSelection();
            return true;
        }
    }
    return false;
}

// -------------------------------------------------------------------------
// Add Module menu, grouped by category, plus view actions.
// -------------------------------------------------------------------------
void NodeCanvas::showAddModuleMenu(juce::Point<int> position)
{
    auto &factory = ModuleFactory::instance();

    juce::PopupMenu root;
    juce::PopupMenu addModule;

    int itemId = 1;
    std::vector<juce::String> idToType;
    idToType.push_back({}); // index 0 unused (PopupMenu item ids start at 1)

    for (auto &category : factory.getCategories())
    {
        juce::PopupMenu sub;
        for (auto &type : factory.getTypesInCategory(category))
        {
            sub.addItem(itemId, tr (type));
            idToType.push_back(type);
            ++itemId;
        }

        juce::String catDisplay = category;
        if (category.equalsIgnoreCase ("Utility")) catDisplay = tr ("CAT_UTILITY");
        else if (category.equalsIgnoreCase ("Dynamics")) catDisplay = tr ("CAT_DYNAMICS");
        else if (category.equalsIgnoreCase ("Frequency")) catDisplay = tr ("CAT_FREQUENCY");
        else if (category.equalsIgnoreCase ("Cleanup")) catDisplay = tr ("CAT_CLEANUP");
        else if (category.equalsIgnoreCase ("Vocal Tone") || category.equalsIgnoreCase ("Radio Tone")) catDisplay = tr ("CAT_VOCAL_TONE");

        addModule.addSubMenu(catDisplay, sub);
    }

    root.addSubMenu(tr ("ADD_MODULE"), addModule);
    root.addSeparator();

    if (selectedNodeIDs.size() > 1)
    {
        bool isTr = LocalizationManager::instance().getLanguage() == Language::Turkish;
        root.addItem (30, isTr ? juce::String (juce::CharPointer_UTF8 ("Secili Modulleri Paketle (Container)"))
                               : juce::String ("Pack Selected Modules into Container"));
        root.addSeparator();
    }

    root.addItem(9001, tr ("RESET_VIEW"));
    root.addItem(9002, tr ("ZOOM_IN"));
    root.addItem(9003, tr ("ZOOM_OUT"));
    root.addSeparator();
    root.addItem(9004, tr ("ADD_STICKY_NOTE"));

    root.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [this, position, idToType] (int result)
    {
        if (result <= 0)
            return;
        if (result == 30)   { packSelectedNodesIntoContainer(); return; }
        if (result == 9001) { resetView(); return; }
        if (result == 9002) { zoomIn(); return; }
        if (result == 9003) { zoomOut(); return; }
        if (result == 9004) {
            auto* note = stickyNotes.add(new StickyNote(*this));
            addAndMakeVisible(note);
            note->setBounds(position.x, position.y, 80, 40);
            note->toFront(true);
            note->grabKeyboardFocus();
            repaint();
            return;
        }

        if (result >= 1 && result < (int) idToType.size())
        {
            auto newNode = processor.addModule (idToType[(size_t) result], position);
            if (newNode != nullptr)
                insertNodeOnCableIfIntersected (newNode->nodeID, position);
        }
    });
}

void NodeCanvas::showNodeContextMenu(NodeComponent &node)
{
    if (node.isSystem())
        return;

    juce::PopupMenu menu;
    auto *graphNode = getGraphNode(node.getNodeID());
    bool isByp = graphNode ? graphNode->isBypassed() : false;

    if (selectedNodeIDs.size() > 1)
    {
        bool isTr = LocalizationManager::instance().getLanguage() == Language::Turkish;
        menu.addItem (30, isTr ? juce::String (juce::CharPointer_UTF8 ("Secili Modulleri Paketle (Container)"))
                               : juce::String ("Pack Selected Modules into Container"));
        menu.addSeparator();
    }

    bool isContainer = false;
    if (graphNode && graphNode->getProcessor())
    {
        isContainer = dynamic_cast<ContainerModule*> (MixWrapperProcessor::getActualProcessor (graphNode->getProcessor())) != nullptr;
    }

    if (isContainer)
    {
        bool isTr = LocalizationManager::instance().getLanguage() == Language::Turkish;
        menu.addItem (31, isTr ? juce::String (juce::CharPointer_UTF8 ("Container'\xc4\xb1 \xc3\x87\xc3\xb6z (Unpack All to Canvas)"))
                               : "Unpack All to Canvas");
        menu.addSeparator();
    }

    menu.addItem(1, isByp ? tr ("ENABLE_MODULE") : tr ("BYPASS_MODULE"));
    menu.addItem(20, tr ("DUPLICATE_MODULE"));
    bool hasUI = graphNode && graphNode->getProcessor() && graphNode->getProcessor()->hasEditor();
    if (hasUI) {
        menu.addItem(4, tr ("OPEN_UI"));
        menu.addSeparator();
    }
    juce::PopupMenu colorMenu;
    colorMenu.addItem(10, tr ("COLOR_GREY"));
    colorMenu.addItem(11, tr ("COLOR_RED"));
    colorMenu.addItem(12, tr ("COLOR_BLUE"));
    colorMenu.addItem(13, tr ("COLOR_YELLOW"));
    colorMenu.addItem(14, tr ("COLOR_GREEN"));
    colorMenu.addItem(15, tr ("COLOR_PURPLE"));
    colorMenu.addItem(16, tr ("COLOR_ORANGE"));
    menu.addSubMenu(tr ("SET_COLOR"), colorMenu);

    menu.addSeparator();
    menu.addItem(5, tr ("DISSOLVE_RECONNECT"));
    menu.addItem(3, tr ("REMOVE_NODE"));

    auto id = node.getNodeID();
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&node).withMousePosition(), [this, id] (int result)
    {
        auto *gNode = getGraphNode(id);
        if (!gNode)
            return;

        if (result == 30)
            packSelectedNodesIntoContainer();
        else if (result == 31)
            unpackContainer (id);
        else if (result == 1)
            gNode->setBypassed(!gNode->isBypassed());
        else if (result == 20)
        {
            setSelectedNodeID (id);
            duplicateSelectedNodes();
        }
        else if (result == 3)
            requestDeleteNode (id);
        else if (result == 5)
            processor.removeModuleAndReconnect(id);
        else if (result == 4)
        {
            if (auto* comp = findComponentFor(id))
                comp->openEditorWindow();
        }
        else if (result == 10)
            gNode->properties.set("color", "grey");
        else if (result == 11)
            gNode->properties.set("color", "red");
        else if (result == 12)
            gNode->properties.set("color", "blue");
        else if (result == 13)
            gNode->properties.set("color", "yellow");
        else if (result == 14)
            gNode->properties.set("color", "green");
        else if (result == 15)
            gNode->properties.set("color", "purple");
        else if (result == 16)
            gNode->properties.set("color", "orange");

        repaint(); // Tıklama sonrası arayüzü güncelliyoruz
    });
}

// -------------------------------------------------------------------------
void NodeCanvas::changeListenerCallback(juce::ChangeBroadcaster *)
{
    rebuildFromGraph();
}

void NodeCanvas::rebuildFromGraph()
{
    nodeComponents.clear();

    for (auto *node : processor.getGraph().getNodes())
    {
        const bool isInput = node->nodeID == processor.getAudioInputNodeID();
        const bool isOutput = node->nodeID == processor.getAudioOutputNodeID();

        juce::String label = isInput ? "Audio In" : isOutput ? "Audio Out"
                                                             : node->properties["type"].toString();

        auto *comp = nodeComponents.add(new NodeComponent(*this, node->nodeID, label,
                                                          /*hasInputPin*/ !isInput,
                                                          /*hasOutputPin*/ !isOutput,
                                                          /*isSystemNode*/ isInput || isOutput));
        addAndMakeVisible(comp);

        int x = node->properties.contains("x") ? (int)node->properties["x"] : 400;
        int y = node->properties.contains("y") ? (int)node->properties["y"] : 400;
        comp->setTopLeftPosition(x, y);
    }

    repaint();
}

NodeComponent *NodeCanvas::findComponentFor(juce::AudioProcessorGraph::NodeID id) const
{
    for (auto *c : nodeComponents)
        if (c->getNodeID() == id)
            return c;
    return nullptr;
}

void NodeCanvas::beginCableDrag(NodeComponent &fromNode, bool fromIsInput, int channel)
{
    dragSourceNode = &fromNode;
    dragSourceIsInput = fromIsInput;
    dragSourceChannel = channel;
    dragCurrentPos = fromNode.getPinPosition(fromIsInput, channel).toInt();
}

void NodeCanvas::updateCableDrag(juce::Point<int> currentPos)
{
    dragCurrentPos = currentPos;
    repaint(); // Repainting the viewport eliminates all cable visual tearing
}

bool NodeCanvas::startReplugCable(NodeComponent &fromComp, bool isInput, int channel)
{
    auto compID = fromComp.getNodeID();
    const auto& conns = processor.getGraph().getConnections();

    for (auto& c : conns)
    {
        if (isInput && c.destination.nodeID == compID && (channel < 0 || c.destination.channelIndex == channel || c.destination.channelIndex == channel + 1))
        {
            // Unplug from this Input: the other attached end is an Output
            auto* srcComp = findComponentFor (c.source.nodeID);
            if (srcComp != nullptr)
            {
                isReplugging = true;
                replugOldSrcID = c.source.nodeID;
                replugOldDstID = c.destination.nodeID;

                dragSourceNode = srcComp;
                dragSourceIsInput = false; // The attached remaining end is OUTPUT
                dragSourceChannel = c.source.channelIndex;
                dragCurrentPos = fromComp.getPinPosition(isInput, channel).toInt();
                repaint();
                return true;
            }
        }
        else if (!isInput && c.source.nodeID == compID && (channel < 0 || c.source.channelIndex == channel || c.source.channelIndex == channel + 1))
        {
            // Unplug from this Output: the other attached end is an Input
            auto* dstComp = findComponentFor (c.destination.nodeID);
            if (dstComp != nullptr)
            {
                isReplugging = true;
                replugOldSrcID = c.source.nodeID;
                replugOldDstID = c.destination.nodeID;

                dragSourceNode = dstComp;
                dragSourceIsInput = true; // The attached remaining end is INPUT
                dragSourceChannel = c.destination.channelIndex;
                dragCurrentPos = fromComp.getPinPosition(isInput, channel).toInt();
                repaint();
                return true;
            }
        }
    }
    return false;
}

void NodeCanvas::endCableDrag (juce::Point<int> dropPos)
{
    if (dragSourceNode == nullptr) return;

    bool connectedNew = false;
    float hitDist = (float) NodeComponent::pinHitRadius + 8.0f;

    for (auto* target : nodeComponents)
    {
        if (target == dragSourceNode) continue;

        bool targetIsInput = ! dragSourceIsInput;
        int numPins = target->getNumPins (targetIsInput);

        for (int p = 0; p < numPins; ++p)
        {
            int targetChan = target->pinIndexToChannel (targetIsInput, p);
            auto pinPos = target->getPinPosition (targetIsInput, targetChan);
            if (pinPos.getDistanceFrom (dropPos.toFloat()) < hitDist)
            {
                auto srcID = dragSourceIsInput ? target->getNodeID() : dragSourceNode->getNodeID();
                int srcCh  = dragSourceIsInput ? targetChan : dragSourceChannel;

                auto dstID = dragSourceIsInput ? dragSourceNode->getNodeID() : target->getNodeID();
                int dstCh  = dragSourceIsInput ? dragSourceChannel : targetChan;

                // Disconnect old cable if this was a replug operation
                if (isReplugging)
                {
                    juce::Array<juce::AudioProcessorGraph::Connection> toRemove;
                    for (auto& c : processor.getGraph().getConnections())
                    {
                        if (c.source.nodeID == replugOldSrcID && c.destination.nodeID == replugOldDstID)
                            toRemove.add (c);
                    }
                    for (auto& c : toRemove)
                        processor.disconnect (c);
                }

                // Clear any existing connection on dstID's input channel pair so there is never duplicate clashing cables
                disconnectPin (dstID, true, dstCh);

                processor.connect (srcID, srcCh, dstID, dstCh);
                processor.connect (srcID, srcCh + 1, dstID, dstCh + 1);
                connectedNew = true;
                break;
            }
        }

        if (connectedNew)
            break;
    }

    // If user was replugging and dropped the cable in empty space, remove the old connection!
    if (isReplugging && ! connectedNew)
    {
        juce::Array<juce::AudioProcessorGraph::Connection> toRemove;
        for (auto& c : processor.getGraph().getConnections())
        {
            if (c.source.nodeID == replugOldSrcID && c.destination.nodeID == replugOldDstID)
                toRemove.add (c);
        }
        for (auto& c : toRemove)
            processor.disconnect (c);
    }

    isReplugging = false;
    dragSourceNode = nullptr;
    repaint();
}

void NodeCanvas::insertNodeOnCableIfIntersected (juce::AudioProcessorGraph::NodeID newNodeID, juce::Point<int> nodePos)
{
    auto* newGraphNode = getGraphNode (newNodeID);
    if (! newGraphNode) return;
    auto* newProc = newGraphNode->getProcessor();
    if (! newProc || newProc->getTotalNumInputChannels() == 0 || newProc->getTotalNumOutputChannels() == 0)
        return;

    auto nodeBounds = juce::Rectangle<float> ((float) nodePos.x, (float) nodePos.y, 160.0f, 90.0f);

    juce::AudioProcessorGraph::Connection targetConn;
    bool found = false;

    // Check all existing connections in the graph
    for (auto& c : processor.getGraph().getConnections())
    {
        if (c.source.nodeID == newNodeID || c.destination.nodeID == newNodeID)
            continue;

        auto* srcComp = findComponentFor (c.source.nodeID);
        auto* dstComp = findComponentFor (c.destination.nodeID);
        if (! srcComp || ! dstComp) continue;

        auto start = srcComp->getPinPosition (false, c.source.channelIndex);
        auto end   = dstComp->getPinPosition (true, c.destination.channelIndex);
        juce::String cableId = juce::String (c.source.nodeID.uid) + "_" + juce::String (c.destination.nodeID.uid);

        juce::Path p = makeCablePath (start, end, cableOffsets.count (cableId) ? cableOffsets.at (cableId) : juce::Point<float>{});

        // Sample points along the cable curve
        juce::PathFlatteningIterator it (p, juce::AffineTransform(), 8.0f);
        while (it.next())
        {
            if (nodeBounds.contains (it.x2, it.y2))
            {
                targetConn = c;
                found = true;
                break;
            }
        }

        if (found)
            break;
    }

    if (found)
    {
        auto srcID = targetConn.source.nodeID;
        auto dstID = targetConn.destination.nodeID;

        // Disconnect existing cable(s) between src and dst
        juce::Array<juce::AudioProcessorGraph::Connection> toRemove;
        for (auto& c : processor.getGraph().getConnections())
        {
            if (c.source.nodeID == srcID && c.destination.nodeID == dstID)
                toRemove.add (c);
        }
        for (auto& c : toRemove)
            processor.disconnect (c);

        // Splice the new node directly into the chain!
        processor.connect (srcID, 0, newNodeID, 0);
        processor.connect (srcID, 1, newNodeID, 1);
        processor.connect (newNodeID, 0, dstID, 0);
        processor.connect (newNodeID, 1, dstID, 1);

        repaint();
    }
}

void NodeCanvas::notifyNodeMoved(NodeComponent &node, juce::Rectangle<int> /*oldBounds*/)
{
    for (auto *n : processor.getGraph().getNodes())
    {
        if (n->nodeID == node.getNodeID())
        {
            n->properties.set("x", node.getX());
            n->properties.set("y", node.getY());
            break;
        }
    }

    // Pinli notları node ile beraber çek!
    for (auto *note : stickyNotes)
    {
        if (note->pinnedNodeID == node.getNodeID())
        {
            note->setTopLeftPosition(node.getPosition() + note->pinOffset);
        }
    }

    repaint();
}

juce::Rectangle<int> NodeCanvas::boundsForCable(juce::Point<float> a, juce::Point<float> b) const
{
    return juce::Rectangle<float>(a, b).expanded(30.0f).getSmallestIntegerContainer();
}

juce::Path NodeCanvas::makeCablePath (juce::Point<float> start, juce::Point<float> end, juce::Point<float> offset)
{
    juce::Path p;
    p.startNewSubPath (start);

    // Bulging the control points vertically (old behaviour) produced tight, self-crossing
    // loops whenever "end" was above "start" — the curve would dip far down before
    // snapping back up, which at low zoom anti-aliases into a broken/dashed-looking line.
    // Pulling the control points *away* from each node instead (patch-cable style) stays
    // well-behaved for cables running in any direction.
    float pull = juce::jlimit (40.0f, 220.0f, std::abs (end.y - start.y) * 0.6f + 30.0f);

    p.cubicTo (start.x + offset.x,        start.y + pull + offset.y,
               end.x   + offset.x,        end.y   - pull + offset.y,
               end.x, end.y);
    return p;
}

void NodeCanvas::disconnectPin(juce::AudioProcessorGraph::NodeID nodeID, bool isInput, int channel)
{
    auto connections = processor.getGraph().getConnections(); // Döngüde silme yapacağımız için listeyi kopyalıyoruz
    for (auto &c : connections)
    {
        if (isInput && c.destination.nodeID == nodeID)
        {
            if (channel < 0 || c.destination.channelIndex == channel || c.destination.channelIndex == channel + 1)
                processor.disconnect(c);
        }
        else if (!isInput && c.source.nodeID == nodeID)
        {
            if (channel < 0 || c.source.channelIndex == channel || c.source.channelIndex == channel + 1)
                processor.disconnect(c);
        }
    }
}

void NodeCanvas::paintOverChildren (juce::Graphics& g)
{
    auto clip = g.getClipBounds().toFloat();

    for (auto& c : processor.getGraph().getConnections())
    {
        // Hide the old static cable while dragging it to a new location
        if (isReplugging && c.source.nodeID == replugOldSrcID && c.destination.nodeID == replugOldDstID)
            continue;

        // For standard stereo pairs, draw only once per pair (on even channel index) if matching pair exists
        if ((c.source.channelIndex % 2 == 1) && (c.destination.channelIndex % 2 == 1))
        {
            bool hasEven = false;
            for (auto& other : processor.getGraph().getConnections())
            {
                if (other.source.nodeID == c.source.nodeID && other.destination.nodeID == c.destination.nodeID &&
                    other.source.channelIndex == c.source.channelIndex - 1 && other.destination.channelIndex == c.destination.channelIndex - 1)
                {
                    hasEven = true;
                    break;
                }
            }
            if (hasEven) continue;
        }

        auto* srcComp = findComponentFor (c.source.nodeID);
        auto* dstComp = findComponentFor (c.destination.nodeID);
        if (srcComp == nullptr || dstComp == nullptr) continue;

        auto start = srcComp->getPinPosition (false, c.source.channelIndex);
        auto end   = dstComp->getPinPosition (true, c.destination.channelIndex);

        // Skip cables fully outside the visible clip region
        if (! clip.expanded (60.0f).intersects (juce::Rectangle<float> (start, end).expanded (60.0f)))
            continue;

        auto* srcNode = getGraphNode (c.source.nodeID);
        auto* dstNode = getGraphNode (c.destination.nodeID);
        bool bypassed = (srcNode && srcNode->isBypassed()) || (dstNode && dstNode->isBypassed());

        auto getColor = [] (const juce::String& col) -> juce::Colour
        {
            if (col == "red")         return juce::Colour (0xffff453a);
            if (col == "blue")        return juce::Colour (0xff0a84ff);
            if (col == "yellow")      return juce::Colour (0xffffd60a);
            if (col == "green")       return juce::Colour (0xff30d158);
            if (col == "purple")      return juce::Colour (0xffbf5af2);
            if (col == "orange")      return juce::Colour (0xffff9f0a);
            return juce::Colour (0xff0a84ff);
        };

        juce::String srcColor = srcNode ? srcNode->properties["color"].toString() : "grey";
        juce::String dstColor = dstNode ? dstNode->properties["color"].toString() : "grey";

        bool hasSrcCustom = srcColor != "grey" && srcColor.isNotEmpty();
        bool hasDstCustom = dstColor != "grey" && dstColor.isNotEmpty();

        juce::Colour cableBase = juce::Colour (0xff0a84ff);
        bool useGradient = false;
        juce::Colour srcBase = cableBase;
        juce::Colour dstBase = cableBase;

        // Band-specific coloring for Splitter / Joiner
        bool isSplitter = srcComp->isSplitterNode();
        bool isJoiner = dstComp->isJoinerNode();

        if (isSplitter)
        {
            if (c.source.channelIndex == 0 || c.source.channelIndex == 1)
                cableBase = UITheme::applePurple;
            else if (c.source.channelIndex == 2 || c.source.channelIndex == 3)
                cableBase = UITheme::appleGreen;
            else if (c.source.channelIndex == 4 || c.source.channelIndex == 5)
                cableBase = UITheme::appleCyan;
        }
        else if (isJoiner)
        {
            if (c.destination.channelIndex == 0 || c.destination.channelIndex == 1)
                cableBase = UITheme::applePurple;
            else if (c.destination.channelIndex == 2 || c.destination.channelIndex == 3)
                cableBase = UITheme::appleGreen;
            else if (c.destination.channelIndex == 4 || c.destination.channelIndex == 5)
                cableBase = UITheme::appleCyan;
        }
        else if (hasSrcCustom && hasDstCustom && srcColor != dstColor)
        {
            useGradient = true;
            srcBase = getColor (srcColor);
            dstBase = getColor (dstColor);
        }
        else if (hasSrcCustom)
        {
            cableBase = getColor (srcColor);
        }
        else if (hasDstCustom)
        {
            cableBase = getColor (dstColor);
        }

        juce::String cableId = juce::String(c.source.nodeID.uid) + "_" + juce::String(c.source.channelIndex) + "_" + juce::String(c.destination.nodeID.uid) + "_" + juce::String(c.destination.channelIndex);
        auto path = makeCablePath (start, end, cableOffsets[cableId]);

        // 1. Soft 3D Drop Shadow
        g.setColour (juce::Colours::black.withAlpha (0.50f));
        g.strokePath (path, juce::PathStrokeType (5.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      juce::AffineTransform::translation (0.0f, 2.5f));

        // 2. Colored Glow matching node / band
        if (showCableGlow)
        {
            if (useGradient)
            {
                juce::ColourGradient grad (srcBase.withAlpha (bypassed ? 0.05f : 0.28f), start.x, start.y,
                                           dstBase.withAlpha (bypassed ? 0.05f : 0.28f), end.x, end.y, false);
                g.setGradientFill (grad);
            }
            else
            {
                g.setColour (cableBase.withAlpha (bypassed ? 0.05f : 0.28f));
            }
            g.strokePath (path, juce::PathStrokeType (6.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // 3. Solid Cable Body (3.6px)
        if (bypassed)
        {
            g.setColour (juce::Colour (0xff475569));
        }
        else if (useGradient)
        {
            juce::ColourGradient grad (srcBase, start.x, start.y, dstBase, end.x, end.y, false);
            g.setGradientFill (grad);
        }
        else
        {
            g.setColour (cableBase);
        }
        g.strokePath (path, juce::PathStrokeType (3.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // 4. Tiny White Specular Spine (Refined light streak)
        juce::Colour spineColor = juce::Colours::white.withAlpha (bypassed ? 0.40f : 0.85f);
        g.setColour (spineColor);
        g.strokePath (path, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Temporary Drag Cable (inherits source band/node color)
    if (dragSourceNode != nullptr)
    {
        auto* dNode = getGraphNode (dragSourceNode->getNodeID());
        juce::String dColor = dNode ? dNode->properties["color"].toString() : "grey";
        juce::Colour dragBase = juce::Colour (0xff0a84ff);

        if (dragSourceNode->isSplitterNode() && !dragSourceIsInput)
        {
            if (dragSourceChannel == 0 || dragSourceChannel == 1) dragBase = UITheme::applePurple;
            else if (dragSourceChannel == 2 || dragSourceChannel == 3) dragBase = UITheme::appleGreen;
            else if (dragSourceChannel == 4 || dragSourceChannel == 5) dragBase = UITheme::appleCyan;
        }
        else if (dragSourceNode->isJoinerNode() && dragSourceIsInput)
        {
            if (dragSourceChannel == 0 || dragSourceChannel == 1) dragBase = UITheme::applePurple;
            else if (dragSourceChannel == 2 || dragSourceChannel == 3) dragBase = UITheme::appleGreen;
            else if (dragSourceChannel == 4 || dragSourceChannel == 5) dragBase = UITheme::appleCyan;
        }
        else if (dColor == "red")         dragBase = juce::Colour (0xffff453a);
        else if (dColor == "blue")   dragBase = juce::Colour (0xff0a84ff);
        else if (dColor == "yellow") dragBase = juce::Colour (0xffffd60a);
        else if (dColor == "green")  dragBase = juce::Colour (0xff30d158);
        else if (dColor == "purple") dragBase = juce::Colour (0xffbf5af2);
        else if (dColor == "orange") dragBase = juce::Colour (0xffff9f0a);

        auto start = dragSourceNode->getPinPosition (dragSourceIsInput, dragSourceChannel);
        auto path = makeCablePath (start, dragCurrentPos.toFloat());

        g.setColour (juce::Colours::black.withAlpha (0.40f));
        g.strokePath (path, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      juce::AffineTransform::translation (0.0f, 2.0f));

        if (showCableGlow)
        {
            g.setColour (dragBase.withAlpha (0.28f));
            g.strokePath (path, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        g.setColour (dragBase);
        g.strokePath (path, juce::PathStrokeType (3.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (juce::Colours::white.withAlpha (0.80f));
        g.strokePath (path, juce::PathStrokeType (1.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Rubber-Band Box Selection Rectangle
    if (isBoxSelecting && ! boxSelectRect.isEmpty())
    {
        g.setColour (UITheme::appleBlue.withAlpha (0.15f));
        g.fillRect (boxSelectRect.toFloat());
        g.setColour (UITheme::appleBlue.withAlpha (0.85f));
        g.drawRect (boxSelectRect.toFloat(), 1.2f);
    }
}

bool NodeCanvas::isPinConnected (juce::AudioProcessorGraph::NodeID id, bool isInput, int channel) const
{
    for (auto& c : processor.getGraph().getConnections()) {
        if (isInput && c.destination.nodeID == id)
        {
            if (channel < 0 || c.destination.channelIndex == channel || c.destination.channelIndex == channel + 1)
                return true;
        }
        if (!isInput && c.source.nodeID == id)
        {
            if (channel < 0 || c.source.channelIndex == channel || c.source.channelIndex == channel + 1)
                return true;
        }
    }
    return false;
}

void NodeCanvas::addModuleAtViewCenter(const juce::String& typeId)
{
    auto parentRect = getParentComponent() != nullptr ? getParentComponent()->getLocalBounds() : getLocalBounds();
    juce::Point<float> screenCenter ((float) parentRect.getWidth() * 0.5f, (float) parentRect.getHeight() * 0.5f);
    juce::Point<int> canvasPos = ((screenCenter - pan) / zoom).toInt() - juce::Point<int>(60, 35);

    if (snapToGrid)
    {
        canvasPos.x = (canvasPos.x / snapGridSize) * snapGridSize;
        canvasPos.y = (canvasPos.y / snapGridSize) * snapGridSize;
    }

    auto newNode = processor.addModule (typeId, canvasPos);
    if (newNode != nullptr)
        insertNodeOnCableIfIntersected (newNode->nodeID, canvasPos);
}

bool NodeCanvas::isInterestedInDragSource (const juce::DragAndDropTarget::SourceDetails& details)
{
    return details.description.toString().startsWith ("module:");
}

void NodeCanvas::dropModule (const juce::String& type, juce::Point<int> dropPos)
{
    dropPos -= juce::Point<int> (60, 35);
    
    if (snapToGrid)
    {
        dropPos.x = (dropPos.x / snapGridSize) * snapGridSize;
        dropPos.y = (dropPos.y / snapGridSize) * snapGridSize;
    }
    
    dropPos.x = juce::jlimit (50, 3850, dropPos.x);
    dropPos.y = juce::jlimit (50, 3850, dropPos.y);
    
    auto newNode = processor.addModule (type, dropPos);
    if (newNode != nullptr)
    {
        setSelectedNodeID (newNode->nodeID);
        insertNodeOnCableIfIntersected (newNode->nodeID, dropPos);
    }
}

void NodeCanvas::itemDropped (const juce::DragAndDropTarget::SourceDetails& details)
{
    juce::String type = details.description.toString().substring (7); 
    dropModule (type, details.localPosition);
}

void NodeCanvas::setSelectedNodeID (juce::AudioProcessorGraph::NodeID id)
{
    addSelectedNode (id, true);
}

bool NodeCanvas::isNodeSelected (juce::AudioProcessorGraph::NodeID id) const
{
    return selectedNodeIDs.find (id) != selectedNodeIDs.end();
}

void NodeCanvas::addSelectedNode (juce::AudioProcessorGraph::NodeID id, bool deselectOthers)
{
    if (deselectOthers)
        selectedNodeIDs.clear();

    if (id != juce::AudioProcessorGraph::NodeID())
        selectedNodeIDs.insert (id);

    selectedNodeID = id;

    for (auto* c : nodeComponents)
        c->repaint();

    if (onNodeSelected)
        onNodeSelected (selectedNodeID);
}

void NodeCanvas::toggleNodeSelection (juce::AudioProcessorGraph::NodeID id)
{
    if (id == juce::AudioProcessorGraph::NodeID()) return;

    if (selectedNodeIDs.find (id) != selectedNodeIDs.end())
    {
        selectedNodeIDs.erase (id);
        selectedNodeID = selectedNodeIDs.empty() ? juce::AudioProcessorGraph::NodeID() : *selectedNodeIDs.rbegin();
    }
    else
    {
        selectedNodeIDs.insert (id);
        selectedNodeID = id;
    }

    for (auto* c : nodeComponents)
        c->repaint();

    if (onNodeSelected)
        onNodeSelected (selectedNodeID);
}

void NodeCanvas::clearSelection()
{
    selectedNodeIDs.clear();
    selectedNodeID = {};

    for (auto* c : nodeComponents)
        c->repaint();

    if (onNodeSelected)
        onNodeSelected ({});
}

juce::Rectangle<float> NodeCanvas::getVisibleCanvasArea() const
{
    auto parent = getParentComponent();
    auto viewBounds = parent != nullptr ? parent->getLocalBounds().toFloat() : getLocalBounds().toFloat();
    float vx = (-pan.x) / zoom;
    float vy = (-pan.y) / zoom;
    float vw = viewBounds.getWidth() / zoom;
    float vh = viewBounds.getHeight() / zoom;
    return { vx, vy, vw, vh };
}

void NodeCanvas::panToLogicalCenter (juce::Point<float> logicalCenter)
{
    auto parent = getParentComponent();
    auto viewBounds = parent != nullptr ? parent->getLocalBounds().toFloat() : getLocalBounds().toFloat();
    float targetPanX = (viewBounds.getWidth() * 0.5f) - (logicalCenter.x * zoom);
    float targetPanY = (viewBounds.getHeight() * 0.5f) - (logicalCenter.y * zoom);

    pan = { targetPanX, targetPanY };
    applyTransform();
    repaint();
}

void NodeCanvas::openModuleEditor (juce::AudioProcessorGraph::NodeID id)
{
    if (auto* comp = findComponentFor (id))
        comp->openEditorWindow();
}

void NodeCanvas::packSelectedNodesIntoContainer()
{
    std::vector<juce::AudioProcessorGraph::NodeID> targetIDs;
    for (auto id : selectedNodeIDs)
    {
        if (id != processor.getAudioInputNodeID() && id != processor.getAudioOutputNodeID())
            targetIDs.push_back (id);
    }
    if (targetIDs.empty() && selectedNodeID != juce::AudioProcessorGraph::NodeID()
        && selectedNodeID != processor.getAudioInputNodeID() && selectedNodeID != processor.getAudioOutputNodeID())
    {
        targetIDs.push_back (selectedNodeID);
    }

    if (targetIDs.empty())
        return;

    // Sort selected nodes from left to right according to their canvas X coordinate
    std::sort (targetIDs.begin(), targetIDs.end(), [this] (auto a, auto b) {
        auto* nA = getGraphNode (a);
        auto* nB = getGraphNode (b);
        int xA = nA && nA->properties.contains ("x") ? (int) nA->properties["x"] : 0;
        int xB = nB && nB->properties.contains ("x") ? (int) nB->properties["x"] : 0;
        return xA < xB;
    });

    // Calculate center point
    int minX = 4000, maxX = 0, minY = 4000, maxY = 0;
    for (auto id : targetIDs)
    {
        if (auto* n = getGraphNode (id))
        {
            int nx = n->properties.contains ("x") ? (int) n->properties["x"] : 400;
            int ny = n->properties.contains ("y") ? (int) n->properties["y"] : 400;
            minX = juce::jmin (minX, nx);
            maxX = juce::jmax (maxX, nx);
            minY = juce::jmin (minY, ny);
            maxY = juce::jmax (maxY, ny);
        }
    }
    int centerX = (minX + maxX) / 2;
    int centerY = (minY + maxY) / 2;

    // Find external input connections (coming from nodes outside selection)
    std::set<juce::AudioProcessorGraph::NodeID> selSet (targetIDs.begin(), targetIDs.end());
    std::vector<juce::AudioProcessorGraph::Connection> externalInputs;
    std::vector<juce::AudioProcessorGraph::Connection> externalOutputs;

    for (const auto& c : processor.getGraph().getConnections())
    {
        bool srcInSel = selSet.find (c.source.nodeID) != selSet.end();
        bool dstInSel = selSet.find (c.destination.nodeID) != selSet.end();

        if (! srcInSel && dstInSel)
            externalInputs.push_back (c);
        else if (srcInSel && ! dstInSel)
            externalOutputs.push_back (c);
    }

    // Create the Container Module
    auto newContainerNode = processor.addModule ("Container", juce::Point<int> (centerX, centerY));
    if (newContainerNode == nullptr)
        return;

    auto* contProc = dynamic_cast<ContainerModule*> (MixWrapperProcessor::getActualProcessor (newContainerNode->getProcessor()));
    if (contProc != nullptr)
    {
        // Adopt all selected processors in order into the container
        for (auto id : targetIDs)
        {
            if (auto* gNode = getGraphNode (id))
            {
                juce::String modType = gNode->properties["type"].toString();
                bool isByp = gNode->isBypassed();
                auto* actualProc = MixWrapperProcessor::getActualProcessor (gNode->getProcessor());

                // Clone state into container's inner module
                contProc->addInnerModule (modType);
                int lastIdx = contProc->getNumInnerModules() - 1;
                if (lastIdx >= 0)
                {
                    auto* inner = contProc->getInnerModule (lastIdx);
                    if (inner != nullptr)
                    {
                        inner->isBypassed = isByp;
                        if (actualProc != nullptr && inner->processor != nullptr)
                        {
                            juce::MemoryBlock stateData;
                            actualProc->getStateInformation (stateData);
                            if (stateData.getSize() > 0)
                                inner->processor->setStateInformation (stateData.getData(), (int) stateData.getSize());
                        }
                    }
                }
            }
        }
    }

    // Connect external inputs into new container
    for (const auto& c : externalInputs)
    {
        processor.connect (c.source.nodeID, c.source.channelIndex, newContainerNode->nodeID, c.destination.channelIndex);
    }

    // Connect external outputs from new container
    for (const auto& c : externalOutputs)
    {
        processor.connect (newContainerNode->nodeID, c.source.channelIndex, c.destination.nodeID, c.destination.channelIndex);
    }

    // Remove old selected nodes
    for (auto id : targetIDs)
        processor.removeModule (id);

    setSelectedNodeID (newContainerNode->nodeID);
    repaint();
}

void NodeCanvas::setContainerDropTarget (juce::AudioProcessorGraph::NodeID containerID, juce::AudioProcessorGraph::NodeID draggedID)
{
    if (containerDropTargetID != containerID || draggedOverNodeID != draggedID)
    {
        containerDropTargetID = containerID;
        draggedOverNodeID = draggedID;
        repaint();
    }
}

bool NodeCanvas::adoptNodeIntoContainer (juce::AudioProcessorGraph::NodeID sourceNodeID, juce::AudioProcessorGraph::NodeID targetContainerID)
{
    if (sourceNodeID == targetContainerID)
        return false;

    auto* srcGNode = getGraphNode (sourceNodeID);
    auto* dstGNode = getGraphNode (targetContainerID);
    if (srcGNode == nullptr || dstGNode == nullptr)
        return false;

    if (srcGNode->nodeID == processor.getAudioInputNodeID() || srcGNode->nodeID == processor.getAudioOutputNodeID())
        return false;

    auto* contProc = dynamic_cast<ContainerModule*> (MixWrapperProcessor::getActualProcessor (dstGNode->getProcessor()));
    if (contProc == nullptr)
        return false;

    juce::String modType = srcGNode->properties["type"].toString();
    bool isByp = srcGNode->isBypassed();
    auto* actualProc = MixWrapperProcessor::getActualProcessor (srcGNode->getProcessor());

    contProc->addInnerModule (modType);
    int lastIdx = contProc->getNumInnerModules() - 1;
    if (lastIdx >= 0)
    {
        auto* inner = contProc->getInnerModule (lastIdx);
        if (inner != nullptr)
        {
            inner->isBypassed = isByp;
            if (actualProc != nullptr && inner->processor != nullptr)
            {
                juce::MemoryBlock stateData;
                actualProc->getStateInformation (stateData);
                if (stateData.getSize() > 0)
                    inner->processor->setStateInformation (stateData.getData(), (int) stateData.getSize());
            }
        }
    }

    // Connect external cables
    auto connections = processor.getGraph().getConnections();
    for (const auto& c : connections)
    {
        if (c.destination.nodeID == sourceNodeID && c.source.nodeID != targetContainerID)
        {
            processor.connect (c.source.nodeID, c.source.channelIndex, targetContainerID, c.destination.channelIndex);
        }
        else if (c.source.nodeID == sourceNodeID && c.destination.nodeID != targetContainerID)
        {
            processor.connect (targetContainerID, c.source.channelIndex, c.destination.nodeID, c.destination.channelIndex);
        }
    }

    processor.removeModule (sourceNodeID);
    setContainerDropTarget ({}, {});
    setSelectedNodeID (targetContainerID);
    repaint();
    return true;
}

void NodeCanvas::extractModuleFromContainer (juce::AudioProcessorGraph::NodeID containerID, int index)
{
    auto* gNode = getGraphNode (containerID);
    if (gNode == nullptr) return;

    auto* contProc = dynamic_cast<ContainerModule*> (MixWrapperProcessor::getActualProcessor (gNode->getProcessor()));
    if (contProc == nullptr) return;

    auto* inner = contProc->getInnerModule (index);
    if (inner == nullptr) return;

    juce::String modType = inner->typeId;
    bool isByp = inner->isBypassed;
    juce::MemoryBlock stateData;
    if (inner->processor != nullptr)
        inner->processor->getStateInformation (stateData);

    contProc->removeInnerModule (index);

    int posX = gNode->properties.contains ("x") ? (int) gNode->properties["x"] : 400;
    int posY = gNode->properties.contains ("y") ? (int) gNode->properties["y"] : 400;
    juce::Point<int> spawnPos (posX + 180, posY + index * 40);

    auto newNode = processor.addModule (modType, spawnPos);
    if (newNode != nullptr)
    {
        newNode->setBypassed (isByp);
        if (stateData.getSize() > 0)
        {
            if (auto* actual = MixWrapperProcessor::getActualProcessor (newNode->getProcessor()))
                actual->setStateInformation (stateData.getData(), (int) stateData.getSize());
        }
        setSelectedNodeID (newNode->nodeID);
    }

    processor.getHistoryManager().pushSnapshot (processor);
    repaint();
}

void NodeCanvas::unpackContainer (juce::AudioProcessorGraph::NodeID containerID)
{
    auto* gNode = getGraphNode (containerID);
    if (gNode == nullptr) return;

    auto* contProc = dynamic_cast<ContainerModule*> (MixWrapperProcessor::getActualProcessor (gNode->getProcessor()));
    if (contProc == nullptr) return;

    int numInner = contProc->getNumInnerModules();
    if (numInner == 0)
    {
        processor.removeModule (containerID);
        return;
    }

    int posX = gNode->properties.contains ("x") ? (int) gNode->properties["x"] : 400;
    int posY = gNode->properties.contains ("y") ? (int) gNode->properties["y"] : 400;

    // Capture incoming and outgoing connections of the container
    std::vector<juce::AudioProcessorGraph::Connection> externalInputs;
    std::vector<juce::AudioProcessorGraph::Connection> externalOutputs;
    for (const auto& c : processor.getGraph().getConnections())
    {
        if (c.destination.nodeID == containerID && c.source.nodeID != containerID)
            externalInputs.push_back (c);
        else if (c.source.nodeID == containerID && c.destination.nodeID != containerID)
            externalOutputs.push_back (c);
    }

    std::vector<juce::AudioProcessorGraph::NodeID> createdNodeIDs;

    for (int i = 0; i < numInner; ++i)
    {
        auto* inner = contProc->getInnerModule (i);
        if (inner == nullptr) continue;

        juce::String modType = inner->typeId;
        bool isByp = inner->isBypassed;
        juce::MemoryBlock stateData;
        if (inner->processor != nullptr)
            inner->processor->getStateInformation (stateData);

        juce::Point<int> spawnPos (posX + i * 160, posY);
        auto newNode = processor.addModule (modType, spawnPos);
        if (newNode != nullptr)
        {
            newNode->setBypassed (isByp);
            if (stateData.getSize() > 0)
            {
                if (auto* actual = MixWrapperProcessor::getActualProcessor (newNode->getProcessor()))
                    actual->setStateInformation (stateData.getData(), (int) stateData.getSize());
            }
            createdNodeIDs.push_back (newNode->nodeID);
        }
    }

    // Connect created nodes in series
    for (size_t i = 0; i + 1 < createdNodeIDs.size(); ++i)
    {
        processor.connect (createdNodeIDs[i], 0, createdNodeIDs[i + 1], 0);
    }

    // Re-wire external input connections to first unpacked node
    if (! createdNodeIDs.empty())
    {
        for (const auto& c : externalInputs)
            processor.connect (c.source.nodeID, c.source.channelIndex, createdNodeIDs.front(), c.destination.channelIndex);

        for (const auto& c : externalOutputs)
            processor.connect (createdNodeIDs.back(), c.source.channelIndex, c.destination.nodeID, c.destination.channelIndex);
    }

    // Remove the container
    processor.removeModule (containerID);
    processor.getHistoryManager().pushSnapshot (processor);
    clearSelection();
    for (auto id : createdNodeIDs)
        selectedNodeIDs.insert (id);
    if (! createdNodeIDs.empty())
        selectedNodeID = createdNodeIDs.front();
    repaint();
}

void NodeCanvas::requestDeleteNode (juce::AudioProcessorGraph::NodeID nodeID)
{
    if (nodeID == processor.getAudioInputNodeID() || nodeID == processor.getAudioOutputNodeID())
        return;

    auto* gNode = getGraphNode (nodeID);
    if (gNode == nullptr)
        return;

    auto* contProc = dynamic_cast<ContainerModule*> (MixWrapperProcessor::getActualProcessor (gNode->getProcessor()));
    if (contProc != nullptr && contProc->getNumInnerModules() > 0)
    {
        int num = contProc->getNumInnerModules();
        bool isTr = LocalizationManager::instance().getLanguage() == Language::Turkish;

        juce::String title = isTr ? juce::String (juce::CharPointer_UTF8 ("Container'\xc4\xb1 Sil"))
                                  : "Delete Container";
        juce::String message = isTr ? (juce::String (juce::CharPointer_UTF8 ("Bu Container i\xc3\xa7inde ")) + juce::String (num) + juce::String (juce::CharPointer_UTF8 (" adet mod\xc3\xbcl bulunuyor. Ne yapmak istersiniz?")))
                                    : ("This Container holds " + juce::String (num) + " modules. What would you like to do?");

        juce::String btn1 = isTr ? juce::String (juce::CharPointer_UTF8 ("Mod\xc3\xbclleri Tuvale \xc3\x87\xc4\xb1kar ve Sil"))
                                 : "Unpack to Canvas & Delete";
        juce::String btn2 = isTr ? juce::String (juce::CharPointer_UTF8 ("T\xc3\xbcm\xc3\xbcn\xc3\xbc Sil"))
                                 : "Delete All";
        juce::String btn3 = isTr ? juce::String (juce::CharPointer_UTF8 ("\xc4\xb0ptal"))
                                 : "Cancel";

        juce::AlertWindow::showYesNoCancelBox (
            juce::AlertWindow::QuestionIcon,
            title,
            message,
            btn1,
            btn2,
            btn3,
            this,
            juce::ModalCallbackFunction::create ([this, nodeID] (int result)
            {
                if (result == 1) // Unpack & Delete
                {
                    unpackContainer (nodeID);
                }
                else if (result == 2) // Delete All
                {
                    processor.removeModule (nodeID);
                }
                // result == 0 or 3 is Cancel
            })
        );
        return;
    }

    processor.removeModule (nodeID);
}

