
// Copyright 2010 Palm Inc.

#include "config.h"
#include "SearchPopupMenuPalm.h"


namespace WebCore {

SearchPopupMenuPalm::SearchPopupMenuPalm(PopupMenuClient* client)
	: m_popup( adoptRef( new PopupMenuPalm(client)) )
{
}

PopupMenu* SearchPopupMenuPalm::popupMenu()
{
    return m_popup.get();
}
    
void SearchPopupMenuPalm::saveRecentSearches(const AtomicString&, const Vector<String>&)
{
}

void SearchPopupMenuPalm::loadRecentSearches(const AtomicString&, Vector<String>&)
{
}

bool SearchPopupMenuPalm::enabled()
{
    return false;
}

}
