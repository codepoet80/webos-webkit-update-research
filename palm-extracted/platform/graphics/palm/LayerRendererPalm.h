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
/*
 * Copyright (C) 2010 Palm, Inc. All rights reserved.
 */


#ifndef LayerRendererPalm_h
#define LayerRendererPalm_h

#if USE(ACCELERATED_COMPOSITING)

#include "CanvasLayerPalm.h"
#include "ContentLayerPalm.h"
#include "IntRect.h"
#include "LayerPalm.h"
#include "palmwebview.h"

#if PLATFORM(CHROMIUM)
#include "SkBitmap.h"
#endif
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

#if PLATFORM(CG)
#include <CoreGraphics/CGContext.h>
#include <wtf/RetainPtr.h>
#endif

namespace WebCore {

class GLES2Context;

// Class that handles drawing of composited render layers using GL.
class LayerRendererPalm : public Noncopyable {
public:
    static PassOwnPtr<LayerRendererPalm> create(PassOwnPtr<GLES2Context> gles2Context);

    LayerRendererPalm(PassOwnPtr<GLES2Context> gles2Context);
    ~LayerRendererPalm();

    GLES2Context* context();

    // updates size of root texture, if needed, and scrolls the backbuffer.
    void prepareToDrawLayers(const IntRect& visibleRect, const IntRect& contentRect, const IntPoint& scrollPosition);

    // updates a rectangle within the root layer texture
    void updateRootLayerTextureRect(const IntRect& updateRect);

    // draws the current layers onto the backbuffer
    void drawLayers(const IntRect& visibleRect, const IntRect& contentRect, IntPoint scrollPosition);

    // draws the custom widgets on top of all the layers, e.g., Clipboard and Spelling
    void drawTopLayer(Palm::WebView* webView, const IntRect& updateRect);

    // waits for rendering to finish
    void finish();

    // puts backbuffer onscreen
    void present();

    void setRootLayer(PassRefPtr<LayerPalm> layer) { m_rootLayer = layer; }
    LayerPalm* rootLayer() { return m_rootLayer.get(); }
    void transferRootLayer(LayerRendererPalm* other) { other->m_rootLayer = m_rootLayer.release(); }

    bool hardwareCompositing() const { return m_hardwareCompositing; }

    void setRootLayerCanvasSize(const IntSize&);

    GraphicsContext* rootLayerGraphicsContext() const { return m_rootLayerGraphicsContext.get(); }
#if PLATFORM(PG)
    PGSurface* rootLayerBackingSurface() const { return m_rootLayerBackingSurface; }
#endif


    unsigned createLayerTexture();
    void deleteLayerTexture(unsigned);

    IntRect currentScissorRect() const { return m_currentScissorRect; }

    static void debugGLCall(const char* command, const char* file, int line);

    const TransformationMatrix& projectionMatrix() const { return m_projectionMatrix; }

    void useShader(unsigned);

    bool checkTextureSize(const IntSize&);

    const LayerPalm::SharedValues* layerSharedValues() const { return m_layerSharedValues.get(); }
    const ContentLayerPalm::SharedValues* contentLayerSharedValues() const { return m_contentLayerSharedValues.get(); }
    const CanvasLayerPalm::SharedValues* canvasLayerSharedValues() const { return m_canvasLayerSharedValues.get(); }

    void resizeOnscreenContent(const IntSize&);

    IntSize rootLayerTextureSize() const { return IntSize(m_rootLayerTextureWidth, m_rootLayerTextureHeight); }
    IntRect rootLayerContentRect() const { return m_rootContentRect; }

    void unmapTextures();
    void unmapLayerTexturesRecursive(LayerPalm* layer);
    
    void unlockTextures();
    void unlockLayerTexturesRecursive(LayerPalm* layer);

private:
    void updateLayersRecursive(LayerPalm* layer, const TransformationMatrix& parentMatrix, float opacity);

    void drawLayersRecursive(LayerPalm*);

    void drawLayer(LayerPalm*);

    bool isLayerVisible(LayerPalm*, const TransformationMatrix&, const IntRect& visibleRect);

    void drawLayerIntoStencilBuffer(LayerPalm*, bool decrement);

    void scissorToRect(const IntRect&);

    bool makeContextCurrent();

    bool initializeSharedObjects();
    void cleanupSharedObjects();

    unsigned m_rootLayerTextureId;
    int m_rootLayerTextureWidth;
    int m_rootLayerTextureHeight;

    // Scroll shader uniform locations.
    unsigned m_scrollShaderProgram;
    int m_scrollShaderSamplerLocation;
    int m_scrollShaderMatrixLocation;

    TransformationMatrix m_projectionMatrix;

    RefPtr<LayerPalm> m_rootLayer;

    IntPoint m_scrollPosition;
    bool m_hardwareCompositing;

    unsigned int m_currentShader;

#if PLATFORM(SKIA)
    OwnPtr<skia::PlatformCanvas> m_rootLayerCanvas;
    OwnPtr<PlatformContextSkia> m_rootLayerSkiaContext;
    OwnPtr<GraphicsContext> m_rootLayerGraphicsContext;
#elif PLATFORM(CG)
    Vector<uint8_t> m_rootLayerBackingStore;
    RetainPtr<CGContextRef> m_rootLayerCGContext;
    OwnPtr<GraphicsContext> m_rootLayerGraphicsContext;
#elif PLATFORM(PG)
    PGSurface* m_rootLayerBackingSurface;
    PGContext* m_rootLayerGraphicsContextPG;
    OwnPtr<GraphicsContext> m_rootLayerGraphicsContext;
#endif

    IntSize m_rootLayerCanvasSize;

    IntRect m_rootVisibleRect;
    IntRect m_rootContentRect;
    IntRect m_currentScissorRect;

    int m_maxTextureSize;

    int m_numStencilBits;

    // Store values that are shared between instances of each layer type
    // associated with this instance of the compositor. Since there can be
    // multiple instances of the compositor running in the same renderer process
    // we cannot store these values in static variables.
    OwnPtr<LayerPalm::SharedValues> m_layerSharedValues;
    OwnPtr<ContentLayerPalm::SharedValues> m_contentLayerSharedValues;
    OwnPtr<CanvasLayerPalm::SharedValues> m_canvasLayerSharedValues;

    OwnPtr<GLES2Context> m_context;
};

// Setting DEBUG_GL_CALLS to 1 will call glGetError() after almost every GL
// call made by the compositor. Useful for debugging rendering issues but
// will significantly degrade performance.
#define DEBUG_GL_CALLS 0

#if DEBUG_GL_CALLS && !defined ( NDEBUG )
#define GLC(x) { (x), LayerRendererPalm::debugGLCall(#x, __FILE__, __LINE__); }
#else
#define GLC(x) (x)
#endif


}

#endif // USE(ACCELERATED_COMPOSITING)

#endif
