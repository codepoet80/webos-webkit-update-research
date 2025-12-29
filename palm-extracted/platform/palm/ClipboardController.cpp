#include "config.h"

#include "ClipboardController.h"
#include "palmwebglobal.h"
#include "palmwebpageprivate.h"
#include "palmwebview.h"
#include "webkitpalmsettings.h"
#include "palmwebviewclient.h"

#include "SelectionController.h"
#include "MouseEventWithHitTestResults.h"
#include "Frame.h"
#include "RenderObject.h"
#include "FocusController.h"
#include "RenderView.h"

using namespace WebCore;

#ifndef NDEBUG
//#define DEBUG_CCP 1
#endif
#ifdef DEBUG_CCP
#define ccpLog(...) do { printf("(ccp: %s) ", __FUNCTION__); printf(__VA_ARGS__); printf("\n"); } while (0)
#else
#define ccpLog(...) do {} while (0)
#endif

namespace Palm {

static const unsigned TAPANDHOLD_TIMEOUT_MS = 700;


ClipboardController::ClipboardController(Palm::WebPage* page)
    : m_page(page)
    , m_TapAndHoldTimer(0)
    , m_selMarkersWidgetController(page)
    , m_clipboardWidgetController(page, PalmBrowserSettings()->spellWidgetYMargin) // TODO have our own YMargin setting
    , m_resendingMouseDown(false)
    , m_usesSimulatedMouseClicks(false)
{
}

ClipboardController::~ClipboardController()
{
    stopTapAndHoldTimer();
}

void ClipboardController::paint(GraphicsContext& ctxt, int contentX, int contentY, float pageScale)
{
    Frame * frame = getFrame();
    if (!frame)
       return;

    m_selMarkersWidgetController.paint(ctxt, contentX, contentY, pageScale);
    m_clipboardWidgetController.paint(ctxt, contentX, contentY, pageScale);
}

bool ClipboardController::handleMouseDownEvent(const PlatformMouseEvent& mouseEvent, bool isSimulated)
{
    bool handled = false;
    stopTapAndHoldTimer();

    m_mouseEventOnSelection = PlatformMouseEvent();

    // markers controller handle event
    handled = m_selMarkersWidgetController.handleMouseDownEvent(mouseEvent);

    // Check if tap was on selected text and if so
    // show the widgets on mouseup.
    // If cut/copy/paste widget is already visible let
    // the event through to cancel the selection.
    if (!handled && !clipboardIsVisible() && !isSimulated && !m_resendingMouseDown) {
        handled = tapOnSelectedText(mouseEvent);
        if (handled)
            m_mouseEventOnSelection = mouseEvent;
    }

    // clipboard widget's turn to handle the event
    if (!handled && !isSimulated)
        handled = m_clipboardWidgetController.handleMouseEvent(mouseEvent);


    if (!handled) {
        // start the tap+hold timer
        m_mouseEvent = mouseEvent;

        // browserserver will call handleMouseHoldEvent api
        // so there is no need to start our own timer
        if (!PalmBrowserSettings()->runningInBrowserServer)
            startTapAndHoldTimer();
    }

    return handled;
}

bool ClipboardController::handleMouseMoveEvent(const PlatformMouseEvent& mouseEvent)
{
    bool handled = false;
    stopTapAndHoldTimer();

    // if previous mouse down was on selected text we handled the event
    // and DOM never received it. Let the DOM handle it now since
    // this is not going to be the "tap" on selection
    if (m_mouseEventOnSelection.timestamp() && m_page->view()) {
        m_resendingMouseDown = true;
        m_page->view()->mouseEvent(Palm::MouseDown, m_mouseEventOnSelection.x(), m_mouseEventOnSelection.y(),
            m_mouseEventOnSelection.clickCount(), m_mouseEventOnSelection.shiftKey(), m_mouseEventOnSelection.ctrlKey(),
            m_mouseEventOnSelection.altKey(), m_mouseEventOnSelection.metaKey());
    }
    m_mouseEventOnSelection = PlatformMouseEvent();
    m_resendingMouseDown = false;

    if (!handled) {
        handled = m_selMarkersWidgetController.handleMouseMoveEvent(mouseEvent);
        // hide clipboard widget while markers are scrolling
        if (handled && m_selMarkersWidgetController.isSelecting())
            m_clipboardWidgetController.hide();
    }

    if (!handled)
        handled = m_clipboardWidgetController.handleMouseEvent(mouseEvent);

    return handled;
}

bool ClipboardController::handleMouseUpEvent(const PlatformMouseEvent& mouseEvent, bool isSimulated)
{
    bool handled = false;
    stopTapAndHoldTimer();

    if (m_mouseEventOnSelection.timestamp()) {
        // show the widget
        m_mouseEventOnSelection = PlatformMouseEvent();
        m_selMarkersWidgetController.setMarkersActive(true);
        showClipboardWidget();
        handled = true;
    }

    if (!handled) {
        handled = m_selMarkersWidgetController.handleMouseUpEvent(mouseEvent);
        if (handled && m_selMarkersWidgetController.markersAreVisible() && !m_selMarkersWidgetController.isSelecting())
            showClipboardWidget();
    }

    if (!handled && !isSimulated)
        handled = m_clipboardWidgetController.handleMouseEvent(mouseEvent);

    return handled;
}

bool ClipboardController::handleMouseHoldEvent(int x, int y)
{
    tapAndHoldCb((gpointer)this);
    return true;
}

void ClipboardController::respondToChangedSelection()
{
    // if markers disappeared due to selection change hide the clipboard widget too
    bool markersWereVisible = m_selMarkersWidgetController.markersAreVisible();
    m_selMarkersWidgetController.respondToChangedSelection();
    if (markersWereVisible && !m_selMarkersWidgetController.markersAreVisible() && clipboardIsVisible())
        m_clipboardWidgetController.hide();
}

void ClipboardController::setPageMetrics(const WebCore::IntPoint& scroll, float scale, int screenWidth, int screenHeight)
{
    m_clipboardWidgetController.setPageMetrics(scroll, scale, screenWidth, screenHeight);
}

void ClipboardController::startTapAndHoldTimer()
{
    stopTapAndHoldTimer();

    m_TapAndHoldTimer = g_timeout_source_new(TAPANDHOLD_TIMEOUT_MS);
    g_source_set_callback(m_TapAndHoldTimer, (GSourceFunc)tapAndHoldCb, this, NULL);
    g_source_attach(m_TapAndHoldTimer, g_main_loop_get_context(Palm::WebGlobal::mainLoop()));
}

void ClipboardController::stopTapAndHoldTimer()
{
    if (m_TapAndHoldTimer != 0) {
        g_source_destroy(m_TapAndHoldTimer);
        g_source_unref(m_TapAndHoldTimer);
        m_TapAndHoldTimer = 0;
    }
}

// When mouseDown happened in editable content just show the widget
// above the caret. When tapped on content was non-editable text
// Manually select the word that was tapped on before showing the widget
// When running in sysmgr first mouse down will not cause the caret to be positioned
// at the tapped location and when our timer expires caret is still at the old position.
// This is why we manually need to figure out the new caret position
gboolean ClipboardController::tapAndHoldCb(gpointer data)
{
    ClipboardController* controller = static_cast<ClipboardController*>(data);
    controller->stopTapAndHoldTimer();

    Frame* frame = controller->getFrame();
    if (!frame)
        return true;

    MouseEventWithHitTestResults mev = frame->document()->prepareMouseEvent(HitTestRequest(HitTestRequest::Active | HitTestRequest::ReadOnly), controller->m_mouseEvent.pos(), controller->m_mouseEvent);
    Node* targetNode = mev.targetNode();
    if (targetNode) {
        RenderObject* targetRenderer = targetNode->renderer();
        if (targetRenderer) {
            VisiblePosition targetPosition(targetRenderer->positionForPoint(mev.localPoint()));
            if (!targetPosition.isNull()) {
                Position pos = targetPosition.deepEquivalent();
                if (frame->selection()->isContentEditable()) {
                    // When running in mojo we have to manually set the caret position to tap target
                    // because the first (non-simulated) mouseDown event was not passed to the DOM and it did not position the caret
                    if (controller->m_usesSimulatedMouseClicks) {
                        VisibleSelection newSelection(pos);
                        if (frame->shouldChangeSelection(newSelection))
                            frame->selection()->setSelection(newSelection, CharacterGranularity, MakeNonDirectionalSelection);
                    }

                    // show the ccp widget at caret position
                    controller->showClipboardWidget();
                }
                else if (pos.inRenderedText() && controller->m_page->view()) {
                    // select the word around current position
                    controller->m_page->view()->selectWordAroundPosition(frame, targetPosition);
                    controller->m_selMarkersWidgetController.setMarkersActive(true);
                    controller->showClipboardWidget();
                }
            }
        }
    }
    return true;
}

bool ClipboardController::tapOnSelectedText(const PlatformMouseEvent& mouseEvent)
{
    Frame* frame = getFrame();
    if (!frame)
        return false;

    if (frame->selection()->contains(mouseEvent.pos())) {
        // also check if tap happened inside selection rect
        if (frame->contentRenderer()) {
            IntRect selBounds = frame->contentRenderer()->selectionBounds();
            if (!selBounds.contains(mouseEvent.pos()))
                return false;
        }
        return true;
    }

    return false;
}

void ClipboardController::showClipboardWidget()
{
    Frame* frame = getFrame();
    if (frame && !frame->selection()->isNone()) {
        IntRect selBounds;
        if (frame->selection()->isCaret())
            selBounds = frame->selection()->absoluteCaretBounds();
        else {
            selBounds = frame->contentRenderer()->selectionBounds();
            if (selBounds.isEmpty())
                return;
            // expand selection bounds by marker height so that widget doesn't overlap with the markers
            selBounds.inflateY(m_selMarkersWidgetController.markerSize().height());
        }

        ccpLog("selBounds (%d,%d %d,%d)", selBounds.x(), selBounds.y(), selBounds.width(), selBounds.height());
        m_clipboardWidgetController.setSelectionRect(selBounds, m_selMarkersWidgetController.markerSize().height());
        m_clipboardWidgetController.scrollTo(0);

        // hide spelling widget before showing the clipboard widget
        if (m_page->view())
            m_page->view()->hideSpellingWidget();

        m_clipboardWidgetController.show();
    }
}

Frame* ClipboardController::getFrame() const
{
    return core(m_page)->focusController()->focusedOrMainFrame();
}

bool ClipboardController::clipboardIsVisible() const
{
    return m_clipboardWidgetController.isVisible();
}

void ClipboardController::hide(bool resetSelection)
{
    if (clipboardIsVisible())
        m_clipboardWidgetController.hide();

    if (m_selMarkersWidgetController.markersActive()) {
        m_selMarkersWidgetController.setMarkersActive(false);
        if (resetSelection) {
            Frame* frame = getFrame();
            VisibleSelection noSelection;
            if (frame && frame->shouldChangeSelection(noSelection))
                frame->selection()->setSelection(noSelection, false, false);
        }
    }
}

}




















