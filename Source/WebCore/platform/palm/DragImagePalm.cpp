// Copyright 2008 Palm Inc.

#include "config.h"
#include "DragImage.h"

#include "CachedImage.h"
#include "GraphicsContext.h"
#include "PGSurface.h"
#include "PGContext.h"
#include "Image.h"

namespace WebCore {

IntSize dragImageSize(DragImageRef ref)
{
	PGSurface* s = (PGSurface*)ref;
	if( !s )
		return IntSize();
    return IntSize(s->width(), s->height());
}

void deleteDragImage(DragImageRef)
{
}

DragImageRef scaleDragImage(DragImageRef image, FloatSize)
{
    return image;
}
    
DragImageRef dissolveDragImageToFraction(DragImageRef image, float)
{
    return image;
}
        
DragImageRef createDragImageFromImage(Image* img)
{
    // Returning an image will result in a leak - and we don't support this on webOS
    // anyhow.
    return 0;
    //return (DragImageRef)img->getSurface();
}
    
DragImageRef createDragImageIconForCachedImage(CachedImage*)
{
    return 0;     
}
    
}
