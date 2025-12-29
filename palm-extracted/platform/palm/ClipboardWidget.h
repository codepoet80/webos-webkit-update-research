/* ============================================================
 * Copyright 2011 Palm, Inc. All rights reserved.
 * ============================================================ */

#ifndef WEBOSCLIPBOARDWIDGET_H
#define WEBOSCLIPBOARDWIDGET_H

#include <PlatformString.h>
#include <Color.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>
#include <IntRect.h>

class PGSurface;

namespace Palm
{
    class ClipboardCommand;
    class WebPage;
}

namespace WebCore
{
	class IntPoint;
	class GraphicsContext;
	class Font;
	class TextRun;
    class Frame;

/**
 * A sort of widget for the display of and interaction with the Clipboard
 * UI balloon that is drawn above or below a selected area. This UI is drawn after
 * the page is rendered by WebKit.
 */
class ClipboardWidget
{
public:
    enum ClipboardArrowDir {
        ClipboardArrowUp,	///< Arrow points up.
        ClipboardArrowDown	///< Arrow points down.
	};


    ClipboardWidget(int yMargin);

	static int height();
    static int getMinBalloonWidth();

    void paint(GraphicsContext* context);
    void updateUI(const IntPoint& position, int width, int arrowPos, ClipboardArrowDir arrowDir);
	void setOpacity(float opacity);
	float getOpacity() const { return m_opacity; }
    const Palm::ClipboardCommand* pointToCommand(const IntPoint& pos) const;
    int getMaxBalloonWidth() const;
	void scrollBy(int deltaX);
	void scrollTo(int x);
    bool setCommandsForFrame(const WebCore::Frame* frame);
	void getMinimalBounds(IntRect& r) const;
	int getScrollPosition() const { return m_scrollPos; }
	bool canScrollToLeft() const;
	bool canScrollToRight() const;
	int maxScroll() const;

private:

	static void DrawText(GraphicsContext* context, const TextRun& run, const IntPoint& loc, Color color, int offset);
	static bool initClass();
    static int calcMinWidgetWidth();

	void paintBalloon(GraphicsContext* context) const;
    void paintText(GraphicsContext* context, const IntRect& bounds);
	void paintDivider(GraphicsContext* context, int left, int top) const;
	void paintFade(GraphicsContext* context) const;
	void layout();
	void clampScrollPos();
	bool isVisible() const { return m_opacity > 0; }
    void inflateCommandRect(IntRect& rect) const;

	static PGSurface* s_leftFadeImage;
	static PGSurface* s_rightFadeImage;
	static PGSurface* s_leftImage;
	static PGSurface* s_middleImage;
	static PGSurface* s_downArrowImage;
	static PGSurface* s_upArrowImage;
	static PGSurface* s_rightImage;
	static PGSurface* s_divider;
	static WebCore::Font* s_font;
	static int s_leftImgWidth;			///< Width of s_leftImage.
	static int s_dividerImgWidth;		///< Width of s_divider.
	static int s_plusImgWidth;			///< Width of s_plus.
    static int s_minWidgetWidth;        ///< The minimum width of this widget.
	static bool s_classInitialized;
    static WTF::Vector<Palm::ClipboardCommand*> s_allCommands;        ///< The list of commands.

    WTF::Vector<Palm::ClipboardCommand*>	m_enabledCommands;	///< The list of commands to display in the widget.
    int m_scrollPos;				///< The horizontal scroll position in pixels. Positive = scroll text to the left.
    IntRect	m_bounds;				///< The bounds of this Clipboard widget.
	int m_arrowPos;					///< Arrow X position (screen coordinates).
    ClipboardArrowDir m_arrowDir;	///< The direction of the arrow.
	float m_opacity;				///< The entire widget's opacity.
    int m_yMargin;                  ///< The # of pixels above/below widget to include for hit detection.
};

}


#endif
