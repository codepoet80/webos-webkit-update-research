
// Copyright 2008-2009 Palm Inc.

#include "config.h"
#include "DiskCachePalm.h"
#include "CString.h"
#include "ResourceHandle.h"
#include "ResourceHandleClient.h"
#include "ResourceHandleInternal.h"
#include "SystemTime.h"
#include "webkitpalmsettings.h"
#include "FileSystem.h"
#include "DiskCacheDisk.h"

#include <dirent.h>
#include <sys/stat.h>
#include <wctype.h>
#include <glib.h>
#include <errno.h>
#include <openssl/sha.h>

namespace WebCore
{
	
static DiskCacheEntry* readFromFile( const char* fn );
static long writeToFile( const std::string& fn, DiskCacheEntry* de );
static int readLineFromFile( FILE* f, String& result );
static void writeLineToFile( FILE* f, const String& value );
static bool readIntFromFile( FILE* f, long& result );
static bool writeIntToFile( FILE* f, const long value );
static String formUrlFileName( const String& url );
static bool eTagHasExpired( DiskCacheEntry* e, const String& url );

const double pollTimeSeconds = 0.0;

static int BLOCK_SIZE;
static inline int roundUpToBlockSize( int s ) {
        return ( ( ( s / BLOCK_SIZE ) + 1 ) * BLOCK_SIZE );
}

/**
 * Just a simple FILE wrapper class to help prevent the file from being left open.
 */
class Stream
{
public:

	Stream(const std::string& fname, const char* mode) :
		m_file(fopen(fname.c_str(), mode))
		, m_deleteOnClose(false)
		, m_fname(fname) {
	}
	~Stream() {
		close();
	}
	void setDeleteOnClose(bool delOnClose) {
		m_deleteOnClose = delOnClose;
	}
	bool opened() const {
		return m_file != NULL;
	}
	void close() {
		if (m_file) {
			fclose(m_file);
			m_file = NULL;
			if (m_deleteOnClose) {
				::unlink(m_fname.c_str());
			}
		}
	}
	long getPos() const {
		return ftell(m_file);
	}
	operator FILE* () { return m_file; }

private:

	FILE* m_file;
	bool m_deleteOnClose;
	std::string m_fname;

