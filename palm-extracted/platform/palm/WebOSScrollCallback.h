
#ifndef __WebOSScrollCallback_h__
#define __WebOSScrollCallback_h__

#include <wtf/Threading.h>

namespace WebCore {

class WebOSScrollCallback : public ThreadSafeShared<WebOSScrollCallback>
{
public:
	virtual ~WebOSScrollCallback() { }
	virtual void handleEvent( Element* el, int x, int y, int status, bool& raisedException ) = 0;
};
	
};

#endif 
