
#include "config.h"
#include "ScrollView.h"

#include "ChromeClient.h"
#include "Chrome.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HostWindow.h"
#include "IntRect.h"
#include "NotImplemented.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
//#include "ScrollbarGtk.h"
#include "ScrollbarTheme.h"

#include "webkitpalmsettings.h"
#include "palmwebview.h"
#include "palmwebpage.h"
#include "palmwebpageprivate.h"
#include "palmwebviewclient.h"
#include "palmwebframeprivate.h"


/*
	How Scaling Works.

	ScrollView::ScrollViewPrivate owns the current page magnification (scaling). It is set
	into the graphics context when a render is requested. To the internal side (webcore), the
	page is **NOT** scaled, but always 1:1.  To the external side (webkit clients, clients of
	ScrollViewPalm), the contents size changes when any of the magnification functions are
	called.

	Note that this allows us to implement scaling without modifying WebKit itself.

 */


using namespace std;

namespace WebCore {

class ScrollView::ScrollViewPrivate
{
public:

	ScrollViewPrivate(ScrollView* _view)
	: view(_view)
	, scaleFactor(1.0)
	, scaleFactorMin( 0.01 )
	, scaleFactorMax( 16.0 ) // FIXME : make this reasonable.
	, fittingPageWidth( PalmBrowserSettings()->enableFitWidth )
	, overrideVirtualSize(-1,-1)
	, scaleControlEnable(true)
	{}

	~ScrollViewPrivate()
	{
	}

	void reset()
	{
		scrollOffset = IntSize(0,0);
		overrideVirtualSize = IntSize(-1,-1);
		scaleControlEnable = true;
		fittingPageWidth= PalmBrowserSettings()->enableFitWidth;
	}

	ScrollView* view;
	IntSize scrollOffset;       // in unscaled coordinate system

	//This is simply m_contentsSize in ScrollView.h
	//IntSize sourceContentsSize; // in unscaled coordinate system, sourceContentsSize == scaledContentSize @ 100%, 1.0 scale factor

	IntSize scaledContentsSize; // in scaled coordinate system
	//IntSize viewPortSize;
	HashSet<Widget*> children;
	IntSize overrideVirtualSize;
	bool scaleControlEnable;

    IntRect viewport;

