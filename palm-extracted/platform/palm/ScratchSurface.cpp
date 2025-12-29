#include "ScratchSurface.h"
#include <wtf/OwnPtr.h>
#include "PGContext.h"

namespace WebCore {

ScratchSurface::ScratchSurface(int w, int h)
{
    m_imgBuff = ImageBuffer::create(IntSize(w, h));
}

void ScratchSurface::adjust(int w, int h)
{
    if (!m_imgBuff)
        return;

    // If the existing buffer is big enough and does not
    // exceed the size limit just clear the rect
    if (w <= m_imgBuff->width() && h <= m_imgBuff->height() && !timeToShrink(w, h)) {
        m_imgBuff->context()->clearRect(FloatRect(0, 0, w, h));
    }
    else {
        // free the old buffer and allocate a new one
        m_imgBuff.clear();
        m_imgBuff = ImageBuffer::create(IntSize(w, h));
    }
}

// if the current buffer size is over the
// max size limit, and the current request for the
// buffer is much smaller than what we have it's time to shrink
bool ScratchSurface::timeToShrink(int newWidth, int newHeight)
{
    int curSizeInBytes = m_imgBuff->width() * m_imgBuff->height();
    if (curSizeInBytes > maxSizeInBytes) {
        int requestedSize = newWidth * newHeight;
        if (curSizeInBytes * percent > requestedSize) {
            return true;
        }
    }
    return false;
}

}
