#include "config.h"

#include "SelectionMarkersWidget.h"
#include "palmwebpage.h"
#include "palmwebview.h"
#include "palmwebviewclient.h"
#include "PGContext.h"
#include "palmwebpageprivate.h"
#include "palmwebglobal.h"
#include "webkitpalmsettings.h"

#include "PlatformMouseEvent.h"
#include "ColorSpace.h"
#include "RefPtr.h"
#include "Image.h"
#include "Frame.h"
#include "FocusController.h"

#include <string>

using namespace WebCore;

//#define DEBUG_CCP 1
#ifdef DEBUG_CCP
#define ccpLog(...) do { printf("(ccp: %s) ", __FUNCTION__); printf(__VA_ARGS__); printf("\n"); } while (0)
#else
#define ccpLog(...) do {} while (0)
#endif

namespace Palm {

const std::string SelectionMarkersWidget::s_topMarkerImgPath = "images/topmarker";
const std::string SelectionMarkersWidget::s_bottomMarkerImgPath = "images/bottommarker";

// returns the rect around the markers increased by tap target size
static IntRect getTapTargetRect(const IntRect& rect, double zoom = 1.0)
{
    if (rect.isEmpty())
        return IntRect();

    IntRect tapRect = rect;
    int targetWidth = PalmBrowserSettings()->selectionMarkersTapTargetWidth * zoom;
    int targetHeight = PalmBrowserSettings()->selectionMarkersTapTargetHeight * zoom;
    if (rect.width() < targetWidth)
        tapRect.inflateX((targetWidth - rect.width()) / 2 + 0.5);
    if (rect.height() < targetHeight)
        tapRect.inflateY((targetHeight - rect.height()) / 2 + 0.5);

    return tapRect;
}

SelectionMarkersWidget::SelectionMarkersWidget(Palm::WebPage* page)
    : m_page(page)
    , m_markersEnabled(false)
{
}

SelectionMarkersWidget::~SelectionMarkersWidget()
{
    const IntRect empty;
    setMarkerRects(empty, empty);
}

void SelectionMarkersWidget::paint(GraphicsContext& ctxt, int contentX, int contentY) const
{
    if (!m_topMarkerRect.isEmpty() || !m_bottomMarkerRect.isEmpty()) {

        Image* topMarker = getTopMarkerImage();
        Image* bottomMarker = getBottomMarkerImage();
        IntPoint topMarkerPosition = m_topMarkerRect.location();
        IntPoint bottomMarkerPosition = m_bottomMarkerRect.location();

        PGContext* pgCtxt = ctxt.platformContext();
        if (pgCtxt->getScale() != 1.0) {
            topMarkerPosition.setX((float)topMarkerPosition.x() * pgCtxt->getScale());
            topMarkerPosition.setY((float)topMarkerPosition.y() * pgCtxt->getScale());
            bottomMarkerPosition.setX((float)bottomMarkerPosition.x() * pgCtxt->getScale());
            bottomMarkerPosition.setY((float)bottomMarkerPosition.y() * pgCtxt->getScale());

            // Reverse scale back to 1.0 because widget is always drawn at 1.0 scale
            // ctxt is already saved and restored in SelectionMarkersWidgetController::paint
            double scale = 1.0 / pgCtxt->getScale();
            ctxt.scale(FloatSize(scale, scale));
         }

        if (topMarker && !m_topMarkerRect.isEmpty())
            ctxt.drawImage(topMarker, DeviceColorSpace, topMarkerPosition);

        if (bottomMarker && !m_bottomMarkerRect.isEmpty())
            ctxt.drawImage(bottomMarker, DeviceColorSpace, bottomMarkerPosition);
    }
}

bool SelectionMarkersWidget::topMarkerSize(WebCore::IntSize& size) const
{
    bool result(false);

    Image* topMarker = getTopMarkerImage();

    if (topMarker && !m_topMarkerRect.isEmpty()) {

        size = topMarker->rect().size();
        result = true;
    }

    return result;
}

bool SelectionMarkersWidget::bottomMarkerSize(WebCore::IntSize& size) const
{
    bool result(false);

    Image* bottomMarker = getBottomMarkerImage();

    if (bottomMarker && !m_bottomMarkerRect.isEmpty()) {

        size = bottomMarker->rect().size();
        result = true;
    }

    return result;
}

void SelectionMarkersWidget::paintTopMarkerTexture(WebCore::GraphicsContext& ctxt, IntPoint &location) const
{
    if (!m_topMarkerRect.isEmpty()) {

        Image* topMarker = getTopMarkerImage();
        location = m_topMarkerRect.location();

        PGContext* pgCtxt = ctxt.platformContext();
        if (pgCtxt->getScale() != 1.0) {
            location.setX((float)location.x() * pgCtxt->getScale());
            location.setY((float)location.y() * pgCtxt->getScale());

            // Reverse scale back to 1.0 because widget is always drawn at 1.0 scale
            // ctxt is already saved and restored in SelectionMarkersWidgetController::paint
            double scale = 1.0 / pgCtxt->getScale();
            ctxt.scale(FloatSize(scale, scale));
         }

        if (topMarker)
            ctxt.drawImage(topMarker, DeviceColorSpace, IntPoint());
    }
}

void SelectionMarkersWidget::paintBottomMarkerTexture(WebCore::GraphicsContext& ctxt, IntPoint &location) const
{
    if (!m_bottomMarkerRect.isEmpty()) {

        Image* bottomMarker = getBottomMarkerImage();
        location = m_bottomMarkerRect.location();

        PGContext* pgCtxt = ctxt.platformContext();
        if (pgCtxt->getScale() != 1.0) {
            location.setX((float)location.x() * pgCtxt->getScale());
            location.setY((float)location.y() * pgCtxt->getScale());

            // Reverse scale back to 1.0 because widget is always drawn at 1.0 scale
            // ctxt is already saved and restored in SelectionMarkersWidgetController::paint
            double scale = 1.0 / pgCtxt->getScale();
            ctxt.scale(FloatSize(scale, scale));
         }

        if (bottomMarker)
            ctxt.drawImage(bottomMarker, DeviceColorSpace, IntPoint());
    }
}

void SelectionMarkersWidget::setMarkerRects(const IntRect& startSelRect, const IntRect& endSelRect)
{
    if (!m_markersEnabled)
        return;

    IntPoint top = startSelRect.location();
    IntPoint bottom = endSelRect.bottomRight();

    double zoom = 1.0 / getZoomLevel();

    if (startSelRect.isEmpty() && endSelRect.isEmpty())
        m_markersEnabled = false;

    IntRect empty;
    if (startSelRect.isEmpty())
        setMarkerTopRect(empty);
    else {
        Image* topMarker = getTopMarkerImage();
        if (topMarker) {
            IntRect markerRect = topMarker->rect();
            markerRect.scale(zoom);
            IntRect topRect(top.x() - markerRect.width()/2, top.y() - markerRect.height(), markerRect.width(), markerRect.height());
            setMarkerTopRect(topRect);
        }
    }

    if (endSelRect.isEmpty())
        setMarkerBottomRect(empty);
    else {
        Image* bottomMarker = getBottomMarkerImage();
        if (bottomMarker) {
            IntRect markerRect = bottomMarker->rect();
            markerRect.scale(zoom);
            IntRect bottomRect(bottom.x() - markerRect.width()/2, bottom.y(), markerRect.width(), markerRect.height());
            setMarkerBottomRect(bottomRect);
        }
    }

    invalidate();
}

void SelectionMarkersWidget::setMarkerTopRect(WebCore::IntRect& rect)
{
    if (rect != m_topMarkerRect) {
        IntRect oldRect = m_topMarkerRect;
        m_topMarkerRect = rect;

        double zoom = 1.0 / getZoomLevel();
        IntRect interactiveRect = getTapTargetRect(m_topMarkerRect, zoom);
        removeInteractiveRect((uintptr_t)this);
        addInteractiveRect((uintptr_t)this, interactiveRect);

        addPaintRect(oldRect);
        addPaintRect(m_topMarkerRect);
    }
}

void SelectionMarkersWidget::setMarkerBottomRect(WebCore::IntRect& rect)
{
    if (rect != m_bottomMarkerRect) {
        IntRect oldRect = m_bottomMarkerRect;
        m_bottomMarkerRect = rect;

        double zoom = 1.0 / getZoomLevel();
        IntRect interactiveRect = getTapTargetRect(m_bottomMarkerRect, zoom);
        removeInteractiveRect((uintptr_t)this + 1);
        addInteractiveRect((uintptr_t)this + 1, interactiveRect);

        addPaintRect(oldRect);
        addPaintRect(m_bottomMarkerRect);
    }
}

void SelectionMarkersWidget::addPaintRect(IntRect& rect)
{
    if (m_paintRect.isEmpty())
        m_paintRect = rect;
    else
        m_paintRect.unite(rect);
}

void SelectionMarkersWidget::invalidate()
{
    if (m_paintRect.isEmpty())
        return;

    IntRect repaintRect(m_paintRect);
    m_paintRect.setSize(IntSize());
    if (m_page && m_page->view() && m_page->view()->client())
        m_page->view()->client()->invalContents(repaintRect.x(), repaintRect.y(), repaintRect.width(), repaintRect.height());
}

double SelectionMarkersWidget::getZoomLevel() const
{
    if (m_page && m_page->view() && m_page->view()->client())
        return m_page->view()->client()->getZoomLevel();
    return 1.0;
}

void SelectionMarkersWidget::addInteractiveRect(uintptr_t id, IntRect& rect) const
{
    if (m_page && m_page->view() && m_page->view()->client())
        m_page->view()->client()->addInteractiveWidgetRect(id, rect.x(), rect.y(), rect.width(), rect.height());
}

void SelectionMarkersWidget::removeInteractiveRect(uintptr_t id) const
{
    if (m_page && m_page->view() && m_page->view()->client())
        m_page->view()->client()->removeInteractiveWidgetRect(id);
}

void SelectionMarkersWidget::hitTest(HitTestResult& result, const WebCore::PlatformMouseEvent& evt) const
{
    double zoom = 1.0 / getZoomLevel();
    IntRect topMarkerRect = getTapTargetRect(m_topMarkerRect, zoom);
    IntRect bottomMarkerRect = getTapTargetRect(m_bottomMarkerRect, zoom);

    if (topMarkerRect.contains(evt.pos())) {
        result.hitResult = HitTestResult::Top;
        result.yOffset = PalmBrowserSettings()->selectionHitTestOffset;
    }
    else if (bottomMarkerRect.contains(evt.pos())) {
        result.hitResult = HitTestResult::Bottom;
        result.yOffset = -PalmBrowserSettings()->selectionHitTestOffset;
    }
    else
        result.hitResult = HitTestResult::NoHit;
}

Image* SelectionMarkersWidget::getTopMarkerImage()
{
    static RefPtr<Image> s_topMarkerImage;

    if (!s_topMarkerImage) {
        s_topMarkerImage = Image::loadPlatformResource(s_topMarkerImgPath.c_str());
        ccpLog("s_topMarkerImage (%dx%d)", s_topMarkerImage->width(), s_topMarkerImage->height());
    }

    return s_topMarkerImage.get();
}

Image* SelectionMarkersWidget::getBottomMarkerImage()
{
    static RefPtr<Image> s_bottomMarkerImage;

    if (!s_bottomMarkerImage) {
        s_bottomMarkerImage = Image::loadPlatformResource(s_bottomMarkerImgPath.c_str());
        ccpLog("bottomMarkerImage (%dx%d)", s_bottomMarkerImage->width(), s_bottomMarkerImage->height());
    }

    return s_bottomMarkerImage.get();
}

}
