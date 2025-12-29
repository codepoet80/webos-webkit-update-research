
// Copyright 2010 Palm Inc.

#include "config.h"

#include <string>
#include "SpellingWidget.h"
#include "Assertions.h"
#include "IntRect.h"
#include "PGSurface.h"
#include "PGContext.h"
#include "GraphicsContext.h"
#include "webkitpalmsettings.h"
#include "Font.h"
#include "FontSelector.h"
#include "PGThreadGlobalContext.h"
#include "CString.h"

namespace WebCore {

PGSurface* SpellingWidget::s_leftFadeImage;
PGSurface* SpellingWidget::s_rightFadeImage;
PGSurface* SpellingWidget::s_leftImage;
PGSurface* SpellingWidget::s_middleImage;
PGSurface* SpellingWidget::s_downArrowImage;
PGSurface* SpellingWidget::s_upArrowImage;
PGSurface* SpellingWidget::s_rightImage;
PGSurface* SpellingWidget::s_divider;
PGSurface* SpellingWidget::s_plus;
WebCore::Font* SpellingWidget::s_font;
int SpellingWidget::s_leftImgWidth(0);
int SpellingWidget::s_dividerImgWidth(0);
int SpellingWidget::s_plusImgWidth(0);
int SpellingWidget::s_minWidgetWidth(0);
bool SpellingWidget::s_classInitialized(false);


/// The space in between a word and the divider.
const int k_WordSpacing = 6;

/// The offset required for the other images when drawing the up arrow.
const int k_UpArrowOffset = 7;

const int k_topMargin = 6;

const int k_ArrowTipOffset = 16;	// Number of X pixels into the image to the tip of the arrow.

const int k_UnderlineOffset = 3;

/**
 * Constructor.
 */
SpellingWidget::SpellingWidget(int yMargin) :
	  m_scrollPos(0)
	, m_bounds(0, 0, 0, height())
	, m_arrowPos(0)
	, m_arrowDir(SpellingArrowDown)
	, m_opacity(0.0f)
	, m_yMargin(yMargin)
{
	if (!s_classInitialized)
		initClass();

	layout();
}

/**
 * Initialize this class.
 */
bool SpellingWidget::initClass()
{
	ASSERT(!s_classInitialized);

	bool success = true;
	std::string dataPath = PalmBrowserSettings()->webkitDataPath;
	dataPath += "/images/";
	std::string path;

	if (s_leftFadeImage == NULL) {
		path = dataPath + "ate-left-scroll-fade.png";
		if ((s_leftFadeImage = PGSurface::createFromPNGFile(path.c_str())) == NULL)
			success = false;
	}
	if (s_rightFadeImage == NULL) {
		path = dataPath + "ate-right-scroll-fade.png";
		if ((s_rightFadeImage = PGSurface::createFromPNGFile(path.c_str())) == NULL)
			success = false;
	}
	if (s_leftImage == NULL) {
		path = dataPath + "ate-left.png";
		if ((s_leftImage = PGSurface::createFromPNGFile(path.c_str())) == NULL)
			success = false;
		else
			s_leftImgWidth = s_leftImage->width();
	}
	if (s_middleImage == NULL) {
		path = dataPath + "ate-middle.png";
		if ((s_middleImage = PGSurface::createFromPNGFile(path.c_str())) == NULL)
			success = false;
	}
	if (s_downArrowImage == NULL) {
		path = dataPath + "ate-arrow-down.png";
		if ((s_downArrowImage = PGSurface::createFromPNGFile(path.c_str())) == NULL)
			success = false;
	}
	if (s_upArrowImage == NULL) {
		path = dataPath + "ate-arrow-up.png";
		if ((s_upArrowImage = PGSurface::createFromPNGFile(path.c_str())) == NULL)
			success = false;
	}
	if (s_rightImage == NULL) {
		path = dataPath + "ate-right.png";
		if ((s_rightImage = PGSurface::createFromPNGFile(path.c_str())) == NULL)
			success = false;
	}
	if (s_divider == NULL) {
		path = dataPath + "ate-divider.png";
		if ((s_divider = PGSurface::createFromPNGFile(path.c_str())) == NULL)
			success = false;
		else
			s_dividerImgWidth = s_divider->width();
	}
	if (s_plus == NULL) {
		path = dataPath + "ate-plus.png";
		if ((s_plus = PGSurface::createFromPNGFile(path.c_str())) == NULL)
			success = false;
		else
			s_plusImgWidth = s_plus->width();
	}

	if (s_font == NULL) {
		FontFamily family;
		family.setFamily(PalmBrowserSettings()->defaultSansSerifFontFamilyName);
		FontDescription	desc;
		desc.setWeight(FontWeightNormal);
		desc.setFamily(family);

		const float fontSize = 18.0f;
		desc.setSpecifiedSize(fontSize);
		desc.setComputedSize(fontSize);
		s_font = new WebCore::Font(desc, 0, k_WordSpacing);
		s_font->update(0);
	}

    s_minWidgetWidth = calcMinWidgetWidth();

	s_classInitialized = true;

	return success;
}

int SpellingWidget::calcMinWidgetWidth()
{
    int width(0);

    if (s_leftImage) {
        width += s_leftImage->width();
    }
    if (s_downArrowImage) {
        width += s_downArrowImage->width();
    }
    if (s_rightImage) {
        width += s_rightImage->width();
    }

    return width;
}

/**
 * Append a word to the end of this widget's list of words to draw.
 */
void SpellingWidget::appendWord(const String& word, WordType type)
{
	Word w;
	w.str = word;
	w.type = type;
	m_words.append(w);
	layout();
}

/**
 * Clear all of the words from this widget.
 */
void SpellingWidget::clearWords()
{
	m_words.clear();
	layout();
}

/**
 * Return the bounds of this widget not including the arrow.
 */
void SpellingWidget::getMinimalBounds(IntRect& r) const
{
	r = m_bounds;
	r.setHeight(r.height() - k_UpArrowOffset);
	if (m_arrowDir == SpellingArrowUp)
		r.move(0, k_UpArrowOffset);
}

/**
 * Layout the position of all of the words.
 *
 * The word bounding rectangle dimensions are:
 *
 * width = k_WordSpacing + <string width> + k_WordSpacing.
 * height = k_topMargin + <string height> + k_topMargin.
 */
void SpellingWidget::layout()
{
	IntRect textBounds;
	getMinimalBounds(textBounds);

	IntPoint tl(textBounds.x() + s_leftImgWidth - m_scrollPos,
				textBounds.y());

	PGThreadGlobalContext::instance(); //Ensure PGThreadGlobalContext is initialized before measuring fonts

	int boundsHeight = s_font->height() + 2 * k_topMargin;
	
    int fontHeight = s_font->height();

	int count = m_words.size();
	int idx = 1;
	WTF::Vector<Word>::iterator word;
	for (word = m_words.begin(); word != m_words.end(); ++word, ++idx) {
		TextRun run(word->str);

        int textWidth = s_font->width(run);
		int rectWidth = textWidth;
        const int k_minWordWidthPx = 50;
        rectWidth = std::max(rectWidth, k_minWordWidthPx);
        int centerOffset = (rectWidth-textWidth)/2;
		if (count > 1)
			rectWidth += k_WordSpacing;
		if (idx != count)
			rectWidth += k_WordSpacing;
		if (word->type == WT_AddToDictionary)
			rectWidth += s_plusImgWidth + k_WordSpacing;

		word->bounds.setLocation(tl);
		word->bounds.setWidth(rectWidth);
		word->bounds.setHeight(boundsHeight);
		
        IntPoint loc(word->bounds.x(), word->bounds.y() + fontHeight + k_topMargin);
        loc.setX(loc.x() + centerOffset);
        word->textLoc = loc;

		tl.move(rectWidth + s_dividerImgWidth, 0);
	}
}

void SpellingWidget::inflateWordRect(IntRect& rect) const
{
    rect.setHeight(rect.height() + m_yMargin);

    // We only want to inflate the widget on the side that is away from the arrow. This is because
    // we want to preserve the ability to tap on a word to which the arrow is pointing and if we
    // inflate on that side then the user can't do that.
    if (m_arrowDir == SpellingArrowDown) {
        rect.move(0, -m_yMargin);
    }
}

/**
 * Return the word whose bounding box contains this point.
 *
 * @param pt The point to test for in the widget.
 *
 * @return The word corresponding to the point. An empty string if there is no
 *         corresponding word.
 */
const SpellingWidget::Word* SpellingWidget::pointToWord(const IntPoint& pt) const
{
	IntRect textBounds;
	getMinimalBounds(textBounds);
    inflateWordRect(textBounds);

	if (textBounds.contains(pt)) {
		WTF::Vector<Word>::const_iterator word;
		for (word = m_words.begin(); word != m_words.end(); ++word) {
            IntRect bounds = word->bounds;
            inflateWordRect(bounds);
			if (bounds.contains(pt)) {
				return &(*word);
			}
		}
	}

	return 0;
}

static void DrawImage(GraphicsContext* context, PGSurface* image, int dstLeft, int dstTop, int dstRight, int dstBottom)
{
	ASSERT(context != NULL);
	ASSERT(image != NULL);


    // The clipboard and spelling widgets are always drawn 1:1 visually. to eliminate
    // rendering artifacts when scaling, we turn off anti-aliasing on the context
    // and the bilinear scaling on the pixmap
    image->pgPixmap()->SetQuality(PSNearest);

	if (dstLeft < dstRight && dstTop < dstBottom) {
		PlatformGraphicsContext* pg = context->platformContext();
        // Magic piranaha API to turn off anti-aliasing
        pg->pgContext()->SetAttribute(0, 0);
		pg->bitblt( image,
				0, 0, image->width(), image->height(),
				dstLeft, dstTop, dstRight, dstBottom );
        // Magic piranaha API to turn anti-aliasing back on
        pg->pgContext()->SetAttribute(0, 1);
	}

    image->pgPixmap()->SetQuality();
}

/**
 * Draw the spelling balloon.
 */
void SpellingWidget::paintBalloon(GraphicsContext* context) const
{
	ASSERT(!m_bounds.isEmpty());

	int minMiddleX(m_bounds.x());		// Middle filler image X coordinate.
	int maxMiddleX(m_bounds.right());	// Middle filler image Y coordinate.

	int yOffset = m_arrowDir == SpellingArrowDown ? 0 : k_UpArrowOffset;

	if (s_leftImage != NULL) {
		DrawImage(context, s_leftImage, m_bounds.x(), m_bounds.y() + yOffset,
				m_bounds.x() + s_leftImage->width(), m_bounds.bottom() + yOffset);
		minMiddleX += s_leftImage->width();
	}
	
	if (s_rightImage != NULL) {
		DrawImage(context, s_rightImage, m_bounds.right()-s_rightImage->width(), m_bounds.y() + yOffset,
				m_bounds.right(), m_bounds.bottom() + yOffset);
		maxMiddleX -= s_rightImage->width();
	}

	PGSurface* arrowImg = m_arrowDir == SpellingArrowUp ? s_upArrowImage : s_downArrowImage;
	
	int minArrowX = minMiddleX + k_ArrowTipOffset;
	int maxArrowX = maxMiddleX - k_ArrowTipOffset;
	int arrowPos = m_arrowPos;

	if (arrowPos < minArrowX)
		arrowPos = minArrowX;
	if (arrowPos > maxArrowX)
		arrowPos = maxArrowX;
	
	int ArrowImgLeftCoord = arrowPos - k_ArrowTipOffset;
	if (s_middleImage != NULL) {
		DrawImage(context, s_middleImage, minMiddleX, m_bounds.y() + yOffset,
				ArrowImgLeftCoord, m_bounds.bottom() + yOffset);
	
		if (arrowImg != NULL) {
			DrawImage(context, s_middleImage, ArrowImgLeftCoord + arrowImg->width(), m_bounds.y() + yOffset,
					maxMiddleX, m_bounds.bottom() + yOffset);
			DrawImage(context, arrowImg, ArrowImgLeftCoord, m_bounds.y(),
					ArrowImgLeftCoord + arrowImg->width(), m_bounds.bottom());
		}
	}
}

/**
 * Draw a run of text with a medium gray offset background.
 */
void SpellingWidget::DrawText(GraphicsContext* context, const TextRun& run, const IntPoint& loc, Color color, int offset)
{
	IntPoint pos(loc.x(), loc.y()+offset);
	
	context->setFillColor(color, sRGBColorSpace);
	context->drawText(*s_font, run, loc);
}

void SpellingWidget::DrawTextUnderline(GraphicsContext* context, const TextRun& run, const IntPoint& loc, int offset)
{
	int rectWidth = s_font->width(run);
	IntPoint nloc(loc.x(), loc.y()+offset+k_UnderlineOffset);
	context->drawLineForMisspellingOrBadGrammar(nloc, rectWidth, false /* !grammar */);
}

/**
 * Paint the vertical divider between two words.
 */
void SpellingWidget::paintDivider(GraphicsContext* context, int left, int top) const
{
	if (s_divider != NULL) {
		DrawImage(context, s_divider, left, top, left + s_divider->width(), top + s_divider->height());
	}
}

/**
 * Draw the text inside the spelling balloon.
 *
 * @param context  The context with which to draw.
 * @param bounds   The bounding rectangle of the balloon. This includes the drop shadow of the
 *                 UI but not the arrow.
 */
void SpellingWidget::paintText(GraphicsContext* context, const IntRect& bounds) const
{
	ASSERT(context != NULL);
	if (s_font == NULL)
		return;

	const Color color(255,255,255);

	context->save();

	// Don't need to clip top and bottom because it's just one line of text.
	FloatRect clipRect(bounds);
	const int margin = 6;
	clipRect.setWidth(clipRect.width() - 2*margin);
	clipRect.setX(clipRect.x() + margin);
	context->clip(clipRect);

	bool first = true;
	size_t wordIdx = 0;
	WTF::Vector<Word>::const_iterator word;
	for (word = m_words.begin(); word != m_words.end(); ++word) {
		TextRun run(word->str);
	
		IntPoint loc(word->textLoc);

		if (word->type == WT_AddToDictionary && s_plus) {
			DrawImage(context, s_plus, loc.x(), loc.y() - s_plusImgWidth,
					loc.x() + s_plusImgWidth, loc.y());
			loc.setX(loc.x() + s_plusImgWidth + k_WordSpacing);
		}

		if (word->type == WT_Guess) {
			DrawText(context, run, loc, color, -1);
		}
		else {
			DrawText(context, run, loc, color, 1);
			DrawTextUnderline(context, run, loc, 1);
		}

		if (wordIdx + 1 < m_words.size()) {	// If not the last one
			paintDivider(context, word->bounds.right() - k_WordSpacing, bounds.y() + 4);
		}
		wordIdx++;
		
		first = false;
	}
	context->restore();
}

void SpellingWidget::updateUI(const IntPoint& position, int width, int arrowPos, SpellingArrowDir arrowDir)
{
    width = std::max(width, s_minWidgetWidth);
	m_bounds.setLocation(position);
	m_bounds.setWidth(width);
	m_arrowPos = arrowPos;
	m_arrowDir = arrowDir;
	layout();
}

/**
 * Set this widget's opacity.
 */
void SpellingWidget::setOpacity(float opacity)
{
	if (opacity > 1) opacity = 1;
	if (opacity < 0) opacity = 0;
	m_opacity = opacity;
}

/**
 * Return the height of this widget.
 * This implementation is currnetly fixed.
 */
int SpellingWidget::height()
{
	return s_downArrowImage ? s_downArrowImage->height() : 60;
}

int SpellingWidget::maxScroll() const
{
	int maxScroll = getMaxBalloonWidth() - m_bounds.width();
	if (maxScroll < 0)
		maxScroll = 0;
	return maxScroll;
}

bool SpellingWidget::canScrollToLeft() const
{
	return m_scrollPos > 0;
}

bool SpellingWidget::canScrollToRight() const
{
	return m_scrollPos < maxScroll();
}

void SpellingWidget::paintFade(GraphicsContext* context) const
{
	ASSERT(!m_bounds.isEmpty());

	int yOffset = m_arrowDir == SpellingArrowDown ? 0 : k_UpArrowOffset;

	if (s_leftFadeImage != NULL && canScrollToLeft()) {
		DrawImage(context, s_leftFadeImage, m_bounds.x(), m_bounds.y() + yOffset,
			m_bounds.x() + s_leftFadeImage->width(), m_bounds.bottom() + yOffset);
	}
	
	if (s_rightFadeImage != NULL && canScrollToRight()) {
		DrawImage(context, s_rightFadeImage, m_bounds.right()-s_rightFadeImage->width(), m_bounds.y() + yOffset,
				m_bounds.right(), m_bounds.bottom() + yOffset);
	}
}

/**
 * Draw the spelling UI.
 *
 * @param context  The context with which to draw.
 */
void SpellingWidget::paint(GraphicsContext* context) const
{
	if (isVisible()) 
	{
#if 0
		g_debug("%s: Drawing spelling widget at (%d, %d), width=%d, height=%d, opacity=%.02f",
				__FUNCTION__,
				m_bounds.x(), m_bounds.y(), m_bounds.width(), m_bounds.height(), m_opacity);
#endif
		PlatformGraphicsContext* pg = context->platformContext();
		pg->setFillOpacity(static_cast<unsigned char>(m_opacity*255));
		pg->setStrokeOpacity(static_cast<unsigned char>(m_opacity*255));
		paintBalloon(context);
		
		IntRect textBounds;
		getMinimalBounds(textBounds);
		paintText(context, textBounds);

		paintFade(context);
	}
}

int SpellingWidget::getMinBalloonWidth()
{
    return s_minWidgetWidth;
}

/**
 * Return the maximum balloon width according to the number of words in this widget.
 */
int SpellingWidget::getMaxBalloonWidth() const
{
	int width = 2 * s_leftImgWidth;

	// The widget is layed out and the word bounding rectangle's are already calculated
	// so we just need to get width of all words.
	if (!m_words.isEmpty()) {
		width += (m_words.last().bounds.right() - m_words.first().bounds.x());
	}

	return width;
}

void SpellingWidget::clampScrollPos()
{
	const int minScroll = 0;
	int maxAllowedScroll = maxScroll();

	if (m_scrollPos < minScroll)
		m_scrollPos = minScroll;
	else if (m_scrollPos > maxAllowedScroll)
		m_scrollPos = maxAllowedScroll;
}

void SpellingWidget::scrollTo(int x)
{
	m_scrollPos = x;
	clampScrollPos();
	layout();
}

void SpellingWidget::scrollBy(int deltaX)
{
	m_scrollPos += deltaX;
	clampScrollPos();
	layout();
}

}
