
// Copyright 2008 Palm Inc.

#ifndef __TextIndexer_h__
#define __TextIndexer_h__

#include <vector>
#include <map>
#include <string>
#include <palmwebglobal.h>

#include "QualifiedName.h"
#include "GRegexTextIndexer.h"

namespace WebCore
{

extern const QualifiedName webOSGeneratedTag;

typedef std::vector<PatternMatch> PatternMatchs;

/**
 * Run the text indexer (linkifier) on the given UTF8 encoded text.  The text returned will
 * have any links encountered expanded based upon the flags passed in.  The flags are defined
 * in palmwebglobal.h.  The result will also be in UTF8, which is the natural format of the
 * regular expression engine being used to discover the link patterns and and perform replacement.
 *
 * @return the utf8 encoded result of processing the given text and expanding the search
 *  patterns to their html form.
 */
std::string RunIndexerOnUtf8( const std::string& in, unsigned int linkFlags );

/**
 * Run the text indexer on the given utf-16 string and collect the expression matches found
 * in the text.  This is used by HTMLTreeBuilder to create links in an elements text if it is not a link,
 * and by TextLinker.
 */
void RunIndexerOnText( const String& in, PatternMatchs& outLinks );

/**
 * Locate expression matches on the given utf8 string, used for testing.
 */
void RunIndexerOnText( const std::string& in, PatternMatchs& m);

} // namespace WebCore

#endif


