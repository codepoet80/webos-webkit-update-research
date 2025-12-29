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


#ifndef ContentLayerPalm_h
#define ContentLayerPalm_h

#if USE(ACCELERATED_COMPOSITING)

#include "LayerPalm.h"
#include <ostream>

namespace WebCore {

class ContentLayerSharedTexturePalm;

// A Layer that requires a GraphicsContext to render its contents.
class ContentLayerPalm : public LayerPalm {
    friend class LayerRendererPalm;
public:
    static PassRefPtr<ContentLayerPalm> create(GraphicsLayerPalm* owner = 0);

    ~ContentLayerPalm();

    virtual void updateContents();
    virtual void draw();
    virtual bool drawsContent() { return m_owner && m_owner->drawsContent(); }
    virtual void unmapTexture();

    virtual void unlockTexture();

    static void dumpMemStats(std::ostream &output);

    // Stores values that are shared between instances of this class that are
    // associated with the same LayerRendererPalm (and hence the same GL
    // context).
    class SharedValues {
    public:
        SharedValues();
        ~SharedValues();

        unsigned contentShaderProgram() const { return m_contentShaderProgram; }
        int shaderSamplerLocation() const { return m_shaderSamplerLocation; }
        int shaderMatrixLocation() const { return m_shaderMatrixLocation; }
        int shaderAlphaLocation() const { return m_shaderAlphaLocation; }

        unsigned bgraContentShaderProgram() const { return m_bgraContentShaderProgram; }
        int bgraShaderSamplerLocation() const { return m_bgraShaderSamplerLocation; }
        int bgraShaderMatrixLocation() const { return m_bgraShaderMatrixLocation; }
        int bgraShaderAlphaLocation() const { return m_bgraShaderAlphaLocation; }

        int initialized() const { return m_initialized; }

    private:
        unsigned m_contentShaderProgram;
        int m_shaderSamplerLocation;
        int m_shaderMatrixLocation;
        int m_shaderAlphaLocation;
		unsigned int m_bgraContentShaderProgram;
		int m_bgraShaderSamplerLocation;
		int m_bgraShaderMatrixLocation;
		int m_bgraShaderAlphaLocation;
        int m_initialized;
    };

#if DEBUG_ACCELERATED_COMPOSITING
    virtual PGSurface* getRenderLayerBacking() { return m_backingSurface; }
#endif

protected:
    ContentLayerPalm(GraphicsLayerPalm* owner);

    void updateTextureRect(void* pixels, const IntSize& bitmapSize, const IntSize& requiredTextureSize,
                           const IntRect& updateRect, unsigned textureId);

    virtual void cleanupResources();
    bool requiresClippedUpdateRect() const;

    unsigned m_contentsTexture;
    IntSize m_allocatedTextureSize;
    bool m_skipsDraw;
#if DEBUG_ACCELERATED_COMPOSITING
    PGSurface* m_backingSurface;
#endif
#if PLATFORM(PG) && !defined(WEBOS_DESKTOP)
    ContentLayerSharedTexturePalm* m_sharedTexture;
#endif

private:
    void calculateClippedUpdateRect(IntRect& dirtyRect, IntRect& drawRect) const;
    IntRect m_largeLayerDrawRect;
    IntRect m_largeLayerDirtyRect;
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif
