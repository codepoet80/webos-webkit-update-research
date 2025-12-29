/**
 * @file UrlParser.h
 * @brief Provides API for parsing text for url strings
 *
 */

#ifndef __UrlParser_h__
#define __UrlParser_h__


#include "Parser.h"

#include <wtf/text/WTFString.h>

namespace WebCore
{

class UrlParser
{
    
public:

    enum CharacterType { UrlChar, InvalidChar };
    
    // Used to expand a candidate string to the left
    static bool searchLeft(const WTF::String& str, 
            const int cursorPos, 
            WebCore::ParserSpan& span); 

    // Used to expand a candidate string to the right
    static bool searchRight(const WTF::String& str, 
            const int cursorPos, 
            WebCore::ParserSpan& span);

    // Used to remove invalid characters between cursor and first matching char
    static void trimFromCursorToEnd(const WTF::String& str, 
            const int limit, 
            int& boundary);
    
    // Used to remove invalid characters between cursor and first matching char
    static void trimFromCursorToStart(const WTF::String& str,
            const int limit, 
            int& boundary);
    
    static int trimFromStart(WebCore::ParserSpan& span);
    
    static int trimFromEnd(WebCore::ParserSpan& span);
    
    static bool isUrlTerminalChar(const UChar ch);
    static bool isUrlChar(const UChar ch);

}; // class UrlParser

} // namespace WebCore

#endif // __UrlParser_h__


