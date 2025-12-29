
// Copyright 2010 Palm Inc.

#ifndef __SearchPopupMenuPalm_h__
#define __SearchPopupMenuPalm_h__

#include "PopupMenuPalm.h"
#include "SearchPopupMenu.h"

namespace WebCore {

class SearchPopupMenuPalm : public SearchPopupMenu {
public:
	SearchPopupMenuPalm( PopupMenuClient* );
	
	virtual PopupMenu* popupMenu();
    virtual void saveRecentSearches(const AtomicString& name, const Vector<String>& searchItems);
    virtual void loadRecentSearches(const AtomicString& name, Vector<String>& searchItems);
    virtual bool enabled();
    
private:
	RefPtr<PopupMenuPalm> m_popup;
};
	
}

#endif 
