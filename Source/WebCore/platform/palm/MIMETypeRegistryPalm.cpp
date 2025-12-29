/*
 * Copyright 2008 Palm Inc.
 */

#include "config.h"
#include "MIMETypeRegistry.h"

namespace WebCore {

struct ExtensionMap {
    const char* extension;
    const char* mimeType;
};

// These are types that are handled by WebKit itself.
static const ExtensionMap extensionMap [] = {
    { "bmp", "image/bmp" },
    { "gif", "image/gif" },
    { "html", "text/html" },
	{ "htm", "text/html" },
    //{ "ico", "image/x-icon" },   
    { "jpeg", "image/jpeg" },
    { "jpg", "image/jpeg" },
    { "js", "application/x-javascript" },
    //{ "pdf", "application/pdf" },
    //{ "ppm", "application/x-ppm" },
    { "png", "image/png" },
    //{ "rss", "application/rss+xml" },
    { "swf", "application/x-shockwave-flash" },
    { "text", "text/plain" },
    { "txt", "text/plain" },
    { "xml", "text/xml" },
    { "xsl", "text/xsl" },
    { "xhtml", "application/xhtml+xml" },
#if ENABLE(SVG)
    { "svg", "image/svg+xml" },
#endif
	// add anything we want to support here ... otherwise
	// FrameLoaderpalmClient will be instructed to NOT load it.
	
    { 0, 0 }
};
    
String MIMETypeRegistry::getMIMETypeForExtension(const String &ext)
{
    String s = ext.lower();
    const ExtensionMap *e = extensionMap;
    while (e->extension) {
        if (s == e->extension)
            return e->mimeType;
        ++e;
    }
	
	return String();
}

bool MIMETypeRegistry::isApplicationPluginMIMEType(const String&)
{
    return false;
}

}
