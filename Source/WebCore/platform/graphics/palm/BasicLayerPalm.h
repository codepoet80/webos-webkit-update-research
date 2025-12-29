/*
 * Copyright (C) 2011 Hewlett Packard Inc. All rights reserved.
 *
 */

#ifndef BasicLayerPalm_h
#define BasicLayerPalm_h

#if USE(ACCELERATED_COMPOSITING)

#include "ContentLayerPalm.h"
#include <PContext.h>

namespace WebCore {

/**
 * A very basic layer that is either totally transparent (nothing drawn)
 * or filled with a flat color where every pixel is the same color.
 */
class BasicLayerPalm : public ContentLayerPalm {
public:
    static PassRefPtr<BasicLayerPalm> create(GraphicsLayerPalm* owner = 0);

    virtual void updateContents();
    virtual bool drawsContent() { return true; }
    virtual void draw();

    void setColor(const Color& color);

private:
    BasicLayerPalm(GraphicsLayerPalm* owner);

    Color m_color;
};

}
#endif // USE(ACCELERATED_COMPOSITING)

#endif

