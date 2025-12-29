
// Copyright 2008,2010 Palm Inc.

#include "config.h"
#include "DirectLoader.h"
#include "CString.h"
#include "MIMETypeRegistry.h"
#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"
#include "SystemTime.h"
#include "HTTPParsers.h"
#include "Base64.h"
#include "TextEncoding.h"

#include "palmwebglobal.h"

#include <sys/types.h>
#include <sys/stat.h>

const int kMaxDirectLoadBufferSize =  ( 500 * 1024 );

namespace WebCore
{

DirectLoader* DirectLoader::instance() 
{
	static DirectLoader* s = 0;
	if( !s ) s = new DirectLoader();
	return s;
}

DirectLoader::DirectLoader()
	: m_serviceQueueTimer(this, &DirectLoader::serviceQueueTimerFired)
	, m_buffer(0)
	, m_bufferSize(0)
{
}

DirectLoader::~DirectLoader()
{
	delete[] m_buffer;
}

bool DirectLoader::load( Frame* frame, const String& path, ResourceHandle* handle )
{
	if( !handle )
		return false;
	
	DirectLoaderQueuedItem* item = new DirectLoaderQueuedItem();
	item->path = path;
	item->handle = handle;
	item->handle->ref();
	m_queue.push_back( item );
    if (!m_serviceQueueTimer.isActive())
		m_serviceQueueTimer.startOneShot(0 /* seconds */);
	return true;
}

void DirectLoader::cancel( ResourceHandle* handle )
{
	for( std::list<DirectLoaderQueuedItem*>::iterator it = m_queue.begin();
		it != m_queue.end(); )
	{
		if( (*it)->handle == handle )
		{
			DirectLoaderQueuedItem* item = *it;
			item->handle->deref();
			delete item;
			it = m_queue.erase( it );
		}
		else
			++it;
	}
}

bool DirectLoader::processDataUrl( DirectLoaderQueuedItem* item )
{
	String url = item->path;
	ASSERT(url.startsWith("data:", false));
	int index = url.find(',');
    if (index == -1) {
        return false;
    }    
    
    String mediaType = url.substring(5, index - 5);
    String data = url.substring(index + 1);
    
    bool base64 = mediaType.endsWith(";base64", false);
    if (base64)
        mediaType = mediaType.left(mediaType.length() - 7);

    if (mediaType.isEmpty())
        mediaType = "text/plain;charset=US-ASCII";

    String mimeType = extractMIMETypeFromMediaType(mediaType);
    String charset = extractCharsetFromMediaType(mediaType);
    
    ResourceHandle* handle = item->handle;
    ResourceResponse response;
    
    response.setURL( KURL( KURL(), url ) );
    response.setHTTPStatusCode( 200 );
    response.setSuggestedFilename( item->path );
    
    response.setMimeType(mimeType);    
    
    if (base64) 
    {
        data = decodeURLEscapeSequences(data);
        response.setTextEncodingName(charset);
        handle->client()->didReceiveResponse(handle, response);

        // WebCore's decoder fails on Acid3 test 97 (whitespace).
        Vector<char> out;
        
        //if (base64Decode(data.latin1().data(), data.latin1().length(), out) && out.size() > 0)
        //    d->client()->didReceiveData(m_job, out.data(), out.size(), 0);
        
        base64Decode(data.latin1().data(), data.latin1().length(), out);
        response.setExpectedContentLength( out.size() );
        
		ResourceRequest redirected( handle->firstRequest() );
		handle->client()->willSendRequest( handle, redirected, ResourceResponse() );
		if( !item->handle->getInternal()->m_cancelled ) 
		{
			handle->client()->didReceiveResponse(handle, response);						
			response.setResponseFired(true);
			handle->client()->didReceiveData(handle,out.data(), out.size(), out.size() );
			handle->client()->didFinishLoading(handle);
		}		
    } 
    else 
    {
        // We have to convert to UTF-16 early due to limitations in KURL
        data = decodeURLEscapeSequences(data, TextEncoding(charset));
        response.setTextEncodingName("UTF-16");
        handle->client()->didReceiveResponse(handle, response);
        
        if (data.length() > 0)
            handle->client()->didReceiveData(handle, reinterpret_cast<const char*>(data.characters()), data.length() * sizeof(UChar), 0);
        
        handle->client()->didFinishLoading(handle);
    }

    
    
	return true;
}

// The file is loaded piecemeal since extractfs,etc does not always return the full file size we have seen.
bool DirectLoader::processLocalFile( DirectLoaderQueuedItem* item )
{
	bool failed = false;
	long sz = 0;
	KURL url(ParsedURLString, item->path);

	const int kMaxFileSizeToLoad = 10 * 1024 * 1024 ; // 10 MB

	String path = url.path();
	FILE* f = fopen( path.utf8().data(), "rb" );
	if (f)
	{
        int fd = fileno(f);

        struct stat info;
        if (fstat( fd, &info ) == 0) {
            sz = info.st_size;
        }
        else {
            failed = true;
        }

        if( !failed && sz < kMaxFileSizeToLoad && sz > 0)
        {
            if( sz > m_bufferSize )
            {
                delete[] m_buffer;
                m_bufferSize = 0;
                m_buffer = new char[ sz+1 ];
                if( m_buffer )
                    m_bufferSize = sz;
            }

            int c = 0;

            if (m_buffer ) {
                while( c < sz ) {
                    int inc = fread( m_buffer + c, 1, sz-c, f );
                    if( 0 == inc || -1 == inc ) {
                        failed = true;
                        break;
                    }
                    c += inc;
                }
            } else
                failed = true;
        }
        else
            failed = true;

        fclose( f );
	}
	else
	    failed = true;
	
	//printf("**** DirectLoader : [%s]  client=%p %d bytes failed=%d\n", 
	//		path.utf8().data(), 
	//		item->handle->getInternal()->client(),
	//		sz, failed
	//		);

	if( !failed )
	{
		ResourceHandle* handle = item->handle;
		if( handle->client() )
		{
			ResourceResponse rsp;
			
			rsp.setURL( url );
			rsp.setHTTPStatusCode( 200 );
			rsp.setMimeType( MIMETypeRegistry::getMIMETypeForPath(path) );
			if( rsp.mimeType() == String("text/html") || rsp.mimeType() == String("text/ascii") )
				rsp.setTextEncodingName( "UTF-8");
			rsp.setSuggestedFilename( path );
			rsp.setExpectedContentLength( sz );
			
			ResourceRequest redirected( handle->firstRequest() );
			handle->client()->willSendRequest( handle, redirected, ResourceResponse() );
			
			if( !item->handle->getInternal()->m_cancelled ) 
			{
				handle->client()->didReceiveResponse(handle, rsp);						
				rsp.setResponseFired(true);
				handle->client()->didReceiveData(handle,m_buffer, sz, 0);
				handle->client()->didFinishLoading(handle);
			}
		}
	}
	else
	{
		ResourceHandle* handle = item->handle;
		if( handle->client() )
		{
			ResourceResponse rsp;
			rsp.setURL( url );
			rsp.setHTTPStatusCode( 404 );
			handle->client()->didReceiveResponse( item->handle, rsp);
			rsp.setResponseFired(true);
			ResourceError err(url.string(), 404, url.string(), String("File not found"));
			handle->client()->didFail( item->handle, err );
		}				
	}
	return true;
}

void DirectLoader::serviceQueueTimerFired(Timer<DirectLoader>* )
{
	if( !m_queue.size() )
		return;

	// Items may be added to the queue as a result of the callbacks
	// we're making, so let's only process what's available right now.
	std::list<DirectLoaderQueuedItem*> items = m_queue;
	m_queue.clear();
	
	// Only handle one item at a time as WebCore is not designed to
	// take results immediately (e.g., XmlHttpRequest)
	for( std::list<DirectLoaderQueuedItem*>::iterator it = items.begin(); it != items.end(); ++it )
	{
		DirectLoaderQueuedItem* item = *it;

		// Defer reads if system is suspending
		if (Palm::WebGlobal::isSystemSuspending()) {
			g_warning("system suspending: deferring read for: %s\n", item->path.utf8().data());
			m_queue.push_back(item);
			continue;
		}
		
		//char ascii[ 512 ];
		//memset(ascii,0,512);
		//strncpy( ascii, item->path.utf8().data(), 511 );
		//printf("**** DirectLoader  %s\n", item->path.utf8().data() );
		
		if( item->handle->getInternal()->m_cancelled )			
		{
			item->handle->deref();
			delete item;
			continue;
		}
		
		KURL url(ParsedURLString, item->path);
		
		if (equalIgnoringCase(url.protocol(), "data"))
		{
			processDataUrl( item );
			item->handle->deref();
			delete item;			
			continue;
		} 
		else
		{
			processLocalFile( item );
			item->handle->deref();
			delete item;			
			continue;
		}

		// free the buffer we used to load the file.
		delete[] m_buffer;
		m_buffer = 0;
		m_bufferSize = 0;
			
	}
	
	// re-queue if there are more items.
	if( m_queue.size() )
	{
		if (!m_serviceQueueTimer.isActive())
			m_serviceQueueTimer.startOneShot(0 /* seconds */);
	}		
	
	// don't keep large buffer around.
	if( m_bufferSize > 250000 ) 
	{
		m_bufferSize = 0;
		delete[] m_buffer;
		m_buffer = 0;
	}
}

}

