#include "config.h"
#include "StyleCachedImage.h"

#include "CachedImage.h"
#include "RenderObject.h"

namespace WebCore {

float StyleCachedImage::resourceScale() const
{
	return m_image->resourceScale();
}

IntSize StyleCachedImage::imageTrueSize(const RenderObject*, float multiplier) const
{
	return m_image->trueImageSize(multiplier);
}

}
