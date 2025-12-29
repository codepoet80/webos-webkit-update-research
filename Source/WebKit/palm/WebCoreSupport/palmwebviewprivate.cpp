/*
 * palmwebviewprivate.cpp
 *
 *  Created on: Sep 23, 2010
 *      Author: lweintraub
 */

#include "palmwebviewprivate.h"
#include "LayerRendererPalm.h"
#include "FrameView.h"
#include "IntRect.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "palmwebpage.h"
#include "palmwebframe.h"
#include "palmwebframeprivate.h"
#include "palmwebpageprivate.h"
#include "palmwebviewclient.h"
#include "Frame.h"
#include "PGContext.h"
#include "GLES2Context.h"
#include "GLES2ContextInternal.h"
#include <webkitstats.h>
#include <SimpleStats/SimpleStats.h>

using namespace WebCore;

namespace Palm {

WebViewPrivate::WebViewPrivate(WebView* parent)
    : m_parent(parent)
#if USE(ACCELERATED_COMPOSITING)
    , m_isAcceleratedCompositingActive(false)
#endif
    , m_paintCount(0)
{
    // TODO Auto-generated constructor stub
}

WebViewPrivate::~WebViewPrivate() {
    // TODO Auto-generated destructor stub
}

#if USE(ACCELERATED_COMPOSITING)
void WebViewPrivate::setRootGraphicsLayer(WebCore::PlatformLayer* layer)
{
    setIsAcceleratedCompositingActive(layer ? true : false);

    if (m_layerRenderer)
        m_layerRenderer->setRootLayer(layer);
}

void WebViewPrivate::setIsAcceleratedCompositingActive(bool active)
{
    if (m_isAcceleratedCompositingActive == active)
        return;

    if (active) {
        m_layerRenderer = LayerRendererPalm::create(getOnscreenGLES2Context());
        if (m_layerRenderer->hardwareCompositing())
            m_isAcceleratedCompositingActive = true;
        else {
            m_layerRenderer.clear();
            m_isAcceleratedCompositingActive = false;
        }
    } else {
        m_layerRenderer = 0;
        m_isAcceleratedCompositingActive = false;
    }
}

WebCore::Page* WebViewPrivate::page() const
{
	return Palm::WebPagePrivate::core(m_parent->page());
}

/**
 * Recomposite the view and optionally update the root layer if specified.
 *
 * @note This routine is originally from Chrome's implementation (WebViewImpl.cpp).
 *
 * @param updateRect   The portion of the document to composite. If not updating layers
 *                     (i.e. updateLayers=false) then topLeft() is the scroll position
 *                     and the width/height is ignored.
 *
 * @param updateLayers true to update the layers before compositing.
 * @param mayScale     true of this function may scale the contents when drawing.
 */
void WebViewPrivate::doComposite(const WebCore::IntRect& updateRect, bool updateLayers, bool mayScale)
{
    ASSERT(isAcceleratedCompositingActive());
    ASSERT(m_layerRenderer != 0);
    if (!page())
        return;
    FrameView* view = page()->mainFrame()->view();

    SimpleStats::Event evt(WebKitStats::getAcDoCompositeEventData());
    webOS::Reporter::Event acEvent(this, "webkit.compositing.doComposite", "updateRect: %d x %d @(%d, %d), updateLayers: %c, mayScale: %c",
            updateRect.width(), updateRect.height(), updateRect.x(), updateRect.y(), updateLayers ? 'Y' : 'N', mayScale ? 'Y' : 'N');

    IntPoint scrollPosition(view->scrollPosition());
    int screenWidth, screenHeight;
    m_parent->client()->getScreenSize(screenWidth, screenHeight);
#if PLATFORM(WEBOS)
    IntRect visibleRect(scrollPosition.x(), scrollPosition.y(), screenWidth, screenHeight);
    IntRect contentRect(visibleRect);
    IntRect viewPort = IntRect(0, 0, view->width(), view->height());
#else
    // The visibleRect includes scrollbars whereas the contentRect doesn't.
    IntRect visibleRect = view->visibleContentRect(true);
    IntRect contentRect = view->visibleContentRect(false);
    IntRect viewPort = IntRect(0, 0, m_size.width, m_size.height);
#endif

    m_layerRenderer->prepareToDrawLayers(visibleRect, contentRect, scrollPosition);

    if (updateLayers) {
        IntRect damagedRect = visibleRect;

        // Intersect this rectangle with the viewPort.
        damagedRect.intersect(viewPort);

        if (!damagedRect.isEmpty()) {
            updateRootLayerContents(damagedRect, mayScale);
            // Accelerated compositing may have gotten turned off. check again
            if (m_layerRenderer)
                m_layerRenderer->updateRootLayerTextureRect(damagedRect);
        }
    } else if (view) {
        view->layoutIfNeededRecursive();
    }

    // If accelerated compositing gets turned off due to layout above, bail out
    if (m_layerRenderer == 0) {
        core(m_parent->page())->chrome()->client()->invalidateContentsAndWindow(
                IntRect(updateRect.x(), updateRect.y(), updateRect.width(), updateRect.height()), false);
        return;
    }

    // Draw the actual layers...
    scrollPosition = IntPoint();
    m_layerRenderer->drawLayers(visibleRect, contentRect, scrollPosition);

    m_layerRenderer->drawTopLayer(m_parent, updateRect);
}

/**
 * Update (repaint) the contents of the root layer.
 *
 * @note This routine is originally from Chrome's implementation (WebViewImpl.cpp).
 *
 * @param rect The portion of the page to update (in document coordinates).
 * @param mayScale  true of this function may scale the contents when drawing.
 */
void WebViewPrivate::updateRootLayerContents(const IntRect& rect, bool mayScale)
{
    if (!isAcceleratedCompositingActive())
        return;


    WebFrame* webframe = m_parent->page()->mainFrame();
    if (!webframe)
        return;
    FrameView* view = core(webframe)->view();
    if (!view)
        return;

    LayerPalm* rootLayer = m_layerRenderer->rootLayer();
    if (rootLayer) {
#if PLATFORM(WEBOS)
		IntPoint scrollPosition(view->scrollPosition());
    	IntRect visibleRect(scrollPosition.x(), scrollPosition.y(), view->width(), view->height());
#else
        IntRect visibleRect = view->visibleContentRect(true);
#endif

        m_layerRenderer->setRootLayerCanvasSize(IntSize(rect.width(), rect.height()));
        GraphicsContext* rootLayerContext = m_layerRenderer->rootLayerGraphicsContext();

#if PLATFORM(SKIA)
        PlatformContextSkia* skiaContext = rootLayerContext->platformContext();
        skia::PlatformCanvas* platformCanvas = skiaContext->canvas();

        platformCanvas->save();

        // Bring the canvas into the coordinate system of the paint rect.
        platformCanvas->translate(static_cast<SkScalar>(-rect.x()), static_cast<SkScalar>(-rect.y()));

        rootLayerContext->save();

        webframe->paintWithContext(*rootLayerContext, rect);
        rootLayerContext->restore();

        platformCanvas->restore();
#elif PLATFORM(CG)
        CGContextRef cgContext = rootLayerContext->platformContext();

        CGContextSaveGState(cgContext);

        // Bring the CoreGraphics context into the coordinate system of the paint rect.
        CGContextTranslateCTM(cgContext, -rect.x(), -rect.y());

        rootLayerContext->save();

        webframe->paintWithContext(*rootLayerContext, rect);
        rootLayerContext->restore();

        CGContextRestoreGState(cgContext);
#elif PLATFORM(PG)
        PGContext* pgContext = rootLayerContext->platformContext();
        PGSurface* pgSurface = m_layerRenderer->rootLayerBackingSurface();

        // paintWithContext triggers relayout when necessary and this can potentially
        // turn off accelerated compositing which will invalidate the PG pointers.
        // so safeguard by refing these here
        pgContext->addRef();
        pgSurface->addRef();

        pgContext->push();
        pgContext->translate(-rect.x(), -rect.y());
        paintWithContext(pgContext, rect, mayScale);
        pgContext->pop();

        pgContext->releaseRef();
        pgSurface->releaseRef();
#else
#error Must port to your platform
#endif
    }
}

void WebViewPrivate::setRootLayerNeedsDisplay()
{
    // FIXME: For now we're posting a repaint event for the entire page which is an overkill.
    if (Frame* frame = core(m_parent->m_page->mainFrame())) {
        if (FrameView* view = frame->view()) {
            // FIXME: Temporary hack to invalidate part of the page so that we get called to render
            //        again.
            core(m_parent->page())->chrome()->client()->invalidateContentsAndWindow(IntRect(0, 0, 1, 1), false);
        }
    }
}
#endif // USE(ACCELERATED_COMPOSITING)

/**
 * Paint the view using a graphics context.
 *
 * @param context   The graphics context to use for drawing.
 * @param paintRect The portion of the page to draw.
 * @param mayScale  true of this function may scale the contents when drawing.
 */
void WebViewPrivate::paintWithContext(PGContext* context, const IntRect& paintRect, bool mayScale)
{
    Frame* frame = core(m_parent->m_page->mainFrame());
    if (!frame)
        return;

    context->push();
	
	GraphicsContext ctxt(context);
	
	if (frame->view()) {
		ScrollView* sv = static_cast<ScrollView*>(frame->view());

        frame->view()->layoutIfNeededRecursive();

		IntRect  clipRect(0, 0, paintRect.width(), paintRect.height());

		if (mayScale)
			sv->paintContentsScaled(&ctxt, paintRect.topLeft(), clipRect);
		else
			sv->paintContents(&ctxt, clipRect, paintRect.x(), paintRect.y());
    }
	
	context->pop();
}

#if USE(GLES2_RENDERING)
// Returns the GLES2 context associated with this View. If one doesn't exist
// it will get created first.
WebGLES2Context* WebViewPrivate::gles2Context()
{
    if (!m_gles2Context) {
        m_gles2Context = m_parent->client()->createGLES2Context();
        if (!m_gles2Context)
            return 0;

        if (!m_gles2Context->initialize(m_parent, 0)) {
            m_gles2Context.clear();
            return 0;
        }

#if OS(DARWIN)
        m_gles2Context->resizeOnscreenContent(WebSize(std::max(1, m_size.width),
                                                      std::max(1, m_size.height)));
#endif
    }
    return m_gles2Context.get();
}

PassOwnPtr<GLES2Context> WebViewPrivate::getOnscreenGLES2Context()
{
    return GLES2Context::create(GLES2ContextInternal::create(gles2Context(), false));
}

PassOwnPtr<GLES2Context> WebViewPrivate::getOffscreenGLES2Context()
{
    WebGLES2Context* context = m_parent->client()->createGLES2Context();
    if (!context)
        return 0;
    if (!context->initialize(0, gles2Context())) {
        delete context;
        return 0;
    }
    return GLES2Context::create(GLES2ContextInternal::create(context, true));
}

bool WebViewPrivate::isAcceleratedCompositingActive() const
{
#if USE(ACCELERATED_COMPOSITING)
    return m_isAcceleratedCompositingActive;
#else
    return false;
#endif
}

#endif // USE(GLES2_RENDERING)

}
