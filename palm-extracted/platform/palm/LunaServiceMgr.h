
#ifndef __LunaServiceMgr_h__
#define __LunaServiceMgr_h__


#include <lunaservice.h>

namespace WebCore
{
	
struct LunaServiceManagerListener
{
		LunaServiceManagerListener() : _token(LSMESSAGE_TOKEN_INVALID), sh(NULL) { }
		virtual ~LunaServiceManagerListener() { }
		virtual void serviceResponse( const char* body ) = 0;
        LSMessageToken _token;
        LSHandle      *sh;
};


//
//	LunaServiceManager
//
// This class is a singleton which handles all the client requests
// for a WebKit instance.

class LunaServiceManager
{
	public:
		~LunaServiceManager();
		
		static 	LunaServiceManager*  instance();
		unsigned long		call( const char* uri, const char* payload, LunaServiceManagerListener*, const char* callerId , bool usePrivateBus = false);
		void				cancel( LunaServiceManagerListener* );

	private:
		bool				init();
							LunaServiceManager();

        LSHandle           *public_bus;
        LSHandle           *private_bus;
        LSPalmService      *palmServiceHandle;

        // The Medium Priority bus is used for the active app
        LSHandle           *publicBusMediumPriority;
        LSHandle           *privateBusMediumPriority;
        LSPalmService      *palmServiceHandleMediumPriority;

        // The High Priority bus is used only for the Phone app
		LSHandle           *publicBusHighPriority;
		LSHandle           *privateBusHighPriority;
		LSPalmService      *palmServiceHandleHighPriority;
};

}

#endif

