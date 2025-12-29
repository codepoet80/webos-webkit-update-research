/**
 * Copyright (C) 2011, Hewlett Packard, Inc.
 */

#include "config.h"
#include "BasicLayerPalm.h"


#if USE(ACCELERATED_COMPOSITING)

namespace WebCore {

PassRefPtr<BasicLayerPalm> BasicLayerPalm::create(GraphicsLayerPalm* owner)
{
    return adoptRef(new BasicLayerPalm(owner));
}

BasicLayerPalm::BasicLayerPalm(GraphicsLayerPalm* owner)
    : ContentLayerPalm(owner)
{
}

void BasicLayerPalm::setColor(const Color& color)
{
    m_color = color;
    setNeedsDisplay();
}

void BasicLayerPalm::draw()
{
    if (m_skipsDraw)
        return;

    // Should probably be an ASSERT because we shouldn't get a full alpha color.
    // If we do then that means that RenderLayerBacking::isSimpleContainerCompositingLayer()
    // Isn't working correctly.
    if (m_color.alpha() == 0)
        return;

    drawFlatShadedQuad(m_color);
}


void BasicLayerPalm::updateContents()
{
    // No contents to update because this layer draws itself.
}

}

#endif // ACCELERATED_COMPOSITING
