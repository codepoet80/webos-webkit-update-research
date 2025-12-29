/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "ContentLayerPalm.h"

#include "LayerRendererPalm.h"
#include "RenderLayerBacking.h"

#if PLATFORM(SKIA)
#include "NativeImageSkia.h"
#include "PlatformContextSkia.h"
#include "skia/ext/platform_canvas.h"
#elif PLATFORM(CG)
#include <CoreGraphics/CGBitmapContext.h>
#elif PLATFORM(PG)
#include <glib.h>
#include "PGSurface.h"
#include "PGThreadGlobalContext.h"
#include "PGContext.h"
#include "webkitpalmsettings.h"
#if !defined(WEBOS_DESKTOP)
#include <napp/NWindow.h>
#include <napp/NRemoteWindow.h>
#endif
#endif
#include <webkitstats.h>
#include <SimpleStats/SimpleStats.h>

#include <GLES2/gl2.h>


namespace WebCore {

#if PLATFORM(PG) && !defined(WEBOS_DESKTOP)

#include <EGL/egl.h>
    
// 
// ContentLayerSharedTexturePalm is a helper class which allows for
// eglimage backed texture sharing of webkit layers.  This allows us
// to minimize the amount of gpu mapping space while maximizing the
// number of cards, by unmapping textures from the gpu on demand,
// while preserving the actual texture memory.  Perf is better than
// destroying and recreating the textures; also, it may improve the
// texture update perf since upload is eliminated.
//
// Notes:
//
// - A webview api is exposed to unmap all layers on demand.  This
// should be called by webappmgr as necessary.  For testing, I call it
// after every paint when not direct rendering.
// 
// - Root layers are not managed by ContentLayerPalm, so I haven't
// done the work to convert root layers to this mechanism (exercise
// for the reader).
//
// - Currently NRemotePixmap is backed by sysv shared memory.  We'll
// be creating a lot of these, which I don't think is a problem; but
// since shared memory is not required, we could move the textures to
// regular malloc'd memory.
//
// c.stout
//
    
class ContentLayerSharedTexturePalm
{
public:

    ContentLayerSharedTexturePalm() {
        m_surface = 0;
        m_remoteTexture = 0;
        m_remoteTextureId = 0;
        m_remotePixmap = 0;
        m_width = 0;
        m_height = 0;

        m_useSharedTexture = PalmBrowserSettings()->sharedTexturesForCompositedLayers;
        //printf( "%s: m_useSharedTexture %d\n", __FUNCTION__, m_useSharedTexture);
    }

    ~ContentLayerSharedTexturePalm() {

        freeResources();
    }

    void setLayerBounds(int width, int height, int textureId) {

        if (m_layerBounds.width() == width && m_layerBounds.height() == height && m_remoteTextureId == textureId)
            return;

        m_layerBounds = IntSize(width, height);

        allocateResources(textureId);
    }

    void allocateResources(int textureId) {

        // Free any existing resources
        freeResources();

        int pitch = ((m_layerBounds.width() + 31) & ~31) * 4;
        //printf( "%s: w %d h %d pitch %d texture %d\n", __FUNCTION__, m_layerBounds.width(), m_layerBounds.height(), pitch, textureId);
        
        m_remoteTexture = new NRemoteTexture;
        {
        Palm::SimpleStats::Event evtStats(Palm::WebKitStats::getAcPixmapCreateEventData());
        webOS::Reporter::Event evt(0, "webkit.pixmap.create", "%d x %d", m_layerBounds.width(), m_layerBounds.height());
        m_remotePixmap = NPixmapPrivate::Create(m_layerBounds.width(), m_layerBounds.height(), pitch);
        }

        setSize(m_layerBounds.width(), m_layerBounds.height());

        m_remoteTextureId = textureId;
    }

    void mapTexture() {
        lockTexture();

        if (!m_remoteTextureId) {
            m_remoteTextureId = createTexture();
        }

        if (m_remoteTexture) {
            if (!m_remoteTexture->exists()) {
                //printf( "%s: mapping texture %d\n", __FUNCTION__, m_remoteTextureId);
                m_remoteTexture->create(eglGetCurrentDisplay(), m_remotePixmap, m_remoteTextureId);
            }
        }
    }

