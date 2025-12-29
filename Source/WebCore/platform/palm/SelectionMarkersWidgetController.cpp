#include "config.h"

#include "SelectionMarkersWidgetController.h"
#include "SelectionMarkersWidget.h"
#include "palmwebpageprivate.h"
#include "PGContext.h"

#include "GraphicsContext.h"
#include "Frame.h"
#include "FocusController.h"
#include "SelectionController.h"
#include "RenderObject.h"
#include "MouseEventWithHitTestResults.h"
#include "RenderView.h"
#include "RenderBlock.h"
#include "RenderText.h"
#include "InlineTextBox.h"

using namespace WebCore;

//#define DEBUG_CCP 1
#ifdef DEBUG_CCP
#define ccpLog(...) do { printf("(ccp: %s) ", __FUNCTION__); printf(__VA_ARGS__); printf("\n"); } while (0)
#define selRectsLog(...) do { printf("(selRectsLog: %s) ", __FUNCTION__); printf(__VA_ARGS__); printf(""); } while (0)
#else
#define ccpLog(...) do {} while (0)
#define selRectsLog(...) do {} while (0)
#endif

namespace Palm {

SelectionMarkersWidgetController::SelectionMarkersWidgetController(Palm::WebPage* page)
    : m_page(page)
    , m_widget(page)
    , m_isSelecting(false)
{
}

SelectionMarkersWidgetController::~SelectionMarkersWidgetController()
{
}

void SelectionMarkersWidgetController::paint(GraphicsContext& ctxt, int contentX, int contentY, float pageScale)
{
    if (!m_widget.markersEnabled())
        return;

    Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
    if (!frame)
        return;

    IntRect startSelRect;
    IntRect endSelRect;
    const VisibleSelection sel(frame->selection()->selection());
    if (sel.isRange()) {
        // When app is scrolled position of the selection changes but no
        // respondToChangedSelection is not called since only position changes.
        // Check if this is the case and update marker positions if needed.
        getSelectionRect(frame, startSelRect, endSelRect);
        ccpLog("frame %p startSelRect (%d,%d %d,%d)\n", frame, startSelRect.x(), startSelRect.y(), startSelRect.width(), startSelRect.height());
        ccpLog("frame %p endSelRect (%d,%d %d,%d)\n", frame, endSelRect.x(), endSelRect.y(), endSelRect.width(), endSelRect.height());
    }

    m_widget.setMarkerRects(startSelRect, endSelRect);
    if (!startSelRect.isEmpty() || !endSelRect.isEmpty()) {
        ctxt.save();
        ctxt.translate(-contentX, -contentY);
        m_widget.paint(ctxt, contentX, contentY);
        ctxt.restore();
    }
}

void SelectionMarkersWidgetController::prePaintUpdate(WebCore::IntSize& top, WebCore::IntSize& bottom)
{
    if (!m_widget.markersEnabled())
        return;

    Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
    if (!frame)
        return;

    IntRect startSelRect;
    IntRect endSelRect;
    const VisibleSelection sel(frame->selection()->selection());
    if (sel.isRange())
        getSelectionRect(frame, startSelRect, endSelRect);

    m_widget.setMarkerRects(startSelRect, endSelRect);

    if (startSelRect.isEmpty() || !m_widget.topMarkerSize(top)) {

        top.setWidth(0);
        top.setHeight(0);
    }

    if (endSelRect.isEmpty() || !m_widget.bottomMarkerSize(bottom)) {

        bottom.setWidth(0);
        bottom.setHeight(0);
    }
}

void SelectionMarkersWidgetController::paintTopMarkerTexture(WebCore::GraphicsContext& ctxt, IntPoint &location) const
{
    ctxt.save();
    m_widget.paintTopMarkerTexture(ctxt, location);
    ctxt.restore();
}

void SelectionMarkersWidgetController::paintBottomMarkerTexture(WebCore::GraphicsContext& ctxt, IntPoint &location) const
{
    ctxt.save();
    m_widget.paintBottomMarkerTexture(ctxt, location);
    ctxt.restore();
}

void SelectionMarkersWidgetController::respondToChangedSelection()
{
    if (!m_widget.markersEnabled())
        return;

    Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
    if (!frame)
        return;

    IntRect startSelRect;
    IntRect endSelRect;
    const VisibleSelection sel(frame->selection()->selection());
    if (sel.isRange()) {
        getSelectionRect(frame, startSelRect, endSelRect);
        ccpLog("startSelRect (%d,%d %d,%d)\n", startSelRect.x(), startSelRect.y(), startSelRect.width(), startSelRect.height());
        ccpLog("endSelRect (%d,%d %d,%d)\n", endSelRect.x(), endSelRect.y(), endSelRect.width(), endSelRect.height());
        ccpLog("selectionBounds (%d,%d %d,%d)\n", frame->contentRenderer()->selectionBounds().x(), frame->contentRenderer()->selectionBounds().y(), frame->contentRenderer()->selectionBounds().width(), frame->contentRenderer()->selectionBounds().height());
    }
    m_widget.setMarkerRects(startSelRect, endSelRect);
}

bool SelectionMarkersWidgetController::handleMouseDownEvent(const PlatformMouseEvent& evt)
{
    m_widget.hitTest(m_lastHitTestResult, evt);
    setSelecting(false);

    if (m_lastHitTestResult.hitResult == SelectionMarkersWidget::HitTestResult::NoHit) {
        m_lastEvtHandled = PlatformMouseEvent();
        return false;
    }
    else {
        m_lastEvtHandled = evt;
        return true;
    }
}

bool SelectionMarkersWidgetController::handleMouseMoveEvent(const PlatformMouseEvent& evt)
{
    if (m_lastEvtHandled.timestamp() && m_lastEvtHandled.eventType() == MouseEventPressed) {
        ASSERT(m_lastHitTestResult.hitResult != SelectionMarkersWidget::HitTestResult::NoHit);
        scrollMarker(evt);
        return true;
    }
    return false;
}

bool SelectionMarkersWidgetController::handleMouseUpEvent(const PlatformMouseEvent& evt)
{
    setSelecting(false);

    if (m_lastEvtHandled.timestamp()) {
        m_lastEvtHandled = PlatformMouseEvent();
        return true;
    }
    return false;
}

void SelectionMarkersWidgetController::setSelecting(bool val)
{
    m_isSelecting = val;
}

void SelectionMarkersWidgetController::setMarkersActive(bool activate)
{
    if (activate) {
        m_widget.enableMarkers();
        // update marker's positions
        respondToChangedSelection();
    }
    else {
        const IntRect empty;
        m_widget.setMarkerRects(empty, empty);
    }
}

void SelectionMarkersWidgetController::scrollMarker(const PlatformMouseEvent& evt)
{
    Frame* frame = core(m_page)->focusController()->focusedOrMainFrame();
    if (!frame)
        return;

    // have to tweak the position by marker height before hit testing the DOM
    IntPoint selPoint = evt.pos();

    // Add y offset before hit testing the renderer (calling positionForPoint())
    selPoint.setY(selPoint.y() + m_lastHitTestResult.yOffset);

    PlatformMouseEvent mouseEvent(selPoint, selPoint, evt.button(), evt.eventType(),
        evt.clickCount(), evt.shiftKey(), evt.ctrlKey(), evt.altKey(), evt.metaKey(), evt.timestamp());

    MouseEventWithHitTestResults mev = frame->document()->prepareMouseEvent(
        HitTestRequest(HitTestRequest::Active | HitTestRequest::ReadOnly), mouseEvent.pos(), mouseEvent);

    Node* targetNode = mev.targetNode();
    if (!targetNode)
        return;

    RenderObject* targetRenderer = targetNode->renderer();
    if (targetRenderer) {
        VisiblePosition targetPosition(targetRenderer->positionForPoint(mev.localPoint()));
        Position pos = targetPosition.deepEquivalent();
        if (!targetPosition.isNull()) { //&& pos.inRenderedText()
            VisibleSelection newSelection = frame->selection()->selection();

            if (!isSelecting()) {
                if (m_lastHitTestResult.hitResult == SelectionMarkersWidget::HitTestResult::Top)
                    m_selectionBase = newSelection.end();
                else
                    m_selectionBase = newSelection.start();

                setSelecting(true);
            }

            newSelection.setBase(m_selectionBase);
            newSelection.setExtent(targetPosition);
            newSelection.expandUsingGranularity(CharacterGranularity);

            bool validSelection = false;
            Position start = newSelection.start();
            Position end = newSelection.end();
            if (start.isNotNull() && start.inRenderedText() &&
                (start.isRenderedCharacter() || start.next().isRenderedCharacter()) &&
                end.isNotNull() && end.inRenderedText() &&
                (end.isRenderedCharacter() || end.previous().isRenderedCharacter())) {

                validSelection = true;
            }

            if (frame->shouldChangeSelection(newSelection) && validSelection) {
                frame->selection()->setIsDirectional(false);
                frame->selection()->setSelection(newSelection, frame->selection()->granularity(), MakeNonDirectionalSelection);
                if (!m_widget.markersEnabled())
                    m_widget.enableMarkers();
            }
        }
    }
}

// copied RenderText::absoluteRectsForRange() except that we don't want absolute coordinates
static void rectsForRange(Vector<IntRect>& rects, RenderText* rt, unsigned start, unsigned end, bool useSelectionHeight)
{
    // Work around signed/unsigned issues. This function takes unsigneds, and is often passed UINT_MAX
    // to mean "all the way to the end". InlineTextBox coordinates are unsigneds, so changing this
    // function to take ints causes various internal mismatches. But selectionRect takes ints, and
    // passing UINT_MAX to it causes trouble. Ideally we'd change selectionRect to take unsigneds, but
    // that would cause many ripple effects, so for now we'll just clamp our unsigned parameters to INT_MAX.
    ASSERT(end == UINT_MAX || end <= INT_MAX);
    ASSERT(start <= INT_MAX);
    start = min(start, static_cast<unsigned>(INT_MAX));
    end = min(end, static_cast<unsigned>(INT_MAX));

    for (InlineTextBox* box = rt->firstTextBox(); box; box = box->nextTextBox()) {
        // Note: box->end() returns the index of the last character, not the index past it
        if (start <= box->start() && box->end() < end) {
            IntRect r = IntRect(box->x(), box->y(), box->width(), box->height());
            if (useSelectionHeight) {
                IntRect selectionRect = box->selectionRect(0, 0, start, end);
                r.setHeight(selectionRect.height());
                r.setY(selectionRect.y());
            }
            rects.append(r);
        } else {
            unsigned realEnd = min(box->end() + 1, end);
            IntRect r = box->selectionRect(0, 0, start, realEnd);
            if (!r.isEmpty())
                rects.append(r);
        }
    }
}

// See Range::pastLastNode() - we want last node here not past last.
static Node* lastNodeForRange(Range* r)
{
   if (r->endContainer()->offsetInCharacters())
       return r->endContainer();
   if (Node* child = r->endContainer()->childNode(r->endOffset()))
       return child->traversePreviousSiblingPostOrder();
   return r->endContainer();
}

// Code from Frame::selectionTextRects() and Range::textRects() modified to match our needs.
// Instead of getting the entire list of selected text rect which can be very slow (even with a single page of text)
// get only the first and last valid rects.
// First for() loop starts from first selected node in the range and bails out as soon as the first valid selection rect is found.
// Second for() loop starts from the last node in selected text range and walks backwards.
void SelectionMarkersWidgetController::getSelectionRect(Frame* frame, IntRect& startSelRect, IntRect& endSelRect)
{
    startSelRect = IntRect();
    endSelRect = IntRect();

    RenderView* root = frame->contentRenderer();
    if (!root)
        return;

    RefPtr<Range> selectedRange = frame->selection()->toNormalizedRange();
    Node* startContainer = selectedRange->startContainer();
    Node* endContainer = selectedRange->endContainer();
    if (!startContainer || !endContainer)
        return;

    Node* stopNode = selectedRange->pastLastNode();
    bool startRectFound = false;
    for (Node* node = selectedRange->firstNode(); node != stopNode; node = node->traverseNextNode()) {
        RenderObject* r = node->renderer();
        if (!r || !r->isText())
            continue;

        RenderText* renderText = toRenderText(r);
        int startOffset = node == startContainer ? selectedRange->startOffset() : 0;
        int endOffset = node == endContainer ? selectedRange->endOffset() : numeric_limits<int>::max();

        Vector<IntRect> intRects;
        rectsForRange(intRects, renderText, startOffset, endOffset, true);
        for (unsigned i = 0; i < intRects.size(); ++i) {
            selRectsLog("%d,%d  %d,%d\n", intRects[i].x(), intRects[i].y(), intRects[i].width(), intRects[i].height());
            if (!intRects[i].isEmpty()) {
                startSelRect = intRects[i];
                renderText->computeRectForRepaint(0, startSelRect);
                startRectFound = true;
                break;
            }
        }

        if (startRectFound)
            break;
    }

    // Walk the list of nodes in the text range in reverse looking for the first valid rect
    Node* firstNode = selectedRange->firstNode();
    if (!firstNode)
        return;
    Node* lastNode = lastNodeForRange(selectedRange.get());
    bool endRectFound = false;

    for (Node* node = lastNode; node;) {
        RenderObject* r = node->renderer();
        if (!r || !r->isText()) {
            if (node == firstNode)
                break;
            node = node->traversePreviousNode();
            continue;
        }

        RenderText* renderText = toRenderText(r);
        int startOffset = node == startContainer ? selectedRange->startOffset() : 0;
        int endOffset = node == endContainer ? selectedRange->endOffset() : numeric_limits<int>::max();

        Vector<IntRect> intRects;
        rectsForRange(intRects, renderText, startOffset, endOffset, true);

        // traverse return text rects in reverse to find the last valid one
        for (int i = intRects.size() - 1; i >= 0; i--) {
            selRectsLog("%d,%d  %d,%d\n", intRects[i].x(), intRects[i].y(), intRects[i].width(), intRects[i].height());
            if (!intRects[i].isEmpty()) {
                endSelRect = intRects[i];
                renderText->computeRectForRepaint(0, endSelRect);
                endRectFound = true;
                break;
            }
        }

        // break from the outer for if we found the rect
        if (endRectFound)
            break;

        // break out if we've reached the beginning of the text range
        if (node == firstNode)
            break;
        node = node->traversePreviousNode();
    }
}

}
