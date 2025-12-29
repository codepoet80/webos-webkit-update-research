#include "config.h"
#include "SelectionController.h"

#include "Frame.h"
#include "Page.h"
#include "RenderView.h"

#include "RenderThemePalm.h"
#include "EditorClient.h"
#include "PGSurface.h"
#include "PGContext.h"

const int caretSpillSize = 24;

namespace WebCore {

bool SelectionController::recomputeCaretRect()
{
    static int sPrevCaretDecoration = -1;

    bool paintCaretDecoration = false;
    if (m_frame->page() && m_frame->page()->editorClient()) {
        if (sPrevCaretDecoration != m_frame->page()->editorClient()->textCaretDecoration())
            paintCaretDecoration = true;
    }

    if (!m_caretRectNeedsUpdate && !paintCaretDecoration)
        return false;

    if (!m_frame)
        return false;

    FrameView* v = m_frame->document()->view();
    if (!v)
        return false;

    IntRect oldRect = m_caretRect;
    IntRect newRect = localCaretRect();
    if (oldRect == newRect && !m_absCaretBoundsDirty && !paintCaretDecoration)
        return false;

    IntRect oldAbsCaretBounds = m_absCaretBounds;
    // FIXME: Rename m_caretRect to m_localCaretRect.
    m_absCaretBounds = absoluteBoundsForLocalRect(m_caretRect);
    m_absCaretBoundsDirty = false;

    if (oldAbsCaretBounds == m_absCaretBounds && !paintCaretDecoration)
        return false;

    IntRect oldAbsoluteCaretRepaintBounds = m_absoluteCaretRepaintBounds;
    // We believe that we need to inflate the local rect before transforming it to obtain the repaint bounds.
    m_absoluteCaretRepaintBounds = caretRepaintRect();

#if ENABLE(TEXT_CARET)
    if (RenderView* view = toRenderView(m_frame->document()->renderer())) {

        oldAbsoluteCaretRepaintBounds.inflate(caretSpillSize);
        m_absoluteCaretRepaintBounds.inflate(caretSpillSize);
        sPrevCaretDecoration = m_frame->page()->editorClient()->textCaretDecoration();

        // FIXME: make caret repainting container-aware.
        view->repaintRectangleInViewAndCompositedLayers(oldAbsoluteCaretRepaintBounds, false);
        if (shouldRepaintCaret(view))
            view->repaintRectangleInViewAndCompositedLayers(m_absoluteCaretRepaintBounds, false);
    }
#endif
    return true;
}


void SelectionController::paintCaret(GraphicsContext* context, int tx, int ty, const IntRect& clipRect)
{
#if ENABLE(TEXT_CARET)
    if (!m_caretVisible)
        return;
    if (!m_caretPaint)
        return;
    if (!m_selection.isCaret())
        return;

    IntRect drawingRect = localCaretRectForPainting();
    drawingRect.move(tx, ty);
    IntRect caret = intersection(drawingRect, clipRect);
    if (caret.isEmpty())
        return;

    Color caretColor = Color::black;
    ColorSpace colorSpace = DeviceColorSpace;
    Element* element = rootEditableElement();
    if (element && element->renderer()) {
        caretColor = element->renderer()->style()->visitedDependentColor(CSSPropertyColor);
        colorSpace = element->renderer()->style()->colorSpace();
    }

    context->fillRect(caret, caretColor, colorSpace);

    // render semi-transparent second caret pixel
    caret.move(1, 0);
    context->fillRect(caret, Color(caretColor.red(),caretColor.green(),caretColor.blue(),191), colorSpace);

    Page* page = m_frame->page();
    if (!page)
        return;

    if (WebCore::TextCaretDecoration::None != page->editorClient()->textCaretDecoration()) {
        caret.move(-1, 0);

        // render the text caret decoration.
        PGSurface* img = ((RenderThemePalm*)page->theme())->platformCaretDecoration(
                    page->editorClient()->textCaretDecoration());

        if (img) {
            int cx = caret.x() - (img->width() / 2)+1;
            int cy = caret.bottom() + 2;
            int cr = cx + img->width();
            int cb = cy + img->height();

            PlatformGraphicsContext* c = context->platformContext();
            c->push();
            c->clearClip();
            c->bitblt(img, 0, 0, (int)(img->width()), (int)(img->height()), cx, cy, cr, cb);
            c->pop();
        }
    }
#else
    UNUSED_PARAM(context);
    UNUSED_PARAM(tx);
    UNUSED_PARAM(ty);
    UNUSED_PARAM(clipRect);
#endif
}

void SelectionController::invalidateCaretRect()
{
    if (!isCaret())
        return;

    Document* d = m_selection.start().node()->document();

    // recomputeCaretRect will always return false for the drag caret,
    // because its m_frame is always 0.
    bool caretRectChanged = recomputeCaretRect();

    // EDIT FIXME: This is an unfortunate hack.
    // Basically, we can't trust this layout position since we
    // can't guarantee that the check to see if we are in unrendered
    // content will work at this point. We may have to wait for
    // a layout and re-render of the document to happen. So, resetting this
    // flag will cause another caret layout to happen the first time
    // that we try to paint the caret after this call. That one will work since
    // it happens after the document has accounted for any editing
    // changes which may have been done.
    // And, we need to leave this layout here so the caret moves right
    // away after clicking.
    m_caretRectNeedsUpdate = true;

    if (!caretRectChanged) {
        RenderView* view = toRenderView(d->renderer());
        if (view && shouldRepaintCaret(view)) {
            IntRect rect = caretRepaintRect();
            // sysmgr sets global clip rect to whatever rect we send into viewClient->invalContents
            // This is why when caret times fires we are unable to paint the caret decoration - it
            // ends up being outside of the clip rect
            // So, inflate the repaint rect here.
            if (m_frame->page() && m_frame->page()->editorClient() &&
                WebCore::TextCaretDecoration::None != m_frame->page()->editorClient()->textCaretDecoration()) {
                    rect.inflate(caretSpillSize);
            }
            view->repaintRectangleInViewAndCompositedLayers(rect, false);
        }
    }
}

}

