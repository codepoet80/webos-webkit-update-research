// Copyright 2010 Palm Inc.

#include "config.h"
#include "PopupMenuPalm.h"
#include <wtf/text/WTFString.h>
#include "CString.h"
#include "FrameView.h"

#include "palmwebview.h"
#include "palmwebviewclient.h"
#include "palmwebpage.h"

#include <stdio.h>
 
namespace WebCore {

PopupMenuPalm::PopupMenuPalm(PopupMenuClient* client) 
	: m_popupClient(client)
	, m_viewClient(NULL)
{
}

PopupMenuPalm::~PopupMenuPalm()
{
	hide();
}

/**
 * Encode a string into JSON format. Output string will always be NULL terminated even
 * if input string is not.
 *
 * @param dst    The destination string that will be JSON encoded.
 * @param src    The source string to JSON encode.
 * @param dstLen The size of the destination string including the space for the terminating char.
 * @param srcLen The size of the source string including the space for the terminating char.
 */
static void encodeJsonString(char* dst, const char* src, size_t dstLen, size_t srcLen)
{
	size_t numDstChars = 0;
	for (size_t i = 0; i < srcLen && numDstChars < dstLen; i++) {
		if (src[i] == '"') {
			dst[numDstChars++] = '\\';
			if (numDstChars >= dstLen)
				break;
		}
		dst[numDstChars++] = src[i];
	}
	dst[dstLen-1] = '\0'; // Just to be safe in case the input string was not terminated.
}

void PopupMenuPalm::show(const IntRect&, FrameView* frameView, int)
{
	hide();
	m_viewClient = frameView->containingWindow()->client();

	Palm::PopupMenuData* mdata = new Palm::PopupMenuData();
	mdata->selectedIdx = m_popupClient->selectedIndex();

	for (int i = 0; i < m_popupClient->listSize(); i++) {

		Palm::PopupMenuItem* item = new Palm::PopupMenuItem();
		item->title       = m_popupClient->itemText(i).utf8().data();
		item->isEnabled   = m_popupClient->itemIsEnabled(i);
		item->isSeparator = m_popupClient->itemIsSeparator(i);
		item->isLabel     = m_popupClient->itemIsLabel(i);

		mdata->items.push_back(item);
	}

	m_viewClient->popupMenuShow(this, mdata);
}

void PopupMenuPalm::hide()
{
	if (m_viewClient) {
		m_viewClient->popupMenuHide(this);
		m_viewClient = NULL;
	}
	if (m_popupClient)
	    m_popupClient->popupDidHide();
}

void PopupMenuPalm::disconnectClient()
{
    m_popupClient = 0;
}

void PopupMenuPalm::updateFromElement() 
{
	if( m_popupClient )
		m_popupClient->setTextFromItem(m_popupClient->selectedIndex());
}

}

