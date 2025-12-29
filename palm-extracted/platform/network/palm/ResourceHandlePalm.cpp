
// Copyright 2008,2009,2010 Palm Inc.

#include "config.h"
#include "ResourceHandle.h"

#include "Frame.h"
#include "Page.h"
#include "Settings.h"
#include "DocLoader.h"
#include "NotImplemented.h"
#include "ResourceHandleInternal.h"

#include "webkitpalmsettings.h"

#include "ResourceLoader.h"
#include "AuthenticationHandler.h"
#include "DiskCachePalm.h"
#include "Timer.h"
#include "CurlHandle.h"
#include "DirectLoader.h"
#include "AsyncLoader.h"

namespace WebCore {

class ResourceSyncHandleClient : public ResourceHandleClient {
public:

    ResourceSyncHandleClient() {
    }

    virtual void didReceiveResponse(ResourceHandle*, const ResourceResponse& response) {
        m_response = response;
    }
    
    virtual void didReceiveData(ResourceHandle*, const char* data, int length, int) {
        m_data.append(data, length);
    }
    
    virtual void didFinishLoading(ResourceHandle*) {
    }
    
    virtual void didFail(ResourceHandle*, const ResourceError& error) {
        m_error = error;
    }
    
    ResourceResponse resourceResponse() const { return m_response; }
    ResourceError resourceError() const { return m_error; }
    Vector<char> data() const { return m_data; }

private:

    ResourceResponse m_response;
    ResourceError m_error;
    Vector<char> m_data;
};


ResourceHandleInternal::~ResourceHandleInternal()
{
}

ResourceHandle::~ResourceHandle()
{
}

bool ResourceHandle::start(Frame* frame)
{
	// for POSTs we do not want to use the cache.	
	if( PalmBrowserSettings()->diskCacheEnabled && 
		 "POST" != firstRequest().httpMethod() )
	{
		// Check the cache to see if we get a hit.
		switch( firstRequest().cachePolicy() )
		{
		case UseProtocolCachePolicy: // normal load
			if( DiskCacheEntry* e = diskCache()->contains( firstRequest().url().string() ) )
			{
				e->m_resourceHandle = this;
				diskCache()->schedule( e );
				return true;
			}
			break;
			
		case ReturnCacheDataDontLoad: // results of a post - allow stale data and only use cache
			if( DiskCacheEntry* e = diskCache()->contains( firstRequest().url().string() ) )
			{
				e->m_resourceHandle = this;
				diskCache()->schedule( e );
				return true;
			}
			else
				return false;
			break;
			
		case ReloadIgnoringCacheData: // reload
			diskCache()->remove( firstRequest().url().string() );
			break;
			
		case ReturnCacheDataElseLoad: // back/forward or encoding change - allow stale data
			if( DiskCacheEntry* e = diskCache()->contains( firstRequest().url().string() ) )
			{
				e->m_resourceHandle = this;
				diskCache()->schedule( e );
				return true;
			}
			break;
			
		}
					
	}
	
	// Handle Data urls with the direct loader.
	KURL kurl = firstRequest().url();
	if (equalIgnoringCase(kurl.protocol(), "data")) 
	{
		return DirectLoader::instance()->load( frame, firstRequest().url().string(), this );
	}
		
	// If this is a local file, (file://) bypass curl and just load the data
	// directly into the handle.
	if( PalmBrowserSettings()->enableFastLocalLoad )
	{
		if( kurl.isLocalFile() )
		{
			d->m_job = 0;

			if (kurl.string().contains("extractfs")) {
				return AsyncLoader::instance()->load(firstRequest().url().string(), this );
			} else {
				return DirectLoader::instance()->load( frame, firstRequest().url().string(), this );
			}

		}
	}


    ResourceHandleInternal* d = getInternal();
    d->m_job = new CurlHandle(this);
    d->m_document = frame->document();

    if (frame->page()->settings()->getHttpProxyEnabled()) {
        d->m_job->useHttpProxy(frame->page()->settings()->getHttpProxyHost(),
                               frame->page()->settings()->getHttpProxyPort());
    }

    if (frame->page()->settings()->getForceNetworkInterfaceEnabled()) {
        d->m_job->useNetworkInterface(frame->page()->settings()->getNetworkInterface());
    }

    if(frame->page()->settings()->getForceDNSServersEnabled()) {
        d->m_job->useDNSServers(frame->page()->settings()->getDNSServers());
    }

    d->m_job->startAsync();

    return true;
}

void ResourceHandle::cancel()
{
	getInternal()->m_cancelled = true;
	
	// the item might be in the directloader.
	DirectLoader::instance()->cancel( this );
	AsyncLoader::instance()->cancel( this );
	
	if( PalmBrowserSettings()->diskCacheEnabled )
	{
		// the item might be scheuled to come from the cache, or from the network:
		diskCache()->cancelScheduled(this);
	}
	
    if (d->m_job) {
        d->m_job->cancel();
        d->m_job = 0;
    }
}

PassRefPtr<SharedBuffer> ResourceHandle::bufferedData()
{
    return 0;
}

bool ResourceHandle::supportsBufferedData()
{
    return false;
}

bool ResourceHandle::willLoadFromCache(ResourceRequest& req, Frame* )
{
	// NOTE : this must be handled for certain POST cases, see FrameLoader.cpp:4085
    return false;
}

bool ResourceHandle::loadsBlocked()
{
    return false;
}

void ResourceHandle::loadResourceSynchronously(const ResourceRequest& req, StoredCredentials cred, ResourceError& error,
                                               ResourceResponse& response, Vector<char>& data, Frame* frame)
{
    ResourceSyncHandleClient client;
    RefPtr<ResourceHandle> handle = adoptRef( new ResourceHandle(req, &client,  true, true));
	
    CurlHandle* job = new CurlHandle(handle.get());
    job->startSync();

    error = client.resourceError();
    data  = client.data();
    response = client.resourceResponse();
    
    WEBOS_LOG("got %d bytes\n", data.size() );

	delete job;
}

void ResourceHandle::receivedCredential(const AuthenticationChallenge& challenge, const Credential& credential)
{
    if (d->m_job)
        d->m_job->receivedCredential(challenge, credential);
}

void ResourceHandle::receivedCancellation(const AuthenticationChallenge& challenge)
{
    if (d->m_job)
        d->m_job->receivedCancellation(challenge);
}

void ResourceHandle::platformSetDefersLoading(bool)
{
    notImplemented();
}

} // namespace WebCore
