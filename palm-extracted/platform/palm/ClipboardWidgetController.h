/* ============================================================
 * Copyright 2011 Palm, Inc. All rights reserved.
 * ============================================================ */

#ifndef WEBOSCLIPBOARDWIDGETCONTROLLER_H
#define WEBOSCLIPBOARDWIDGETCONTROLLER_H

#include "WidgetMouseEventInfo.h"
#include "ClipboardWidget.h"
#include "ClipboardCommands.h"
#include "RenderObject.h"
#include "glib.h"
#include "PlatformMouseEvent.h"

namespace Palm
{
	class WebPage;
}

namespace WebCore
{

class IntRect;
class Frame;

/**
 * This class owns the ClipboardWidget. It controls its state and rendering.
 */
class ClipboardWidgetController
{
public:
    ClipboardWidgetController(Palm::WebPage* page, int yMargin);
    ~ClipboardWidgetController();

	void hide();
    bool show();
	bool isVisible() const;
    const Palm::ClipboardCommand* pointToCommand(const IntPoint& pos) const;
	void flick(double speed);
	bool isFlicking();
	void stopFlick();
	void scrollBy(int deltaX);
	void scrollTo(int x);
    ClipboardWidget* widget() const;
	bool invalidate();
    void setPageMetrics(const IntPoint& scroll, float scale, int screenWidth, int screenHeight);
    void setSelectionRect(const WebCore::IntRect& r, int inflateY);
	void positionWidget();
    void paint(GraphicsContext& ctxt, int contentX, int contentY, float pageScale, bool accelComposited = false);
    bool handleMouseEvent(const PlatformMouseEvent& mouseEvent);
    const WebCore::IntRect& screenWidgetRect() { return m_screenWidgetRect; }

private:

	class FlickScroller
	{
        ClipboardWidgetController* 	m_controller;///outer class ptr passed to flickCb
		double 						m_dStartSpeed;///< Initial speed
		double						m_dDuration;///< The scroll duration in seconds
		short  						m_direction; ///< direction of motion negative = left to right
		GSource* 					m_flickTimer;///< Timer that controls flick
		double						m_curFlickDuration;///< how far (time wise) we are in the flick animation

		// after some experimentation it seemed that scrolling
		// feels more natural if initial speed is decreased instead
		// of increasing acceleration. That is why initial speed is
		// divided by SPEED_DIVIDER.
		static const double 		ACCELERATION = 1000;
		static const short			SPEED_DIVIDER = 2;
		static const unsigned 		FLICK_TIMEOUT_MS = 25; ///< miliseconds
		static const double 		FLICK_TIMEOUT_SEC = 0.025; ///< seconds
		static const unsigned  		MIN_FLICK_SPEED = 500; ///< min speed in pixs/second required to perform flick

	public:

        FlickScroller(ClipboardWidgetController* controller);
		~FlickScroller();
		// returns position at the specified time
		int getPosition(double t) const;
		bool initialize(double speed);
		// returns flick duration, that is, time until speed reaches 0
		double getDuration() const { return m_dDuration; }
		void stopFlickTimer();
		void startFlickTimer();
		bool isFlicking();
		void stopFlick();
		static gboolean flickCb(gpointer data);
	};

    class MouseEventInfo;


	bool dirty() const;
	void updateVisibleRect();
    void updateWidgetRect();
    void documentToScreen(WebCore::IntRect& r) const;
	void documentToScreen(WebCore::IntPoint& pt) const;
	void screenToDocument(WebCore::IntRect& r) const;
	void screenToDocument(WebCore::IntPoint& pt) const;
    IntRect selectionRect() const;

	void startAnimation(short direction);
	void startAnimationTimer();
	void stopAnimationTimer();
	static gboolean animateCb(gpointer data);
    WebCore::Frame* getFrame() const;

	bool            m_dirty;      ///< Is the page or screen dirty?
    ClipboardWidget* m_widget;	  ///< The widget this controller is managing.
    WidgetMouseEventInfo* m_mouseEventInfo;	  ///< The mouse event info
    Palm::WebPage*  m_page;       ///< The page on which the widget is drawn.
    IntRect         m_selectionRect;   ///< The selection rectangle in document coordinates.
    int             m_selectionInflateY;   ///< Inflate the height of the selection rect to account for selection markers
    IntRect         m_screenWidgetRect;   ///< The widget rectangle in screen coordinates.
    ClipboardWidget::ClipboardArrowDir m_arrowDir; ///< The direction of the arrow
    int             m_arrowPos;   ///< The position of the arrow
    IntPoint        m_pageScroll; ///< The page scroll position (in document coordinates).
	float           m_pageScale;  ///< The page scale (when zooming).
	int             m_screenWidth;///< The screen width.
    int             m_screenHeight;///< The screen height.
    bool            m_acceleratedComposited;

	GSource* 		m_animationTimer;///< Timer that controls fade-in/fade-out effect
	float			m_animationStep;///< negative or positive value, depending on the direction of the animation
    IntRect 		m_curVisibleRect;///< currently visible rectangle of Clipboard widget in screen coord
	FlickScroller	m_flickScroller;///< controlls scroll physics
};

}

#endif
