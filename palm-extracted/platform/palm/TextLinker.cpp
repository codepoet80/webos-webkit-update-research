/**
 * TextLinker.cpp
 * 
 * Searches for link candidates by walking a rich text field tree structure
 * and calls various parsers on candidate strings.
 * 
 * Modifies rich text field structure by creating and removing anchor elements.
 * 
 */

#include "config.h"
 
#include <wtf/text/WTFString.h>

#include "TextLinker.h"

#include "PhoneNumberParser.h" 
#include "UrlParser.h"

#include "Editor.h"
#include "Frame.h"
#include "CString.h"
#include "PlatformString.h"
#include "Node.h"
#include "RenderObject.h"
#include "TextIndexer.h"
#include "VisiblePosition.h"
#include "VisibleSelection.h"
#include "visible_units.h"

#undef LINKER_DEBUG

#ifdef LINKER_DEBUG 
#define LINKER_TRACE(...) \
do { \
    fprintf(stdout, "TextLinker: %s: ", __FUNCTION__ ); \
    fprintf(stdout, __VA_ARGS__); \
} while (0)

#else

#define LINKER_TRACE(...) (void)0

#endif


namespace WebCore {

static const String kSearchTypeAttributes [3] = { "phone", "url", "email" };


bool TextLinker::findLinksInText(const VisibleSelection& selection, Editor* editor) {
    
    bool urlLinksFound = findUrlLinksInText(selection, editor);
    
    LINKER_TRACE("%s: url links found? %s\n", __FUNCTION__, urlLinksFound ? "YES" : "NO");
    
    VisibleSelection newSel = editor->frame()->selection()->selection();
    
    bool phoneNumberLinksFound = findPhoneNumberLinksInText(newSel, editor);
    
    LINKER_TRACE("%s: phone number links found? %s\n", __FUNCTION__, phoneNumberLinksFound ? "YES" : "NO");
    
    return (urlLinksFound || phoneNumberLinksFound);
}

bool TextLinker::findUrlLinksInText(const VisibleSelection& selection, Editor* editor)
{
    LINKER_TRACE(">>>>>>> starting linker\n");

    if (!selection.isContentRichlyEditable()) {
        LINKER_TRACE("<<<<<<< content is not richly editable.\n");
        return false;
    }

    Node* node = selection.start().node();
    if (!node) {
        return false;
    }

    String nodeText = node->textContent();
    int textLength = nodeText.length();
    int cursorPos = selection.extent().computeOffsetInContainerNode();

    ParserSpan leftSpan, rightSpan;

    // TODO (adauria) iterate over all SearchTypes. Currently using PhoneNumberSearch

    // look for partial match left of cursor
    bool partialLeftMatchFound = false;

    if (cursorPos > 0) {
        partialLeftMatchFound = UrlParser::searchLeft(nodeText, cursorPos, leftSpan);
    } else if (cursorPos == 0){
        // prepare leftSpan for searching left siblings
        leftSpan.start = 0;
    }

    if (partialLeftMatchFound) {
        leftSpan.str = nodeText.substring(leftSpan.start, leftSpan.length());
        LINKER_TRACE("%s: UrlParser sez valid LEFT boundary contains string: <---'%s'<---\n",
                __FUNCTION__,
                leftSpan.str.utf8().data());
    }

    // check if left sibling extends our match if we have a partial match 
    // or are at very beginning of leaf
    bool leftSpanExpanded = false;
    if (partialLeftMatchFound || (cursorPos == 0 && textLength != 0)) 
    {
        leftSpanExpanded = searchPreviousSiblings(editor, selection.start().node(), 
                TextLinker::UrlSearch,
                leftSpan);

        if (leftSpanExpanded) {
            LINKER_TRACE("%s: LEFT match EXPANDED with span: start=%d, end=%d, cursor=%d \n",
                    __FUNCTION__,
                    rightSpan.start, rightSpan.end, cursorPos);
            partialLeftMatchFound = true;
        }

    }

    if (!partialLeftMatchFound && !leftSpanExpanded) {
        LINKER_TRACE("%s: NO LINKS on LEFT\n", __FUNCTION__); 
    }

    // get match fragment on right

    // if candidate lies flush to a text leaf, continue walking to the text leaf
    // use ParserSpan.start, ParserSpan.end to represent start and end index 
    // of match within its text leaf, and use ParserSpan.cursorOffset to know
    // how far away the string is from current cursor position
    //

    bool partialRightMatchFound = false;

    if (cursorPos < textLength) {
        partialRightMatchFound = UrlParser::searchRight(nodeText, cursorPos, rightSpan);

    } else if (cursorPos == textLength){
        // cursor is at the very end of the leaf
        rightSpan.start = cursorPos; 
        rightSpan.end = cursorPos - 1;
    }

    if (partialRightMatchFound)
    {
        rightSpan.str = nodeText.substring(rightSpan.start, rightSpan.length());

        LINKER_TRACE("%s: PhoneNumberParser sez valid RIGHT boundary contains string: --->'%s'--->\n",
                __FUNCTION__,
                rightSpan.str.utf8().data());
    }

    // expand span to right sibling if partial match is flush right
    bool rightSpanExpanded = false;

    if (partialRightMatchFound  
            || (cursorPos == textLength && textLength != 0)) // at end of a non-empty node 
    {
        rightSpanExpanded = searchNextSiblings(editor, selection.start().node(),
                TextLinker::UrlSearch,
                rightSpan);

        if (rightSpanExpanded) {
            LINKER_TRACE("%s: RIGHT match EXPANDED with span: start=%d, end=%d, cursor=%d __\n",
                    __FUNCTION__,
                    rightSpan.start, rightSpan.end, cursorPos);
            partialRightMatchFound = true;
        }

    } else {
        LINKER_TRACE("%s: NO LINKS on RIGHT\n", __FUNCTION__);
    }

    // trim right border if it wasn't expanded
    ParserSpan finalLeft, finalRight;

    if (partialLeftMatchFound && partialRightMatchFound) 
    {
        LINKER_TRACE("%s: LEFT **and** RIGHT matches found. left.end=%d, right.start=%d\n",
                __FUNCTION__,
                leftSpan.end, rightSpan.start);

        if (leftSpan.end + 1 == rightSpan.start) {
            leftSpan.str += rightSpan.str;  // concatenate strings
            leftSpan.end += rightSpan.length();  // reuse leftSpan to represent both adjacent spans

            finalLeft = leftSpan;  // everything is in finalLeft

        } else {
            // matches are separate
            finalLeft = leftSpan;
            finalRight = rightSpan;
        }

    } else if (partialLeftMatchFound && !partialRightMatchFound){
        // match was only on left... copying to avoid pasting of linking code 
        finalLeft = leftSpan;
    } else if (!partialLeftMatchFound && partialRightMatchFound) {
        // match was only on right
        finalRight = rightSpan;
    }

    bool inLink = node->parentNode() && node->parentNode()->isLink();

    if (inLink) {
        LINKER_TRACE("%s: we're IN A LINK of textLength=%d\n", __FUNCTION__, textLength);
        unlinkNode(editor, node);
    }

    // turn left and right candidate spans into one or two separate links
    // if the spans are validated
    if (finalLeft.str.length() > 0) {

        //unlinkSpan(editor, selection, finalLeft);
        ParserSpan preTrimSpan = finalLeft;
        LINKER_TRACE("%s: match before trim: '%s'\n", __FUNCTION__, finalLeft.str.utf8().data());
        UrlParser::trimFromStart(finalLeft);
        UrlParser::trimFromEnd(finalLeft);
        LINKER_TRACE("%s: match after trim: '%s'\n", __FUNCTION__, finalLeft.str.utf8().data());

        // set "text linker is running" 
        String scheme;
        bool linkFound = validateSpan(finalLeft, UrlSearch, scheme);

        if (linkFound) {
            createLink(editor, selection, finalLeft, scheme, UrlSearch);
        }
    }

    // Right side of cursor contains a link that is not part of the left side

    if (finalRight.str.length() > 0) {

        // unlinkSpan(editor, selection, finalRight);
        ParserSpan preTrimSpan = finalRight;
        LINKER_TRACE("%s: match before trim: '%s'\n", __FUNCTION__, finalRight.str.utf8().data());
        UrlParser::trimFromStart(finalRight);
        UrlParser::trimFromEnd(finalRight);
        LINKER_TRACE("%s: match after trim: '%s'\n", __FUNCTION__, finalRight.str.utf8().data());

        // set "text linker is running" 
        String scheme;
        bool linkFound = validateSpan(finalRight, UrlSearch, scheme);

        if (linkFound) {
            createLink(editor, selection, finalRight, scheme, UrlSearch);
        }
    }

    LINKER_TRACE("<<<<<< finished linking\n");



    return true;
}


bool TextLinker::findPhoneNumberLinksInText(const VisibleSelection& selection, Editor* editor)
{
    LINKER_TRACE(">>>>>>> starting linker\n");

    if (!selection.isContentRichlyEditable()) {
        LINKER_TRACE("<<<<<<< content is not richly editable.\n");
        return false;
    }

    
    
    Node* node = selection.start().node();
    if (!node) {
        return false;
    }

    // check if we're in a link of a different parser
    if (node->parentNode() && node->parentNode()->isLink()) {
        Element* el = static_cast<Element*>(node->parentNode());
        if (el) {
            String type = el->getAttribute("type");
            if (type != kSearchTypeAttributes[PhoneNumberSearch]) {
                LINKER_TRACE("not entering LINK of different type\n");
                return false;
            } else {
                LINKER_TRACE("entering LINK of same type\n");
            }
        }
    }

    
    String nodeText = node->textContent();
    int textLength = nodeText.length();
    int cursorPos = selection.extent().computeOffsetInContainerNode();

    ParserSpan leftSpan, rightSpan;
    
    // TODO (adauria) iterate over all SearchTypes. Currently using PhoneNumberSearch
    
    // look for partial match left of cursor
    bool partialLeftMatchFound = false;
    
    if (cursorPos > 0) {
        partialLeftMatchFound = PhoneNumberParser::searchLeft(nodeText, cursorPos, leftSpan);
    } else if (cursorPos == 0){
        // prepare leftSpan for searching left siblings
        leftSpan.start = 0;
    }
    
    if (partialLeftMatchFound) {
        leftSpan.str = nodeText.substring(leftSpan.start, leftSpan.length());
        LINKER_TRACE("%s: PhoneNumberParser sez valid LEFT boundary contains string: <---'%s'<---\n",
                __FUNCTION__,
                leftSpan.str.utf8().data());
    }
    
    // check if left sibling extends our match if we have a partial match 
    // or are at very beginning of leaf
    bool leftSpanExpanded = false;
    if (partialLeftMatchFound || (cursorPos == 0 && textLength != 0)) 
    {
        leftSpanExpanded = searchPreviousSiblings(editor, selection.start().node(), 
                TextLinker::PhoneNumberSearch, 
                leftSpan);

        if (leftSpanExpanded) {
            LINKER_TRACE("%s: LEFT match EXPANDED with span: start=%d, end=%d, cursor=%d \n",
                    __FUNCTION__,
                    rightSpan.start, rightSpan.end, cursorPos);
            partialLeftMatchFound = true;
        }
        
    }
    
    if (!partialLeftMatchFound && !leftSpanExpanded) {
        LINKER_TRACE("%s: NO LINKS on LEFT\n", __FUNCTION__); 
    }
    
    // get match fragment on right
    
    // if candidate lies flush to a text leaf, continue walking to the text leaf
    // use ParserSpan.start, ParserSpan.end to represent start and end index 
    // of match within its text leaf, and use ParserSpan.cursorOffset to know
    // how far away the string is from current cursor position
    //
    
    bool partialRightMatchFound = false;
    
    if (cursorPos < textLength) {
        partialRightMatchFound = PhoneNumberParser::searchRight(nodeText, cursorPos, rightSpan);
        
    } else if (cursorPos == textLength){
        // cursor is at the very end of the leaf
        rightSpan.start = cursorPos; 
        rightSpan.end = cursorPos - 1;
    }

    if (partialRightMatchFound)
    {
        rightSpan.str = nodeText.substring(rightSpan.start, rightSpan.length());
        
        LINKER_TRACE("%s: PhoneNumberParser sez valid RIGHT boundary contains string: --->'%s'--->\n",
                __FUNCTION__,
                rightSpan.str.utf8().data());
    }
    
    // expand span to right sibling if partial match is flush right
    bool rightSpanExpanded = false;
    
    if (partialRightMatchFound  
            || (cursorPos == textLength && textLength != 0)) // at end of a non-empty node 
    {
       rightSpanExpanded = searchNextSiblings(editor, selection.start().node(),
                TextLinker::PhoneNumberSearch,
                rightSpan);
        
        if (rightSpanExpanded) {
             LINKER_TRACE("%s: RIGHT match EXPANDED with span: start=%d, end=%d, cursor=%d __\n",
                     __FUNCTION__,
                     rightSpan.start, rightSpan.end, cursorPos);
             partialRightMatchFound = true;
         }

    } else {
        LINKER_TRACE("%s: NO LINKS on RIGHT\n", __FUNCTION__);
    }
    
    // trim right border if it wasn't expanded
    ParserSpan finalLeft, finalRight;
    
    if (partialLeftMatchFound && partialRightMatchFound) 
    {
        LINKER_TRACE("%s: LEFT **and** RIGHT matches found. left.end=%d, right.start=%d\n",
                __FUNCTION__,
                leftSpan.end, rightSpan.start);
        
        if (leftSpan.end + 1 == rightSpan.start) {
            leftSpan.str += rightSpan.str;  // concatenate strings
            leftSpan.end += rightSpan.length();  // reuse leftSpan to represent both adjacent spans
            
            finalLeft = leftSpan;  // everything is in finalLeft
            
        } else {
            // matches are separate
            finalLeft = leftSpan;
            finalRight = rightSpan;
        }
 
    } else if (partialLeftMatchFound && !partialRightMatchFound){
        // match was only on left... copying to avoid pasting of linking code 
        finalLeft = leftSpan;
    } else if (!partialLeftMatchFound && partialRightMatchFound) {
        // match was only on right
        finalRight = rightSpan;
    }


    // turn left and right candidate spans into one or two separate links
    // if the spans are validated
    if (finalLeft.str.length() > 0) {
        
        //unlinkSpan(editor, selection, finalLeft);
        ParserSpan preTrimSpan = finalLeft;
        LINKER_TRACE("%s: match before trim: '%s'\n", __FUNCTION__, finalLeft.str.utf8().data());
        PhoneNumberParser::trimFromStart(finalLeft);
        PhoneNumberParser::trimFromEnd(finalLeft);
        LINKER_TRACE("%s: match after trim: '%s'\n", __FUNCTION__, finalLeft.str.utf8().data());
        
        // set "text linker is running" 
        String scheme;
        bool linkFound = validateSpan(finalLeft, PhoneNumberSearch, scheme);

        if (linkFound) {
            createLink(editor, selection, finalLeft, scheme, PhoneNumberSearch);
        }
    }
    
    // Right side of cursor contains a link that is not part of the left side
    
    if (finalRight.str.length() > 0) {
        
        // unlinkSpan(editor, selection, finalRight);
        ParserSpan preTrimSpan = finalRight;
        LINKER_TRACE("%s: match before trim: '%s'\n", __FUNCTION__, finalRight.str.utf8().data());
        PhoneNumberParser::trimFromStart(finalRight);
        PhoneNumberParser::trimFromEnd(finalRight);
        LINKER_TRACE("%s: match after trim: '%s'\n", __FUNCTION__, finalRight.str.utf8().data());
        
        // set "text linker is running" 
        String scheme;
        bool linkFound = validateSpan(finalRight, PhoneNumberSearch, scheme);
        
        if (linkFound) {
            createLink(editor, selection, finalRight, scheme, PhoneNumberSearch);
        }
    }
    
    LINKER_TRACE("<<<<<< finished linking\n");
    
    
    
    return true;
}



/**
 * Used to expand given match span to characters to the left of given text leaf
 * 
 * 
 */
bool TextLinker::searchPreviousSiblings(Editor* editor, const Node* node, 
        const SearchType searchType, 
        ParserSpan& matchSpan) 
{
    if (!node) {
        return false;
    }

    // match span must be flush left to its text leaf, otherwise, we won't continue
    if (matchSpan.start != 0) {
        return false;
    }
    
    bool spanExpanded = false;
    
    Node* leftSibling = NULL;
    
    if (node->parentNode() && node->parentNode()->isLink()) {
        // next node is a link, so get its first text child
        leftSibling = node->parentNode()->previousSibling();
    } else {
        leftSibling = node->previousSibling();
    }

    while (leftSibling) 
    {
        bool isLink = false;  // used to destroy a link node if it becomes part of a match
        
        if (leftSibling->isLink()) {
            // we'll only enter a link of the same type
            Element* el = static_cast<Element*>(leftSibling);
            String type = el->getAttribute("type");
            if (type != kSearchTypeAttributes[searchType]) {
                LINKER_TRACE("not entering LINK of different type\n");
                break;
            } else {
                LINKER_TRACE("entering LINK of same type\n");
                isLink = true;
            }
            
            if (leftSibling->lastChild()) {  // want rightmost child of left sibling
                leftSibling = leftSibling->lastChild();
            } else {
                break;
            }
        }

        RenderObject* r = leftSibling->renderer();
        if (!r || !r->isText() || r->isBR()) {
            LINKER_TRACE("Not expanding to left: not text or at BR\n");
            break;
        }

        // search for candidates in left sibling
        ParserSpan expandSpan;
        
        // TODO (dauria) add switch on SearchType
        
        bool partialMatchFound = false; 
        
        switch (searchType) {
        case (PhoneNumberSearch) :
        {
            partialMatchFound = PhoneNumberParser::searchLeft(leftSibling->textContent(),
                    leftSibling->textContent().length(), // cursor pos at end of left sibling
                    expandSpan);
            break;
        }
        
        case (UrlSearch) :
        {
            partialMatchFound = UrlParser::searchLeft(leftSibling->textContent(),
                    leftSibling->textContent().length(), // cursor pos at end of left sibling
                    expandSpan);
            break;
        }
        
        }
        

        if (partialMatchFound 
                && (expandSpan.end == (int)leftSibling->textContent().length() - 1)) 
        {
            // new candidate started at the very end of previous candidate
            // so assume it's a continuation

            LINKER_TRACE("expanded LEFT span by %d, to string '%s'\n",
                    expandSpan.length(),
                    leftSibling->textContent().substring(expandSpan.start, 
                            expandSpan.length()).utf8().data());

            // our left span can be negative because it's with respect to
            // the leaf where the cursor is
            matchSpan.str = leftSibling->textContent().substring(expandSpan.start, 
                    expandSpan.length()) + matchSpan.str;
            matchSpan.start -= expandSpan.length();

            spanExpanded = true;
            
            if (isLink) {
                unlinkNode(editor, leftSibling);
            }
            // check if our candidate match on left reaches the left boundary
            // of the left sibling. If it does, then we have to move on to the 
            // next sibling to our left
            if (expandSpan.length() == (int)leftSibling->textContent().length()) {
                leftSibling = leftSibling->previousSibling();

                LINKER_TRACE("expanded LEFT to begging of sibling, continuing left...\n");
            } else {
                // can't expand leftward anymore
                LINKER_TRACE("Will not expand LEFT span by %d, to string '%s'\n",
                        expandSpan.length(),
                        leftSibling->textContent().substring(expandSpan.start, 
                                expandSpan.length()).utf8().data());
                

                break;
            }
        } else {
            // no partial match found

            break;
        }
    }
    
    return spanExpanded;
}

bool TextLinker::searchNextSiblings(Editor* editor, const Node* node, const SearchType searchType, ParserSpan& matchSpan)
{
    if (!node) {
        return false;
    }
    
    if (matchSpan.end < (int)node->textContent().length() - 1) {
        return false;
    }
    
    // Right span may need to be extended if a text leaf to the right is flush left 
    // to the end of rightSpan. Update rightSpan.end to end of flush left match
    // to the right

    bool spanExpanded = false;
    
    Node* rightSibling = NULL;
    
    if (node->parentNode() && node->parentNode()->isLink()) {
        // next node is a link, so get its first text child
        rightSibling = node->parentNode()->nextSibling();
    } else {
        rightSibling = node->nextSibling();
    }

    while (rightSibling) 
    {
        
        bool isLink = false;

        if (rightSibling->isLink()) {
            // we'll only enter a link of the same type
            Element* el = static_cast<Element*>(rightSibling);
            String type = el->getAttribute("type");
            if (searchType == UrlSearch && !type.isEmpty()) {
                LINKER_TRACE("allowing urlsearch to override other parser links\n");
                isLink = true;
            } else if (type != kSearchTypeAttributes[searchType]) {
                LINKER_TRACE("not entering LINK of different type\n");
                break;
            } else {
                LINKER_TRACE("entering LINK of same type\n");
                isLink = true;
            }
            
             if (rightSibling->firstChild()) {
                rightSibling = rightSibling->firstChild();
            } else {
                break;
            }
        }
        
        RenderObject* r = rightSibling->renderer();
        if (!r || !r->isText() || r->isBR()) {
            LINKER_TRACE("Not expanding to right: not text or at BR\n");
            break;
        }
        
        // search for candidates in right sibling
        ParserSpan expandSpan;
  
        bool matchFound = false; 
        
        switch (searchType) {
        
        case (PhoneNumberSearch) :
        {
            matchFound = PhoneNumberParser::searchRight(rightSibling->textContent(),
                    0, // cursor position -- at very beginning of sibling
                    expandSpan);
            break;
        }
        
        case (UrlSearch) :
        {
            matchFound = UrlParser::searchRight(rightSibling->textContent(),
                    0, // cursor position -- at very beginning of sibling
                    expandSpan);
            break;
        }
        
        }
        
        if (matchFound && expandSpan.start == 0) 
        {
            spanExpanded = true;

            if (isLink) {
                unlinkNode(editor, rightSibling);
            }
            
            // new candidate started at the very end of previous candidate
            // so assume it's a continuation
            matchSpan.str += rightSibling->textContent().substring(0, expandSpan.length());
            
            LINKER_TRACE("expanded RIGHT span by %d, to string '%s'\n",
                    expandSpan.length(),
                    matchSpan.str.utf8().data());
            
            // Test if partial candidate is flush left and flush right
            // if it is, then continue expanding rightSpan.end
            // rightSpan.end is an index of a flush right match. We will grow
            // the end index by the size of expandSpan

            matchSpan.end += expandSpan.length();
            
            if (expandSpan.length() == (int)rightSibling->textContent().length()) {
                // candidate span matches entire text content of right sibling
                // continue to next sibling
                rightSibling = rightSibling->nextSibling();
                
                LINKER_TRACE("expanded RIGHT to end of sibling, continuing right...\n");
            } else {
                // can't expand rightward anymore
                break;
            }
        } else {
            // no partial match found
            break;
        }
    }
    
    return spanExpanded;
}

bool TextLinker::validateSpan(const ParserSpan& span, const SearchType searchType, String& scheme)
{

    LINKER_TRACE("given word: '%s', searchType: %d\n", span.str.utf8().data(), (int)searchType);

    WebCore::PatternMatchs links;
    RunIndexerOnText(span.str, links);

    bool linkFound = false;
    
    for (size_t i=0; !linkFound && i < links.size(); i++) 
    {
        const WebCore::PatternMatch& link = links[i];

        // only interested in the first match of the following link types
        switch (link.type)
        {
        case Palm::TextIndexer_EmailAddress:
            scheme = String("mailto:");
            if (searchType == EmailSearch || searchType == UrlSearch) {
                linkFound = true;
            }
            break;
        case Palm::TextIndexer_PhoneNumber:
            scheme = String("tel:");
            if (searchType == PhoneNumberSearch) {
                linkFound = true;
            }
            break;
        case Palm::TextIndexer_WebLink:
            scheme = String("");
            if (searchType == UrlSearch && (int)link.len() == span.length()) {
                linkFound = true;
            }
            break;
        case Palm::TextIndexer_SchemalessWebLink:
            scheme = String("http://");
            if (searchType == UrlSearch && (int)link.len() == span.length()) {
                linkFound = true;
            }
            break;
        }
    }

    if (linkFound) {
        LINKER_TRACE("link found: '%s'\n", (scheme + span.str).utf8().data());
    }
    
    return linkFound;
}

/**
 * Creates link by first unlinking span, then using the editor command to 
 * insert html.
 *
 */
bool TextLinker::createLink(Editor* editor, 
        const VisibleSelection& selection,
        const ParserSpan& span, 
        const String& scheme,
        const SearchType searchType)
{
    if (!editor || selection.isNone()) {
        return false;
    }

    int cursorPos = selection.extent().computeOffsetInContainerNode();
    
    if (span.start < cursorPos) {
        // span start is to the left of cursor
        
        // walk to start 
        int numMovesLeft = cursorPos - span.start;
        for (int p = 0; p < numMovesLeft; p++) {
            editor->command("MoveBackward").execute();
        }

    } else {
        // span is to the right of the cursor

        // walk right to start
        int numMovesRight = span.start - cursorPos;
        for (int p = 0; p < numMovesRight; p++) {
            editor->command("MoveForward").execute();
        }
    }
    
    // select span
    //int numMovesRight = span.end - (cursorPos - span.start) + 1;
    int numMovesRight = span.str.length();
    for (int p = 0; p < numMovesRight; p++) {
        LINKER_TRACE("walking forward at '%c'\n", span.str[p]);
        editor->command("MoveForwardAndModifySelection").execute();
    }
    
    String linkType = kSearchTypeAttributes[searchType];
    String anchorTag = "<a href=\"" + scheme + span.str 
        + "\" type=\"" + linkType +"\">" + span.str + "</a>";
    //editor->command("Delete").execute();
    editor->command("InsertHTML").execute(anchorTag);
    
    LINKER_TRACE("Created link '%s'\n", anchorTag.utf8().data());
    
    // move cursor to original position and unlink any space between link and cursor
    
    if (span.end < cursorPos) {  // span end is to the left of cursor
        int numMoves = cursorPos - span.end - 1;
        // cursor is to the right of end
        for (int p = 0; p < numMoves; p++) {
            editor->command("MoveForwardAndModifySelection").execute();
        }
        if (numMoves > 0) {
            // we already know that the end is behind cursor
            editor->command("Unlink").execute();

            //editor->command("MoveBackward").execute();  // TODO change range the right way
            editor->command("MoveForward").execute();
        }
        
    } else if (span.start <= cursorPos && cursorPos < span.end) {  // cursor is between start and end
        int numMoves = span.end - cursorPos + 1;
        for (int p = 0; p < numMoves; p++) {
            editor->command("MoveBackward").execute();
        }
        
    } else if (cursorPos < span.start && cursorPos < span.end){  // cursor is behind start, so need to unlink gap between start and cursor
        
        // first move back to span.start without highlighting
        int numMovesToStart = span.end - span.start + 1;
        for (int p = 0; p < numMovesToStart; p++) {
            editor->command("MoveBackward").execute();
        }

        int numMovesToCursor = span.start;

        for (int p = 0; p < numMovesToCursor; p++) {
            editor->command("MoveBackwardAndModifySelection").execute();
        }
        if (numMovesToCursor > 0) {
            editor->command("Unlink").execute();
            //editor->command("MoveBackward").execute();  // TODO change range the right way
            editor->command("MoveForward").execute();
        }
    } else {
        LINKER_TRACE(" cursor (%d) at start (%d) and end (%d)\n", 
                cursorPos, span.start, span.end);
    }
    
    return true;
    
}

/**
 * Moves selection to boundary of span and calls editor's Unlink command
 */
bool TextLinker::unlinkSpan(Editor* editor,
        const VisibleSelection& selection,
        const ParserSpan& span)
{
    
    if (span.length() == 0) {
        return false;
    }
    
    if (!editor || selection.isNone()) {
        return false;
    }

    int cursorPos = selection.extent().computeOffsetInContainerNode();
    
    if (span.start < cursorPos) {
        // span start is to the left of cursor
        
        // walk to start 
        int numMovesLeft = cursorPos - span.start;
        for (int p = 0; p < numMovesLeft; p++) {
            editor->command("MoveBackward").execute();
        }
    } else {
        // span is to tnhe right of the cursor

        // walk right to start
        int numMovesRight = span.start - cursorPos;
        for (int p = 0; p < numMovesRight; p++) {
            editor->command("MoveForward").execute();
        }
    }
    
    // select span
    //int numMovesRight = span.end - (cursorPos - span.start) + 1;
    int numMovesRight = span.length();
    for (int p = 0; p < numMovesRight; p++) {
        LINKER_TRACE("walking forward at '%c'\n", span.str[p]);
        editor->command("MoveForwardAndModifySelection").execute();
    }

    editor->command("Unlink").execute();
   
    // remove highlight. this positions cursor at the far right
    editor->command("MoveForward").execute();
 
    // move cursor to original position and unlink any space between link and cursor
    LINKER_TRACE("span.start: %d, span.end: %d, cursorPos: %d\n", span.start, span.end, cursorPos);
    if (span.end < cursorPos) {  // span end is to the left of cursor
        int numMoves = cursorPos - span.end - 1;
        
        // cursor is to the right of end
        for (int p = 0; p < numMoves; p++) {
            editor->command("MoveForward").execute();
            LINKER_TRACE(">");
        }
        LINKER_TRACE("\n");
    } else if (span.start <= cursorPos && cursorPos < span.end) {  // cursor is between start and end
        int numMoves = span.end - cursorPos + 1;
        for (int p = 0; p < numMoves; p++) {
            editor->command("MoveBackward").execute();
            LINKER_TRACE("<");
        }
        LINKER_TRACE("\n");
        
    } else if (cursorPos < span.start && cursorPos < span.end){  // cursor is behind start, so need to unlink gap between start and cursor
        
        int numMovesToStart = span.end + 1;
        for (int p = 0; p < numMovesToStart; p++) {
            editor->command("MoveBackward").execute();
            LINKER_TRACE("<");
        }
        LINKER_TRACE("\n");
    } else {
        LINKER_TRACE(" cursor (%d) at start (%d) and end (%d)\n", 
                cursorPos, span.start, span.end);
    }
     
    return true;
}

bool TextLinker::unlinkNode(Editor* editor, Node* node) 
{
    if (!editor || !editor->frame()) {
        return false;
    }

    LINKER_TRACE("Unlinking contents of node: '%s'\n", node->textContent().utf8().data());

    // save current selection
    VisibleSelection prevSel(editor->frame()->selection()->selection());

    // get selection from contents of current node
    VisibleSelection nodeContents = VisibleSelection::selectionFromContentsOfNode(node);
    
    // set it to current selection so we can perform editor commands on it
    editor->frame()->selection()->setSelection(nodeContents);
    
    editor->command("Unlink").execute();
    
    // restore previous selection
    editor->frame()->selection()->setSelection(prevSel);

    
    return true;
}

}  // namespace WebCore