	Stream& operator=(const Stream&) {return *this;}
};

DiskCacheDisk::DiskCacheDisk() : 
    m_serviceRequestsTimer(this, &DiskCacheDisk::serviceRequestsTimerFired),
    m_lastPruneExpiredAndScore(0),
    m_scores()
{
	init();
}

DiskCacheDisk::~DiskCacheDisk()
{
}
	
void DiskCacheDisk::init()
{
	m_maxSizeBytes = 8 * 1024 * 1024; 
	m_currentSizeBytes = 0;
	m_diskCachePath  = String::fromUTF8( PalmBrowserSettings()->diskCachePath );
	
	makeAllDirectories( m_diskCachePath );

	inventoryDiskCache();
}

// Here we can the items already on disk so we can get a current
// capacity -- then we can enforce capacity without having to
// hit the disk everytime. 
void DiskCacheDisk::inventoryDiskCache()
{
	m_currentSizeBytes = 0;
	std::string path = m_diskCachePath.utf8().data();
	
        struct stat fileStats;
        if (stat(path.c_str(), &fileStats) == 0)
            BLOCK_SIZE = fileStats.st_blksize;
        else
            BLOCK_SIZE = 32768; // stat failed; use correct hardcoded value for /media as on 6/9/11

	DIR* dir = opendir(path.c_str());
	if( dir )
	{
		struct dirent* entry = readdir(dir);
		while( entry )
		{
			std::string fullpath = path + std::string(entry->d_name);
			if (stat(fullpath.c_str(), &fileStats) == 0)
				m_currentSizeBytes += fileStats.st_blocks * S_BLKSIZE;
			entry = readdir(dir);
		}

		closedir(dir);
	}
	
}

void DiskCacheDisk::setMaxCapacity( unsigned long sz, bool prune )
{
	m_maxSizeBytes = sz;
	
	if( !sz )
	{
		clear();
		return;
	}
	if( prune )
		pruneToFit( 1 );
}

void DiskCacheDisk::schedule( DiskCacheEntry* e )
{
	m_scheduled.append(e);
    if (!m_serviceRequestsTimer.isActive())
		m_serviceRequestsTimer.startOneShot(pollTimeSeconds);
}

void DiskCacheDisk::cancelScheduled( ResourceHandle* h )
{
	int idx=0;
	for( Vector<DiskCacheEntry*>::iterator i = m_scheduled.begin(); i <m_scheduled.end(); ++i, ++idx )
	{
		if( (*i)->m_resourceHandle ==h )
		{
			m_scheduled.remove(idx);
			return;
		}
	} 
	
}

void DiskCacheDisk::serviceRequestsTimerFired(Timer<DiskCacheDisk>*)
{
	// Pull out the entries we want to process as sending new data back
	// into webkit might populate the disk cache with new entries which 
	// we do not want to process on this callback, since WebKit can not
	// always handle immediate data available (e.g., XmlHttpRequest)
	Vector<DiskCacheEntry*> entriesToProcess;
	
	entriesToProcess = m_scheduled;
	m_scheduled.clear();
	
	while( entriesToProcess.size() )
	{
		DiskCacheEntry* e = entriesToProcess[0];
		entriesToProcess.remove(0);
	
		if( !e->m_resourceHandle )
			return;	
		ResourceHandleInternal* d = e->m_resourceHandle->getInternal();
		ResourceHandleClient* client = d->client();
		
		if( !client )
			continue ;
		
		d->m_response.setURL( KURL(ParsedURLString,e->m_url.utf8().data()) );
		d->m_response.setHTTPStatusCode( e->m_statusCode );
		d->m_response.setMimeType( e->m_mimeType );
		d->m_response.setTextEncodingName( e->m_encodingName );
		d->m_response.setSuggestedFilename( e->m_fileName );
		d->m_response.setExpectedContentLength( e->m_expectedContentLength );
		
		d->m_response.setHTTPHeaderField( "Warning", "110" );
		
		for( unsigned i=0; i<e->m_headers.size(); i++ ) {
			Vector<String> pair = splitHttpHeaderPair( e->m_headers[i] );
			if( pair.size() == 2 ) {
				d->m_response.setHTTPHeaderField( pair[0], pair[1] );
			}
		}
		
		client->didReceiveResponse( e->m_resourceHandle, d->m_response);
		d->m_response.setResponseFired(true);
		
		if( e->size() )
			client->didReceiveData(e->m_resourceHandle,(char*)(e->m_payload->data()), e->m_payload->size(), 0);
		client->didFinishLoading(e->m_resourceHandle);

#if USE(CURL)
		e->m_resourceHandle->deref(); // Should I be doing this ?
#endif        
		delete e;
	}
	if (!m_serviceRequestsTimer.isActive() && m_scheduled.size() )
		m_serviceRequestsTimer.startOneShot(pollTimeSeconds);
}

void DiskCacheDisk::statistics()
{
	if( !getenv("WEBKIT_LAYOUT_TESTS") )
		printf( "Disk(RAM) Cache current=% 8ld k   max=% 8ld k\n", m_currentSizeBytes/1024, m_maxSizeBytes/1024 );
}

static size_t writeFunction( void *ptr, size_t size, size_t nmemb, void *stream) { return -1; }

static String etagString;
static size_t etagHeaderCallback( void *ptr, size_t size, size_t nmemb, void *stream)
{
	String header( (const char*)ptr, nmemb * size );
	if( header ==String( "\r\n") )
		return -1; // cancel this curl request.
	
	if( header.find( "ETag:" ) != notFound  )
	{
		Vector<String> pair = splitHttpHeaderPair( header);
		if( pair.size() == 2 ) {
			etagString = pair[1];
			return -1;  // cancel this curl request.
		}
	}
	
	return (nmemb * size);
}


// If the cached item contains an ETag, check 
static bool eTagHasExpired( DiskCacheEntry* e, const String& url )
{
	String etag_cached;
	bool debugOn = PalmBrowserSettings()->diskCacheDebug;
	
	for( unsigned i=0; i<e->m_headers.size(); i++ ) {
		if( debugOn ) 
			printf(" %s\n", e->m_headers[i].stripWhiteSpace().utf8().data()  ); 
		if( e->m_headers[i].find( String("ETag:") ) != notFound ) {
			Vector<String> pair = splitHttpHeaderPair( e->m_headers[i] );
			if( pair.size() == 2 ) {
				etag_cached = pair[1];
			}
			break;
		}		
	}
	
	if( !etag_cached.length() )
		return false; // no etag means -- not expired.
	
	if( debugOn ) 
		printf( "ETAG : check for expiration. url=%s  etag=[%s]\n", url.utf8().data(), etag_cached.utf8().data() );

	unsigned long startTime;
	
	if( debugOn )
		startTime = palm_time_ms();
	
	
	// Unfortunatly we have to contact the server to find out if it has expired.
	CURL* h  = 0;
	etagString ="";
	h = curl_easy_init();
	if( !h )
		return false;
	
    curl_easy_setopt( h, CURLOPT_PRIVATE, 0);
	curl_easy_setopt( h, CURLOPT_WRITEDATA, 0);
	curl_easy_setopt( h, CURLOPT_HEADERFUNCTION, etagHeaderCallback);
    curl_easy_setopt( h, CURLOPT_WRITEHEADER, 0);
	curl_easy_setopt( h, CURLOPT_WRITEFUNCTION, writeFunction );
	curl_easy_setopt( h, CURLOPT_TIMEOUT, 3 /* seconds */ );
    curl_easy_setopt( h, CURLOPT_URL, url.utf8().data());
	
	CURLcode r = curl_easy_perform( h );
	curl_easy_cleanup( h );
	
	if( debugOn )
		printf("eTag check took %ld ms\n", palm_time_ms()-startTime );
	
	if( etagString.length() )
	{
		if( debugOn ) 
			printf("new:[%s]   old:[%s]\n", etagString.utf8().data(), etag_cached.utf8().data() );
		if( etagString != etag_cached )
			return true;
	}
	else
	{
		if( debugOn ) 
			printf("etagString == zero. result code is %s\n", curl_easy_strerror(r) );
	}
	
	return false;
}

DiskCacheEntry* DiskCacheDisk::contains( const String& url )
{
	if( !url.length() )
		return 0;
	
	String filename = formUrlFileName(url);
	std::string path = m_diskCachePath.utf8().data() + std::string( filename.utf8().data() );
	DiskCacheEntry* e = readFromFile( path.c_str() );
		
	if( e )
	{
		if( !e->isExpired()/*Temporarily disable eTag support, it hangs BrowserServer when refresh endaget.com && !eTagHasExpired(e,url) */)
		{
			// update the access count field in the meta data.
			Stream f( path, "r+b" );
			if( f.opened() ) {
				long payloadsize;
				if (readIntFromFile( f, payloadsize )) {
					fseek( f, payloadsize+sizeof(long), SEEK_SET ); // skip past payload size and payload
					long accessCount = e->m_accessCount + 1;
					if (!writeIntToFile( f, accessCount )) {	// the first int is the access count
						g_critical("Error writing access count");
					}
				}
				else {
					g_critical("Error reading cache payload size");
				}
			}
			e->m_url = url;
		}
		else
		{
			if( PalmBrowserSettings()->diskCacheDebug )
				printf("DISK-CACHE: EXPIRED : %s\n",  url.utf8().data() );
			remove( url );
			delete e;
			e =0 ;
		}
	}
	
	if( PalmBrowserSettings()->diskCacheDebug ) 
	{
		if( !e )
			printf( "DISK-CACHE: MISS : %s\n", url.utf8().data() );
		else
			printf( "DISK-CACHE:  HIT : %s  size=%ld\n", url.utf8().data(), e->size() );
	}
	
	return e;
}

void DiskCacheDisk::remove( const String& url )
{	
	if( !url.length() )
		return;
	
	struct stat fileStats;
	long before = m_currentSizeBytes;
	
	String filename = formUrlFileName(url);
	std::string path = m_diskCachePath.utf8().data() + std::string( filename.utf8().data() );
	if (stat(path.c_str(), &fileStats) == 0)
		m_currentSizeBytes -= fileStats.st_blocks * S_BLKSIZE;
	int r = unlink( path.c_str() );
	if( PalmBrowserSettings()->diskCacheDebug ) 
		printf(" --> unlink %s (%d)\n", path.c_str(), r );
	
	
	if( PalmBrowserSettings()->diskCacheDebug ) 
		printf("DISK-CACHE REMOVE %s  %ld k -> %ld k \n", url.utf8().data(), before/1024, m_currentSizeBytes /1024 );
}

void DiskCacheDisk::remove( DiskCacheEntry* e )
{
	// Deprecated
	abort();
}

void DiskCacheDisk::add( DiskCacheEntry* e )
{
	// if this entry is larger than the cache size, we'll never be able to add it.
	if( e->size() >= PalmBrowserSettings()->diskCacheLargestObjectSize )
	{
		if( PalmBrowserSettings()->diskCacheDebug )
			printf("DISK-CACHE DiskCacheDisk::add() not adding %s because object is %d bytes ( > %d ) \n", 
					e->m_url.utf8().data(), (int)e->size(), (int)PalmBrowserSettings()->diskCacheLargestObjectSize );
		delete e;
		return;
	}
	
	String filename = formUrlFileName(e->m_url);
	
	// if there's no URL, we can't compute the hash, etc., so bail.
	if( !pruneToFit( roundUpToBlockSize( e->size() ) ) )
	{
		if( PalmBrowserSettings()->diskCacheDebug )
			printf("DISK-CACHE DiskCacheDisk::add() not adding %s because prune failed \n", e->m_url.utf8().data() );
		delete e;
		return;
	}	
	
	std::string path = m_diskCachePath.utf8().data() + std::string( filename.utf8().data() );
	long fileSizeBytes = writeToFile( path, e );
	if (fileSizeBytes == 0 && errno != ENAMETOOLONG) {
        // Our cache implementation doesn't support long URL's so don't cinsider this a warning
        // since it happens fairly often.
		g_warning("ERROR %d writing disk cache entry to file", errno);
	}
	m_currentSizeBytes += roundUpToBlockSize(fileSizeBytes);
	
	if( PalmBrowserSettings()->diskCacheDebug )
		printf("DISK-CACHE DiskCacheDisk::add() added %s, %ld bytes\n", e->m_url.utf8().data(), fileSizeBytes );
	
	delete e;
}

// Prune the cache to fit this "new" item. Worst case would be
// clearing all items in the database to fit this item.
bool DiskCacheDisk::pruneToFit(unsigned long deltaSize)
{
    if (!PalmBrowserSettings()->diskCacheEnabled)
        return true;

    if (PalmBrowserSettings()->diskCacheDebug) {
        printf("------------ pruneToFit( %lu )\n", deltaSize );
        printf(" prune(1) deltaSize=%lu  m_currentSize=%lu k  m_maxSize=%lu k\n", deltaSize, m_currentSizeBytes/1024, m_maxSizeBytes/1024 );
    }

    if ((deltaSize + m_currentSizeBytes) <= m_maxSizeBytes)
        return true;

    unsigned long before = m_currentSizeBytes;
    pruneExpiredAndScore();

    if ((deltaSize + m_currentSizeBytes) <= m_maxSizeBytes)
        return true;

    if (PalmBrowserSettings()->diskCacheDebug)
        printf(" prune-force(2) deltaSize=%lu  m_currentSize=%lu k  m_maxSize=%lu k\n", deltaSize, m_currentSizeBytes/1024, m_maxSizeBytes/1024 );

    while (pruneLowestScore())
        if ((deltaSize + m_currentSizeBytes) <= m_maxSizeBytes)
            return true;

    if (PalmBrowserSettings()->diskCacheDebug)
        printf("DISK-CACHE pruneToFit(3)  %ld k -> %ld k (can't erase enything else)\n", before/1024, m_currentSizeBytes/1024 );

    return false; // we didn't make our target (we'll never get here)
}

void DiskCacheDisk::pruneExpiredAndScore()
{
    time_t now = time(0);

    // Prune on expiration time no more than once per minute. We still check expiration
    // time in contains(), so we will never return an expired resource, even if it is
    // still in the cache.
    //
    // Rescore files no more than once per minute. Scores are based on access count
    // and last accessed time. These are unlikely to change within one minute. New
    // items are not added to the scored list, so they are not even eligible for
    // purging! This could cause a rare condition where the cache is completely filled
    // with new items, less than one minute old, and we want to discard one of them
    // in order to add another. To cover that case, we also test if m_scores is empty.
    if (difftime(now, m_lastPruneExpiredAndScore) >= 60 || m_scores.empty())
    {
        m_scores.clear();

        unsigned long before = m_currentSizeBytes;
        if (PalmBrowserSettings()->diskCacheDebug)
            printf(" prune-expire(1) m_currentSize=%lu k  m_maxSize=%lu k\n", m_currentSizeBytes/1024, m_maxSizeBytes/1024 );

        std::string path = m_diskCachePath.utf8().data();
        DIR* dir = opendir(path.c_str());
        if (dir)
        {
            for (struct dirent* entry = readdir(dir); entry; entry = readdir(dir))
            {
                if (entry->d_name[0] != '.')
                {
                    std::string fullPath = path + std::string(entry->d_name);
                    Stream f(fullPath,"rb");
                    if (f.opened()) {
                        long expiresAt(0), lastAccess(0), accessCount(0), payloadsize(0);

                        readIntFromFile(f, payloadsize);
                        fseek(f, payloadsize + sizeof(long), SEEK_SET); // skip past payload size & payload
                        readIntFromFile(f, lastAccess);
                        readIntFromFile(f, expiresAt);
                        readIntFromFile(f, accessCount);

                        if (expiresAt && difftime(now, expiresAt) >= 0) {
                            if (PalmBrowserSettings()->diskCacheDebug) {
                                readIntFromFile(f, expiresAt);
                                readIntFromFile(f, expiresAt);
                                String url_expired;
                                readLineFromFile(f, url_expired);
                                printf("DISK-CACHE (during prune) expired: %s\n", url_expired.utf8().data());
                            }

                            f.close();

                            struct stat fileStats;
                            if (stat(fullPath.c_str(), &fileStats) == 0) {
                                unlink(fullPath.c_str());
                                m_currentSizeBytes -= fileStats.st_blocks * S_BLKSIZE;
                            }
                        }
                        else {
                            f.close();

                            unsigned long sz = 0;
                            struct stat fileStats;

                            if (stat(fullPath.c_str(), &fileStats) == 0)
                                sz = fileStats.st_blocks * S_BLKSIZE;
                            if (!sz)
                                sz = 1;
                            if (!accessCount)
                                accessCount = 1;
                            int denom = sz * (now - lastAccess);
                            if (!denom)
                                denom = 1;
                            int score = (accessCount << 16) / denom;

                            m_scores.push_back(ScoredFile(entry->d_name, score));
                        }
                    }
                }
            }

            closedir(dir);
            sort(m_scores.begin(), m_scores.end());
        }

        m_lastPruneExpiredAndScore = time(0);

        if (PalmBrowserSettings()->diskCacheDebug)
            printf("DISK-CACHE pruneToFit(1)  %ld -> %ld, took %ld seconds\n", before, m_currentSizeBytes, (long int) difftime(m_lastPruneExpiredAndScore, now));
    }
    else if (PalmBrowserSettings()->diskCacheDebug) {
        printf(" skipping prune-expire(1)\n");
    }
}

bool DiskCacheDisk::pruneLowestScore()
{
    if (m_scores.empty())
        return false;

    // Remove lowest scored item from the disk cache and from m_score
    struct stat fileStats;
    std::string fullPath = m_diskCachePath.utf8().data() +  m_scores.back().m_name;
    if (stat(fullPath.c_str(), &fileStats) == 0) {
        m_currentSizeBytes -= fileStats.st_blocks * S_BLKSIZE;
        unlink(fullPath.c_str());
        if (PalmBrowserSettings()->diskCacheDebug)
            printf(" unlink %s\n", fullPath.c_str());
    }
    m_scores.pop_back();

    return true;
}

DiskCacheEntry* DiskCacheDisk::create()
{
	return new DiskCacheEntry( this );
}

void DiskCacheDisk::clear()
{
	std::string path = m_diskCachePath.utf8().data();
	DIR* dir = opendir(path.c_str());
	if( dir )
	{
		struct dirent* entry = readdir(dir);
		while( entry )
		{
			if( entry->d_name[0] != '.' ) 
			{
				std::string fullpath = path + std::string(entry->d_name);
				unlink( fullpath.c_str() );
			}
			entry = readdir(dir);
		}
		closedir(dir);
	}
	m_currentSizeBytes = 0;

    m_lastPruneExpiredAndScore = 0;
    m_scores.clear();
}

bool readIntFromFile( FILE* f, long& result )
{
	result = 0;
	long ch;

	if (EOF == (ch=fgetc(f)))
		return false;
	result = ch << 24;
	
	if (EOF == (ch=fgetc(f)))
		return false;
	result |= ch << 16;
	
	if (EOF == (ch=fgetc(f)))
		return false;
	result |= ch << 8;

	if (EOF == (ch=fgetc(f)))
		return false;
	result |= ch;

	return true;
}

bool writeIntToFile( FILE* f, const long value )
{
	if (EOF == fputc( (value >> 24) & 0xff, f ))
		return false;
	if (EOF == fputc( (value >> 16) & 0xff, f ))
		return false;
	if (EOF == fputc( (value >> 8) & 0xff, f ))
		return false;
	if (EOF == fputc( value & 0xff, f ))
		return false;

	return true;
}

void writeLineToFile( FILE* f, const String& value )
{
	const UChar* s = value.characters();
	for( unsigned i=0; i<value.length(); i++, s++ ) {
		fputc( *s, f );
	}
	fputc( 0, f );
}

// Read a null-delimited string from a file.
int readLineFromFile( FILE* f, String& result )
{
	int ch;
	ch = fgetc(f);
	result = "";
	while( ch && ch != -1 )
	{
		result.append( (char)ch );
		ch = fgetc(f);
	}
	return result.length();
}

DiskCacheEntry* readFromFile( const char* fn )
{
	long size;
	
	// read the payload
	Stream f(fn,"rb");
	if( !f.opened() ) {
		return 0;
	}
	
	readIntFromFile( f, size );
	
	// read the payload.
	unsigned char* data = new unsigned char[ size+1 ];
	if( !data ) {
		return 0;
	}
	DiskCacheEntry* cacheItem = new DiskCacheEntry( diskCache() );
	size = fread( data, 1, size, f );
	cacheItem->addData( data, size );
	delete[] data;

	// read the meta-data.
	long v;
	readIntFromFile( f, cacheItem->m_lastAccess );
	readIntFromFile( f, cacheItem->m_expiresAt );
	readIntFromFile( f, cacheItem->m_accessCount );
	readIntFromFile( f, cacheItem->m_statusCode );
	readIntFromFile( f, v );
	cacheItem->m_expectedContentLength = v;
	readLineFromFile( f, cacheItem->m_mimeType );
	readLineFromFile( f, cacheItem->m_fileName );
	readLineFromFile( f, cacheItem->m_encodingName );
	readLineFromFile( f, cacheItem->m_setCookie );
	long headerCount=0; 
	readIntFromFile( f, headerCount );
	for( long i=0; i<headerCount; i++ ) {
		String header;
		readLineFromFile( f, header );
		cacheItem->m_headers.append( header );
	}
	
	return cacheItem;
}

long writeToFile( const std::string& fn, DiskCacheEntry* cacheItem )
{
	// The file looks like this:
	// [ size (32bits) ][ payload ][ meta-data ]
	
	if( PalmBrowserSettings()->diskCacheDebug )
		printf( " DISK-CACHE: adding url %s\n", cacheItem->m_url.utf8().data() );
	
	const unsigned long size = (unsigned long) cacheItem->m_payload->size();

	Stream f(fn, "wb");
	if( !f.opened() )
		return 0;

	f.setDeleteOnClose(true);	// In case of error so we don't leave partial file in cache.
	
	// write the payload size
	if (!writeIntToFile( f, size ))
		return 0;
	
	// write the payload
	size_t r = fwrite( cacheItem->m_payload->data(), sizeof(char), size, f );	
	ASSERT( r == size );
	if (r != size) {
		fprintf(stderr, "Tried to write %ld, but only wrote %u\n", size, r);
		return 0;
	}

	// write the meta-data.
	if (!writeIntToFile( f, cacheItem->m_lastAccess ))
		return 0;
	if (!writeIntToFile( f, cacheItem->m_expiresAt ))
		return 0;
	if (!writeIntToFile( f, cacheItem->m_accessCount ))
		return 0;
	if (!writeIntToFile( f, cacheItem->m_statusCode ))
		return 0;
	if (!writeIntToFile( f, (long)cacheItem->m_expectedContentLength ))
		return 0;
	writeLineToFile( f, cacheItem->m_mimeType );
	writeLineToFile( f, cacheItem->m_fileName );
	writeLineToFile( f, cacheItem->m_encodingName );
	writeLineToFile( f, cacheItem->m_setCookie );
	if (!writeIntToFile( f, cacheItem->m_headers.size() ))
		return 0;
	for( unsigned i=0; i<cacheItem->m_headers.size(); i++ ) {
		writeLineToFile( f, cacheItem->m_headers[i].utf8().data() );
		if( PalmBrowserSettings()->diskCacheDebug )
			printf(" DISK-CACHE: adding header -> %s", cacheItem->m_headers[i].utf8().data() );  
	}

	f.setDeleteOnClose(false);

	return f.getPos();
}

String formUrlFileName( const String& url )
{
    const char* kCharMap = "0123456789abcdef";

    String result;
    unsigned char digest[SHA_DIGEST_LENGTH];

    CString content = url.utf8();

    if (SHA1((const unsigned char *)content.data(), content.length(), digest)) {

        for (int ix = 0; ix < SHA_DIGEST_LENGTH; ++ix) {

            result.append(kCharMap[(digest[ix] >> 4) & 0x0f]);
            result.append(kCharMap[digest[ix] & 0x0f]);
        }
    }

    return result;
}


}