    void unmapTexture() {
        if (m_remoteTexture && m_remoteTexture->exists()) {
            //printf( "%s: UNmapping texture %d\n", __FUNCTION__, m_remoteTextureId);
            m_remoteTexture->destroy(eglGetCurrentDisplay());
        }

		if (m_remoteTextureId) {
            //printf( "%s: deleting texture id %d\n", __FUNCTION__, m_remoteTextureId);
            deleteTexture(m_remoteTextureId);
            m_remoteTextureId = 0;
        }
    }

    void lockTexture() {
        if (m_remotePixmap) {
            m_remotePixmap->Mlock();
        }
    }

    void unlockTexture() {
        if (m_remotePixmap) {
            m_remotePixmap->Munlock();
        }
    }

    void freeResources() {

        unmapTexture();
        
        if (m_remoteTexture) {
            delete m_remoteTexture;
            m_remoteTexture = 0;
        }

        if (m_remotePixmap) {
            m_remotePixmap->Free();
            delete m_remotePixmap;
            m_remotePixmap = 0;
        }

        if (m_surface) {
            m_surface->releaseRef();
            m_surface = 0;
        }
    }

    void setSize(int width, int height) {

        if (m_width == width && m_height == height && m_surface)
            return;

        m_width = width;
        m_height = height;

        if (m_surface)
            m_surface->releaseRef();

        PGContextIface* contextPG = PGThreadGlobalContext::instance()->graphicsContext();
        m_surface = contextPG->wrapBitmapData(m_width, m_height, false,
                                              (const unsigned char*) m_remotePixmap->Vaddr(),
                                              m_remotePixmap->Pitch());
    }

