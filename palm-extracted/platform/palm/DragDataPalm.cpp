// Copyright 2008 Palm Inc.

#include "config.h"
#include "DragData.h"

#include "ClipboardPalm.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "webkitpalmclipboard.h"

namespace WebCore {

bool DragData::canSmartReplace() const
{
    return false;
}
    
bool DragData::containsColor() const
{
    return false;
}

bool DragData::containsFiles() const
{
    return false;
}

void DragData::asFilenames(Vector<String>& result) const
{
}

bool DragData::containsPlainText() const
{
    return false;
}

String DragData::asPlainText() const
{
    return String();
}
    
Color DragData::asColor() const
{
    return Color();
}

/*PassRefPtr<Clipboard> DragData::createClipboard(ClipboardAccessPolicy policy ) const
{
	RefPtr<ClipboardPalm> c =  ClipboardPalm::create( policy, true, m_platformDragData );
	c->releaseSystemClipboard();
	return c;
}
  */  
bool DragData::containsCompatibleContent() const
{
    return true;
}
    
bool DragData::containsURL(FilenameConversionPolicy /*filenamePolicy*/ ) const
{
    return false;
}
    
String DragData::asURL(FilenameConversionPolicy filenamePolicy /* ConvertFilenames*/, String* /*title*/) const
{
    return String();
}
    
    
PassRefPtr<DocumentFragment> DragData::asFragment(Document*) const
{
    return 0;
}
    
}
