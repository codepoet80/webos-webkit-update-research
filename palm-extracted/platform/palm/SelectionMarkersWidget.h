#ifndef SELECTIONMARKERSWIDGET_H
#define SELECTIONMARKERSWIDGET_H

#include "config.h"

#include "IntPoint.h"
#include "IntRect.h"
#include "GraphicsContext.h"
#include "IntSize.h"

namespace WebCore {
    class IntPoint;
    class PlatformMouseEvent;
}

namespace Palm
{
    class WebPage;

class SelectionMarkersWidget {
public:
    SelectionMarkersWidget(Palm::WebPage* page);
    ~SelectionMarkersWidget();

    void paint(WebCore::GraphicsContext& ctxt, int contextX, int contextY) const;
    void paintTopMarkerTexture(WebCore::GraphicsContext& ctxt, WebCore::IntPoint& location) const;
    void paintBottomMarkerTexture(WebCore::GraphicsContext& ctxt, WebCore::IntPoint& location) const;
    bool markersAreVisible() const { return !m_topMarkerRect.isEmpty() || !m_bottomMarkerRect.isEmpty(); }

    bool topMarkerSize(WebCore::IntSize&) const; // actual pixel size of the marker image
    bool bottomMarkerSize(WebCore::IntSize&) const; // actual pixel size of the marker image

    // returns marker rect size adjusted by the current scale.
    // When page is zoomed in browser server marker rects need to shrink by the zoom factor
    // to keep the same target pixel size on the screen.
    WebCore::IntSize markerSize() const { return m_topMarkerRect.size(); }

    void setMarkerRects(const WebCore::IntRect& startSelRect, const WebCore::IntRect& endSelRect);
    void enableMarkers() { m_markersEnabled = true; }
    bool markersEnabled() const { return m_markersEnabled; }

    class HitTestResult {
    public:
        HitTestResult() : hitResult(NoHit) {}
        enum HitResult {
            NoHit,
            Top,
            Bottom
        };
        HitResult   hitResult;

        // Tap could have happened anywhere on the marker rect
        // but we want to always adjust the tap to place it inside of
        // the text before we hit test render objects to adjust the selection.
        int         yOffset;
    };

    void hitTest(HitTestResult& result, const WebCore::PlatformMouseEvent& mouseEvent) const;

private:
    void setMarkerTopRect(WebCore::IntRect& top);
    void setMarkerBottomRect(WebCore::IntRect& bottom);
    void addPaintRect(WebCore::IntRect& rect);
    void invalidate();
    void addInteractiveRect(uintptr_t id, WebCore::IntRect& rect) const;
    void removeInteractiveRect(uintptr_t id) const;
    double getZoomLevel() const;

    static WebCore::Image* getBottomMarkerImage();
    static WebCore::Image* getTopMarkerImage();

    static const std::string s_topMarkerImgPath;
    static const std::string s_bottomMarkerImgPath;

    Palm::WebPage*      m_page;
    WebCore::IntRect    m_topMarkerRect; // The bounds of the top marker
    WebCore::IntRect    m_bottomMarkerRect; // The bounds of the bottom marker

    // m_paintRect: is the rect to be invalidated usually the sum of these four rects
    // topMarkerOldRect, topMarkerNewRect, bottomMarkerOldRect, bottomMarkerNewRect
    WebCore::IntRect    m_paintRect;
    bool                m_markersEnabled;
};


}


#endif

