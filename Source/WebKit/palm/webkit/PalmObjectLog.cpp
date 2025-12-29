
// Copyright 2008-2010 Palm Inc.

#include "config.h"

#include "PalmObjectLog.h"

#include "webkitpalmsettings.h"

#include "StringImpl.h"
#include "Node.h"
#include "Document.h"
#include "NodeList.h"
#include "EventTarget.h"
#include "Cache.h"
#include "webkitpalmsettings.h"
#include "Text.h"
#include "Event.h"
#include "Database.h"
#include "V8Proxy.h"
#include "CString.h"
#include "PalmServiceBridge.h"
#include "PGSurface.h"
#include "CachedImage.h"

#include <list>
#include <string>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <map>
#include <set>

#include <stdarg.h>
#include <glib.h>

	

// Add class names here that you want to call out and collect in their
// own container. This way you can dump status from them, etc. Note you
// must also use the WEBOS_OBJECT_NEW/DELETE here.
#define TYPES_TO_COLLECT(V)  \
	V(Event) \
	V(Document) \
	V(Database) \
	V(CachedImage) \
	V(PGSurface) \
	V(PalmServiceBridge) \
	V(Node) 


	
using namespace WebCore;

namespace Palm {

struct Counter {
	std::string name;
	int count;
};
static std::list<Counter>	s_counters;	
	
	static FILE* s_Log = 0;
	static FILE* s_xl =0 ;
	static bool s_headerDirty = false;
	const int kCsvHeaderSize = 4096;
	
