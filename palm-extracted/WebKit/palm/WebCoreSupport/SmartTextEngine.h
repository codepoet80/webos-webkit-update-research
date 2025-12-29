// Copyright 2008 Palm Inc.

#ifndef __SmartTextEngine_h__
#define __SmartTextEngine_h__

#include <unicode/uchar.h>
#include <string>

namespace std
{
	typedef std::basic_string<UChar> ustring;
}

namespace Nova
{

	namespace Utils
	{
		bool isBreakChar(UChar ch);
		bool isTerminalPunctuation(UChar ch);
		bool alwaysSpellCheckAfterChar(UChar32 ch);
		bool isWordBreakChar(UChar ch);
	}


class SmartTextEngine
{
	public:
		SmartTextEngine();
		~SmartTextEngine();

		void resetUndo();
		
		bool Compare( const UChar* s1, const char* s2, size_t n );
		bool Compare( const UChar* s1, const UChar* s2, size_t n );
		
		void reset() { lastCapOffset = -1; }
		
		// last capitolization
		int				lastCapOffset;
		UChar			lastChar;
	
	private:
		class SmartTextEnginePrivate*		e;
};

}

#endif // __SmartTextEngine_h__


