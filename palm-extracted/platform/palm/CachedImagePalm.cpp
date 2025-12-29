#include "config.h"
#include "CachedImage.h"
#include "BitmapImage.h"

using std::max;

namespace WebCore {

CachedImage::CachedImage(const String& url, float scale)
    : CachedResource(url, ImageResource)
    , m_image(0)
    , m_decodedDataDeletionTimer(this, &CachedImage::decodedDataDeletionTimerFired)
    , m_httpStatusCodeErrorOccurred(false)
    , m_resourceScale(scale)
{
    setStatus(Unknown);
}

/*
 * In situations when we load a scale-specific resource
 * we need to lie to webkit about the image size in order to
 * keep the same page layout. We load bigger images for display
 * quality, but would still like them to occupy the same rect
 * on the page. Look at adjustPathForScaleFactor in CachePalm.cpp
 */
IntSize CachedImage::imageSize(float multiplier) const
{
	return imageSize(multiplier, false);
}

IntSize CachedImage::trueImageSize(float multiplier) const
{
	return imageSize(multiplier, true);
}

IntSize CachedImage::imageSize(float multiplier, bool useTrueSize) const
{
	ASSERT(!isPurgeable());
	if (!m_image)
        return IntSize();

	IntSize imgSize(m_image->size());
	if (m_resourceScale != 1.0 && !useTrueSize) {
		int w = static_cast<float>(imgSize.width()) / m_resourceScale + 0.5;
		imgSize.setWidth(w);
		int h = static_cast<float>(imgSize.height()) / m_resourceScale + 0.5;
		imgSize.setHeight(h);
	}

	if (multiplier == 1.0f)
		return imgSize;

	// Don't let images that have a width/height >= 1 shrink below 1 when zoomed.
	int w = imgSize.width();
	int h = imgSize.height();

	bool hasWidth = imgSize.width() > 0;
	bool hasHeight = imgSize.height() > 0;
	int width = imgSize.width() * (m_image->hasRelativeWidth() ? 1.0f : multiplier);
	int height = imgSize.height() * (m_image->hasRelativeHeight() ? 1.0f : multiplier);
	if (hasWidth)
		width = max(1, width);
	if (hasHeight)
		height = max(1, height);
	return IntSize(width, height);

}


}
