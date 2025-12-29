
// Copyright 2008 Palm Inc.

// Based on :
// http://wiki.palm.com/display/HI/Text+Edit

#include "SmartTextEngine.h"
#include "webosDeviceKeydefs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <functional>
#include <map>
#include <algorithm>
#include "PlatformString.h"

#include <palmwebglobal.h>

#include <wchar.h>
#include <wctype.h>


namespace Nova
{

namespace Utils
{

const UChar k_newline = '\x0a';
const UChar k_carriage_return = '\x0d';
const UChar k_nbsp = 160;

bool isTerminalPunctuation( UChar ch )
{
		if( ch == k_newline ||
			ch == k_carriage_return ||
			ch == '.' ||
			ch == 190 ||  // period raw key
			ch == Key_Period ||
			ch == '!' ||
			ch == '?' )
		return true;
	else
		return false;
}

//TODO: HACK: added the nospace break char (nbsp) 0xA0 for EditorClientPalm use
bool isBreakChar( UChar ch )
{
	if( ch == ' ' ||
		ch == k_nbsp ||
		ch == ',' ||
		ch == 188 ||  // comma raw value
		ch == ':' ||
		ch == ';' ||
//		ch == '\'' ||
		ch == '\"' ||
		ch == '(' ||
		ch == ')' ||
		ch == '{' ||
		ch == '}' ||
		ch == '[' ||
		ch == ']' ||
		ch == '<' ||
		ch == '>' ||
		ch == '\\' ||
		ch == '/' ||
		ch == '|' )
		return true;
	else 
		return false;

}

/**
 * Is this character *always* a word break character? Remember that we want to support
 * entering URL's, email addresses, etc.
 */
bool isWordBreakChar( UChar ch )
{
	return ch == k_nbsp ||
		ch == ' ' ||
		ch == '\t' ||
		ch == k_newline ||
		ch == k_carriage_return ||
		ch == '\0';
}

bool alwaysSpellCheckAfterChar(UChar32 ch)
{
	if (ch == 0)
		return true;
	return false;
}

}

// Internal state.
class SmartTextEnginePrivate
{
public:
	SmartTextEnginePrivate();
	~SmartTextEnginePrivate();

	int				m_lastReplaceIndex;

	static bool	s_Tables_init;
};

bool 									SmartTextEnginePrivate::s_Tables_init=false;

SmartTextEnginePrivate::SmartTextEnginePrivate()
{
	m_lastReplaceIndex = -1;
}

SmartTextEnginePrivate::~SmartTextEnginePrivate()
{
}

// ---------------------------- outer wrapper ---------------------------

SmartTextEngine::SmartTextEngine()
{
	e = new SmartTextEnginePrivate();
}

SmartTextEngine::~SmartTextEngine()
{
	delete e;
}

bool SmartTextEngine::Compare( const UChar* s1, const char* s2, size_t n)
{
	for( size_t i=0; i<n; i++ )
		if( *s1++ != *s2++ )
			return false;
	return true;
}

bool SmartTextEngine::Compare( const UChar* s1, const UChar* s2, size_t n )
{
	for( size_t i=0; i<n; i++ )
		if( *s1++ != *s2++ )
			return false;
	return true;
}


void SmartTextEngine::resetUndo()
{
	lastCapOffset = -1;
}

} // namespace Nova

