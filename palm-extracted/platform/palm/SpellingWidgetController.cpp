
#include "config.h"

#include "SpellingWidgetController.h"
#include "SpellingWidget.h"
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
#include "FocusController.h"
#include "RenderView.h"

namespace WebCore
{

static const unsigned ANIMATION_HZ = 30;
static const float    ANIMATION_DURATION_SEC = 0.2;
static const unsigned ANIMATION_NUM_FRAMES = 1000.0f * ANIMATION_DURATION_SEC / ANIMATION_HZ;
static const unsigned ANIMATION_TIMEOUT_MS = 1000 / ANIMATION_HZ;
static const float	  ANIMATION_STEP = 1.0f / ANIMATION_NUM_FRAMES;

SpellingWidgetController::SpellingWidgetController(Palm::WebPage* page, int yMargin) :
	  m_widget(new SpellingWidget(yMargin))
    , m_dirty(true)
    , m_page(page)
	, m_wordRect(0, 0, 0, 0)
	, m_pageScroll(0,0)
	, m_pageScale(0.0f)
    , m_screenWidth(0)
    , m_screenHeight(0)
    , m_screenWidgetRect(0,0,0,0)
    , m_arrowDir(SpellingWidget::SpellingArrowUp)
    , m_acceleratedComposited(false)
    , m_animationTimer(0)
	, m_animationStep(0.0)
	, m_curVisibleRect(0, 0, 0, 0)
	, m_flickScroller(this)
{
	ASSERT(page != NULL);
}

SpellingWidgetController::~SpellingWidgetController()
{
	// TODO hide the widget?
	stopAnimationTimer();
	delete m_widget;
}

/**
 * Paint (draw) the spelling widget.
 *
 * @param ctxt      The graphics context with which to draw.
 * @param contentX  The left coord of the content rectangle being drawn by the Palm::WebView.
 * @param contentY  The top coord of the content rectangle being drawn by the Palm::WebView.
 * @param pageScale The scale factor of the page being drawn.
 * @param accelComposited The flag indicating whether or not to this is being rendered through the accelerated composited path
 */
void SpellingWidgetController::paint(GraphicsContext& ctxt, int contentX, int contentY, float pageScale, bool accelComposited)
{
	if (pageScale == 0.0f)
		return;

	if (m_widget->getOpacity() == 0)
		return;

	ctxt.save();

    m_acceleratedComposited = accelComposited;
    if (!m_acceleratedComposited)
        // Add the page scroll position (to get to screen coordinates)
        // and subtract the content rect position which places us at the
        // right place because the contect was translated before calling into WebKit.
        ctxt.translate(m_pageScroll.x()-contentX, m_pageScroll.y()-contentY);

	// Reverse scale back to 1.0 because widget is always drawn in screen coordinates.
	// with scale factor of 1.0.
	float scale = 1.0f / pageScale;
	ctxt.scale(FloatSize(scale, scale));

    if (!accelComposited)
        positionWidget(m_screenWidgetRect.topLeft());
    else
        positionWidget(IntPoint(contentX, contentY), m_acceleratedComposited);
	m_widget->paint(&ctxt);

	updateVisibleRect();

	ctxt.restore();
}

/**
 * Set the bounding box of the mispelled word for which this will be displayed.
 *
 * @param r The word bounding rectangle in document coordinates.
 */
void SpellingWidgetController::setWordRect(const WebCore::IntRect& r)
{
    if (m_wordRect != r) {
        invalidate();
        m_wordRect = r;
        updateWidgetRect();
        m_dirty = true;
        positionWidget(m_screenWidgetRect.topLeft());
    }
}

/**
 * Is the controller/widget dirty and in need of redraw?
 */
bool SpellingWidgetController::dirty() const
{
	return m_dirty;
}

/**
 * Calculate the widget rectangle in screen coordinates.
 */
void SpellingWidgetController::updateWidgetRect()
{
	IntRect wordRect(m_wordRect);
	documentToScreen(wordRect);

	const int minPaddingForAboveWidget = 50;
    int arrowPosX = wordRect.x() + wordRect.width()/2;

    const int balloonYPadding = 5;
	if (wordRect.y() < SpellingWidget::height() + minPaddingForAboveWidget) {
        m_screenWidgetRect.setY( wordRect.bottom() + balloonYPadding );
        m_arrowDir = SpellingWidget::SpellingArrowUp;
	}
	else {
        //If wordRect position is covered by VKB, view shift to visible area but the wordRect still has old co-ordinates
        if(wordRect.bottom() + balloonYPadding < m_screenHeight)
            m_screenWidgetRect.setY( wordRect.y() - SpellingWidget::height() - balloonYPadding );
        else
            m_screenWidgetRect.setY( m_screenHeight - wordRect.height() - SpellingWidget::height() - balloonYPadding * 3 );
        m_arrowDir = SpellingWidget::SpellingArrowDown;
	}

	const int balloonXpadding = 5; // space between balloon and edge of screen
	int maxWidthOnScreen = m_screenWidth - 2 * balloonXpadding;	// Padding on left & right
	int balloonWidth = m_widget->getMaxBalloonWidth();
	if (balloonWidth > maxWidthOnScreen)
		balloonWidth = maxWidthOnScreen;

    int left = arrowPosX - balloonWidth / 2;
	if (left < balloonXpadding)
		left = balloonXpadding;
	if (left + balloonWidth > m_screenWidth - balloonXpadding)
		left = m_screenWidth - balloonWidth - balloonXpadding;
    m_screenWidgetRect.setX(left);
    m_screenWidgetRect.setHeight(SpellingWidget::height());
    balloonWidth = std::max(balloonWidth, SpellingWidget::getMinBalloonWidth());
    m_screenWidgetRect.setWidth(balloonWidth);

}

/**
 * Place the spelling widget in screen space based on the word bounding rectangle.
 */
void SpellingWidgetController::positionWidget(const IntPoint& position, bool accelComposited)
{
	if (!dirty())
		return;

	invalidate();

    IntRect wordRect(m_wordRect);
    documentToScreen(wordRect);
    int arrowPosX = wordRect.x() + wordRect.width()/2;
    if (accelComposited)
        arrowPosX -= m_screenWidgetRect.x();

    m_widget->updateUI(position, m_screenWidgetRect.width(), arrowPosX, m_arrowDir);
	m_dirty = false;
}

/**
 * Update the size and position of the visible rect on screen.
 * If visibe rect changed notify clients. Client (webview widget) will then
 * stop or continue sending mouse events into webkit. If widget is visible
 * we want to receive mouse events and skip handlers in the brserver - adapter
 * (e.g mouse move event on the widget should scroll the widget and not the page).
 * By having the visible rect, client knows when mouse event belongs to the widget
 * and will pass them down.
 */
void SpellingWidgetController::updateVisibleRect() {

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
			m_page->view()->client()->spellingWidgetVisibleRectUpdate(m_curVisibleRect.x(), m_curVisibleRect.y(), m_curVisibleRect.width(), m_curVisibleRect.height());
		}
	}
}

