#ifndef VideoLayerPalm_h
#define VideoLayerPalm_h

#if ENABLE(VIDEO)
#if USE(ACCELERATED_COMPOSITING)

#include "ContentLayerPalm.h"

namespace WebCore {

class MediaPlayerPrivate;

// A Layer that contains a Video element.
class VideoLayerPalm : public ContentLayerPalm {
public:
    static PassRefPtr<VideoLayerPalm> create(MediaPlayerPrivate* mp, GraphicsLayerPalm* owner = 0);
    static bool supportsAcceleratedRendering();

    virtual bool drawsContent() { return true; }
    virtual void updateContents();
    virtual void draw();

    ~VideoLayerPalm();

private:
    VideoLayerPalm(MediaPlayerPrivate* mp, GraphicsLayerPalm* owner);

    MediaPlayerPrivate* m_mp;
    int m_textureId[2];
};

}
#endif // USE(ACCELERATED_COMPOSITING)
#endif // ENABLE(VIDEO)

#endif
