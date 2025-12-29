#ifndef CLIPBOARDCONTROLLER_H
#define CLIPBOARDCONTROLLER_H

#include "PlatformMouseEvent.h"

#include "SelectionMarkersWidgetController.h"
#include "palmwebpage.h"
#include "ClipboardWidgetController.h"

#include <glib.h>

namespace WebCore {
    class Frame;
    class GraphicsContext;
    class IntPoint;
}

namespace Palm
{

class ClipboardController {
public:
    ClipboardController(Palm::WebPage* page);
    ~ClipboardController();

    void paint(WebCore::GraphicsContext& ctxt, int contentX, int contentY, float pageScale);

    // event handling
    bool handleMouseDownEvent(const WebCore::PlatformMouseEvent& mouseEvent, bool isSimulated);
    bool handleMouseMoveEvent(const WebCore::PlatformMouseEvent& mouseEvent);
    bool handleMouseUpEvent(const WebCore::PlatformMouseEvent& mouseEvent, bool isSimulated);
    bool handleMouseHoldEvent(int x, int y);

    bool clipboardIsVisible() const;
    void hide(bool resetSelection);

    void respondToChangedSelection();
    void setMarkersActive(bool val) { m_selMarkersWidgetController.setMarkersActive(val); }
    void usesSimulatedMouseClicks(bool val) { m_usesSimulatedMouseClicks = val; }

    void setPageMetrics(const WebCore::IntPoint& scroll, float scale, int screenWidth, int screenHeight);

    SelectionMarkersWidgetController* getSelectionMarkerWidgetController() { return &m_selMarkersWidgetController; }
    WebCore::ClipboardWidgetController* getClipboardWidgetController() { return &m_clipboardWidgetController; }
private:
    bool tapOnSelectedText(const WebCore::PlatformMouseEvent& mouseEvent);
    void stopTapAndHoldTimer();
    void startTapAndHoldTimer();
    static gboolean tapAndHoldCb(gpointer data);

    WebCore::Frame* getFrame() const;
    void showClipboardWidget();

    Palm::WebPage*                      m_page;
    GSource*                            m_TapAndHoldTimer; // timer used to implement tap+hold event
    WebCore::PlatformMouseEvent         m_mouseEvent; // mouseDown that initiated the tap+hold
    WebCore::PlatformMouseEvent         m_mouseEventOnSelection; // mouseDown that happened on selected text
    SelectionMarkersWidgetController    m_selMarkersWidgetController;
    WebCore::ClipboardWidgetController  m_clipboardWidgetController;
    bool                                m_resendingMouseDown;
    bool                                m_usesSimulatedMouseClicks;
};


}


#endif

