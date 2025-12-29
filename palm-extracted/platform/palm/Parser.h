/**
 * @file Parser.h
 * 
 */

#ifndef _Parser_h_
#define _Parser_h_

#include <wtf/text/WTFString.h>

namespace WebCore 
{

struct ParserSpan {
    
    int start;  // can be negative if span expansion goes left over node boundaries
    int end;
    
    WTF::String str;  // string within the span
    
    ParserSpan() { 
        start = -1; 
        end = -1; 
        str = "";
    }
    
    int length() const { return ((start <= end) ? end - start + 1 : 0); } 
    
};

}  // namespace WebCore 
#endif // _Parser_H_
