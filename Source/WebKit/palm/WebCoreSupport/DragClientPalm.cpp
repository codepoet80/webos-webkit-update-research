
// Copyright 2008,2010 Palm Inc.

#include "config.h"
#include "DragClientPalm.h"
#include "ClipboardPalm.h"
#include "Page.h"
#include "Frame.h"
#include "Clipboard.h"
#include "FrameView.h"
#include "NotImplemented.h"
#include <stdio.h>             

#include "palmwebframe.h"
#include "palmwebframeprivate.h"
#include "palmwebpage.h"
#include "palmwebview.h"
#include "palmwebviewclient.h"

using namespace WebCore;

namespace WebKit {

void DragClient::dragControllerDestroyed()
{
    delete this;
}

void DragClient::willPerformDragDestinationAction(DragDestinationAction a, DragData*)
{
}

void DragClient::willPerformDragSourceAction(DragSourceAction a, const IntPoint&, Clipboard*)
{
}

DragDestinationAction DragClient::actionMaskForDrag(DragData*)
{
    return DragDestinationActionAny;
}

DragSourceAction DragClient::dragSourceActionMaskForPoint(const IntPoint&)
{
    return DragSourceActionAny;
}

// Start drag is supposed to block until the drag is finished. If you look in 
// DragController.cpp: at DragController::doSystemDrag() you can see this is the
// intention. 
void DragClient::startDrag(DragImageRef pgSurface, 
						   const IntPoint& dragImageOrigin, 
						   const IntPoint& eventPos, 
						   Clipboard* clipboardPalmType, 
						   Frame* frame, bool linkDrag)
{
	// This clipboard is the static drag clipboard from EventHandler.cpp (managed by EventHandler)
	// It is writable.
	
	if( !frame )
		return;
	
	Palm::WebFrame* webFrame = kit(frame->page()->mainFrame());
	if (!webFrame)
		return;

	Palm::WebViewClient* viewClient = webFrame->page()->view()->client();
	if (!viewClient)
		return;

	// NOTE: eventPos represents the MouseDown x,y and not the coordinates of the 
	// the MoveMove event which actually triggered the dragstart event
	viewClient->startDrag( eventPos.x(), eventPos.y(),
						   eventPos.x() - dragImageOrigin.x(), 
						   eventPos.y() - dragImageOrigin.y(), 
						   (void*)pgSurface, ((ClipboardPalm*)clipboardPalmType)->palmClipboard() );
	
}

DragImageRef DragClient::createDragImageForLink(KURL&, const String& label, Frame*)
{
    return 0;
}

}

