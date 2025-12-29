#ifndef SELECTIONMARKERSWIDGETCONTROLLER_H
#define SELECTIONMARKERSWIDGETCONTROLLER_H

#include "config.h"

#include "SelectionMarkersWidget.h"
#include "palmwebpage.h"

#include "GraphicsContext.h"
#include "PlatformMouseEvent.h"
#include "IntSize.h"
#include "RenderObject.h"
#include "Position.h"

namespace Palm
{

class SelectionMarkersWidgetController {
public:
    SelectionMarkersWidgetController(Palm::WebPage* page);
    ~SelectionMarkersWidgetController();

    bool handleMouseDownEvent(const WebCore::PlatformMouseEvent& evt);
    bool handleMouseMoveEvent(const WebCore::PlatformMouseEvent& evt);
    bool handleMouseUpEvent(const WebCore::PlatformMouseEvent& evt);

    void respondToChangedSelection();
    void paint(WebCore::GraphicsContext& ctxt, int contentX, int contentY, float pageScale);

    // For painting with accelerated compositing
    void prePaintUpdate(WebCore::IntSize& top, WebCore::IntSize& bottom);
    void paintTopMarkerTexture(WebCore::GraphicsContext& ctxt, WebCore::IntPoint& location) const;
    void paintBottomMarkerTexture(WebCore::GraphicsContext& ctxt, WebCore::IntPoint& location) const;

    void scrollMarker(const WebCore::PlatformMouseEvent& evt);
    bool isSelecting() const { return m_isSelecting; }
    bool markersAreVisible() const { return m_widget.markersAreVisible(); }
    WebCore::IntSize markerSize() const { return m_widget.markerSize(); }
    bool markersActive() const { return m_widget.markersEnabled(); }
    void setMarkersActive(bool activate);

private:
    void setSelecting(bool val);

    enum SelectedRect {FirstSelectedRect, LastSelectedRect};
    static void getSelectionRect(WebCore::Frame* frame, WebCore::IntRect& startSelRect, WebCore::IntRect& endSelRect);

    Palm::WebPage*                          m_page;
    SelectionMarkersWidget                  m_widget;
    bool                                    m_isSelecting;
    WebCore::Position                       m_selectionBase;
    WebCore::PlatformMouseEvent             m_lastEvtHandled;
    SelectionMarkersWidget::HitTestResult   m_lastHitTestResult;
};

}
#endif

