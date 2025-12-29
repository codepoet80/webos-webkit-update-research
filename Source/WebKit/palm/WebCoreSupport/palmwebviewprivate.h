/*
 * palmwebviewprivate.h
 *
 *  Created on: Sep 23, 2010
 *      Author: Levi
 */

#ifndef PALMWEBVIEWPRIVATE_H_
#define PALMWEBVIEWPRIVATE_H_

#include "palmwebview.h"
#include "WebGLES2Context.h"
#include "config.h"
#if USE(ACCELERATED_COMPOSITING)
#include "GraphicsLayer.h"
#endif

namespace WebCore {
    class GLES2Context;
    class LayerRendererPalm;
	class Page;
	class IntRect;
}

class PGContext;

namespace Palm {

class WebViewPrivate {

	friend class WebView;

public:
    WebViewPrivate(WebView*);
    virtual ~WebViewPrivate();

	WebCore::Page* page() const;
    void paintWithContext(PGContext* context, const WebCore::IntRect& updateRect, bool mayScale);

#if USE(ACCELERATED_COMPOSITING)
    void setRootGraphicsLayer(WebCore::PlatformLayer*);
    void setIsAcceleratedCompositingActive(bool);
    void updateRootLayerContents(const WebCore::IntRect&, bool mayScale);
    void setRootLayerNeedsDisplay();

    bool isAcceleratedCompositingActive() const;


    OwnPtr<WebCore::LayerRendererPalm> m_layerRenderer;
    bool m_isAcceleratedCompositingActive;
#endif

#if USE(GLES2_RENDERING)
    WebGLES2Context* gles2Context();

    PassOwnPtr<WebCore::GLES2Context> getOnscreenGLES2Context();
    PassOwnPtr<WebCore::GLES2Context> getOffscreenGLES2Context();

    OwnPtr<WebGLES2Context> m_gles2Context;
#endif
    WebView* m_parent;
    unsigned long m_paintCount;
private:
	void doComposite(const WebCore::IntRect& updateRect, bool updateLayers, bool mayScale);
};

}

#endif /* PALMWEBVIEWPRIVATE_H_ */
