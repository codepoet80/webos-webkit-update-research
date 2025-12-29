// Copyright 2008 Palm Inc.

#include "config.h"
#include "PlatformString.h"
#include "CString.h"
#include "webkitpalmsettings.h"

#include "LunaResources.h"

namespace WebCore
{

const int kTemplateResourceMaxSize = 4 * 1024 * 1024 ;	

LunaResources*	LunaResources::instance()
{	
	static  LunaResources* s_instance = 0;
	
	if( !s_instance ) {
		s_instance = new LunaResources();
	}
	return s_instance;
}
	
LunaResources::LunaResources()
	: m_cacheSize(0)
	, m_cacheSizeLimit( kTemplateResourceMaxSize )
{
}	
	
LunaResources::~LunaResources()
{
}

bool LunaResources::getResource( const String& wfilename, String& outResult )
{
	CString filename;
	
	if( wfilename.startsWith("file://") )
		filename = wfilename.substring(	7 ).utf8();
	else
		filename = wfilename.utf8();
	
	if( PalmBrowserSettings()->lunaCacheResources )
	{
		std::map<std::string,String>::const_iterator it = m_cache.find(filename.data());
		if( it != m_cache.end() )
		{
			outResult = it->second;
			return true;
		}
	}
	
	FILE * f = fopen( filename.data(), "r" );
	if( !f ) {
		return false;
	}
	fseek( f, 0L, SEEK_END );
	long sz = ftell( f );
	fseek( f, 0L, SEEK_SET );
	char * s = new char[ sz+1 ];
	if( !s ) {
		fprintf( stderr, " LunaResources::getResource malloc() failed (%d bytes)\n", (int)(sz+1) );
		fclose(f);
		return false;
	}
	size_t readSz = fread( s, 1, sz, f );
	fclose(f);
	
	outResult = String::fromUTF8( s, readSz );
	delete[] s;
	
	if( PalmBrowserSettings()->lunaCacheResources )
	{
		// FIXME : we want to do someting more clever than this.
		if( m_cacheSize >= m_cacheSizeLimit )
			clearResourcesCache();
		
		m_cache[ filename.data() ] = outResult;
		m_cacheSize += readSz+1;
	}
	
	return true;	
}
	
void LunaResources::setMaxCacheSize( unsigned long inBytes )	
{
	m_cacheSizeLimit=inBytes;
}

void LunaResources::clearResourcesCache()
{
	m_cacheSize = 0;
	m_cache.clear();
}
	
}
