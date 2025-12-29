
#ifndef __LunaResources_h__
#define __LunaResources_h__

#include <map>
#include <string>
#include "PlatformString.h"

namespace WebCore
{

class LunaResources
{
	public:
		static LunaResources* instance();
		
		void		setMaxCacheSize( unsigned long inBytes );
		bool		getResource( const String& filename, String& outFile );
		void 		clearResourcesCache();
		
	private:
	
		unsigned long m_cacheSize;
		unsigned long m_cacheSizeLimit;
		std::map<std::string,String> m_cache;
		
		LunaResources();
		~LunaResources();
};

}

#endif

