/**
 * Walks left and right of cursor position to find phone number candidate strings
 * 
 * 
 */

#include "UrlParser.h"

#include <wctype.h>

#undef PARSER_DEBUG

#ifdef PARSER_DEBUG 
#define PARSER_TRACE(...) \
do { \
    fprintf(stdout, "UrlParser: %s: ", __FUNCTION__ ); \
    fprintf(stdout, __VA_ARGS__); \
} while (0)

#else
#define PARSER_TRACE(...) (void)0
#endif


namespace WebCore {


/**
 * 
 * 
 * @return true if a candidate phone number string was found
 * 
 */
bool UrlParser::searchLeft(const WTF::String& str, 
        const int cursorPos, WebCore::ParserSpan& span)
{
    // we're looking to the left of the given cursor position, so need to move
    // back to the previous character
    span.start = cursorPos - 1;
    span.end = cursorPos - 1;

    PARSER_TRACE("left: %d, right: %d, cursor: %d\n", span.start, span.end, cursorPos);
    
    // validate cursor position
    if (span.start < -1 || cursorPos > (int)str.length()) {
        PARSER_TRACE("left boundary (%d) or cursor pos > str.length() (%d > %d)\n",
                span.start, cursorPos, str.length());
        return false;
    }

    bool candidateFound = false;
    
    int firstValidCharPos = -1;
    
    CharacterType lastCharType = InvalidChar;
    
    while (span.start >= 0)  // if zeroth char is invalid, it will be trimmed
    {
        UChar ch = str[span.start];
        
        // if we've seen a number, we can stop searching at next failure
        if (isUrlChar(ch))
        {
            if (firstValidCharPos == -1) {  
                firstValidCharPos = span.start;
            }
            
            candidateFound = true;
            lastCharType = UrlChar;
            span.start--;
            
        } else if (candidateFound) {
            lastCharType = InvalidChar;
            break;
            
        } else {

            lastCharType = InvalidChar;
            
            // allow one invalid char. we may be expanding the anchor tag with 
            // the current invalid char
            if (span.start == cursorPos - 1) {
                span.start--;
            } else {
                break;
            }
        }
    }
    
    
    if (span.start < 0) {
        // may have had a valid first char
        span.start = 0;
        /*
        if (!candidateFound && isDelimiterChar(str[span.start])) {
            // the first candidate is actually a delimiter at the very beginning
            candidateFound = true;
        }
        */
    }
    

    if (firstValidCharPos != -1) {
        PARSER_TRACE("firstValidDelimiterPos resets rightBoundary to (%d)\n",
                firstValidCharPos);
        span.end = firstValidCharPos;
    }
    
    PARSER_TRACE("stopped expansion at char '%c'=%d, left: %d, right: %d, cursor: %d\n",
            (char)(str[span.start]), (int)(str[span.start]), span.start, span.end, cursorPos);
    
    // move right until we encounter a valid prefix, delimiter, or phone number char
    // not trimming delimiters if we are at a leaf boundary
    // example: 
    // '|' is text leaf boundary
    // "415 | 640-9882"
    //       ^-- cursor 
    // In this case, we don't want to lose the " " at the boundary
    
    if (candidateFound) {
        while (span.start < cursorPos) 
        {
            UChar ch = str[span.start];
        
            if (!isUrlChar(ch)) {
                span.start++;
            } else {
                break;
            }
        }

        trimFromCursorToStart(str, 
                span.start, // limit of trim 
                span.end);

        PARSER_TRACE("after trim, END: %d\n", span.end);
    }
    
    if (! candidateFound) {
        PARSER_TRACE("no candidates found. Left: %d, right: %d, cursorPos: %d\n",
                span.start, span.end, cursorPos);
    } else {
        PARSER_TRACE("candidates found: left: %d, right %d, cursorPos: %d\n", 
                span.start, span.end, cursorPos);
    }
    
    return candidateFound;
}

int UrlParser::trimFromStart(WebCore::ParserSpan& span)
{
    int start = 0; 
    int strLen = span.str.length();
    while (start < strLen)
    {
        UChar ch = span.str[start];
        if (!isUrlChar(ch)) {
            start++;
        } else {
            break;
        }
    }
    
    if (start > 0) {
        span.start += start;
        span.str = span.str.substring(start, strLen - start);
    }
    
    
    return span.length();
}

int UrlParser::trimFromEnd(WebCore::ParserSpan& span)
{
    
    int end = span.str.length() - 1;
    int strLen = span.str.length();
    while (end >= 0) 
    {
        UChar ch = span.str[end];
        
        if (isUrlTerminalChar(ch) || !isUrlChar(ch)) {  // set of valid terminal chars for partial match
            end--;
        } else {
            break;
        }
    }
    
    int movesLeft = strLen - 1 - end; 
        
    if (movesLeft > 0) {
        span.end -= movesLeft;
        span.str = span.str.substring(0, span.str.length() - movesLeft);
    }
   
    return span.length();
}

bool UrlParser::searchRight(const WTF::String& str, 
        const int cursorPos, WebCore::ParserSpan& span)
{
    // we're looking to the left of the given cursor position, so need to move
    // back to the previous character
    span.start = cursorPos;
    span.end = cursorPos;

    // validate cursor position, which is a zero-based index
    if (span.end < 0 || cursorPos >= (int)str.length()) {
        return false;
    }

    bool candidateFound = false;
    
    int firstValidCharPos = -1;
    
    CharacterType lastCharType = InvalidChar;
    
    // move leftward until the first match is found
    int lastCharIndex = (int)str.length() - 1;
    while (span.end <= lastCharIndex) 
    {
        UChar ch = str[span.end];
        
        // if we've seen a url char, we can stop searching at next failure
        if (isUrlChar(ch))
        {
            if (firstValidCharPos == -1) {
                firstValidCharPos = span.end;
            }
            
            candidateFound = true;
            lastCharType = UrlChar;
            span.end++;
            
        } else {
            lastCharType = InvalidChar;
            
            break;
        }

    }
    
    PARSER_TRACE("stopped expansion at char '%c'=%d, left: %d, right: %d, cursor: %d\n",
            (char)(str[span.end]), (int)(str[span.end]), span.start, span.end, cursorPos);
    
    // move right boundary back to the left until we encounter a valid phone number char
    // ex: "last. (415)555-1212. Go"
    //                          ^-- cursor pos, should move left to '2'
    
    // TODO (AD) For capturing of delimiter at leaf boundary, we can't perform
    // the right trim, as we do here. Caller of this function should use trimEnd
    while (span.end >= span.start) 
    {
        UChar ch = str[span.end];
        
        if (!isUrlChar(ch)) {  // set of valid terminal chars for partial match
            span.end--;
        } else {
            candidateFound = true;
            break;
        }
    }
    
    if (!candidateFound) {
        PARSER_TRACE("no candidates found. Left: %d, right: %d, cursorPos: %d\n",
                span.start, span.end, cursorPos);
        // no candidates found to the right. Since numerals are the only valid
        // number suffixes, we can safely exit
    } else {
        PARSER_TRACE("candidates found: left: %d, right %d, cursorPos: %d\n", 
                span.start, span.end, cursorPos);

        PARSER_TRACE("after trim, END: %d\n", span.end);
    }
    
    return candidateFound;
}

void UrlParser::trimFromCursorToStart(const WTF::String& str,
        const int limit, int& boundary)
{
    // move left until we encounter a valid phone number char 
    while (boundary > limit) 
    {
        UChar ch = str[boundary];
        
        PARSER_TRACE("see char '%c' (%d)\n", (char)ch, (int)ch);
        
        if (!isUrlChar(ch)) {
            boundary--;
        } else {
            break;
        }
    }
}

/**
 * Should be called only if a number is found to the right
 * 
 * 
 */
void UrlParser::trimFromCursorToEnd(const WTF::String& str, 
        const int limit, int& boundary)
{

    // move left boundary to right until we hit a potential number
    // ex: last. Phone-(415)555-1212
    //        ^--- cursor pos, should move right to '('
    
    // ex: cursor is in the middle of a valid number
    // ph: 415 555-1212
    //        ^--- cursor, should not skip space
    
    CharacterType lastCharType = InvalidChar;
    
    while (boundary < limit)  
    {
        UChar ch = str[boundary];

        PARSER_TRACE("see char '%c' (%d)\n", (char)ch, (int)ch);
        
        if (!isUrlChar(ch)) {
            boundary++;
        } else {
            break;
        }
    }
}

// Valid url terminals for as-you-type linking that won't be included in the link
bool UrlParser::isUrlTerminalChar(const UChar c) 
{
    if (c == '.' || c == ',' || c == '!' || c == ' ' || c == ';' || c == 13 ||
        c == '(' || c == ')' || c == '?' || c == '-') {
        return true;
    }

    return false;
}

bool UrlParser::isUrlChar(const UChar c)
{
        if (iswalnum(c)) {  // is wide alphanumeric
            return true;
        }
        
        if (c == ':' || c == '/' || c == '-' || c == '_' || c == '&' ||
            c == '.' || c == '?' || c == '%' || c == '=' || c == '#' || 
            c == '~' || c == '[' || c == ']' || c == '@' || c == '!' ||
            c == '*' || c == '+' || c == ',' || c == ';' || c == '(' ||
            c == ')') { 
            return true;
        }

        return false;
    }

} // namespace WebCore