	// NOTE : ScrollView owns the scroll factor for the given page. Since there is a tree of page views,
	// scale factors need to be propagated *down* to children.
	double scaleFactor;		// 1.0 == 100% magnification
	double scaleFactorMin;	// computed based on contents size.
	double scaleFactorMax;	// pre-determined max ?
	bool fittingPageWidth;	// if true, then we should re-fit the page everytime contentsSize changes (on the WebCore side)
};

void ScrollView::platformInit()
{
	m_maxVisibleHeightWhenScaled = 0;
	m_data = new ScrollViewPrivate(this);
	setPlatformWidget(this);
}

void ScrollView::platformDestroy()
{
	delete m_data;
}

IntRect ScrollView::actualVisibleContentRect() const
{
	return IntRect(m_scrollOffset.width(), m_scrollOffset.height(),
	        viewport().width(), viewport().height());
}

int ScrollView::visibleWidth() const
{
	//int scrollWidth = (m_data->vBar ? m_data->vBar->width() : 0);

	if( m_data->overrideVirtualSize.width() != -1 )
		return m_data->overrideVirtualSize.width();

	if (parent()) {
		// subframe case
		return width();// - scrollWidth;
	}

	Palm::WebView* view = containingWindow();
	if (!view)
		return 0;

	Palm::WebViewClient* viewClient = view->client();
	if (!viewClient)
		return 0;

	int w, h;
	viewClient->getVirtualWindowSize(w, h);
	if( m_data->scaleFactor != 1.0 )
		return std::max( (int)w, (int)m_contentsSize.width() );// - scrollWidth;
	else
		return w;// - scrollWidth;

}

int ScrollView::visibleHeight() const
{
	//int scrollHeight = (m_data->hBar ? m_data->hBar->height() : 0);

	if( m_data->overrideVirtualSize.height() != -1 )
		return m_data->overrideVirtualSize.height();

	if (parent()) {
		// subframe case
		return height();
	}

	Palm::WebView* view = containingWindow();
	if (!view)
		return 0;

	Palm::WebViewClient* viewClient = view->client();
	if (!viewClient)
		return 0;

	int w, h;
	viewClient->getVirtualWindowSize(w, h);

	if (m_maxVisibleHeightWhenScaled)
		h = std::min(m_maxVisibleHeightWhenScaled, h);

	if( m_data->scaleFactor != 1.0 )
		return std::max( (int)h, (int)m_contentsSize.height() );// - scrollHeight;
	else
		return h;// - scrollHeight;
}

// setting m_maxVisibleHeightWhenScaled makes sense only
// when default zoom factor is != 1.0
void ScrollView::setMaxVisibleHeightWhenScaled(int val)
{
    if (PalmBrowserSettings()->defaultZoomFactor != 1.0)
        m_maxVisibleHeightWhenScaled = val;
}

void ScrollView::resizeViewport(int w, int h)
{
     m_data->viewport = IntRect(x(), y(), w, h);
}

IntRect ScrollView::viewport() const
{
    return m_data->viewport;
}

void ScrollView::setFixVirtualWidth( int width )
{
	Palm::WebView* view = containingWindow();
	if (!view) {
		return;
	}

	Palm::WebViewClient* viewClient = view->client();
	if (!viewClient) {
		return;
	}

	int deviceWidth, deviceHeight;
	viewClient->getScreenSize(deviceWidth, deviceHeight);

	m_data->overrideVirtualSize.setWidth(MAX(width, deviceWidth));
}

void ScrollView::setFixVirtualHeight( int height )
{
	Palm::WebView* view = containingWindow();
	if (!view) {
		return;
	}

	Palm::WebViewClient* viewClient = view->client();
	if (!viewClient) {
		return;
	}

	int deviceWidth, deviceHeight;
	viewClient->getScreenSize(deviceWidth, deviceHeight);
	m_data->overrideVirtualSize.setHeight(MAX(height, deviceHeight));
}

void ScrollView::setScaleAndScroll( double mag, int newX, int newY )
{
	IntSize newScrollOffset;

	if( !m_data->scaleControlEnable )
		return;

	m_data->fittingPageWidth = false;

	// If we're already at page view, then set this as the "minimum" page view.
	if (mag < m_data->scaleFactorMin)
		return;

	if (mag > m_data->scaleFactorMax)
		return;

	m_data->scaleFactor = mag;
	m_data->scaledContentsSize.setWidth( (int)(  m_data->scaleFactor * m_contentsSize.width()  + 0.5) );
	m_data->scaledContentsSize.setHeight( (int)( m_data->scaleFactor * m_contentsSize.height() + 0.5) );
	// TODO : new scroll
	int dx = newX - scrollOffset().width();
	int dy = newY - scrollOffset().height();
	{
		IntSize scrollOffset = m_scrollOffset;
		newScrollOffset = (scrollOffset + IntSize(dx, dy)).shrunkTo(maximumScroll());
		newScrollOffset.clampNegativeToZero();

		// We don't actually want to change the scrollOffset
	}

	// update child ScrollViews.
	frameRectsChanged();

	Palm::WebView* view = containingWindow();
	if (view && view->client()) {
		view->client()->zoomedContents( m_data->scaleFactor,
				m_data->scaledContentsSize.width(),
				m_data->scaledContentsSize.height(),
				(newScrollOffset.width()  * m_data->scaleFactor),
				(newScrollOffset.height() * m_data->scaleFactor));
	}

	// Send invalidated regions to client
	update();

}

double ScrollView::getScale(  ) const
{
	if (parent()) {
		ScrollView* p = static_cast<ScrollView*>(parent());
		return p->getScale();
	}

	return m_data->scaleFactor;
}

void ScrollView::setScale( double mag, bool sendUpdates )
{
	if( !m_data->scaleControlEnable )
		return;

	setScaleInternal(mag,sendUpdates);
}

void ScrollView::setInitialScale( double mag )
{
	setScaleInternal( mag );
}

void ScrollView::setScaleInternal( double mag, bool sendUpdates )
{

	m_data->fittingPageWidth = false;

	// If we're already at page view, then set this as the "minimum" page view.
	if (mag < m_data->scaleFactorMin)
		mag = m_data->scaleFactorMin;

	if (mag > m_data->scaleFactorMax)
		mag = m_data->scaleFactorMax;

	m_data->scaleFactor = mag;
	m_data->scaledContentsSize.setWidth( (int)(  m_data->scaleFactor * m_contentsSize.width()  + 0.5) );
	m_data->scaledContentsSize.setHeight( (int)( m_data->scaleFactor * m_contentsSize.height() + 0.5) );

	if( sendUpdates )
	{
		frameRectsChanged();
		update();

		Palm::WebView* view = containingWindow();
		if (view && view->client()) {
			view->client()->zoomedContents( m_data->scaleFactor,
					m_data->scaledContentsSize.width(),
					m_data->scaledContentsSize.height(),
					( m_scrollOffset.width()  * m_data->scaleFactor),
					( m_scrollOffset.height()  * m_data->scaleFactor)
			);
		}
	}
}

void ScrollView::setMinScale( float scale )
{
	m_data->scaleFactorMin = scale;
}

void ScrollView::setMaxScale( float scale )
{
	m_data->scaleFactorMax = scale;
}

void ScrollView::setUserCanScale( bool bEnable )
{
	m_data->scaleControlEnable = false;
}

bool ScrollView::isFitWidth( )
{
	return m_data->fittingPageWidth;
}

double ScrollView::fitWidth( )
{
	// TODO : Set scroll offset to 0,0

 	setScale( m_data->scaleFactorMin );
	m_data->fittingPageWidth = true;
	return m_data->scaleFactor;
}

void ScrollView::scrollBy(const IntSize& s)
{
    IntSize scrollOffset = m_scrollOffset;
    IntSize newScrollOffset = (scrollOffset + s).shrunkTo(maximumScroll());
    newScrollOffset.clampNegativeToZero();

    // We don't want to actually change the scrollOffset, but this is where we would

   Palm::WebView* view = containingWindow();
	if (view && view->client()) {
        view->client()->scrolledContents((int) (newScrollOffset.width()  * m_data->scaleFactor),
										 (int) (newScrollOffset.height() * m_data->scaleFactor));
    }

    // RR (11/30/07): When scrolling the expose paint is triggered from above,
    // instead of from here.
    // update();

    //updateScrollbars(newScrollOffset);
}

IntSize ScrollView::scrollOffset() const
{
    return IntSize();
}

IntSize ScrollView::actualScrollOffset() const
{
    return m_scrollOffset;
}

IntSize ScrollView::maximumScroll() const
{
	// we scroll based on the unscaled coordinates
    IntSize delta = m_contentsSize - scrollOffset();

/*  This code is just wrong.
    Palm::WebView* view = containingWindow();
    if (view && view->client()) {
        int width, height;

        view->client()->getWindowSize(width, height);

        // Adjust containing window for zoom to get the real maximum
        width  = (int) (width  / m_data->scaleFactor);
        height = (int) (height / m_data->scaleFactor);

        delta -= IntSize(width, height);
    }
*/

    delta.clampNegativeToZero();
    return delta;
}

// This call comes from WebCore at unscaled coordinates.
void ScrollView::resizeContents(int w, int h)
{
    resizeContents(IntSize(w, h));
}

void ScrollView::resizeContents(const IntSize& newSize)
{
    Palm::WebView* view = containingWindow();
    bool topMost = isTopMostScrollView();
    // When the page initially loads the content size is set to zero. we want to
    // send this info up to client so that it can reset itself
    if ((m_contentsSize.width() == 0 || m_contentsSize.height() == 0) && view && view->client() && topMost) {
        Palm::WebViewClient* viewClient = view->client();
        viewClient->zoomedContents(1.0, 0, 0, 0, 0);
    }

    int w = newSize.width();
    int h = newSize.height();
    m_contentsSize = newSize;

	if (view && view->client()) {

		Palm::WebViewClient* viewClient = view->client();

        int win_height, win_width;
        viewClient->getWindowSize(win_width, win_height);
		/*
		// recompute scale min. We want "min" to fit the width of the window.
		// It might make more sense to make it the "best fit" of the whole page,
		// based on the aspect ratio of the LCD screen.
        if (w != 0) {
            m_data->scaleFactorMin = ((float)win_width) / (float)w;
        }

		// we're automatically adjusting the scale to fit page width, so update accordingly.
		if( m_data->fittingPageWidth )
			m_data->scaleFactor = m_data->scaleFactorMin;
		*/

		// recompute scale min. We want "min" to fit the width of the window.
		// if the page height is too small to fill up the contents of the window
		// (happens when content width is larger than the virtual width), we will
		// expand the height to maintain the aspect ratio of the window.
		if (w != 0) {

			float scaleW = ((float)win_width) / (float) w;
			float scaleH = ((float)win_height) / (float) h;
            m_data->scaleFactorMin = scaleW;
			/*
			if (scaleH > scaleW && !parent()) {
				h = (int) (win_height / scaleW + 0.5);
				m_contentsSize.setHeight(h);

				fprintf(stderr, "virtual page height too small. expanding height...\n");
				fprintf(stderr, "original h: %d, new h: %d\n", newSize.height(), h);
			}
			*/
		}

		// we're automatically adjusting the scale to fit page width, so update accordingly.
		if( m_data->fittingPageWidth )
			m_data->scaleFactor = m_data->scaleFactorMin;
	}

	m_data->scaledContentsSize.setWidth( (int)( m_data->scaleFactor * w + 0.5 ) );
	m_data->scaledContentsSize.setHeight( (int)( m_data->scaleFactor * h + 0.5 ) );

    updateScrollbars(m_scrollOffset);

    if (w == 0 || h == 0) {
		// GREG : This is where the scroll offset is reset to "zero" for a new page load.
		// this is also where we want to start "snapping the zoom to page width" for a new
		// page load. We may want to turn this off based on what the UI guys say.
        m_data->reset();
    }

    // send the new contents size to window client only if we are top level scrollview
    if (view && view->client() && topMost) {

		Palm::WebViewClient* viewClient = view->client();

		viewClient->zoomedContents(m_data->scaleFactor,
								   m_data->scaledContentsSize.width(),
								   m_data->scaledContentsSize.height(),
								   ( m_scrollOffset.width()  * m_data->scaleFactor),
								   ( m_scrollOffset.height()  * m_data->scaleFactor)
								  );
    }
}

void ScrollView::paintAt( GraphicsContext* context, const IntRect& rect)
{
	notImplemented();
}

/**
 * This paint routine is used by <b>LunaSysMgr/QtPalmLauncher</b> which <b>do not scale</b> when
 * drawing.
 */
void ScrollView::paintContents(GraphicsContext* context, const IntRect& rect, int cX, int cY)
{
    ASSERT(isFrameView());

    if (context->paintingDisabled())
        return;

    IntRect documentDirtyRect = rect;
    //documentDirtyRect.intersect(frameRect());

    context->save();

    context->translate(x(), y());
    documentDirtyRect.move(-x(), -y());

    context->translate(-cX, -cY);
    documentDirtyRect.move(cX, cY);

    paintContents(context, documentDirtyRect);
    context->restore();
}

/**
 * This paint routine is used by <b>BrowserServer</b> which <b>does scale</b> when drawing.
 */
void ScrollView::paintContentsScaled(GraphicsContext* const context, const IntPoint& scaledPos,
                                     const IntRect& scaledClipRect)
{
    ASSERT(isFrameView());

    if (context->paintingDisabled())
        return;

    float cX = scaledPos.x() / m_data->scaleFactor;
    float cY = scaledPos.y() / m_data->scaleFactor;

    int dLeft   = (int) (scaledClipRect.x() / m_data->scaleFactor);
    int dTop    = (int) (scaledClipRect.y() / m_data->scaleFactor);
    int dRight  = (int) ((scaledClipRect.x() + scaledClipRect.width())  / m_data->scaleFactor + 0.5);
    int dBottom = (int) ((scaledClipRect.y() + scaledClipRect.height()) / m_data->scaleFactor + 0.5);

    dLeft   = std::max(0, dLeft - 1);
    dTop    = std::max(0, dTop  - 1);
    dRight  = std::min(contentsWidth(),  dRight  + 1);
    dBottom = std::min(contentsHeight(), dBottom + 1);

    IntRect documentDirtyRect(dLeft, dTop, dRight - dLeft, dBottom - dTop);
    //documentDirtyRect.intersect(frameRect());

    if (documentDirtyRect == IntRect()) {
        g_warning("WebKit called with an empty dirty rect (empty clip rect or no contents)\n");
        return;
    }

    documentDirtyRect.move(-x(), -y());
    documentDirtyRect.move((int)cX, (int)cY);

    context->save();

	context->scale(FloatSize(m_data->scaleFactor, m_data->scaleFactor));
    context->translate(x() - cX, y() - cY);

    paintContents(context, documentDirtyRect);
    context->restore();
}

void ScrollView::update()
{
    //int scrollWidth = (	m_data->vBar ? m_data->vBar->width() : 0);
    //int scrollHeight = (m_data->hBar ? m_data->hBar->height() : 0);

	Palm::WebView* view = containingWindow();
	if (!view)
		return;

	if (!parent()) {

        Palm::WebPagePrivate::core(view->page())->chrome()->client()->invalidateContentsAndWindow(
                IntRect(0, 0, m_data->scaledContentsSize.width(), m_data->scaledContentsSize.height()), false);
	}
	else {

		parent()->update();
	}
}

bool ScrollView::isTopMostScrollView()
{
    Palm::WebView* view = containingWindow();
    if (view && view->page() && view->page()->mainFrame()
        && core(view->page()->mainFrame())
        && core(view->page()->mainFrame())->view() == this)
        return true;

    return false;
}

void ScrollView::platformSetScrollbarModes()
{

}

void ScrollView::platformScrollbarModes(ScrollbarMode& horizontal, ScrollbarMode& vertical) const
{

}

void ScrollView::platformSetCanBlitOnScroll(bool b) {
	m_canBlitOnScroll = b;

    // We shouldn't inform client if a child scrollview (frame/iframe could be non-scroll-blittable
    // but we don't care since we don't scroll child scrollviews)
    if (parent())
        return;

    Palm::WebView* view = containingWindow();
    if (view && view->client())
        view->client()->setCanBlitOnScroll(b);
}

bool ScrollView::platformCanBlitOnScroll() const {
	return m_canBlitOnScroll;
}

IntRect ScrollView::platformVisibleContentRect(bool includeScrollbars) const {
    if (PalmBrowserSettings()->runningInBrowserServer) {
        return IntRect(m_scrollOffset.width(), m_scrollOffset.height(),
            m_contentsSize.width(),
            m_contentsSize.height());
    } else {
        return IntRect(0, 0, m_contentsSize.width(), m_contentsSize.height());
    }
}

// This call comes from WebCore and wants unscaled coordinates
IntSize ScrollView::platformContentsSize() const {
	return m_contentsSize;
}

void ScrollView::platformSetContentsSize() {
	resizeContents(m_contentsSize.width(), m_contentsSize.height());
}

IntRect ScrollView::platformContentsToScreen(const IntRect& rect) const {
	if (!hostWindow())
		return IntRect();
	return hostWindow()->windowToScreen(contentsToWindow(rect));
}

IntPoint ScrollView::platformScreenToContents(const IntPoint& point) const {
	if (!hostWindow())
		return IntPoint();
	return windowToContents(hostWindow()->screenToWindow(point));
}

void ScrollView::platformSetScrollPosition(const IntPoint& scrollPoint, bool alertClient) {
	IntSize newScrollOffset = IntSize(scrollPoint.x(), scrollPoint.y()).shrunkTo(maximumScroll());
	newScrollOffset.clampNegativeToZero();

	if (PalmBrowserSettings()->runningInBrowserServer)
	    m_scrollOffset = newScrollOffset;

	// The ScrollView doesn't actually scroll, but only tells the view to start painting a new region
	Palm::WebView* view = containingWindow();
	if (view && view->client() && alertClient) {
		view->client()->scrolledContents((int) (newScrollOffset.width()  * m_data->scaleFactor),
										 (int) (newScrollOffset.height() * m_data->scaleFactor));
	}
}

bool ScrollView::platformScroll(ScrollDirection, ScrollGranularity) {
	// We don't currently support scrolling with a granularity
	return false;
}

void ScrollView::platformRepaintContentRectangle(const IntRect& rect, bool now) {
	if (hostWindow()) 
		hostWindow()->invalidateWindow(rect, now);
}

bool ScrollView::platformIsOffscreen() const {
	if (!isVisible())
		return true;

	// FIXME: Add a HostWindow::isOffscreen method here.  Since only Mac implements this method
	// currently, we can add the method when the other platforms decide to implement this concept.
	return false;
}

void ScrollView::platformFrameRectsChanged() {
	// propagate scale changes to all frames, so the "page" is scaled evenly.
	ScrollView* parentView = dynamic_cast<ScrollView*>( parent() );
	if( parentView )
	{
		m_data->scaleFactor = parentView->m_data->scaleFactor;

		// NOTE :  I think only the master scroll view needs to have these.
		//m_data->scaleFactorMin = parentView->m_data->scaleFactorMin;
		//m_data->scaleFactorMax = parentView->m_data->scaleFactorMax;
		//m_data->fittingPageWidth = parentView->m_data->fittingPageWidth;
	}
}

void ScrollView::platformAddChild(Widget* child)
{
    child->setContainingWindow(containingWindow());
}

void ScrollView::platformRemoveChild(Widget* child)
{
}

}
