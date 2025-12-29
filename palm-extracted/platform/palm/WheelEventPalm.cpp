
// Copyright 2010 Palm Inc.


#include "config.h"

#include "PlatformWheelEvent.h"



namespace WebCore {

	/*
#define HIGH_BIT_MASK_SHORT 0x8000

static IntPoint positionForEvent(HWND hWnd, LPARAM lParam)
{
    POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    ScreenToClient(hWnd, &point);
    return point;
}

static IntPoint globalPositionForEvent(HWND hWnd, LPARAM lParam)
{
    POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
    return point;
} */

PlatformWheelEvent::PlatformWheelEvent( int delta_x, int delta_y )
    : //m_position(positionForEvent(hWnd, lParam))
    //, m_globalPosition(globalPositionForEvent(hWnd, lParam))
     m_isAccepted(false)
    , m_shiftKey(0)
    , m_ctrlKey(0)
    , m_altKey(0)
    , m_metaKey(m_altKey) // FIXME: We'll have to test other browsers
{
	m_deltaX = delta_x;
	m_deltaY = delta_y;
}

}
