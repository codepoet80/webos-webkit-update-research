#include "config.h"
#include "InlineTextBox.h"

namespace WebCore {

const int k_UnderlineOffset = 2;

void InlineTextBox::paintAutoReplacementMarker(GraphicsContext* pt, int tx, int ty, const DocumentMarker& marker, RenderStyle* style, const Font& font)
{
    // Never print replacement markers
    if (textRenderer()->document()->printing())
        return;

    if (m_truncation == cFullTruncation)
        return;

    int start = 0;                  // start of line to draw, relative to tx
    int width = m_width;            // how much line to draw

    // Determine whether we need to measure text
    bool markerSpansWholeBox = true;
    if (m_start <= (int)marker.startOffset)
        markerSpansWholeBox = false;
    if ((end() + 1) != marker.endOffset)      // end points at the last char, not past it
        markerSpansWholeBox = false;
    if (m_truncation != cNoTruncation)
        markerSpansWholeBox = false;

    if (!markerSpansWholeBox) {
        int startPosition = std::max<int>(marker.startOffset - m_start, 0);
        int endPosition = std::min<int>(marker.endOffset - m_start, m_len);

        if (m_truncation != cNoTruncation)
            endPosition = std::min<int>(endPosition, m_truncation);

        // Calculate start & width
        IntPoint startPoint(tx + m_x, ty + selectionTop());
        TextRun run(textRenderer()->text()->characters() + m_start, m_len, textRenderer()->allowTabs(), textPos(), m_toAdd, direction() == RTL, m_dirOverride || style->visuallyOrdered());
        int h = selectionHeight();

        IntRect markerRect = enclosingIntRect(font.selectionRectForText(run, startPoint, h, startPosition, endPosition));
        start = markerRect.x() - startPoint.x();
        width = markerRect.width();
    }

    // IMPORTANT: The misspelling underline is not considered when calculating the text bounds, so we have to
    // make sure to fit within those bounds.  This means the top pixel(s) of the underline will overlap the
    // bottom pixel(s) of the glyphs in smaller font sizes.  The alternatives are to increase the line spacing (bad!!)
    // or decrease the underline thickness.  The overlap is actually the most useful, and matches what AppKit does.
    // So, we generally place the underline at the bottom of the text, but in larger fonts that's not so good so
    // we pin to two pixels under the baseline.
    int lineThickness = cMisspellingLineThickness;
    int baseline = renderer()->style(m_firstLine)->font().ascent();
    int descent = height() - baseline;
    int underlineOffset;
    if (descent <= (2 + lineThickness)) {
        // Place the underline at the very bottom of the text in small/medium fonts.
        underlineOffset = height() - lineThickness;
    } else {
        // In larger fonts, though, place the underline up near the baseline to prevent a big gap.
        underlineOffset = baseline + 2;
    }

    underlineOffset += k_UnderlineOffset;

    if (pt->paintingDisabled()) {
		return;
    }

    IntPoint origin(tx + m_x + start, ty + m_y + underlineOffset);

	static const Color replacedWordColor(128, 128, 128);
    static const float replacedWordThickness = 3.0;

    Color prevColor(pt->strokeColor());
    WebCore::ColorSpace colorSpace = pt->strokeColorSpace();
    float prevThickness(pt->strokeThickness());
    StrokeStyle prevStyle(pt->strokeStyle());

    pt->setStrokeColor(replacedWordColor, sRGBColorSpace);
    pt->setStrokeThickness(replacedWordThickness);
    pt->setStrokeStyle(DottedStroke);

    IntPoint endPoint = origin + IntSize(width, 0);
    pt->drawLine(origin, endPoint);

    pt->setStrokeColor(prevColor, colorSpace);
    pt->setStrokeThickness(prevThickness);
    pt->setStrokeStyle(prevStyle);
}

void InlineTextBox::paintWordCompletionMarker(GraphicsContext* context, int tx, int ty, RenderStyle* style, const Font& font, int startPos, int endPos)
{
    int offset = m_start;
    int sPos = std::max<int>(startPos - offset, 0);
    int ePos = std::min<int>(endPos - offset, (int)m_len);

    if (sPos >= ePos)
        return;

    context->save();

    // use a baby blue background
    static const Color c = Color(166, 223, 255);

    updateGraphicsContext(context, c, c, 0, style->colorSpace()); // Don't draw text at all!

    int y = selectionTop();
    int h = selectionHeight();
    context->drawHighlightForText(font, TextRun(textRenderer()->text()->characters() + m_start, m_len, textRenderer()->allowTabs(), textPos(), m_toAdd,
                                  direction() == RTL, m_dirOverride || style->visuallyOrdered()),
                                  IntPoint(m_x + tx, y + ty), h, c, style->colorSpace(), sPos, ePos);
    context->restore();
}

}
