// Copyright 2011 Palm Inc.

#include "config.h"
#include "WidgetMouseEventInfo.h"
#include "IntPoint.h"
#include <cmath>

namespace WebCore
{

WidgetMouseEventInfo::WidgetMouseEventInfo()
    : m_prevPtTime(0)
    , m_currPtTime(0)
    , m_prevPt(0,0)
    , m_currPt(0,0)
    , m_downOnWidget(false)
    , m_mouseDown(false)
{
}

void WidgetMouseEventInfo::clear() {
    m_prevPtTime = m_currPtTime = 0;
    m_prevPt = m_currPt = WebCore::IntPoint(0,0);
    m_mouseDown = false;
    m_downOnWidget = false;
}

void WidgetMouseEventInfo::setMouseDownInfo(bool down, bool downOnWidget) {
    if (down) {
        clear();
    }
    m_mouseDown = down;
    m_downOnWidget = downOnWidget;
}

void WidgetMouseEventInfo::flickStart(int x) {
    m_flickStartX = x;
    m_flickStartTime = palm_monotonic_time_ms();
}

void WidgetMouseEventInfo::flickEnd(int x) {
    m_flickEndX = x;
    m_flickEndTime = palm_monotonic_time_ms();
}

void WidgetMouseEventInfo::flickClear() {
    m_flickStartX = 0;
    m_flickStartTime = 0;
    m_flickEndX = 0;
    m_flickEndTime = 0;
}

double WidgetMouseEventInfo::getFlickSpeed() {
    static short min_flick_delta_x = 10;

    double dx = static_cast<double>(m_flickStartX - m_flickEndX);
    double dt = static_cast<double>(m_flickEndTime - m_flickStartTime) / 1000;
    if (std::abs(dx) > min_flick_delta_x && dt > 0) {
        return dx/dt;
    }
    return 0;
}

bool WidgetMouseEventInfo::isMouseDown() const {
    return m_mouseDown;
}

bool WidgetMouseEventInfo::isDownOnWidget() const {
    return m_downOnWidget;
}

bool WidgetMouseEventInfo::didDrag() const {
    return m_prevPtTime != 0 && m_currPtTime != 0;
}

int WidgetMouseEventInfo::moveX() {
    return m_currPt.x() - m_prevPt.x();
}

void WidgetMouseEventInfo::setCurrPt(const WebCore::IntPoint& pt) {
    m_prevPt = m_currPt;
    m_prevPtTime = m_currPtTime;
    m_currPt = pt;
    m_currPtTime = palm_monotonic_time_ms();
}

}
