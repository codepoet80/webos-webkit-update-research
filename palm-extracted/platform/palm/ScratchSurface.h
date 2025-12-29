#ifndef ScratchSurface_h
#define ScratchSurface_h

#include "config.h"

#include "ImageBuffer.h"
#include <wtf/OwnPtr.h>
#include "GraphicsContext.h"

namespace WebCore {

    class ScratchSurface : public Noncopyable {
    public:
        // Reuse a single scratch surface for scaled 9-tile
        // drawing if existing surface is big enough.
        static ScratchSurface* get(int w, int h)
        {
            static ScratchSurface surface(w, h);
            surface.adjust(w, h);
            if (surface.isEmpty())
                return 0;
            return &surface;
        }

        ImageBuffer* buffer() { return m_imgBuff.get(); }
        bool isEmpty() { return !m_imgBuff; }

    private:
        ScratchSurface(int w, int h);

        void adjust(int w, int h);
        bool timeToShrink(int newWidth, int newHeight);

        WTF::OwnPtr<ImageBuffer> m_imgBuff;
        static const int maxSizeInBytes = 37500; // 150K = 37500 * 4 pixels
        static const float percent = 0.5; // 50%
    };
}

#endif
