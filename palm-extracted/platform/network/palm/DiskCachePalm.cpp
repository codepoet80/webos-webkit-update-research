
// Copyright 2008 Palm Inc.

#include "config.h"
#include "DiskCachePalm.h"
#include "CString.h"
#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"
#include "SystemTime.h"
#include "webkitpalmsettings.h"

#include "DiskCacheDisk.h"

namespace WebCore
{




	                     
DiskCacheEntry::DiskCacheEntry( DiskCache* cache)
	: m_diskCache(cache)
	, m_expiresAt(0)
	, m_accessCount(0)
	, m_lastAccess( time(0) )
	, m_next(0)
	, m_prev(0)
	, m_payload(0)
{
	m_payload = SharedBuffer::create();
}

DiskCacheEntry::~DiskCacheEntry()
{
}

void DiskCacheEntry::addHeader( const String& header )
{
	m_headers.append( header );
}

bool DiskCacheEntry::addData( const unsigned char* data, long long int inSize)
{
    if (size() + inSize >= PalmBrowserSettings()->diskCacheLargestObjectSize) {
        m_payload->clear();
        return false;
    }

	m_payload->append( (const char*)data, inSize );
	return true;
}

bool DiskCacheEntry::getHeader( const char* headerName, String& outValue ) const
{
	String hn(headerName);	
	hn += ":";
	
	for( unsigned i=0; i<m_headers.size(); i++ ) 
	{
		if( m_headers[i].startsWith( hn ) ) 
		{
			// Separate out the name/value, and trim white space from both sides of each.
			Vector<String> r = splitHttpHeaderPair( m_headers[i] );
			outValue = r[1];
			return true;
		}
	}
	
	return false;
}

int DiskCacheEntry::score() const
{
	int s;
	unsigned long sz = size();
	if( !sz ) sz=1;
	int count = m_accessCount ? m_accessCount : 1 ;
	
	int denom = ( sz * (time(NULL)-m_lastAccess) );
	if( !denom ) denom = 1;
	
	s = ( count << 16 ) / denom ;
	//printf( "score %ld for %s\n", s, m_url.utf8().data() );
	return s;
}

bool DiskCacheEntry::isExpired() const
{
    if (!m_expiresAt)
        return true;

	time_t now = time(0);
	return (difftime(now, m_expiresAt) >= 0);
}

// -------------------------------------------------------------------------------------------

DiskCache*	diskCache() 
{
	static DiskCache* gCache = 0;
	if( !gCache )
		gCache = new DiskCacheDisk();
	
	return gCache;
}	
	
// Separate out the name/value, and trim white space from both sides of each.
Vector<String> splitHttpHeaderPair( const String& headerPair )
{
	Vector<String> pair;
	
	int splitPos = headerPair.find(":");
	if (splitPos != -1)
	{
		pair.append( headerPair.left(splitPos).stripWhiteSpace() );
		pair.append( headerPair.substring(splitPos+1).stripWhiteSpace() );
	}	
	
	return pair;
}

} // namespace WebCore

