#include "config.h"

#if ENABLE(VIDEO)
#if USE(ACCELERATED_COMPOSITING)
#include "VideoLayerPalm.h"

#include "LayerRendererPalm.h"
#include "MediaPlayerPrivatePalm.h"
#include "RenderLayerBacking.h"

#include <GLES2/gl2.h>
#define GL_GLEXT_PROTOTYPES
#include <GLES2/gl2ext.h>

#define TRACE(...)

namespace WebCore {

PassRefPtr<VideoLayerPalm> VideoLayerPalm::create(MediaPlayerPrivate* mp, GraphicsLayerPalm* owner)
{
    return adoptRef(new VideoLayerPalm(mp, owner));
}

// Rendering of eglimage-wrapped shared memory buffers is on-device only
bool VideoLayerPalm::supportsAcceleratedRendering()
{
#if defined(WEBOS_DESKTOP)
    return false;
#else
    return true;
#endif
}

VideoLayerPalm::VideoLayerPalm(MediaPlayerPrivate* mp, GraphicsLayerPalm* owner)
    : ContentLayerPalm(owner)
    , m_mp(mp)
{
    TRACE("%s\n", __FUNCTION__);

    for (int i = 0; i < sizeof(m_textureId) / sizeof(m_textureId[0]); i++)
        m_textureId[i] = 0;
}

VideoLayerPalm::~VideoLayerPalm()
{
    TRACE("%s\n", __FUNCTION__);

    for (int i = 0; i < sizeof(m_textureId) / sizeof(m_textureId[0]); i++) {
        if (m_textureId[i])
            layerRenderer()->deleteLayerTexture(m_textureId[i]);
    }
}

// Does nothing because we're compositing textures directly/
void VideoLayerPalm::updateContents()
{
    TRACE("%s\n", __PRETTY_FUNCTION__);
}

void VideoLayerPalm::draw()
{
    for (int i = 0; i < sizeof(m_textureId) / sizeof(m_textureId[0]); i++) {
        if (!m_textureId[i])
            m_textureId[i] = layerRenderer()->createLayerTexture();
    }

    m_contentsTexture = m_mp->mapBufferTexture(m_textureId);

    ASSERT(layerRenderer());
    const ContentLayerPalm::SharedValues* sv = layerRenderer()->contentLayerSharedValues();
    ASSERT(sv && sv->initialized());

    GLC(glActiveTexture(GL_TEXTURE0));
    GLC(glBindTexture(GL_TEXTURE_2D, m_contentsTexture));

    // Note: using BGRA here because source texture is assumed to be an eglimage wrapper
    layerRenderer()->useShader(sv->bgraContentShaderProgram());
    GLC(glUniform1i(sv->bgraShaderSamplerLocation(), 0));

    drawTexturedQuad(layerRenderer()->projectionMatrix(),
                     drawTransform(), m_bounds.width(), m_bounds.height(),
                     drawOpacity(), sv->bgraShaderMatrixLocation(),
                     sv->bgraShaderAlphaLocation());
}

}
#endif // USE(ACCELERATED_COMPOSITING)
#endif // ENABLE(VIDEO)
