
#include "config.h"

#include "ClipboardWidgetController.h"
#include "ClipboardWidget.h"
#include "palmwebpage.h"
#include "palmwebpageprivate.h"
#include "palmwebview.h"
#include "palmwebviewclient.h"
#include "palmwebglobal.h"
#include "GraphicsContext.h"
#include "htmlediting.h"
#include "RenderBlock.h"
#include "PGContext.h"
#include <cmath>
#include "Frame.h"
#include "FrameView.h"
#include "ScrollView.h"
#include "FocusController.h"
#include "RenderView.h"

namespace WebCore
{

static const unsigned ANIMATION_HZ = 30;
static const float    ANIMATION_DURATION_SEC = 0.2;
static const unsigned ANIMATION_NUM_FRAMES = 1000.0f * ANIMATION_DURATION_SEC / ANIMATION_HZ;
static const unsigned ANIMATION_TIMEOUT_MS = 1000 / ANIMATION_HZ;
static const float	  ANIMATION_STEP = 1.0f / ANIMATION_NUM_FRAMES;
static const int      BALLOON_PADDING_Y = 0;
static const int      BALLOON_PADDING_X = 5;
static const int      MIN_FRAME_PADDING = 50;

ClipboardWidgetController::ClipboardWidgetController(Palm::WebPage* page, int yMargin)
    : m_widget(new ClipboardWidget(yMargin))
    , m_mouseEventInfo(new WidgetMouseEventInfo())
    , m_page(page)
    , m_selectionRect(0, 0, 0, 0)
	, m_pageScroll(0,0)
	, m_pageScale(0.0f)
	, m_screenWidth(320)
    , m_screenHeight(480)
    , m_dirty(true)
	, m_animationTimer(0)
	, m_animationStep(0.0)
	, m_curVisibleRect(0, 0, 0, 0)
	, m_flickScroller(this)
    , m_acceleratedComposited(false)
{
	ASSERT(page != NULL);
}

ClipboardWidgetController::~ClipboardWidgetController()
{
	// TODO hide the widget?
	stopAnimationTimer();
	delete m_widget;
    delete m_mouseEventInfo;
}

/**
 * Paint (draw) the Clipboard widget.
 *
 * @param ctxt      The graphics context with which to draw.
 * @param contentX  The left coord of the content rectangle being drawn by the Palm::WebView.
 * @param contentY  The top coord of the content rectangle being drawn by the Palm::WebView.
 * @param pageScale The scale factor of the page being drawn.
 */
void ClipboardWidgetController::paint(GraphicsContext& ctxt, int contentX, int contentY, float pageScale, bool accelComposited)
{
	if (pageScale == 0.0f)
		return;

	if (m_widget->getOpacity() == 0)
		return;

    IntRect curSelRect = selectionRect();
    if (m_selectionRect != curSelRect) {
        hide();
        m_selectionRect = curSelRect;
        return;
    }

    ctxt.save();

    m_acceleratedComposited = accelComposited;

	// Add the page scroll position (to get to screen coordinates)
	// and subtract the content rect position which places us at the
    // right place because the context was translated before calling into WebKit.
    if (!m_acceleratedComposited)
        ctxt.translate(m_pageScroll.x()-contentX, m_pageScroll.y()-contentY);

	// Reverse scale back to 1.0 because widget is always drawn in screen coordinates.
	// with scale factor of 1.0.
	float scale = 1.0f / pageScale;
	ctxt.scale(FloatSize(scale, scale));

	positionWidget();
	m_widget->paint(&ctxt);

	updateVisibleRect();

	ctxt.restore();
}

bool ClipboardWidgetController::handleMouseEvent(const PlatformMouseEvent& mouseEvent)
{
    if (!this->isVisible())
    {
        //we're not visible, so don't handle the event
        return false;
    }

    IntRect rect(m_curVisibleRect);
    screenToDocument(rect);
    IntPoint documentPt = IntPoint(mouseEvent.globalX(), mouseEvent.globalY());
    IntPoint point(documentPt);
    if (m_acceleratedComposited)
        point.move(-m_screenWidgetRect.x(), -m_screenWidgetRect.y());

    bool eventHandled = false;
    switch (mouseEvent.eventType()) {
    case MouseEventPressed:
    {
        if (!rect.contains(point)) {
            //the mouse event was outside this widget, so hide it
            m_mouseEventInfo->clear();
            m_mouseEventInfo->flickClear();
            this->hide();
            eventHandled = false;
            break;
        }
        const Palm::ClipboardCommand* command = this->pointToCommand(documentPt);
        if (command) {
            // We got a mouse down on one of the commands, so swallow the event
            m_mouseEventInfo->setMouseDownInfo(true, true);
            m_mouseEventInfo->setCurrPt(documentPt);
            m_mouseEventInfo->flickStart(documentPt.x());
            eventHandled = true;
        }
        break;
    }
    case MouseEventMoved:
    {
        if (m_mouseEventInfo->isMouseDown() && m_mouseEventInfo->isDownOnWidget()) {
            m_mouseEventInfo->setCurrPt(documentPt);
            this->scrollBy(-m_mouseEventInfo->moveX());
            this->invalidate();
            eventHandled = true;
        } else
            hide();
        break;
    }
    case MouseEventReleased:
    {
        if (rect.contains(point)) {
            eventHandled = true;
            if (m_mouseEventInfo->isMouseDown() && m_mouseEventInfo->isDownOnWidget()) {
                    m_mouseEventInfo->flickEnd(documentPt.x());
                    this->flick(m_mouseEventInfo->getFlickSpeed());
            }
            if (!m_mouseEventInfo->didDrag()) {
                const Palm::ClipboardCommand* command = this->pointToCommand(documentPt);
                if (!command) {
                    this->hide();
                    eventHandled = false;
                } else {
                    if (this->isFlicking())
                        this->stopFlick();
                    else {
                        this->hide();
                        command->execute();
                    }
                }
            }
        } else {
            eventHandled = false;
        }
        m_mouseEventInfo->setMouseDownInfo(false);
        m_mouseEventInfo->clear();
        m_mouseEventInfo->flickClear();

        break;
    }
    case MouseEventScroll:
        break;
    }

    return eventHandled;
}

/**
 * Set the bounding box of the selection for which this will be displayed.
 *
 * @param r The selection bounding rectangle in document coordinates.
 */
void ClipboardWidgetController::setSelectionRect(const WebCore::IntRect& r, int selectionInflateY)
{
    m_selectionInflateY = selectionInflateY;
    if (m_selectionRect != r) {
        invalidate();
        m_selectionRect = r;
        updateWidgetRect();
        m_dirty = true;
        positionWidget();
    }
}

/**
 * Is the controller/widget dirty and in need of redraw?
 */
bool ClipboardWidgetController::dirty() const
{
	return m_dirty;
}

/**
 * Calculate the widget rectangle in screen coordinates.
 * Call this function when the commands, selection rect or page metrics change
 * to update the widget rectangle, arrow direction and arrow position.
 */
void ClipboardWidgetController::updateWidgetRect()
{
    IntRect screenSelectionRect(m_selectionRect);
    documentToScreen(screenSelectionRect);

    m_arrowPos = screenSelectionRect.x() + screenSelectionRect.width()/2;

    if (screenSelectionRect.y() > ClipboardWidget::height() + MIN_FRAME_PADDING) {
        // Draw the widget above the selection
        m_screenWidgetRect.setY( screenSelectionRect.y() - ClipboardWidget::height() - BALLOON_PADDING_Y );
        m_arrowDir = ClipboardWidget::ClipboardArrowDown;
    } else if (screenSelectionRect.bottom() + ClipboardWidget::height() + MIN_FRAME_PADDING < m_screenHeight) {
        // Draw the widget below the selection
        m_screenWidgetRect.setY( screenSelectionRect.bottom() + BALLOON_PADDING_Y );
        m_arrowDir = ClipboardWidget::ClipboardArrowUp;
    } else {
        // Draw the widget centered vertically on the screen
        m_screenWidgetRect.setY((m_screenHeight - ClipboardWidget::height() - BALLOON_PADDING_Y) / 2);
        m_arrowDir = ClipboardWidget::ClipboardArrowDown;
    }

    int maxWidthOnScreen = m_screenWidth - 2 * BALLOON_PADDING_X;	// Padding on left & right
	int balloonWidth = m_widget->getMaxBalloonWidth();
	if (balloonWidth > maxWidthOnScreen)
		balloonWidth = maxWidthOnScreen;

    int left = m_arrowPos - balloonWidth / 2;
    if (left < BALLOON_PADDING_X)
        left = BALLOON_PADDING_X;
    if (left + balloonWidth > m_screenWidth - BALLOON_PADDING_X)
        left = m_screenWidth - balloonWidth - BALLOON_PADDING_X;
    m_screenWidgetRect.setX(left);
    m_screenWidgetRect.setHeight(ClipboardWidget::height());
    balloonWidth = std::max(balloonWidth, ClipboardWidget::getMinBalloonWidth());
    m_screenWidgetRect.setWidth(balloonWidth);

}

/**
 * Place the Clipboard widget in screen space based on the selection bounding rectangle.
 */
void ClipboardWidgetController::positionWidget()
{
	if (!dirty())
		return;

	invalidate();
    IntPoint position(m_screenWidgetRect.topLeft());
    int arrowPosX = m_arrowPos;

    if (m_acceleratedComposited) {

        position.setX(0);
        position.setY(0);
        arrowPosX -= m_screenWidgetRect.x();
    }

    m_widget->updateUI(position, m_screenWidgetRect.width(), arrowPosX, m_arrowDir);
	m_dirty = false;
}

/**
 * Update the size and position of the visible rect on screen.
 * If visible rect changed notify clients. Client (webview widget) will then
 * stop or continue sending mouse events into webkit. If widget is visible
 * we want to receive mouse events and skip handlers in the brserver - adapter
 * (e.g mouse move event on the widget should scroll the widget and not the page).
 * By having the visible rect, client knows when mouse event belongs to the widget
 * and will pass them down.
 */
void ClipboardWidgetController::updateVisibleRect() {

	// check if visible rect has changed
	IntRect newRect;
	m_widget->getMinimalBounds(newRect);

	if (!isVisible()) {
		newRect.setHeight(0);
		newRect.setWidth(0);
	}

	if (m_curVisibleRect != newRect) {
        m_curVisibleRect = newRect;
        if (m_page && m_page->view() && m_page->view()->client()) {
            m_page->view()->client()->removeInteractiveWidgetRect((uintptr_t)this);
            m_page->view()->client()->addInteractiveWidgetRect((uintptr_t)this, m_curVisibleRect.x(), m_curVisibleRect.y(), m_curVisibleRect.width(), m_curVisibleRect.height());
        }
    }
}

/**
 * @return true if invalidated, false if not.
 */
bool ClipboardWidgetController::invalidate()
{
	m_dirty = true;
	if (m_page && m_page->view() && m_page->view()->client()) {
        IntRect documentWidgetRect = m_screenWidgetRect;
        screenToDocument(documentWidgetRect);
        m_page->view()->client()->invalContents(documentWidgetRect.x(), documentWidgetRect.y(), documentWidgetRect.width(), documentWidgetRect.height());
		return true;
	}
	else {
		return false;
	}
}

/**
 * Return pointer to widget.
 */
ClipboardWidget* ClipboardWidgetController::widget() const
{
	return m_widget;
}

/**
 * Hide the widget.
 */
void ClipboardWidgetController::hide()
{
	invalidate();

	// TODO figure out how to solve two events sent from SysMgr
    // and the fact that in first case, the selected area is incorrectly set

	// stop timers
	stopFlick();
	stopAnimationTimer();
	if (m_widget->getOpacity() > 0) {
		startAnimation(-1);
	}
}

/**
 * Show the widget.
 * Returns true if the widget is shown, false otherwise
 */
bool ClipboardWidgetController::show()
{
    if (m_widget->setCommandsForFrame(getFrame())) {
        updateWidgetRect();
        invalidate();

        // stop the current timer
        stopAnimationTimer();
        if (m_widget->getOpacity() < 1) {
            startAnimation(1);
        }
        return true;
    }
    return false;
}

void ClipboardWidgetController::startAnimation(short direction)
{
	// set animation step to positive or negative value depending on direction
	m_animationStep = ANIMATION_STEP * direction;
	m_widget->setOpacity(m_widget->getOpacity() + m_animationStep);
	invalidate();
	// start the new timer
	startAnimationTimer();
}

/**
 * Is the widget visible?
 */
bool ClipboardWidgetController::isVisible() const
{
	return m_animationStep > 0;
}

/**
 * Return the command whose bounding box contains this point.
 *
 * @param pt The point (in document coordinates) to test for in the widget.
 *
 * @return The command corresponding to the point. An empty string if there is no
 *         corresponding command.
 */
const Palm::ClipboardCommand* ClipboardWidgetController::pointToCommand(const IntPoint& pos) const
{
	IntPoint pt(pos);
	documentToScreen(pt);

    if (m_acceleratedComposited)
        pt.move(-m_screenWidgetRect.x(), -m_screenWidgetRect.y());

    return m_widget->pointToCommand(pt);
}


/**
 * Start flick animation with given speed
 */
void ClipboardWidgetController::flick(double speed)
{
	if (m_flickScroller.initialize(speed))
		m_flickScroller.startFlickTimer();
}

/**
 * returns true if flick is currently active
 */
bool ClipboardWidgetController::isFlicking()
{
	return m_flickScroller.isFlicking();
}

/**
 * Stop active flick
 */
void ClipboardWidgetController::stopFlick()
{
	m_flickScroller.stopFlickTimer();
}

/**
 * Scroll by the specified X amount in document coordinates.
 */
void ClipboardWidgetController::scrollBy(int deltaX)
{
	m_widget->scrollBy(deltaX * m_pageScale);
}

/**
 * Scroll to the specified X coordinate in screen coordinates.
 */
void ClipboardWidgetController::scrollTo(int x)
{
	m_widget->scrollTo(x);
}

/** FIXME Widget should not be doing anything when it is not visible.
 *
 * Set metrics of the page on which this widget will be drawn. These metrics will be used
 * to properly convert document to screen coordinates when drawing this widget.
 */
void ClipboardWidgetController::setPageMetrics(const IntPoint& scroll, float scale, int screenWidth, int screenHeight)
{
    if (m_pageScroll != scroll || m_pageScale != scale || m_screenWidth != screenWidth || m_screenHeight != screenHeight) {
		// We invalidate first so that we can calculate the current widget position in 
		// document coordinates before we change values on which this math relies.
		invalidate();

		m_pageScroll = scroll;
		m_pageScale = scale;
		m_screenWidth = screenWidth;
        m_screenHeight = screenHeight;

        // Update the selection rect if the page metrics have changed
        m_selectionRect = selectionRect();

        // Now invalidate a second time to get the widget to draw at it's new position.
        updateWidgetRect();
		invalidate();
	}
}

IntRect ClipboardWidgetController::selectionRect() const
{
    IntRect selRect;
    WebCore::Frame * frame = getFrame();
    if (frame && !frame->selection()->isNone()) {
        if (frame->selection()->isCaret())
            selRect = frame->selection()->absoluteCaretBounds();
        else {
            selRect = frame->contentRenderer()->selectionBounds();
            selRect.inflateY(m_selectionInflateY);
        }
    }
    return selRect;
}

void ClipboardWidgetController::documentToScreen(WebCore::IntPoint& pt) const
{
	pt.setX((pt.x() - m_pageScroll.x()) * m_pageScale );
	pt.setY((pt.y() - m_pageScroll.y()) * m_pageScale );
}

void ClipboardWidgetController::documentToScreen(WebCore::IntRect& r) const
{
	int docWidth = r.width();
	int docHeight = r.height();

	r.setX((r.x() - m_pageScroll.x()) * m_pageScale );
	r.setY((r.y() - m_pageScroll.y()) * m_pageScale );
	r.setWidth(docWidth * m_pageScale);
	r.setHeight(docHeight * m_pageScale);
}

void ClipboardWidgetController::screenToDocument(WebCore::IntPoint& pt) const
{
	if (m_pageScale != 0.0f) {
		pt.setX(pt.x() / m_pageScale + m_pageScroll.x());
		pt.setY(pt.y() / m_pageScale + m_pageScroll.y());
	}
}

void ClipboardWidgetController::screenToDocument(WebCore::IntRect& r) const
{
	if (m_pageScale != 0.0f) {
		int screenWidth = r.width();
		int screenHeight = r.height();

		r.setX(r.x() / m_pageScale + m_pageScroll.x());
		r.setY(r.y() / m_pageScale + m_pageScroll.y());
		r.setWidth(screenWidth / m_pageScale);
		r.setHeight(screenHeight / m_pageScale);
	}
}

void ClipboardWidgetController::startAnimationTimer()
{
	if (m_animationTimer != 0) return;

	m_animationTimer = g_timeout_source_new(ANIMATION_TIMEOUT_MS);
	g_source_set_callback(m_animationTimer, (GSourceFunc)animateCb, this, NULL);
	g_source_attach(m_animationTimer, g_main_loop_get_context(Palm::WebGlobal::mainLoop()));
}

void ClipboardWidgetController::stopAnimationTimer()
{
	if (m_animationTimer != 0)
	{
		g_source_destroy(m_animationTimer);
		g_source_unref(m_animationTimer);
		m_animationTimer = 0;
	}
}


gboolean ClipboardWidgetController::animateCb(gpointer data)
{
    ClipboardWidgetController* controller = static_cast<ClipboardWidgetController*>(data);
	controller->m_widget->setOpacity(controller->m_widget->getOpacity() + controller->m_animationStep);
	controller->invalidate();

	float curOpacity = controller->m_widget->getOpacity();
	if (curOpacity == 0 || curOpacity == 1)
	{
		// we have reached the end, stop the timer
		controller->stopAnimationTimer();
	}
	return true;
}

WebCore::Frame* ClipboardWidgetController::getFrame() const
{
    return core(m_page)->focusController()->focusedOrMainFrame();
}



/*
 * Flick scroller implementation
 */
ClipboardWidgetController::FlickScroller::FlickScroller(ClipboardWidgetController* controller)
	: m_controller(controller)
	, m_dStartSpeed(0.0)
	, m_dDuration(0.0)
	, m_direction(1)
	, m_flickTimer(0)
	, m_curFlickDuration(0.0)
{}

ClipboardWidgetController::FlickScroller::~FlickScroller()
{
	stopFlickTimer();
}

bool ClipboardWidgetController::FlickScroller::initialize(double speed)
{
	if (std::abs(speed) < MIN_FLICK_SPEED)
		return false;

	stopFlickTimer();
	m_direction = speed < 0 ? -1 : 1;
	m_dStartSpeed = std::abs(speed / SPEED_DIVIDER);
	m_dDuration = m_dStartSpeed / ACCELERATION;
	return true;
}

int ClipboardWidgetController::FlickScroller::getPosition(double t) const
{
	if (t > m_dDuration)
		t = m_dDuration;

	int distance = m_dStartSpeed * t - (ACCELERATION/2) * t*t;
	if (distance < 0)
		distance = 0;

	return m_direction * distance;
}

void ClipboardWidgetController::FlickScroller::startFlickTimer()
{
	m_curFlickDuration = 0.0;
	if (m_flickTimer != 0) return;

	m_flickTimer = g_timeout_source_new(FLICK_TIMEOUT_MS);
	g_source_set_callback(m_flickTimer, (GSourceFunc)flickCb, m_controller, NULL);
	g_source_attach(m_flickTimer, g_main_loop_get_context(Palm::WebGlobal::mainLoop()));
}

void ClipboardWidgetController::FlickScroller::stopFlickTimer()
{
	m_curFlickDuration = 0.0;
	if (m_flickTimer != 0) {
		g_source_destroy(m_flickTimer);
		g_source_unref(m_flickTimer);
		m_flickTimer = 0;
	}
}

bool ClipboardWidgetController::FlickScroller::isFlicking()
{
	return m_flickTimer != 0;
}

gboolean ClipboardWidgetController::FlickScroller::flickCb(gpointer data)
{
    ClipboardWidgetController* controller = static_cast<ClipboardWidgetController*>(data);
    controller->m_flickScroller.m_curFlickDuration += ClipboardWidgetController::FlickScroller::FLICK_TIMEOUT_SEC;
	int deltaX = controller->m_flickScroller.getPosition(controller->m_flickScroller.m_curFlickDuration) * controller->m_pageScale;
	int oldScrollPosition = controller->m_widget->getScrollPosition();
	controller->m_widget->scrollBy(deltaX);
	controller->invalidate();

	if (controller->m_flickScroller.m_curFlickDuration >= controller->m_flickScroller.getDuration() ||
		oldScrollPosition == controller->m_widget->getScrollPosition()) {
		// we have reached the end, stop the timer
		controller->m_flickScroller.stopFlickTimer();
	}
	return true;
}


}
