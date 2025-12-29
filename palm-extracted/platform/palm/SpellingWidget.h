/* ============================================================
 * Copyright 2010 Palm, Inc. All rights reserved.
 * ============================================================ */

#ifndef WEBOSSPELLINGWIDGET_H
#define WEBOSSPELLINGWIDGET_H

#include <PlatformString.h>
#include <Color.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>
#include <IntRect.h>

class PGSurface;

namespace WebCore
{
	class IntPoint;
	class GraphicsContext;
	class Font;
	class TextRun;

/**
 * A sort of widget for the display of and interraction with the Spelling/Grammar
 * UI balloon that is drawn above or below a mispelled word. This UI is drawn after
 * the page is rendered by WebKit.
 */
class SpellingWidget
{
public:
	enum SpellingArrowDir {
		SpellingArrowUp,	///< Arrow points up.
		SpellingArrowDown	///< Arrow points down.
	};

	enum WordType {
		WT_AddToDictionary,
		WT_Original,
		WT_Guess
	};

	struct Word {
		String	 str;     ///< The string to draw.
		IntRect  bounds;  ///< The screen coordinates of the word's bounding box.
        IntPoint textLoc; ///< Text location
		WordType type;    ///< Why is this word here?
		const String& getString() const { return str; } // TODO ok to just return a reference?
	};

	SpellingWidget(int yMargin);

	static int height();
    static int getMinBalloonWidth();

	void paint(GraphicsContext* context) const;
	void updateUI(const IntPoint& position, int width, int arrowPos, SpellingArrowDir arrowDir);
	void setOpacity(float opacity);
	float getOpacity() const { return m_opacity; }
	void appendWord(const String& word, WordType type);
	void clearWords();
	const Word* pointToWord(const IntPoint& pos) const;
	int getMaxBalloonWidth() const;
	void scrollBy(int deltaX);
	void scrollTo(int x);
	void getMinimalBounds(IntRect& r) const;
	int getScrollPosition() const { return m_scrollPos; }
	bool canScrollToLeft() const;
	bool canScrollToRight() const;
	int maxScroll() const;

private:

	static void DrawText(GraphicsContext* context, const TextRun& run, const IntPoint& loc, Color color, int offset);
	static void DrawTextUnderline(GraphicsContext* context, const TextRun& run, const IntPoint& loc, int offset);
	static bool initClass();
    static int calcMinWidgetWidth();

	void paintBalloon(GraphicsContext* context) const;
	void paintText(GraphicsContext* context, const IntRect& bounds) const;
	void paintDivider(GraphicsContext* context, int left, int top) const;
	void paintFade(GraphicsContext* context) const;
	void layout();
	void clampScrollPos();
	bool isVisible() const { return m_opacity > 0; }
    void inflateWordRect(IntRect& rect) const;

	static PGSurface* s_leftFadeImage;
	static PGSurface* s_rightFadeImage;
	static PGSurface* s_leftImage;
	static PGSurface* s_middleImage;
	static PGSurface* s_downArrowImage;
	static PGSurface* s_upArrowImage;
	static PGSurface* s_rightImage;
	static PGSurface* s_divider;
	static PGSurface* s_plus;
	static WebCore::Font* s_font;
	static int s_leftImgWidth;			///< Width of s_leftImage.
	static int s_dividerImgWidth;		///< Width of s_divider.
	static int s_plusImgWidth;			///< Width of s_plus.
    static int s_minWidgetWidth;        ///< The minimum width of this widget.
	static bool s_classInitialized;

	WTF::Vector<Word>	m_words;	///< The list of words to display in the list.
	int m_scrollPos;				///< The horizontal scroll position in pixels. Positive = scroll text to the left.
	IntRect	m_bounds;				///< The bounds of this spelling widget.
	int m_arrowPos;					///< Arrow X position (screen coordinates).
	SpellingArrowDir m_arrowDir;	///< The direction of the arrow.
	float m_opacity;				///< The entire widget's opacity.
    int m_yMargin;                  ///< The # of pixels above/below widget to include for hit detection.
};

}


#endif
