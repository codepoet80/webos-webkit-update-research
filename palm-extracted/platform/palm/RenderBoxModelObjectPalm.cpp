#include "config.h"
#include "RenderBoxModelObject.h"

#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "ScratchSurface.h"

using namespace std;

namespace WebCore {

bool RenderBoxModelObject::paintNinePieceImage(GraphicsContext* graphicsContext, int tx, int ty, int w, int h, const RenderStyle* style,
                                               const NinePieceImage& ninePieceImage, CompositeOperator op)
{
    StyleImage* styleImage = ninePieceImage.image();
    if (!styleImage)
        return false;

    if (!styleImage->isLoaded())
        return true; // Never paint a nine-piece image incrementally, but don't paint the fallback borders either.

    if (!styleImage->canRender(style->effectiveZoom()))
        return false;

    styleImage->setImageContainerSize(IntSize(w, h));
    IntSize imageSize = styleImage->imageTrueSize(this, 1.0f);
    int imageWidth = imageSize.width();
    int imageHeight = imageSize.height();

    float zoom = style->effectiveZoom();
    float resourceScale = styleImage->resourceScale();
    if (resourceScale != 1.0f) {
        // Resource that we loaded must be at the current scale
        ASSERT(resourceScale == zoom);
        zoom = 1.0;
    }

    // When current page scale is different than loaded 9-tile image scale
    // that is, in the case when resource for the current scale was not found
    // we need to paint the 1.0x scaled 9-tile image onto for example 1.5x scaled surface.
    // for example: the image at 1.0 scale would be 50x50. The same image at 1.5x would be 75x75 pixels.
    int scaledTx = tx;
    int scaledTy = ty;
    int scaledW = w;
    int scaledH = h;
    GraphicsContext* originalGC = graphicsContext;
    ScratchSurface* scratchSurface = 0;
    if (zoom != 1.0f) {
        // Paint the 9-tile in 1.0 zoom into a temporary surface at 1.0 zoom w & h
        // then stretch the 1.0 surface to passed Rect(tx, ty, w, h)
        tx = 0;
        ty = 0;
        w = ceil(w / zoom);
        h = ceil(h / zoom);
        scratchSurface = ScratchSurface::get(w, h);
        if (scratchSurface)
            graphicsContext = scratchSurface->buffer()->context();
    }

    int topSlice = min(imageHeight, ninePieceImage.slices().top().calcValue(imageHeight, resourceScale));
    int bottomSlice = min(imageHeight, ninePieceImage.slices().bottom().calcValue(imageHeight, resourceScale));
    int leftSlice = min(imageWidth, ninePieceImage.slices().left().calcValue(imageWidth, resourceScale));
    int rightSlice = min(imageWidth, ninePieceImage.slices().right().calcValue(imageWidth, resourceScale));

    ENinePieceImageRule hRule = ninePieceImage.horizontalRule();
    ENinePieceImageRule vRule = ninePieceImage.verticalRule();

    bool fitToBorder = style->borderImage() == ninePieceImage;

    int leftWidth = fitToBorder ? style->borderLeftWidth() : leftSlice;
    int topWidth = fitToBorder ? style->borderTopWidth() : topSlice;
    int rightWidth = fitToBorder ? style->borderRightWidth() : rightSlice;
    int bottomWidth = fitToBorder ? style->borderBottomWidth() : bottomSlice;

    leftWidth = ceil(leftWidth / zoom);
    topWidth = ceil(topWidth / zoom);
    rightWidth = ceil(rightWidth / zoom);
    bottomWidth = ceil(bottomWidth / zoom);

    bool drawLeft = leftSlice > 0 && leftWidth > 0;
    bool drawTop = topSlice > 0 && topWidth > 0;
    bool drawRight = rightSlice > 0 && rightWidth > 0;
    bool drawBottom = bottomSlice > 0 && bottomWidth > 0;
    bool drawMiddle = (imageWidth - leftSlice - rightSlice) > 0 && (w - leftWidth - rightWidth) > 0 &&
                      (imageHeight - topSlice - bottomSlice) > 0 && (h - topWidth - bottomWidth) > 0;

    Image* image = styleImage->image(this, imageSize);
    ColorSpace colorSpace = style->colorSpace();

    if (drawLeft) {
        // Paint the top and bottom left corners.

        // The top left corner rect is (tx, ty, leftWidth, topWidth)
        // The rect to use from within the image is obtained from our slice, and is (0, 0, leftSlice, topSlice)
        if (drawTop)
            graphicsContext->drawImage(image, colorSpace, IntRect(tx, ty, leftWidth, topWidth),
                                       IntRect(0, 0, leftSlice, topSlice), op);

        // The bottom left corner rect is (tx, ty + h - bottomWidth, leftWidth, bottomWidth)
        // The rect to use from within the image is (0, imageHeight - bottomSlice, leftSlice, botomSlice)
        if (drawBottom)
            graphicsContext->drawImage(image, colorSpace, IntRect(tx, ty + h - bottomWidth, leftWidth, bottomWidth),
                                       IntRect(0, imageHeight - bottomSlice, leftSlice, bottomSlice), op);

        // Paint the left edge.
        // Have to scale and tile into the border rect.
        graphicsContext->drawTiledImage(image, colorSpace, IntRect(tx, ty + topWidth, leftWidth,
                                        h - topWidth - bottomWidth),
                                        IntRect(0, topSlice, leftSlice, imageHeight - topSlice - bottomSlice),
                                        Image::StretchTile, (Image::TileRule)vRule, op);
    }

    if (drawRight) {
        // Paint the top and bottom right corners
        // The top right corner rect is (tx + w - rightWidth, ty, rightWidth, topWidth)
        // The rect to use from within the image is obtained from our slice, and is (imageWidth - rightSlice, 0, rightSlice, topSlice)
        if (drawTop)
            graphicsContext->drawImage(image, colorSpace, IntRect(tx + w - rightWidth, ty, rightWidth, topWidth),
                                       IntRect(imageWidth - rightSlice, 0, rightSlice, topSlice), op);

        // The bottom right corner rect is (tx + w - rightWidth, ty + h - bottomWidth, rightWidth, bottomWidth)
        // The rect to use from within the image is (imageWidth - rightSlice, imageHeight - bottomSlice, rightSlice, bottomSlice)
        if (drawBottom)
            graphicsContext->drawImage(image, colorSpace, IntRect(tx + w - rightWidth, ty + h - bottomWidth, rightWidth, bottomWidth),
                                       IntRect(imageWidth - rightSlice, imageHeight - bottomSlice, rightSlice, bottomSlice), op);

        // Paint the right edge.
        graphicsContext->drawTiledImage(image, colorSpace, IntRect(tx + w - rightWidth, ty + topWidth, rightWidth,
                                        h - topWidth - bottomWidth),
                                        IntRect(imageWidth - rightSlice, topSlice, rightSlice, imageHeight - topSlice - bottomSlice),
                                        Image::StretchTile, (Image::TileRule)vRule, op);
    }

    // Paint the top edge.
    if (drawTop)
        graphicsContext->drawTiledImage(image, colorSpace, IntRect(tx + leftWidth, ty, w - leftWidth - rightWidth, topWidth),
                                        IntRect(leftSlice, 0, imageWidth - rightSlice - leftSlice, topSlice),
                                      (Image::TileRule)hRule, Image::StretchTile, op);

    // Paint the bottom edge.
    if (drawBottom)
        graphicsContext->drawTiledImage(image, colorSpace, IntRect(tx + leftWidth, ty + h - bottomWidth,
                                        w - leftWidth - rightWidth, bottomWidth),
                                        IntRect(leftSlice, imageHeight - bottomSlice, imageWidth - rightSlice - leftSlice, bottomSlice),
                                        (Image::TileRule)hRule, Image::StretchTile, op);

    // Paint the middle.
    if (drawMiddle) {
        graphicsContext->drawTiledImage(image, colorSpace, IntRect(tx + leftWidth, ty + topWidth, w - leftWidth - rightWidth,
                                        h - topWidth - bottomWidth),
                                        IntRect(leftSlice, topSlice, imageWidth - rightSlice - leftSlice, imageHeight - topSlice - bottomSlice),
                                        (Image::TileRule)hRule, (Image::TileRule)vRule, op);
    }

    if (zoom != 1.0f && scratchSurface) {
        ImageBuffer* buffer = scratchSurface->buffer();
        originalGC->drawImageBuffer(buffer, colorSpace, IntRect(scaledTx, scaledTy, scaledW, scaledH), IntRect(0, 0, w, h), op, false);
    }

    return true;
}

}