    unsigned int createTexture() {
        unsigned int textureId = 0;
        GLC(glGenTextures(1, &textureId));
        GLC(glBindTexture(GL_TEXTURE_2D, textureId));
        // Do basic linear filtering on resize.
        GLC(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
        GLC(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
        // NPOT textures in GL ES only work when the wrap mode is set to GL_CLAMP_TO_EDGE.
        GLC(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
        GLC(glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
        return textureId;
    }

    void deleteTexture(unsigned int textureId) {
        if (textureId == 0)
            return;

        GLC(glDeleteTextures(1, &textureId));
    }

    PGSurface* m_surface;
    NRemoteTexture* m_remoteTexture;
    unsigned int m_remoteTextureId;
    NPixmapPrivate* m_remotePixmap;
    int m_width;
    int m_height;
    IntSize m_layerBounds;
    bool m_useSharedTexture;
    IntRect m_dirtyRect;
};

#endif

// ----------------------------------------------------------------------------------------------------------------

ContentLayerPalm::SharedValues::SharedValues()
    : m_contentShaderProgram(0)
    , m_shaderSamplerLocation(-1)
    , m_shaderMatrixLocation(-1)
    , m_shaderAlphaLocation(-1)
    , m_bgraContentShaderProgram(0)
    , m_initialized(false)
{
    // Shaders for drawing the layer contents.
    char vertexShaderString[] =
        "attribute vec4 a_position;   \n"
        "attribute vec2 a_texCoord;   \n"
        "uniform mat4 matrix;         \n"
        "varying vec2 v_texCoord;     \n"
        "void main()                  \n"
        "{                            \n"
        "  gl_Position = matrix * a_position; \n"
        "  v_texCoord = a_texCoord;   \n"
        "}                            \n";

    // Color is in BGRA order.
    char fragmentShaderString[] =
        "precision mediump float;                            \n"
        "varying vec2 v_texCoord;                            \n"
        "uniform sampler2D s_texture;                        \n"
        "uniform float alpha;                                \n"
        "void main()                                         \n"
        "{                                                   \n"
        "  vec4 texColor = texture2D(s_texture, v_texCoord); \n"
        "  gl_FragColor = vec4(texColor.z, texColor.y, texColor.x, texColor.w) * alpha; \n"
        "}                                                   \n";

    // BGRA -> BGRA
    char bgraFragmentShaderString[] =
        "precision mediump float;                            \n"
        "varying vec2 v_texCoord;                            \n"
        "uniform sampler2D s_texture;                        \n"
        "uniform float alpha;                                \n"
        "void main()                                         \n"
        "{                                                   \n"
        "  vec4 texColor = texture2D(s_texture, v_texCoord); \n"
        "  gl_FragColor = vec4(texColor.x, texColor.y, texColor.z, texColor.w) * alpha; \n"
        "}                                                   \n";

    m_contentShaderProgram = createShaderProgram(vertexShaderString, fragmentShaderString);
    if (!m_contentShaderProgram) {
        LOG_ERROR("ContentLayerPalm: Failed to create shader program");
        return;
    }

    m_bgraContentShaderProgram = createShaderProgram(vertexShaderString , bgraFragmentShaderString);
    if (!m_bgraContentShaderProgram) {
        LOG_ERROR("ContentLayerPalm: Failed to create shader program");
        return;
    }

    m_shaderSamplerLocation = glGetUniformLocation(m_contentShaderProgram, "s_texture");
    m_shaderMatrixLocation = glGetUniformLocation(m_contentShaderProgram, "matrix");
    m_shaderAlphaLocation = glGetUniformLocation(m_contentShaderProgram, "alpha");
    m_bgraShaderSamplerLocation = glGetUniformLocation(m_bgraContentShaderProgram, "s_texture");
    m_bgraShaderMatrixLocation = glGetUniformLocation(m_bgraContentShaderProgram, "matrix");
    m_bgraShaderAlphaLocation = glGetUniformLocation(m_bgraContentShaderProgram, "alpha");
    ASSERT(m_shaderSamplerLocation != -1);
    ASSERT(m_shaderMatrixLocation != -1);
    ASSERT(m_shaderAlphaLocation != -1);
    ASSERT(m_bgraShaderSamplerLocation != -1);
    ASSERT(m_bgraShaderMatrixLocation != -1);
    ASSERT(m_bgraShaderAlphaLocation != -1);

    m_initialized = true;
}

ContentLayerPalm::SharedValues::~SharedValues()
{
    if (m_bgraContentShaderProgram)
        GLC(glDeleteProgram(m_bgraContentShaderProgram));

    if (m_contentShaderProgram)
        GLC(glDeleteProgram(m_contentShaderProgram));
}


PassRefPtr<ContentLayerPalm> ContentLayerPalm::create(GraphicsLayerPalm* owner)
{
    return adoptRef(new ContentLayerPalm(owner));
}

ContentLayerPalm::ContentLayerPalm(GraphicsLayerPalm* owner)
    : LayerPalm(owner)
    , m_contentsTexture(0)
    , m_skipsDraw(false)
#if DEBUG_ACCELERATED_COMPOSITING
    , m_backingSurface(0)
#endif
#if PLATFORM(PG) && !defined(WEBOS_DESKTOP)
    , m_sharedTexture(new ContentLayerSharedTexturePalm)
#endif
{
}

ContentLayerPalm::~ContentLayerPalm()
{
#if	PLATFORM(PG) && !defined(WEBOS_DESKTOP)
    if (m_sharedTexture->m_useSharedTexture) {
        // will be deleted by shared texture
        m_contentsTexture = 0;
    }
	delete m_sharedTexture;
#endif
    
    cleanupResources();
#if DEBUG_ACCELERATED_COMPOSITING
    if (m_backingSurface)
        m_backingSurface->releaseRef();
#endif
}

void ContentLayerPalm::cleanupResources()
{
    if (layerRenderer()) {
        if (m_contentsTexture) {
            layerRenderer()->deleteLayerTexture(m_contentsTexture);
            m_contentsTexture = 0;
        }
    }
}

bool ContentLayerPalm::requiresClippedUpdateRect() const
{
    // To avoid allocating excessively large textures, switch into "large layer mode" if
    // one of the layer's dimensions is larger than 2000 pixels or the size of
    // surface it's rendering into. This is a temporary measure until layer tiling is implemented.
    static GLint maxTextureSize = 0;

    if (!maxTextureSize)
        GLC(glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize));

    return (m_bounds.width() > max(maxTextureSize, layerRenderer()->rootLayerContentRect().width())
            || m_bounds.height() > max(maxTextureSize, layerRenderer()->rootLayerContentRect().height()));
}

void ContentLayerPalm::calculateClippedUpdateRect(IntRect& dirtyRect, IntRect& drawRect) const
{
    // For the given layer size and content rect, calculate:
    // 1) The minimal texture space rectangle to be uploaded, returned in dirtyRect.
    // 2) The rectangle to draw this texture in relative to the target render surface, returned in drawRect.

    ASSERT(layerRenderer());
    const FloatRect clipRect = layerRenderer()->rootLayerContentRect();

    TransformationMatrix layerOriginTransform = drawTransform();
    layerOriginTransform.translate3d(-0.5 * m_bounds.width(), -0.5 * m_bounds.height(), 0);

    // For now we apply the large layer treatment only for layers that are either untransformed
    // or are purely translated. Their matrix is expected to be invertible.
    ASSERT(layerOriginTransform.isInvertible());

    TransformationMatrix targetToLayerMatrix = layerOriginTransform.inverse();
    FloatRect clipRectInLayerCoords = targetToLayerMatrix.mapRect(clipRect);
    clipRectInLayerCoords.intersect(IntRect(0, 0, m_bounds.width(), m_bounds.height()));

    dirtyRect = IntRect(clipRectInLayerCoords);

    // Map back to the target surface coordinate system.
    drawRect = IntRect(layerOriginTransform.mapRect(clipRectInLayerCoords));
}

void ContentLayerPalm::unmapTexture() 
{
#if	PLATFORM(PG) && !defined(WEBOS_DESKTOP)    
    if (m_sharedTexture->m_useSharedTexture) {
        m_sharedTexture->unmapTexture();

        m_contentsTexture = 0;
    }
#endif    
}

void ContentLayerPalm::unlockTexture()
{
#if PLATFORM(PG) && !defined(WEBOS_DESKTOP)
    if (m_sharedTexture->m_useSharedTexture) {
        m_sharedTexture->unlockTexture();
    }
#endif
}

void ContentLayerPalm::dumpMemStats(std::ostream &output)
{
#if PLATFORM(PG) && !defined(WEBOS_DESKTOP)
    NPixmapPrivate::MemStats(output);
#endif
}
        
void ContentLayerPalm::updateContents()
{
    Palm::SimpleStats::Event evt(Palm::WebKitStats::getAcUpdateContentsEventData());
    webOS::Reporter::Event evt_function(this, "webkit.compositing.updateContents");
    RenderLayerBacking* backing = static_cast<RenderLayerBacking*>(m_owner->client());
    if (!backing || backing->paintingGoesToWindow())
        return;

    ASSERT(drawsContent());

    ASSERT(layerRenderer());

    void* pixels = 0;
    IntRect dirtyRect;
    IntRect updateRect;
    IntSize requiredTextureSize;
    IntSize bitmapSize;

    // FIXME: Remove this test when tiled layers are implemented.
    if (requiresClippedUpdateRect()) {
        // A layer with 3D transforms could require an arbitrarily large number
        // of texels to be repainted, so ignore these layers until tiling is
        // implemented.
        if (!drawTransform().isIdentityOrTranslation()) {
            m_skipsDraw = true;
            return;
        }

        calculateClippedUpdateRect(dirtyRect, m_largeLayerDrawRect);
        if (!layerRenderer()->checkTextureSize(m_largeLayerDrawRect.size())) {
            m_skipsDraw = true;
            return;
        }

        // If the portion of the large layer that's visible hasn't changed
        // then we don't need to update it, _unless_ its contents have changed
        // in which case we only update the dirty bits.
        if (m_largeLayerDirtyRect == dirtyRect) {
            if (!m_dirtyRect.intersects(dirtyRect))
                return;
            dirtyRect.intersect(IntRect(m_dirtyRect));
            updateRect = dirtyRect;
            updateRect.setX(updateRect.x() - m_largeLayerDirtyRect.x());
            updateRect.setY(updateRect.y() - m_largeLayerDirtyRect.y());
            requiredTextureSize = m_largeLayerDirtyRect.size();
        } else {
            m_largeLayerDirtyRect = dirtyRect;
            requiredTextureSize = dirtyRect.size();
            updateRect = IntRect(IntPoint(0, 0), dirtyRect.size());
        }
    } else {
        dirtyRect = IntRect(m_dirtyRect);
        IntRect boundsRect(IntPoint(0, 0), m_bounds);
        requiredTextureSize = m_bounds;
        // If the texture needs to be reallocated then we must redraw the entire
        // contents of the layer.
        if (requiredTextureSize != m_allocatedTextureSize)
            dirtyRect = boundsRect;
        else {
            // Clip the dirtyRect to the size of the layer to avoid drawing
            // outside the bounds of the backing texture.
            dirtyRect.intersect(boundsRect);
        }
        updateRect = dirtyRect;
    }

    if (dirtyRect.isEmpty())
        return;

    //printf( "%s with dirtyRect x %d y %d w %d h %d\n", __FUNCTION__, dirtyRect.x(), dirtyRect.y(), dirtyRect.width(), dirtyRect.height() );

#if PLATFORM(SKIA)
    const SkBitmap* skiaBitmap = 0;
    OwnPtr<skia::PlatformCanvas> canvas;
    OwnPtr<PlatformContextSkia> skiaContext;
    OwnPtr<GraphicsContext> graphicsContext;

    canvas.set(new skia::PlatformCanvas(dirtyRect.width(), dirtyRect.height(), false));
    skiaContext.set(new PlatformContextSkia(canvas.get()));

    // This is needed to get text to show up correctly.
    // FIXME: Does this take us down a very slow text rendering path?
    skiaContext->setDrawingToImageBuffer(true);

    graphicsContext.set(new GraphicsContext(reinterpret_cast<PlatformGraphicsContext*>(skiaContext.get())));

    // Bring the canvas into the coordinate system of the paint rect.
    canvas->translate(static_cast<SkScalar>(-dirtyRect.x()), static_cast<SkScalar>(-dirtyRect.y()));

    m_owner->paintGraphicsLayerContents(*graphicsContext, dirtyRect);
    const SkBitmap& bitmap = canvas->getDevice()->accessBitmap(false);
    skiaBitmap = &bitmap;
    ASSERT(skiaBitmap);

    SkAutoLockPixels lock(*skiaBitmap);
    SkBitmap::Config skiaConfig = skiaBitmap->config();
    // FIXME: do we need to support more image configurations?
    if (skiaConfig == SkBitmap::kARGB_8888_Config) {
        pixels = skiaBitmap->getPixels();
        bitmapSize = IntSize(skiaBitmap->width(), skiaBitmap->height());
    }
#elif PLATFORM(CG)
    Vector<uint8_t> tempVector;
    int rowBytes = 4 * dirtyRect.width();
    tempVector.resize(rowBytes * dirtyRect.height());
    memset(tempVector.data(), 0, tempVector.size());
    RetainPtr<CGColorSpaceRef> colorSpace(AdoptCF, CGColorSpaceCreateDeviceRGB());
    RetainPtr<CGContextRef> contextCG(AdoptCF, CGBitmapContextCreate(tempVector.data(),
                                                                     dirtyRect.width(), dirtyRect.height(), 8, rowBytes,
                                                                     colorSpace.get(),
                                                                     kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host));
    CGContextTranslateCTM(contextCG.get(), 0, dirtyRect.height());
    CGContextScaleCTM(contextCG.get(), 1, -1);

    GraphicsContext graphicsContext(contextCG.get());

    // Translate the graphics context into the coordinate system of the dirty rect.
    graphicsContext.translate(-dirtyRect.x(), -dirtyRect.y());

    m_owner->paintGraphicsLayerContents(graphicsContext, dirtyRect);

    pixels = tempVector.data();
    bitmapSize = dirtyRect.size();
#elif PLATFORM(PG)

    /*        
#if PLATFORM(PG) && !defined(WEBOS_DESKTOP)
    if (m_sharedTexture->m_useSharedTexture) {
        m_sharedTexture->setLayerBounds(requiredTextureSize.width(), requiredTextureSize.height());
        
        PGContextIface* contextPG = PGThreadGlobalContext::instance()->graphicsContext();
        PGSurface* surf = m_sharedTexture->m_surface;
        surf->addRef();

        contextPG->setSurface(surf);
        contextPG->clearRect(dirtyRect.x(), dirtyRect.y(), dirtyRect.x() + dirtyRect.width(), dirtyRect.y() + dirtyRect.height());
        
        GraphicsContext graphicsContext(static_cast<PlatformGraphicsContext*>(contextPG));
        graphicsContext.save();

        // Bring the canvas into the coordinate system of the paint rect.
        //graphicsContext.translate(-dirtyRect.x(), -dirtyRect.y());
        m_owner->paintGraphicsLayerContents(graphicsContext, dirtyRect);

        graphicsContext.restore();

#if DEBUG_ACCELERATED_COMPOSITING
        // FIXME: Leak?
        if (m_backingSurface)
            m_backingSurface->releaseRef();
        m_backingSurface = surf;
        m_backingSurface->addRef();
#else
        surf->releaseRef();
#endif

        // from UpdateTextureRect
        m_allocatedTextureSize = requiredTextureSize;
        m_dirtyRect.setSize(FloatSize());
        // Large layers always stay dirty, because they need to update when the content rect changes.
        m_contentsDirty = requiresClippedUpdateRect();        
        return;
    }
#endif
    */

    PGContextIface* contextPG = PGThreadGlobalContext::instance()->graphicsContext();
    PGSurface* surf = 0;

#if	PLATFORM(PG) && !defined(WEBOS_DESKTOP)    
    if (m_sharedTexture->m_useSharedTexture) {
        m_sharedTexture->m_dirtyRect = dirtyRect;
    }
    else
#endif
    {
        surf = PGSurface::create(dirtyRect.width(), dirtyRect.height(), false);
        memset(surf->raster(), 0, surf->rowbytes() * surf->height());

        contextPG->setSurface(surf);
        GraphicsContext graphicsContext(static_cast<PlatformGraphicsContext*>(contextPG));

        graphicsContext.save();

        // Bring the canvas into the coordinate system of the paint rect.
        graphicsContext.translate(-dirtyRect.x(), -dirtyRect.y());

        m_owner->paintGraphicsLayerContents(graphicsContext, dirtyRect);

        pixels = surf->raster();

        graphicsContext.restore();        
    }
    
    bitmapSize = dirtyRect.size();
#else
#error "Need to implement for your platform."
#endif

    unsigned textureId = m_contentsTexture;
    if (!textureId)
        textureId = layerRenderer()->createLayerTexture();

#if	PLATFORM(PG) && !defined(WEBOS_DESKTOP)
    // Call this even with null pixels
    updateTextureRect(pixels, bitmapSize, requiredTextureSize, updateRect, textureId);
#else    
    if (pixels)
        updateTextureRect(pixels, bitmapSize, requiredTextureSize, updateRect, textureId);
#endif
    
#if PLATFORM(PG)    
    if (surf) {
#if DEBUG_ACCELERATED_COMPOSITING
        if (m_backingSurface)
            m_backingSurface->releaseRef();
        m_backingSurface = surf;
        m_backingSurface->addRef();
#endif
        surf->releaseRef();
    }
#endif    
}

void ContentLayerPalm::updateTextureRect(void* pixels, const IntSize& bitmapSize, const IntSize& requiredTextureSize, const IntRect& updateRect, unsigned textureId)
{
#if	PLATFORM(PG) && !defined(WEBOS_DESKTOP)
    // Null pixels a-ok for shared textures
#else
    if (!pixels)
        return;
#endif

    glBindTexture(GL_TEXTURE_2D, textureId);
    
    // If the texture id or size changed since last time then we need to tell GL
    // to re-allocate a texture.

    // HACK: if the bitmapSize doesn't match the requiredTextureSize, then doing a glTexImage2D will cause a crash.
    // In this scenario the updateRect is set up correctly and matches bitmapSize. We therefore do texSubImage2D
    // This is a temporary hack. We should understand why webkit gets to this point and has mismatched
    // sizes, yet has a correct updateRect
    if ((bitmapSize == requiredTextureSize) && (m_contentsTexture != textureId || requiredTextureSize != m_allocatedTextureSize)) {

#if PLATFORM(PG) && !defined(WEBOS_DESKTOP)
        if (m_sharedTexture->m_useSharedTexture) {
            m_sharedTexture->setLayerBounds(requiredTextureSize.width(), requiredTextureSize.height(), textureId);

            // Now update it
            PGContextIface* contextPG = PGThreadGlobalContext::instance()->graphicsContext();
            PGSurface* surf = m_sharedTexture->m_surface;
            if (surf) {
                surf->addRef();

                contextPG->setSurface(surf);
                contextPG->clearRect(0, 0, requiredTextureSize.width(), requiredTextureSize.height());

                if (pixels) {
                    // Copy in the bits
                    PGSurface* image = contextPG->wrapBitmapData(requiredTextureSize.width(), requiredTextureSize.height(),
                                                                 false, pixels);
                    contextPG->bitblt(image, 0, 0, requiredTextureSize.width(), requiredTextureSize.height());

                    image->releaseRef();
                }
                else {
                    // Paint
                    GraphicsContext graphicsContext(static_cast<PlatformGraphicsContext*>(contextPG));
                    graphicsContext.save();

                    IntRect& dirtyRect = m_sharedTexture->m_dirtyRect;
                    // Bring the canvas into the coordinate system of the paint rect.
                    graphicsContext.translate(-dirtyRect.x(), -dirtyRect.y());
                    m_owner->paintGraphicsLayerContents(graphicsContext, dirtyRect);

                    graphicsContext.restore();
                }

                surf->releaseRef();
            }
        }
        else
#endif
        {
            Palm::SimpleStats::Event evt(Palm::WebKitStats::getAcTextureUpdateEventData());
            webOS::Reporter::Event evt_textupload(this, "webkit.compositing.texture.upload", "%s:%d\n TextureId = %i \n Required Texture Size = (w,h) (%i, %i)\n ",
                    __PRETTY_FUNCTION__, __LINE__, textureId, requiredTextureSize.width(), requiredTextureSize.height());
            GLC(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, requiredTextureSize.width(), requiredTextureSize.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels));
	    }

        m_contentsTexture = textureId;
        m_allocatedTextureSize = requiredTextureSize;
    } else {

        ASSERT(updateRect.width() <= m_allocatedTextureSize.width() && updateRect.height() <= m_allocatedTextureSize.height());
        ASSERT(updateRect.width() == bitmapSize.width() && updateRect.height() == bitmapSize.height());
#if PLATFORM(WEBOS)

#if PLATFORM(PG) && !defined(WEBOS_DESKTOP)
        if (m_sharedTexture->m_useSharedTexture) {
            // Just update it
            PGContextIface* contextPG = PGThreadGlobalContext::instance()->graphicsContext();
            PGSurface* surf = m_sharedTexture->m_surface;
            if (surf) {
                surf->addRef();

                IntRect& dirtyRect = m_sharedTexture->m_dirtyRect;
                int xoffset = -dirtyRect.x() + updateRect.x();
                int yoffset = -dirtyRect.y() + updateRect.y();

                contextPG->setSurface(surf);

                contextPG->push();
                contextPG->clipRect(updateRect.x(), updateRect.y(), updateRect.x() + updateRect.width(), updateRect.y() + updateRect.height());

                contextPG->clearRect(updateRect.x(), updateRect.y(), updateRect.x() + updateRect.width(), updateRect.y() + updateRect.height());

                if (pixels) {
                    // Copy in the bits
                    Palm::SimpleStats::Event evt(Palm::WebKitStats::getAcTextureUpdateEventData());
                    webOS::Reporter::Event evt_textupload(this, "webkit.compositing.texture.blit", "%s:%d\n %d x %d @ (%d, %d)\n",
                                                          __PRETTY_FUNCTION__, __LINE__, updateRect.width(), updateRect.height(), updateRect.x(), updateRect.y());

                    PGSurface* image = contextPG->wrapBitmapData(requiredTextureSize.width(), requiredTextureSize.height(),
                                                                 false, pixels);
                    contextPG->bitblt(image, updateRect.x(), updateRect.y(), updateRect.x() + updateRect.width(), updateRect.y() + updateRect.height());

                    image->releaseRef();
                }
                else {
                    // Paint
                    GraphicsContext graphicsContext(static_cast<PlatformGraphicsContext*>(contextPG));
                    graphicsContext.save();

                    // Bring the canvas into the coordinate system of the paint rect.
                    graphicsContext.translate(xoffset, yoffset);
                    m_owner->paintGraphicsLayerContents(graphicsContext, dirtyRect);

                    graphicsContext.restore();
                }
                contextPG->pop();

                surf->releaseRef();
            }
        }
        else {
            Palm::SimpleStats::Event evtStats(Palm::WebKitStats::getAcTextureUpdateEventData());
            webOS::Reporter::Event evt(this, "webkit.compositing.texture.upload",
                "%s:%d \n TextureId = %i \n(x,y) (%i, %i)\n(w,h) (%i,%i)\n ", __PRETTY_FUNCTION__, __LINE__,
                textureId, updateRect.x(), updateRect.y(), updateRect.width(), updateRect.height());

#endif
            GLC(glTexSubImage2D(GL_TEXTURE_2D, 0, updateRect.x(), updateRect.y(), updateRect.width(), updateRect.height(), GL_RGBA, GL_UNSIGNED_BYTE, pixels));
#else
            GLC(context, context->texSubImage2D(GraphicsContext3D::TEXTURE_2D, 0, updateRect.x(), updateRect.y(), updateRect.width(), updateRect.height(), GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE, pixels));
#endif
#if PLATFORM(PG) && !defined(WEBOS_DESKTOP)
        }
#endif
    }

    m_dirtyRect.setSize(FloatSize());

    // Large layers always stay dirty, because they need to update when the content rect changes.
    m_contentsDirty = requiresClippedUpdateRect();
   
    // Given a texture to use, but decided not to so deleting it now.
    if (textureId && m_contentsTexture != textureId && layerRenderer())
        layerRenderer()->deleteLayerTexture(textureId);
}

void ContentLayerPalm::draw()
{
    if (m_skipsDraw)
        return;

    ASSERT(layerRenderer());
    const ContentLayerPalm::SharedValues* sv = layerRenderer()->contentLayerSharedValues();
    ASSERT(sv && sv->initialized());

#if PLATFORM(PG) && !defined(WEBOS_DESKTOP)
    if (m_sharedTexture->m_useSharedTexture) {

        // CAS: ensure texture is mapped in here
        m_sharedTexture->mapTexture();
        
        GLC(glActiveTexture(GL_TEXTURE0));
        GLC(glBindTexture(GL_TEXTURE_2D, m_sharedTexture->m_remoteTextureId));
        layerRenderer()->useShader(sv->bgraContentShaderProgram());
        GLC(glUniform1i(sv->bgraShaderSamplerLocation(), 0));

        if (requiresClippedUpdateRect()) {
            float m43 = drawTransform().m43();
            TransformationMatrix transform;
            transform.translate3d(m_largeLayerDrawRect.center().x(), m_largeLayerDrawRect.center().y(), m43);
            drawTexturedQuad(layerRenderer()->projectionMatrix(),
                             transform, m_largeLayerDrawRect.width(),
                             m_largeLayerDrawRect.height(), drawOpacity(),
                             sv->bgraShaderMatrixLocation(), sv->bgraShaderAlphaLocation());
        } else {
            drawTexturedQuad(layerRenderer()->projectionMatrix(),
                             drawTransform(), m_bounds.width(), m_bounds.height(),
                             drawOpacity(), sv->bgraShaderMatrixLocation(),
                             sv->bgraShaderAlphaLocation());

        }
        return;
    }
#endif

    GLC(glActiveTexture(GL_TEXTURE0));
    GLC(glBindTexture(GL_TEXTURE_2D, m_contentsTexture));
    layerRenderer()->useShader(sv->contentShaderProgram());
    GLC(glUniform1i(sv->shaderSamplerLocation(), 0));

    if (requiresClippedUpdateRect()) {
        float m43 = drawTransform().m43();
        TransformationMatrix transform;
        transform.translate3d(m_largeLayerDrawRect.center().x(), m_largeLayerDrawRect.center().y(), m43);
        drawTexturedQuad(layerRenderer()->projectionMatrix(),
                         transform, m_largeLayerDrawRect.width(),
                         m_largeLayerDrawRect.height(), drawOpacity(),
                         sv->shaderMatrixLocation(), sv->shaderAlphaLocation());
    } else {
        drawTexturedQuad(layerRenderer()->projectionMatrix(),
                         drawTransform(), m_bounds.width(), m_bounds.height(),
                         drawOpacity(), sv->shaderMatrixLocation(),
                         sv->shaderAlphaLocation());

    }
}

}
#endif // USE(ACCELERATED_COMPOSITING)
