/**
 * TextLinker.h
 * 
 * Used to find link-like patterns in richly editable text fields 
 * and turn them into anchor elements. 
 * 
 */

#ifndef __TextLinker_h__
#define __TextLinker_h__

namespace WebCore {

class VisibleSelection;
class Editor;
class ParserSpan;
class Node;

class VisiblePosition;

class TextLinker
{
public:
    enum SearchType { PhoneNumberSearch = 0, UrlSearch = 1, EmailSearch = 2 };
    
    // Used to find and create links in the richly editable text field that is in focus
    static bool findLinksInText(const VisibleSelection& selection, Editor* editor);
    
    static bool findPhoneNumberLinksInText(const VisibleSelection& selection, Editor* editor);
    
    static bool findUrlLinksInText(const VisibleSelection& selection, Editor* editor);
    
    // Used to extend search for matches to previous text leaves
    static bool searchPreviousSiblings(Editor* editor, 
            const Node* node, 
            const SearchType searchType, 
            ParserSpan& matchSpan);
    
    // Used to extend search for matches to next text leaves
    static bool searchNextSiblings(Editor* editor, 
            const Node* node, 
            const SearchType searchType, 
            ParserSpan& matchSpan);
    
    // Used to test a candidate span for matches by parsers, like PhoneNumberParser
    static bool validateSpan(const ParserSpan& candidate, 
            const SearchType searchType, 
            String& scheme);
    
    // Used to create an anchor element out of a selection covering one or more
    // text nodes
    static bool createLink(Editor* editor, 
            const VisibleSelection& selection,
            const ParserSpan& span, 
            const String& scheme,
            const SearchType searchType);
    
    // Used to remove anchor elements within a range specified by ParserSpan,
    // which may cover multiple text nodes
    static bool unlinkSpan(Editor* editor,
            const VisibleSelection& selection,
            const ParserSpan& span);
    
    // Used to unlink contents of one node
    static bool unlinkNode(Editor* editor, Node* node);

private:
    
}; // class TextLinker

} // namespace WebCore

#endif // __TextLinker_h__