/**
 * @return true if invalidated, false if not.
 */
bool SpellingWidgetController::invalidate()
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
SpellingWidget* SpellingWidgetController::widget() const
{
	return m_widget;
}

/**
 * Hide the widget.
 */
void SpellingWidgetController::hide()
{
	invalidate();

	// TODO figure out how to solve two events sent from SysMgr
	// and the fact that in first case, the selected word is incorrectly set

	// stop timers
	stopFlick();
	stopAnimationTimer();
	if (m_widget->getOpacity() > 0) {
		startAnimation(-1);
	}
}

/**
 * Show the widget.
 */
void SpellingWidgetController::show()
{
	invalidate();

	// stop the current timer
	stopAnimationTimer();
	if (m_widget->getOpacity() < 1) {
		startAnimation(1);
	}
}

void SpellingWidgetController::startAnimation(short direction)
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
bool SpellingWidgetController::isVisible() const
{
	return m_animationStep > 0;
}

/**
 * Remove all the words in this widget.
 */
void SpellingWidgetController::clearWords()
{
	m_widget->clearWords();
}

/**
 * Return the word whose bounding box contains this point.
 *
 * @param pt The point (in document coordinates) to test for in the widget.
 *
 * @return The word corresponding to the point. An empty string if there is no
 *         corresponding word.
 */
const SpellingWidget::Word* SpellingWidgetController::pointToWord(const IntPoint& pos) const
{
	IntPoint pt(pos);
	documentToScreen(pt);

    if (m_acceleratedComposited)
        pt.move(-m_screenWidgetRect.x(), -m_screenWidgetRect.y());
	return m_widget->pointToWord(pt);
}


/**
 * Start flick animation with given speed
 */
void SpellingWidgetController::flick(double speed)
{
	if (m_flickScroller.initialize(speed))
		m_flickScroller.startFlickTimer();
}

/**
 * returns true if flick is currently active
 */
bool SpellingWidgetController::isFlicking()
{
	return m_flickScroller.isFlicking();
}

/**
 * Stop active flick
 */
void SpellingWidgetController::stopFlick()
{
	m_flickScroller.stopFlickTimer();
}

/**
 * Scroll by the specified X amount in document coordinates.
 */
void SpellingWidgetController::scrollBy(int deltaX)
{
	m_widget->scrollBy(deltaX * m_pageScale);
}

/**
 * Scroll to the specified X coordinate in screen coordinates.
 */
void SpellingWidgetController::scrollTo(int x)
{
	m_widget->scrollTo(x);
}

/**
 * Set metrics of the page on which this widget will be drawn. These metrics will be used
 * to properly convert document to screen coordinates when drawing this widget.
 */
