
// Copyright 2008 Palm Inc.

#include "config.h"
#include "ClipboardPalm.h"

#include "NotImplemented.h"
#include "StringHash.h"
#include "RenderObject.h"
#include "Editor.h"
#include "CString.h"
#include "KURL.h"
#include "PGContext.h"
#include "PGSurface.h"
#include "Frame.h"
#include "Node.h"
#include "HTMLElement.h"
#include "FileList.h"
#include "DragData.h"
#include "RenderImage.h"
#include "Pasteboard.h"

#include "webkitpalmclipboard.h"

namespace WebCore {
	
PassRefPtr<Clipboard> Editor::newGeneralClipboard(ClipboardAccessPolicy policy, Frame* frame )
{
    return ClipboardPalm::create(policy, false, 0, frame);
}	

PassRefPtr<Clipboard> Clipboard::create(ClipboardAccessPolicy policy, DragData* dragData, Frame* frame)
{
    return ClipboardPalm::create(policy, true, dragData->platformData(), frame);
}

ClipboardPalm::ClipboardPalm(ClipboardAccessPolicy policy, bool forDragging, PalmClipboard* cb, Frame* frame)
    : Clipboard(policy, forDragging)
{
	m_DragNode = 0;
	m_dragImage = 0;
	m_frame = frame;
	m_element = 0;
	m_systemClipboard = cb;
	m_destroySystemClipboard=true;
}

ClipboardPalm::~ClipboardPalm()
{
	if( m_dragImage )
		m_dragImage->releaseRef();   
}

void ClipboardPalm::clearData(const String& key)
{
	if( m_systemClipboard )
		palm_clipboard_clear( m_systemClipboard );
	
	HashMap<String,String>::iterator it = m_data.find(key);
	if( it != m_data.end() ) {
		m_data.remove(it);
	}
		
	m_DragNode = 0;
}

void ClipboardPalm::clearAllData()
{
	m_data.clear();
	m_DragNode = 0;
}

String ClipboardPalm::getData(const String& key, bool &success) const
{
    if (policy() != ClipboardReadable) {
        success = false;
        return String();
    }

	if( m_systemClipboard )
	{
		const char* r;
		if( palm_clipboard_get( m_systemClipboard, key.utf8().data(), &r ) )
		{
			success = true;
			return String::fromUTF8(r);
		}
	}
	
	if (!isForDragging())
	{
        success = true;
        return Pasteboard::generalPasteboard()->plainText(m_frame);
	}
	
	HashMap<String,String>::const_iterator it = m_data.find(key);
	if( it == m_data.end() ) {
		success = false;
		return "";
	}
	success = true;
    return it->second;
}

bool ClipboardPalm::setData(const String& type, const String& data)
{
    if (policy() != ClipboardWritable)
        return false;
	
	if( m_systemClipboard )
		palm_clipboard_set( m_systemClipboard, type.utf8().data(), data.utf8().data() );
	
	m_data.set( type, data );
	
	if (!isForDragging())
	{
        Pasteboard::generalPasteboard()->writePlainText(data);
	}
	
    return true;
}

HashSet<String> ClipboardPalm::types() const
{
    return HashSet<String>();
}

IntPoint ClipboardPalm::dragLocation() const
{
    return m_dragPt;
}

CachedImage* ClipboardPalm::dragImage() const
{
    return 0;
}

void ClipboardPalm::setDragImage(CachedImage*, const IntPoint&)
{
}

Node* ClipboardPalm::dragImageElement()
{
    return m_DragNode;
}

void ClipboardPalm::setDragImageElement(Node* node, const IntPoint& pt)
{
	m_dragPt = pt;
	m_DragNode = node;
}

DragImageRef ClipboardPalm::createDragImage(IntPoint& pt) const
{
	if(!m_frame)
		return (DragImageRef)0;

	IntRect imageRect;
	IntRect elementRect;
	//return m_frame->snapshotDragImage( m_DragNode, imageRect, elementRect );
	return 0;
}

static CachedImage* getCachedImage(Element* element)
{
    // Attempt to pull CachedImage from element
    ASSERT(element);
    RenderObject* renderer = element->renderer();
    if (!renderer || !renderer->isImage())
        return 0;

    RenderImage* image = static_cast<RenderImage*>(renderer);
    if (image->cachedImage() && !image->cachedImage()->errorOccurred())
        return image->cachedImage();

    return 0;
}

void ClipboardPalm::declareAndWriteDragImage(Element* element, const KURL& url, const String& label, Frame* frame)
{
	m_url = url.string();
	m_frame = frame;
	m_element = element;
	
	// Note: this is how you grab an image of what is being dragged.
	//CachedImage* image = getCachedImage(element);
    //if (!image || !image->isLoaded())
    //    return;	
}

void ClipboardPalm::writeURL(const KURL&, const String&, Frame*)
{
}

void ClipboardPalm::writeRange(Range*, Frame*)
{
}

void ClipboardPalm::writePlainText(const String&)
{
}

bool ClipboardPalm::hasData()
{
    return (m_dragImage != 0) ;
}

PassRefPtr<FileList> ClipboardPalm::files() const
{
	return 0;
}


}
