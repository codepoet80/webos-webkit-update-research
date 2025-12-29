
// Copyright 2011 Palm Inc.

#include "config.h"

#include <string>
#include "ClipboardWidget.h"
#include "Assertions.h"
#include "IntRect.h"
#include "Frame.h"
#include "PGSurface.h"
#include "PGContext.h"
#include "GraphicsContext.h"
#include "palmwebpage.h"
#include "palmwebframe.h"
#include "webkitpalmsettings.h"
#include "webkitpalmstrings.h"
#include "Font.h"
#include "FontSelector.h"
#include "PGThreadGlobalContext.h"
#include "CString.h"
#include "ClipboardCommands.h"

namespace WebCore {

PGSurface* ClipboardWidget::s_leftFadeImage;
PGSurface* ClipboardWidget::s_rightFadeImage;
PGSurface* ClipboardWidget::s_leftImage;
PGSurface* ClipboardWidget::s_middleImage;
PGSurface* ClipboardWidget::s_downArrowImage;
PGSurface* ClipboardWidget::s_upArrowImage;
PGSurface* ClipboardWidget::s_rightImage;
PGSurface* ClipboardWidget::s_divider;
WebCore::Font* ClipboardWidget::s_font;
int ClipboardWidget::s_leftImgWidth(0);
int ClipboardWidget::s_dividerImgWidth(0);
int ClipboardWidget::s_minWidgetWidth(0);
bool ClipboardWidget::s_classInitialized(false);
WTF::Vector<Palm::ClipboardCommand*> ClipboardWidget::s_allCommands;

/// The space in between a command and the divider.
const int k_CommandSpacing = 6;

/// The offset required for the other images when drawing the up arrow.
const int k_UpArrowOffset = 7;

const int k_topMargin = 6;

const int k_ArrowTipOffset = 16;	// Number of X pixels into the image to the tip of the arrow.

const int k_UnderlineOffset = 3;
const int k_minCommandWidthPx = 50;
const int k_numCommands = 5;

/**
 * Constructor.
 */
ClipboardWidget::ClipboardWidget(int yMargin) :
	  m_scrollPos(0)
	, m_bounds(0, 0, 0, height())
	, m_arrowPos(0)
    , m_arrowDir(ClipboardArrowDown)
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
bool ClipboardWidget::initClass()
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

	if (s_font == NULL) {
		FontFamily family;
		family.setFamily(PalmBrowserSettings()->defaultSansSerifFontFamilyName);
		FontDescription	desc;
		desc.setWeight(FontWeightNormal);
		desc.setFamily(family);

		const float fontSize = 18.0f;
		desc.setSpecifiedSize(fontSize);
		desc.setComputedSize(fontSize);
        s_font = new WebCore::Font(desc, 0, k_CommandSpacing);
		s_font->update(0);
	}

    s_minWidgetWidth = calcMinWidgetWidth();

    //load the commands
    s_allCommands.append(new Palm::ClipboardCutCommand());
    s_allCommands.append(new Palm::ClipboardCopyCommand());
    s_allCommands.append(new Palm::ClipboardSelectCommand());
    s_allCommands.append(new Palm::ClipboardSelectAllCommand());
    s_allCommands.append(new Palm::ClipboardPasteCommand());

	s_classInitialized = true;

	return success;
}

int ClipboardWidget::calcMinWidgetWidth()
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
 * Sets the commands to be displayed based on the given frame
 * Returns true if any commands were enabled, false otherwise
 */
bool ClipboardWidget::setCommandsForFrame(const WebCore::Frame* frame)
{
    m_enabledCommands.clear();
    int idx = 1;
    WTF::Vector<Palm::ClipboardCommand*>::iterator commandIter;
    for (commandIter = s_allCommands.begin(); commandIter != s_allCommands.end(); ++commandIter, ++idx) {
        Palm::ClipboardCommand* command = *commandIter;
        if (command->enabled(frame)) {
            command->setFrame(frame);
            m_enabledCommands.append(command);
        }
        else {
            command->setFrame(0);
            command->setBounds(WebCore::IntRect());
            command->setTextPosition(WebCore::IntPoint());
        }
    }
    if (m_enabledCommands.isEmpty())
        return false;
    else {
        layout();
        return true;
    }
}


