
#ifndef __PopupMenuPalm_h__
#define __PopupMenuPalm_h__

#include "IntRect.h"
#include "PopupMenu.h"
#include "PopupMenuClient.h"
#include <glib.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace Palm {
	class WebViewClient;
}

namespace WebCore
{

class FrameView;
class ScrollBar;

class PopupMenuPalm : public PopupMenu {
	
public:
	PopupMenuPalm( PopupMenuClient* client );
	~PopupMenuPalm();

    virtual void show(const IntRect&, FrameView*, int index);
    virtual void hide();
    virtual void updateFromElement();
    virtual void disconnectClient();
    
    PopupMenuClient* client() { return m_popupClient; }
	
private:
	PopupMenuClient* m_popupClient;
	Palm::WebViewClient* m_viewClient; // Only valid when menu is visible
};


}

#endif
