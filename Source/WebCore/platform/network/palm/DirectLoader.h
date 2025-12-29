
#ifndef __DirectLoader_h__
#define __DirectLoader_h__

#include "Timer.h"
#include "PlatformString.h"
#include <list>

namespace WebCore {
	
class ResourceHandle;
class DirectLoader;
class Frame;

struct DirectLoaderQueuedItem
{
	String path;
	ResourceHandle* handle;
};

class DirectLoader
{
public:
	static DirectLoader* instance();
	
	bool load( Frame * f, const String& path, ResourceHandle* handle );
	void cancel( ResourceHandle* handle );
	void serviceQueueTimerFired(Timer<DirectLoader>*);
	
	bool processDataUrl( DirectLoaderQueuedItem* item );
	bool processLocalFile( DirectLoaderQueuedItem* item );
	
private:
	Timer<DirectLoader> m_serviceQueueTimer;
	std::list<DirectLoaderQueuedItem*> m_queue;
		
	char* m_buffer;
	long m_bufferSize;
	
	DirectLoader();
	~DirectLoader();
};

}

#endif