void SpellingWidgetController::setPageMetrics(const IntPoint& scroll, float scale, int screenWidth,int screenHeight)
{
    if (m_pageScroll != scroll || m_pageScale != scale || m_screenWidth != screenWidth || m_screenHeight != screenHeight) {
		// We invalidate first so that we can calculate the current widget position in 
		// document coordinates before we change values on which this math relies.
		invalidate();

		m_pageScroll = scroll;
		m_pageScale = scale;
		m_screenWidth = screenWidth;
        m_screenHeight = screenHeight;

        IntRect newWordRect = m_page->view()->wordBoundsAroundCaret(core(m_page)->focusController()->focusedOrMainFrame());
        if (!newWordRect.isEmpty())
            m_wordRect = newWordRect;

        if (isVisible() && m_wordRect.isEmpty())
           hide();

        // Now invalidate a second time to get the widget to draw at it's new position.
        updateWidgetRect();
        invalidate();
	}
}

void SpellingWidgetController::documentToScreen(WebCore::IntPoint& pt) const
{
	pt.setX((pt.x() - m_pageScroll.x()) * m_pageScale );
	pt.setY((pt.y() - m_pageScroll.y()) * m_pageScale );
}

void SpellingWidgetController::documentToScreen(WebCore::IntRect& r) const
{
	int docWidth = r.width();
	int docHeight = r.height();

	r.setX((r.x() - m_pageScroll.x()) * m_pageScale );
	r.setY((r.y() - m_pageScroll.y()) * m_pageScale );
	r.setWidth(docWidth * m_pageScale);
	r.setHeight(docHeight * m_pageScale);
}

void SpellingWidgetController::screenToDocument(WebCore::IntPoint& pt) const
{
	if (m_pageScale != 0.0f) {
		pt.setX(pt.x() / m_pageScale + m_pageScroll.x());
		pt.setY(pt.y() / m_pageScale + m_pageScroll.y());
	}
}

void SpellingWidgetController::screenToDocument(WebCore::IntRect& r) const
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

void SpellingWidgetController::startAnimationTimer()
{
	if (m_animationTimer != 0) return;

	m_animationTimer = g_timeout_source_new(ANIMATION_TIMEOUT_MS);
	g_source_set_callback(m_animationTimer, (GSourceFunc)animateCb, this, NULL);
	g_source_attach(m_animationTimer, g_main_loop_get_context(Palm::WebGlobal::mainLoop()));
}

void SpellingWidgetController::stopAnimationTimer()
{
	if (m_animationTimer != 0)
	{
		g_source_destroy(m_animationTimer);
		g_source_unref(m_animationTimer);
		m_animationTimer = 0;
	}
}


gboolean SpellingWidgetController::animateCb(gpointer data) 
{
	SpellingWidgetController* controller = static_cast<SpellingWidgetController*>(data);
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


/*
 * Flick scroller implementation
 */
SpellingWidgetController::FlickScroller::FlickScroller(SpellingWidgetController* controller)
	: m_controller(controller)
	, m_dStartSpeed(0.0)
	, m_dDuration(0.0)
	, m_direction(1)
	, m_flickTimer(0)
	, m_curFlickDuration(0.0)
{}

SpellingWidgetController::FlickScroller::~FlickScroller()
{
	stopFlickTimer();
}

bool SpellingWidgetController::FlickScroller::initialize(double speed)
{
	if (std::abs(speed) < MIN_FLICK_SPEED)
		return false;

	stopFlickTimer();
	m_direction = speed < 0 ? -1 : 1;
	m_dStartSpeed = std::abs(speed / SPEED_DIVIDER);
	m_dDuration = m_dStartSpeed / ACCELERATION;
	return true;
}

int SpellingWidgetController::FlickScroller::getPosition(double t) const
{
	if (t > m_dDuration)
		t = m_dDuration;

	int distance = m_dStartSpeed * t - (ACCELERATION/2) * t*t;
	if (distance < 0)
		distance = 0;

	return m_direction * distance;
}

void SpellingWidgetController::FlickScroller::startFlickTimer()
{
	m_curFlickDuration = 0.0;
	if (m_flickTimer != 0) return;

	m_flickTimer = g_timeout_source_new(FLICK_TIMEOUT_MS);
	g_source_set_callback(m_flickTimer, (GSourceFunc)flickCb, m_controller, NULL);
	g_source_attach(m_flickTimer, g_main_loop_get_context(Palm::WebGlobal::mainLoop()));
}

void SpellingWidgetController::FlickScroller::stopFlickTimer()
{
	m_curFlickDuration = 0.0;
	if (m_flickTimer != 0) {
		g_source_destroy(m_flickTimer);
		g_source_unref(m_flickTimer);
		m_flickTimer = 0;
	}
}

bool SpellingWidgetController::FlickScroller::isFlicking()
{
	return m_flickTimer != 0;
}

gboolean SpellingWidgetController::FlickScroller::flickCb(gpointer data)
{
	SpellingWidgetController* controller = static_cast<SpellingWidgetController*>(data);
	controller->m_flickScroller.m_curFlickDuration += SpellingWidgetController::FlickScroller::FLICK_TIMEOUT_SEC;
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
