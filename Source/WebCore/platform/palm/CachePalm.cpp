
// Copyright 2010 Palm Inc.

#include "config.h"
#include "Cache.h"

#include "CachedCSSStyleSheet.h"
#include "CachedFont.h"
#include "CachedImage.h"
#include "CachedScript.h"
#include "CachedXSLStyleSheet.h"
#include "DocLoader.h"
#include "Document.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "CString.h"
#include "ImageDecoder.h"
#include "SharedBuffer.h"
#include "Image.h"
#include "ResourceHandle.h"
#include <stdio.h>
#include <wtf/CurrentTime.h>
#include "MIMETypeRegistry.h"

#include <glib.h>

namespace WebCore {

/*
 * adjustPathForScaleFactor
 *
 * Check if the resource designed for curent scale exists
 * and if so, change the URL path to load it.
 * When frameView has scale factor other than 1.0
 * we try to load image resources created for the specific scale.
 * The expected path for scale-specific image resource is cur_dir/zoom_factor/image_name
 * for example: "app_path/images/1.5/img.png"
*/
static const double APP_DEFAULT_ZOOM = 1.0;

static void adjustPathForScaleFactor(String& path, DocLoader* docLoader, float& zoomFactor)
{
    zoomFactor = APP_DEFAULT_ZOOM;
    Document* doc = docLoader->doc();
    if (doc && doc->frame() && doc->frame()->view()) {
        float zoom = doc->frame()->view()->zoomFactor();
        if (zoom != APP_DEFAULT_ZOOM) {
            String dir = String::number(zoom);
            dir.append('/');

            int idx = path.reverseFind('/');
            if (idx >= 0) {
                path.insert(dir.characters(), dir.length(), idx+1);
                // check that file exists, if not do not alter the path
                if (!g_file_test(path.utf8().data(), G_FILE_TEST_EXISTS )) {
                    path.remove(idx+1, dir.length());
                }
                else {
                    zoomFactor = zoom;
                }
            }
        }
    }
}

bool Cache::synchronousLoadResource( const KURL& url, DocLoader* docLoader )
{

    if (url.string().contains("extractfs")) {
    	// fall back on AsyncLoader when loading from extractfs
    	return false;
    }

    ASSERT(url.isLocalFile());
    ASSERT(url.protocolIs("file"));

    String strUrl(url);

	// Already cached.
	if( m_resources.get(strUrl) )
		return true;

    String path = url.path();
	
	String  extn;
	int idx = path.reverseFind('.');
	if (idx >= 0) {
		extn = path.substring(idx).lower();
	}

	float zoomFactor = APP_DEFAULT_ZOOM;
	if (!extn.isEmpty() &&
		( extn == ".png" || extn == ".jpg" || extn == ".gif" || extn == ".jpeg" || extn == ".bmp" )) {
		adjustPathForScaleFactor(path, docLoader, zoomFactor);
	}

	if (zoomFactor == APP_DEFAULT_ZOOM && !g_file_test(path.utf8().data(), G_FILE_TEST_EXISTS)) {
		//g_debug( "failed to load %s", path.utf8().data() );
		return false;
	}

	// Load the file contents.
	RefPtr<SharedBuffer> data = SharedBuffer::createWithContentsOfFile( path );

	if( !data )
		return false;
	
	CachedResource* resource = 0;
    if (!extn.isEmpty()) {
    	
        if( extn == ".png" || extn == ".jpg" || extn == ".gif" || extn == ".jpeg" || extn == ".bmp" )
        {
            //printf(" Cache::synchronousLoadResource/img [%s] \n", strUrl.utf8().data() );
            resource = new CachedImage( url, zoomFactor );
            resource->data( data, true );
        }
        else if( extn == ".js" )
        {
            //printf(" Cache::synchronousLoadResource/js [%s] \n", strUrl.utf8().data() );
            resource = new CachedScript( url, String("UTF-8") );
            resource->data( data, true );
        } 
        /*else if( extn == ".css" )
        {
            //printf(" Cache::synchronousLoadResource/css [%s] %d bytes\n", strUrl.utf8().data(), data->size() );
            resource = new CachedCSSStyleSheet( url, String("UTF-8") );
            //resource->setEncoding( String("UTF-8") );
            resource->data( data, true );
        } */
    }
	
	if( resource )
	{
		if (resource->errorOccurred()) {
            // Delete the resource now and return false because otherwise
            // it would leak if no ref/deref was ever done on it.
            resource->setInCache(false);
            delete resource;
			return false;
        }

		ResourceResponse rsp;
		rsp.setURL(url);
		rsp.setHTTPStatusCode(200);
		rsp.setMimeType(MIMETypeRegistry::getMIMETypeForPath(path));
		resource->setResponse(rsp);

		resource->setDocLoader(docLoader);
        resource->finish();
        resource->setLoading(false);

        docLoader->m_documentResources.set(strUrl, resource);
        m_resources.set(strUrl, resource);
        resource->setInCache(true);
                
        return true;
	}
	
	return false;
}

}

