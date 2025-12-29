/* ============================================================
 * Copyright 2011 Palm, Inc. All rights reserved.
 * ============================================================ */

#ifndef WEBOSWIDGETMOUSEEVENTINFO_H
#define WEBOSWIDGETMOUSEEVENTINFO_H

#include <IntPoint.h>

namespace WebCore
{
class IntPoint;

class WidgetMouseEventInfo {
public:

    WidgetMouseEventInfo();

    void clear();
    bool didDrag() const;
    void flickStart(int x);
    void flickEnd(int x);
    void flickClear();
    double getFlickSpeed();
    bool isMouseDown() const;
    bool isDownOnWidget() const;
    int moveX();
    void setCurrPt(const WebCore::IntPoint& pt);
    void setMouseDownInfo(bool down, bool downOnWidget = false);

private:

	WebCore::IntPoint m_prevPt; ///< Previous mouse position
	WebCore::IntPoint m_currPt; ///< Current mouse position
	unsigned long m_prevPtTime; ///< Time when the previous point was recorded
	unsigned long m_currPtTime; ///< Time when the current point was recorded
    bool m_downOnWidget;        ///< Did the mouse go down on the widget?
	bool m_mouseDown;           ///< Is the mouse currently down?
	int m_flickStartX;			///< x coord at mouse down
	int m_flickEndX;			///< x coord at mouse up
	int m_flickStartTime;		///< time at mouse down
	int m_flickEndTime;			///< time at mouse up
};

}
#endif // WEBOSWIDGETMOUSEEVENTINFO_H