/**
 * Return the bounds of this widget not including the arrow.
 */
void ClipboardWidget::getMinimalBounds(IntRect& r) const
{
	r = m_bounds;
	r.setHeight(r.height() - k_UpArrowOffset);
    if (m_arrowDir == ClipboardArrowUp)
		r.move(0, k_UpArrowOffset);
}

/**
 * Layout the position of all of the commands.
 *
 * The commands bounding rectangle dimensions are:
 *
 * width = k_CommandSpacing + <string width> + k_CommandSpacing.
 * height = k_topMargin + <string height> + k_topMargin.
 */
void ClipboardWidget::layout()
{
	IntRect textBounds;
	getMinimalBounds(textBounds);

	IntPoint tl(textBounds.x() + s_leftImgWidth - m_scrollPos,
				textBounds.y());

	PGThreadGlobalContext::instance(); //Ensure PGThreadGlobalContext is initialized before measuring fonts

	int boundsHeight = s_font->height() + 2 * k_topMargin;
	
    int fontHeight = s_font->height();

    int count = m_enabledCommands.size();
    int idx = 1;
    WTF::Vector<Palm::ClipboardCommand*>::iterator commandIter;
    for (commandIter = m_enabledCommands.begin(); commandIter != m_enabledCommands.end(); ++commandIter, ++idx) {
        Palm::ClipboardCommand* command = *commandIter;
        String cmdStr(command->toLocalizedString());
        TextRun run(cmdStr);
        int textWidth = s_font->width(run);
        int rectWidth = std::max(textWidth, k_minCommandWidthPx);
        int centerOffset = (rectWidth-textWidth)/2;
        if (count > 1)
            rectWidth += k_CommandSpacing;
        if (idx != count)
            rectWidth += k_CommandSpacing;

        command->setBounds(WebCore::IntRect(tl.x(), tl.y(), rectWidth, boundsHeight));
        IntPoint loc(command->bounds().x(), command->bounds().y() + fontHeight + k_topMargin);
        loc.setX(loc.x() + centerOffset);
        command->setTextPosition(loc);
        tl.move(rectWidth + s_dividerImgWidth, 0);
    }
}

void ClipboardWidget::inflateCommandRect(IntRect& rect) const
{
    rect.setHeight(rect.height() + m_yMargin);

    // We only want to inflate the widget on the side that is away from the arrow. This is because
    // we want to preserve the ability to tap on a command to which the arrow is pointing and if we
    // inflate on that side then the user can't do that.
    if (m_arrowDir == ClipboardArrowDown) {
        rect.move(0, -m_yMargin);
    }
}

/**
 * Return the command whose bounding box contains this point.
 *
 * @param pt The point to test for in the widget.
 *
 * @return The command corresponding to the point. An empty string if there is no
 *         corresponding command.
 */
