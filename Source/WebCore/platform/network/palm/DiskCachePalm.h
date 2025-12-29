
// Copyright 2008 Palm Inc.

#ifndef __PalmDiskCache_h__
#define __PalmDiskCache_h__

#include <wtf/text/WTFString.h>
#include "Cache.h"
#include "Timer.h"
#include "HashMap.h"
#include "SharedBuffer.h"

namespace WebCore {
	
class DiskCache;
class DiskCacheEntry;
class ResourceHandle;

// -------------------------------------------------------------------

	
class DiskCacheEntry
{
public:
	DiskCacheEntry(DiskCache* cache);
	~DiskCacheEntry();
	 
	unsigned long size() const { return m_payload->size(); }
	time_t age() const { return time(NULL)-m_lastAccess; }
	void addHeader( const String& header );

	// addData() returns false if size of the object exceeds
	// PalmBrowserSettings()->diskCacheLargestObjectSize.
	// In that case, internal buffer is cleared and DiskCacheEntry
	// should be deleted since it is no longer valid.
	bool addData(const unsigned char* data, long long int inSize);
	int score() const;
	bool isExpired() const ;
	bool getHeader( const char* headerName, String& outValue ) const ;
	
	String			m_url;
	long			m_statusCode;
	long long int	m_expectedContentLength;
	String			m_mimeType;
	String			m_fileName;
	String			m_encodingName;
	String			m_setCookie;
	time_t			m_expiresAt;
	long			m_accessCount;
	Vector<String>	m_headers;
	RefPtr<SharedBuffer>	m_payload;
	ResourceHandle*	m_resourceHandle;
	
	time_t			m_lastAccess;
	DiskCache*		m_diskCache;
	
	DiskCacheEntry* m_prev;
	DiskCacheEntry* m_next;
};

// -------------------------------------------------------------------

class DiskCache 
{
public:
	DiskCache() { }
	virtual ~DiskCache() { }
	
	virtual void			clear() = 0;
	virtual DiskCacheEntry*	contains( const String& url ) = 0;
	virtual DiskCacheEntry* create( ) = 0;
		
	virtual void			remove( const String& url ) = 0;
	virtual void			remove( DiskCacheEntry* ) = 0;
	virtual void			add( DiskCacheEntry* ) = 0;
	virtual void			schedule( DiskCacheEntry* ) = 0;
	virtual void			cancelScheduled( ResourceHandle*  ) = 0;
	
	virtual void			setMaxCapacity( unsigned long sz, bool prune = true ) = 0;
	
};


// -------------------------------------------------------------------

// -------------------------------------------------------------------

Vector<String> splitHttpHeaderPair( const String& headerPair );

DiskCache*	diskCache();

}

#endif // __PalmDiskCache_h__
