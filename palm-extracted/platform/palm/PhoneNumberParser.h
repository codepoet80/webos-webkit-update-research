/**
 * @file PhoneNumberParser.h
 * @brief Provides API for parsing text for phone number links
 *
 */

#ifndef __PhoneNumberParser_h__
#define __PhoneNumberParser_h__


#include "Parser.h"

#include <wtf/text/WTFString.h>

namespace WebCore
{

class PhoneNumberParser
{    
public:

    enum CharacterType { NumeralChar, DelimiterChar, InvalidChar };
    
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
    
    static bool isPrefixChar(const UChar ch);
    
    static bool isSuffixChar(const UChar ch);
    
    static bool isDelimiterChar(const UChar ch);
    
    static bool isNumeralChar(const UChar ch);

}; // class PhoneNumberParser

} // namespace WebCore

#endif // __PhoneNumberParser_h__