const Palm::ClipboardCommand* ClipboardWidget::pointToCommand(const IntPoint& pt) const
{
	IntRect textBounds;
	getMinimalBounds(textBounds);
    inflateCommandRect(textBounds);

	if (textBounds.contains(pt)) {
        WTF::Vector<Palm::ClipboardCommand*>::const_iterator commandIter;
        for (commandIter = m_enabledCommands.begin(); commandIter != m_enabledCommands.end(); ++commandIter) {
            const Palm::ClipboardCommand* command = *commandIter;
            IntRect bounds = command->bounds();
            inflateCommandRect(bounds);
            if (bounds.contains(pt)) {
                return command;
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
 * Draw the Clipboard balloon.
 */
void ClipboardWidget::paintBalloon(GraphicsContext* context) const
{
	ASSERT(!m_bounds.isEmpty());

	int minMiddleX(m_bounds.x());		// Middle filler image X coordinate.
	int maxMiddleX(m_bounds.right());	// Middle filler image Y coordinate.

    int yOffset = m_arrowDir == ClipboardArrowDown ? 0 : k_UpArrowOffset;

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

    PGSurface* arrowImg = m_arrowDir == ClipboardArrowUp ? s_upArrowImage : s_downArrowImage;
	
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
void ClipboardWidget::DrawText(GraphicsContext* context, const TextRun& run, const IntPoint& loc, Color color, int offset)
{
	IntPoint pos(loc.x(), loc.y()+offset);
	
	context->setFillColor(color, sRGBColorSpace);
	context->drawText(*s_font, run, loc);
}

/**
 * Paint the vertical divider between two commandss.
 */
void ClipboardWidget::paintDivider(GraphicsContext* context, int left, int top) const
{
	if (s_divider != NULL) {
		DrawImage(context, s_divider, left, top, left + s_divider->width(), top + s_divider->height());
	}
}

/**
 * Draw the text inside the Clipboard balloon.
 *
 * @param context  The context with which to draw.
 * @param bounds   The bounding rectangle of the balloon. This includes the drop shadow of the
 *                 UI but not the arrow.
 */
void ClipboardWidget::paintText(GraphicsContext* context, const IntRect& bounds)
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
    size_t idx = 0;
    WTF::Vector<Palm::ClipboardCommand*>::iterator commandIter;
    for (commandIter = m_enabledCommands.begin(); commandIter != m_enabledCommands.end(); ++commandIter) {
        Palm::ClipboardCommand* command = *commandIter;
        String cmdStr(command->toLocalizedString());
        TextRun run(cmdStr);
        IntPoint loc(command->textPosition());
        DrawText(context, run, loc, color, -1);
        if (idx + 1 < m_enabledCommands.size()) {	// If not the last one
            paintDivider(context, command->bounds().right() - k_CommandSpacing, bounds.y() + 4);
        }
        idx++;
        first = false;
    }

    context->restore();
}

void ClipboardWidget::updateUI(const IntPoint& position, int width, int arrowPos, ClipboardArrowDir arrowDir)
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
void ClipboardWidget::setOpacity(float opacity)
{
	if (opacity > 1) opacity = 1;
	if (opacity < 0) opacity = 0;
	m_opacity = opacity;
}

/**
 * Return the height of this widget.
 * This implementation is currently fixed.
 */
int ClipboardWidget::height()
{
	return s_downArrowImage ? s_downArrowImage->height() : 60;
}

int ClipboardWidget::maxScroll() const
{
	int maxScroll = getMaxBalloonWidth() - m_bounds.width();
	if (maxScroll < 0)
		maxScroll = 0;
	return maxScroll;
}

bool ClipboardWidget::canScrollToLeft() const
{
	return m_scrollPos > 0;
}

bool ClipboardWidget::canScrollToRight() const
{
	return m_scrollPos < maxScroll();
}

void ClipboardWidget::paintFade(GraphicsContext* context) const
{
	ASSERT(!m_bounds.isEmpty());

    int yOffset = m_arrowDir == ClipboardArrowDown ? 0 : k_UpArrowOffset;

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
 * Draw the Clipboard UI.
 *
 * @param context  The context with which to draw.
 */
void ClipboardWidget::paint(GraphicsContext* context)
{
	if (isVisible()) 
	{
#if 0
        g_debug("%s: Drawing Clipboard widget at (%d, %d), width=%d, height=%d, opacity=%.02f",
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

int ClipboardWidget::getMinBalloonWidth()
{
    return s_minWidgetWidth;
}

/**
 * Return the maximum balloon width according to the number of commands in this widget.
 */
int ClipboardWidget::getMaxBalloonWidth() const
{
	int width = 2 * s_leftImgWidth;

    // The widget is layed out and the command bounding rectangle's are already calculated
    // so we just need to get width of all commands.
    if (!m_enabledCommands.isEmpty()) {
        width += (m_enabledCommands.last()->bounds().right() - m_enabledCommands.first()->bounds().x());
	}

	return width;
}

void ClipboardWidget::clampScrollPos()
{
	const int minScroll = 0;
	int maxAllowedScroll = maxScroll();

	if (m_scrollPos < minScroll)
		m_scrollPos = minScroll;
	else if (m_scrollPos > maxAllowedScroll)
        m_scrollPos = maxAllowedScroll;
}

void ClipboardWidget::scrollTo(int x)
{
	m_scrollPos = x;
	clampScrollPos();
	layout();
}

void ClipboardWidget::scrollBy(int deltaX)
{
	m_scrollPos += deltaX;
	clampScrollPos();
	layout();
}

}
