
#ifndef ClipboardPalm_h
#define ClipboardPalm_h

#include "Clipboard.h"

#include "PlatformString.h"
#include "CachedResourceClient.h"
#include "StringHash.h"
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/PassRefPtr.h>

struct PalmClipboard;

class PGSurface;

namespace WebCore {
    class CachedImage;
    class Frame;

    // State available during IE's events for drag and drop and copy/paste
    // Created from the EventHandlerPalm to be used by the dom
    class ClipboardPalm : public Clipboard, public CachedResourceClient {
    public:
		static PassRefPtr<ClipboardPalm> create( ClipboardAccessPolicy p, bool b, PalmClipboard* cb=0, Frame* frame=0 ) {
			return adoptRef( new ClipboardPalm( p, b, cb ) );
		}


        void clearData(const String&);
        void clearAllData();
        String getData(const String&, bool&) const;
        bool setData(const String&, const String&);

        HashSet<String> types() const;
        IntPoint dragLocation() const;
        CachedImage* dragImage() const;
        void setDragImage(CachedImage*, const IntPoint&);
        Node* dragImageElement();
        void setDragImageElement(Node*, const IntPoint&);

        virtual DragImageRef createDragImage(IntPoint&) const;
        virtual void declareAndWriteDragImage(Element*, const KURL&, const String&, Frame*);
        virtual void writeURL(const KURL&, const String&, Frame*);
        virtual void writeRange(Range*, Frame*);
        virtual void writePlainText(const String&);
		virtual PassRefPtr<FileList> files() const;
		
		PalmClipboard* palmClipboard() { return m_systemClipboard; }
		void releaseSystemClipboard() { m_destroySystemClipboard=false; }

        virtual bool hasData();
		
private:
        ClipboardPalm(ClipboardAccessPolicy, bool, PalmClipboard* cb=0, Frame* frame=0 );
        ~ClipboardPalm();
		
		HashMap<String,String> m_data;
		PalmClipboard* m_systemClipboard;
		bool m_destroySystemClipboard;
		PGSurface* m_dragImage;
		Node* m_DragNode;
		String m_url;
		Element* m_element;
		IntPoint m_dragPt;
		Frame* m_frame;
    };   
}

#endif
