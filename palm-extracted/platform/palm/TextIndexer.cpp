
// Copyright 2008 Palm Inc.

#include "config.h"

#include "TextIndexer.h"

#include "GRegexTextIndexer.h"
#include "PlatformString.h"
#include "QualifiedName.h"
#include "webkitpalmsettings.h"

#include <glib.h>

/*
#undef INDEXER_DEBUG

#ifdef INDEXER_DEBUG
#define INDEXER_TRACE(...) \
do { \
    fprintf(stdout, "TextIndexer: %s: ", __FUNCTION__ ); \
    fprintf(stdout, __VA_ARGS__); \
} while (0)

#else
#define INDEXER_TRACE(...) (void)0
#endif
*/

namespace WebCore
{

const QualifiedName webOSGeneratedTag (nullAtom, "x-webos-generated-tag", AtomicString("http://www.hp.com/webos/"));

static GRegexTextIndexer* indexer = NULL;

GRegexTextIndexer& getIndexer()
{
    if (! indexer) {
        indexer = new GRegexTextIndexer();
        indexer->setImagePath(std::string("file://") + PalmBrowserSettings()->emoticonImagesPath);
    }
    return *indexer;
}

std::string RunIndexerOnUtf8( const std::string& in, unsigned int linkFlags )
{
    return getIndexer().processTextString(in, linkFlags);
}

void RunIndexerOnText( const String& in, PatternMatchs& outLinks )
{
    // HTMLTreeBuilder is only prepared to deal with the following matches
    int flags =
        Palm::TextIndexer_EmailAddress | Palm::TextIndexer_PhoneNumber
        | Palm::TextIndexer_WebLink | Palm::TextIndexer_SchemalessWebLink;

    getIndexer().findMatches(in.utf8().data(), outLinks, flags);
}

void RunIndexerOnText( const std::string& in, PatternMatchs& m)
{
    getIndexer().findMatches(in, m);
}

} //end namespace WebCore

