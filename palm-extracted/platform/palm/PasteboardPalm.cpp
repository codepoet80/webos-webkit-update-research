
// Copyright 2008 Palm, Inc.

#include "config.h"
#include <sys/stat.h>
#include "Pasteboard.h"

#include "CDATASection.h"
#include "CSSComputedStyleDeclaration.h"
#include "CSSPropertyNames.h"
#include "CSSRule.h"
#include "CSSRuleList.h"
#include "CSSStyleRule.h"
#include "CSSStyleSelector.h"
#include "CSSValueKeywords.h"
#include "Comment.h"
#include "DeleteButtonController.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "DocumentType.h"
#include "Editor.h"
#include "Frame.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "InlineTextBox.h"
#include "KURL.h"
#include "Logging.h"
#include "ProcessingInstruction.h"
#include "QualifiedName.h"
#include "Range.h"
#include "TextIterator.h"
#include "htmlediting.h"
#include "visible_units.h"
#include "CString.h"
#include "HTMLInterchange.h"
#include "FileSystem.h"

#include "webkitpalmsettings.h"

namespace WebCore {
	
	using namespace HTMLNames;
	
	static void palmFillContainerFromString(ContainerNode* paragraph, const String& string);
	
static String* sPasteboardTextStorage = 0;

Pasteboard* Pasteboard::generalPasteboard()
{
    static Pasteboard* pasteboard = 0;
	
	if( !sPasteboardTextStorage )
		sPasteboardTextStorage = new String();
	
	if( !pasteboard )
		pasteboard = new Pasteboard();
	
    return pasteboard;
}

static void writePasteboard()
{
    String temporaryFileName(PalmBrowserSettings()->sharedClipboardFile);
    temporaryFileName.append(".tmp");
    FILE* f = fopen(temporaryFileName.utf8().data(), "w");

    if (f) {
        CString utf8clip = sPasteboardTextStorage->utf8();
        fwrite(utf8clip.data(), utf8clip.length(), 1, f);
        fflush(f);
        fsync(fileno(f));
        fclose(f);

        // LunaSysMgr runs as root, but others (like BrowserServer) run with
        // fewer privileges. Open up access for any process in the luna group.
        // Fixes NOV-122602.
        const uid_t k_LunaUserId(1000);
        const gid_t k_LunaGroupId(1000);
        int error = chown(temporaryFileName.utf8().data(), k_LunaUserId, k_LunaGroupId);
        chmod(temporaryFileName.utf8().data(), S_IRUSR|S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);

        rename(temporaryFileName.utf8().data(), PalmBrowserSettings()->sharedClipboardFile);
    }
}

Pasteboard::Pasteboard()
{    
}

Pasteboard::~Pasteboard()
{    
}

void Pasteboard::writeSelection(Range* selectedRange, bool canSmartCopyOrDelete, Frame* frame)
{
    *sPasteboardTextStorage = frame->selectedText();
    writePasteboard();
}

void Pasteboard::writeURL(const KURL&, const String&, Frame*)
{
}

void Pasteboard::writeImage(Node*, const KURL&, const String&)
{
}

void Pasteboard::writePlainText(const String& text)
{
    *sPasteboardTextStorage = text;
    writePasteboard();
}

void Pasteboard::clear()
{
	*sPasteboardTextStorage = "";
	deleteFile( PalmBrowserSettings()->sharedClipboardFile );
}

bool Pasteboard::canSmartReplace()
{
    return false;
}

PassRefPtr<DocumentFragment> Pasteboard::documentFragment(Frame* frame, PassRefPtr<Range> context, bool allowPlainText, bool& chosePlainText )
{
    Node* styleNode = context->startContainer();
    if (!styleNode) {
        styleNode = context->startPosition().node();
        if (!styleNode)
            return 0;
    }

	Document* document = styleNode->document();
	RefPtr<DocumentFragment> fragment = document->createDocumentFragment();  

	
	FILE* f = fopen( PalmBrowserSettings()->sharedClipboardFile, "r" );
	if( f ) {
		fseek( f, 0, SEEK_END );
		long sz = ftell(f);
		fseek( f, 0, SEEK_SET );
		if (sz > 0) {
			char* buffer = new char[ sz+1 ];
			if( buffer )
			{
				if( fread( buffer, sz, 1, f ) ) {
					*sPasteboardTextStorage = String::fromUTF8( buffer, sz );
				}
				delete[] buffer;
			}
			fclose(f);
		}
		else {
			fclose(f);
			return fragment.release();
		}
	} 
	else
		return fragment.release();
	
	// WebCore::createFragmentFromText creates <div> for each paragraph, which is not
	// what we want. So we'll create the fragment ourselves.
    String string = *sPasteboardTextStorage;
    string.replace("\r\n", "\n");
    string.replace('\r', '\n');

    ExceptionCode ec = 0;
    RenderObject* renderer = styleNode->renderer();
    if (renderer && renderer->style()->preserveNewline()) {
        fragment->appendChild(document->createTextNode(string), ec);
        ASSERT(ec == 0);
        if (string.endsWith("\n")) {
            RefPtr<Element> element;
            element = document->createElementNS(xhtmlNamespaceURI, "br", ec);
            ASSERT(ec == 0);
            element->setAttribute(classAttr, AppleInterchangeNewline);            
            fragment->appendChild(element.release(), ec);
            ASSERT(ec == 0);
        }
        return fragment.release();
    }

    // A string with no newlines gets added inline, rather than being put into a paragraph.
    if (string.find('\n') == WTF::notFound ) {
        palmFillContainerFromString(fragment.get(), string);
        return fragment.release();
    }

    // Break string into paragraphs. Extra line breaks turn into empty paragraphs.
    Node* block = enclosingBlock(context->startContainer());
    bool useClonesOfEnclosingBlock = block && !block->hasTagName(bodyTag) && !block->hasTagName(htmlTag);
    
    Vector<String> list;
	string.split( String("\n"), list ); // true gets us empty strings in the list
	int index= 0;
    while (!list.isEmpty()) {
        String s = list.first();
        list.remove(0);

        RefPtr<Element> element;
        if (s.isEmpty() && list.isEmpty()) {
            // For last line, use the "magic BR" rather than a P.
            element = document->createElementNS(xhtmlNamespaceURI, "br", ec);
            ASSERT(ec == 0);
            element->setAttribute(classAttr, AppleInterchangeNewline);
			fragment->appendChild(element.release(), ec);
		} else {
			
			if( index > 0 ) {
				element = document->createElementNS(xhtmlNamespaceURI, "br", ec);
				fragment->appendChild(element.release(), ec);
			}
			RefPtr<Node> textNode = document->createTextNode(s);
			fragment->appendChild(textNode.release(), ec);
        } 
        ASSERT(ec == 0);
		index++;
    }
	
	return fragment.release();
}

static void palmFillContainerFromString(ContainerNode* paragraph, const String& string)
{
    Document* document = paragraph->document();

    ExceptionCode ec = 0;
    if (string.isEmpty()) {
        paragraph->appendChild(createBlockPlaceholderElement(document), ec);
        ASSERT(ec == 0);
        return;
    }

    ASSERT(string.find('\n') == WTF::notFound);

    Vector<String> tabList;
	string.split("\t", tabList );
    String tabText = "";
    bool first = true;
    while (!tabList.isEmpty()) {
        String s = tabList.first();
        tabList.remove(0);

        // append the non-tab textual part
        if (!s.isEmpty()) {
            if (!tabText.isEmpty()) {
                paragraph->appendChild(createTabSpanElement(document, tabText), ec);
                ASSERT(ec == 0);
                tabText = "";
            }
            RefPtr<Node> textNode = document->createTextNode(stringWithRebalancedWhitespace(s, first, tabList.isEmpty()));
            paragraph->appendChild(textNode.release(), ec);
            ASSERT(ec == 0);
        }

        // there is a tab after every entry, except the last entry
        // (if the last character is a tab, the list gets an extra empty entry)
        if (!tabList.isEmpty())
            tabText += "\t";
        else if (!tabText.isEmpty()) {
            paragraph->appendChild(createTabSpanElement(document, tabText), ec);
            ASSERT(ec == 0);
        }
        
        first = false;
    }
}


String Pasteboard::plainText(Frame* )
{
	FILE* f = fopen( PalmBrowserSettings()->sharedClipboardFile, "r" );
	if( f ) {
		fseek( f, 0, SEEK_END );
		long sz = ftell(f);
		if (sz >= 0) {
			fseek( f, 0, SEEK_SET );
			char* buffer = new char[ sz+1 ];
			if( buffer )
			{
				if( fread( buffer, sz, 1, f ) ) {
					*sPasteboardTextStorage = String::fromUTF8( buffer, sz );
				}
				delete[] buffer;
			}
		}
		else {
			// Not sure that ftell can return <0 for valid file stream ptr, but adding
			// check because Coverity warns.
			*sPasteboardTextStorage = "";
		}
		fclose(f);
	}
	return *sPasteboardTextStorage;
	
}

}