	static bool openLogFile();
	static void dumpStats();
	
static Mutex& countersMutex()
{
    DEFINE_STATIC_LOCAL(Mutex, mutex, ());
    return mutex;
}

bool openLogFile( ) 
{
	if( !PalmBrowserSettings()->logObjects )
		return false;
	
	if( s_Log )
		return true;
	
	s_Log =  fopen( std::string( std::string(PalmBrowserSettings()->logObjectsFile)+".txt").c_str(), "a" );
	if( s_Log ) 
		fprintf(s_Log,"--- sysmgr restart ---\n");
	
	s_xl = fopen( std::string( std::string(PalmBrowserSettings()->logObjectsFile)+".csv").c_str(), "w" );
	
	if( s_xl )
	{
		// write the dummy header.
		for( int i=0; i<kCsvHeaderSize; i++ ) {
			fprintf(s_xl, " ");
		}
		fprintf(s_xl,"\n");
	}
	
	return ( s_Log != NULL );
}

struct objectinfo {
	objectinfo() : obj(0),timeCreated(time(0)) { } 
	void* obj;
	int timeCreated;	 // seconds time(0) when object was created.
	int age() const { return time(0) - timeCreated; }
};

// the static storage for specific types we want to collect.
#define MAKE_LIST(name)\
	static std::list<objectinfo> s##name;
    TYPES_TO_COLLECT(MAKE_LIST)
#undef MAKE_LIST

#define LIST_ITERATE(name) \
	for( std::list<objectinfo>::iterator it=s##name.begin(); it != s##name.end(); ++it )

void debugGetObjectCountersJSONString( std::string& outString )
{
	const int kMaxBufferPrintSize = 64;
	char buffer[ kMaxBufferPrintSize+1 ];
	outString = "{ ";
	for( std::list<Counter>::const_iterator it = s_counters.begin(); it != s_counters.end(); ++it )
	{
		snprintf(buffer,kMaxBufferPrintSize,"\"%s\":%d", it->name.c_str(), it->count );
		
		if( it != s_counters.begin() )
			outString += ",";
		
		outString += buffer;
	}
	outString += "}";
}

//
// Caller is responsible for acquiring counters mutex.
//
void dumpStats() 
{
	if( !WTF::isMainThread() )
		return;
	
	static time_t s_lastLog = 0;
	
	// Only dump every 'n' seconds in case we're collecting something of high-frequency (e.g. Node)
	if( (time(0)-s_lastLog) < 1 )
		return;
	
	s_lastLog = time(0);
	
	fprintf(s_Log,"\n");
	int idx =1 ;
	for( std::list<Counter>::const_iterator it = s_counters.begin(); it != s_counters.end(); ++it )
	{
		fprintf(s_Log,"[%d]%s=%d ", idx++,it->name.c_str(), it->count );
	}
	fprintf(s_Log,"\n");
	
	
	const int kMaxTracked = 16;
	if( s_xl )
	{
		if( s_headerDirty )
		{
			long pos = ftell(s_xl);
			if (pos >= 0) {
			
				fseek( s_xl, 0, SEEK_SET );
				for( std::list<Counter>::const_iterator it = s_counters.begin(); it != s_counters.end(); ++it )
				{
					fprintf(s_xl,"%s ",it->name.c_str() );
				}

				fseek( s_xl, pos, SEEK_SET );
			}
		}
		
		
		int numZeros=kMaxTracked-s_counters.size();
		for( std::list<Counter>::const_iterator it = s_counters.begin(); it != s_counters.end(); ++it ) 
		{
			if( it != s_counters.begin() )
				fprintf(s_xl," ");
			fprintf(s_xl,"%d",it->count );
		}
		for( int i=0; i<numZeros; i++ ) {
			if( !i && !s_counters.size( ) )
				fprintf(s_xl,"0");
			else
				fprintf(s_xl," 0");
		}
		fprintf(s_xl,"\n");
		fflush(s_xl);
	}
	
    const char* const bindingsState[] = { "no", "yes", "?" };

    /*
    // PalmServiceBridges
	// log services to alternate files
	static int _fileCounter=0;
	_fileCounter = (_fileCounter + 1) % 4;
	char filename[64];
	sprintf(filename,"/media/internal/services_%d.txt",_fileCounter ); 
	if( FILE* f = fopen( filename, "w" ) )
	{
		LIST_ITERATE(PalmServiceBridge)
		{
			PalmServiceBridge* b = (PalmServiceBridge*)it->obj;
		
			fprintf(f,"%s %s [%d sec]\n",
				b->uri().utf8().data(), 
				b->document()->url().string().utf8().data(),
				it->age() );
		}		
		fprintf(f,"--complete--\n");
		fclose(f);
	}
	*/
	
	// Documents
	LIST_ITERATE(Document)
	{
		Document* doc = (Document*)it->obj;
        int bindingsIdx = WTF::isMainThread() ? (V8Proxy::hasBindingForDOMNode(doc)?1:0) : 2;
		fprintf(s_Log, "  Document %p ref=%d selfOnlyRefCount=%d jsbinding=%s %s\n", doc, doc->refCount(),
			doc->selfOnlyRefCount(),
			bindingsState[bindingsIdx],
			doc->url().string().utf8().data() );
	}
	
	// PGSurfaces
	int rasterTotalBytes = 0;
	LIST_ITERATE(PGSurface)
	{
		PGSurface* s = (PGSurface*)it->obj;
		rasterTotalBytes += ( s->height() * s->rowbytes() );
	}
	fprintf( s_Log, "%d k raster data\n", rasterTotalBytes / 1024 );
	
	// Databases
	LIST_ITERATE(Database)
	{
		Database* db = (Database*)it->obj;
		Document* doc = 0;//db->document();
        int bindingsIdx = WTF::isMainThread() ? (V8Proxy::hasBindingForDOMObject(db)?1:0) : 2;
		fprintf(s_Log, "  . Database %p ref=%d binding=%s  for %s\n", 
			db, 
			db->refCount(), 
			bindingsState[bindingsIdx],
			doc->url().string().utf8().data() );
	}
	
	// Events
	{
		char buffer[64];
		static int fe_count = 0;
		fe_count  = ( fe_count +1 ) % 2;
		sprintf(buffer,"/media/internal/e%d.log", fe_count );
		FILE* fe = fopen(buffer,"w");
		if( fe )
		{
			LIST_ITERATE(Event)
			{
				Event* e = (Event*)it->obj;
				fprintf(fe," Event %p phase=%d %s \n", e, e->eventPhase(),  e->type().string().utf8().data() );
			}
			fclose(fe);
		}
	}
	
	
	
	// Cache stats
	//Cache::Statistics stats = cache()->getStatistics();
	//fprintf(s_Log,"Cache:\n");
	//fprintf(s_Log,"images: %dk, %dk (%dk)\n",stats.images.liveSize/1024,stats.images.decodedSize/1024,stats.images.size/1024);
	//fprintf(s_Log,"css: %dk, %dk (%dk)\n",stats.cssStyleSheets.liveSize/1024,stats.cssStyleSheets.decodedSize/1024,stats.cssStyleSheets.size/1024);
	//fprintf(s_Log,"scripts: %dk, %dk (%dk)\n",stats.scripts.liveSize/1024,stats.scripts.decodedSize/1024,stats.scripts.size/1024);
	
	fprintf(s_Log,"\n\n");
	fflush( s_Log );
}

	
void ObjectLog::LogNew( void* obj, const char* name )
{
	if( !openLogFile() )
		return;
	
#define MAKE_LIST(_name) \
	if( !strcmp( name ,#_name ) ) { objectinfo inf; inf.obj=obj; s##_name.push_back( inf ); }
    TYPES_TO_COLLECT(MAKE_LIST)
#undef MAKE_LIST

    MutexLocker locker(countersMutex());
	for( std::list<Counter>::iterator it = s_counters.begin(); it != s_counters.end(); ++it )
	{
		if( !strcmp( it->name.c_str(), name ) ) 
		{
			it->count++;
			dumpStats();
			return;
		}
	}

	Counter c;
	c.name = name;
	c.count = 1;
	s_counters.push_back( c );
	s_headerDirty = true;
}	
	
	
void ObjectLog::LogDelete( void* obj, const char* name )
{
	if( !openLogFile() )
		return;
	
#define MAKE_LIST(_name) \
	if( !strcmp( name ,#_name) ) { std::list<objectinfo>::iterator it=s##_name.begin(); for( ; it!=s##_name.end(); ++it) { if(it->obj==obj) { s##_name.erase(it); break; } } }
    TYPES_TO_COLLECT(MAKE_LIST)
#undef MAKE_LIST

    MutexLocker locker(countersMutex());
for( std::list<Counter>::iterator it = s_counters.begin(); it != s_counters.end(); ++it )
	{
		if( !strcmp( it->name.c_str(), name ) )
		{
			it->count--;
			dumpStats();
			return;
		}
	}	
	
	
}

#pragma GCC diagnostic ignored "-Wmissing-format-attribute"
void ObjectLog::Log( const char* ch, ... )
{
	if(!openLogFile() )
		return;
	
	va_list ap;
	va_start(ap, ch);
		vfprintf(s_Log,ch,ap);
	va_end(ap);
}


} // Palm

