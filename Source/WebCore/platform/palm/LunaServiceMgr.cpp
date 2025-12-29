
#include "config.h"
#include "LunaServiceMgr.h"
#include "palmwebglobal.h"
#include "PlatformString.h"
#include "CString.h"

#include <glib.h>
#include <stdlib.h>
#include <stdio.h>

#include <lunaservice.h>


namespace WebCore
{

/** 
* @brief Internal callback for service responses.
* 
* @param  sh 
* @param  reply 
* @param  ctx 
* 
* @retval
*/
static bool 
message_filter(LSHandle *sh, LSMessage* reply, void* ctx)
{
    const char* payload = LSMessageGetPayload(reply);

	LunaServiceManagerListener *listener = (LunaServiceManagerListener*)ctx;

    if (listener)
    {
		//g_debug(" (message_filter) calling back with token = %ld", listener->_token );
        ASSERT(listener->_token == LSMessageGetResponseToken(reply));
        listener->serviceResponse(payload);
        return true;
    }

	return false;
}

LunaServiceManager*		s_instance=0;

/** 
* @brief Obtains the singleton LunaServiceManager.
* 
* @retval the LunaServiceManager
*/
LunaServiceManager*		LunaServiceManager::instance()
{
    bool retVal;
	if( s_instance ) 
		return s_instance;
	
	s_instance = new LunaServiceManager();
	retVal = s_instance->init();
    if (!retVal) goto error;

	return s_instance;

error:
	fprintf(stderr, "*******************************************************************\n");
	fprintf(stderr, "*  Could got get an instance of LunaServiceManager.               *\n");
	fprintf(stderr, "*  Try running with luna-dbus start; luna-dbus run <executable>.  *\n");
	fprintf(stderr, "*******************************************************************\n");
	exit(-1);
}
	
/** 
* @brief Private constructor to enforce singleton.
*/
LunaServiceManager::LunaServiceManager() :
	  public_bus(NULL)
	, private_bus(NULL)
	, palmServiceHandle(NULL)
	, publicBusHighPriority(NULL)
	, privateBusHighPriority(NULL)
	, palmServiceHandleHighPriority(NULL)
{
}

LunaServiceManager::~LunaServiceManager()
{
	// ED : Close the single connection to DBUS.
	if (palmServiceHandle)
	{
	    bool retVal;
        LSError lserror;
        LSErrorInit(&lserror);

		retVal = LSUnregisterPalmService(palmServiceHandle, &lserror);
        if (!retVal)
        {
            g_warning("LSUnregisterPalmService ERROR %d: %s (%s @ %s:%d)",
				lserror.error_code, lserror.message,
				lserror.func, lserror.file, lserror.line);
            LSErrorFree(&lserror);
        }
	}
}

bool LunaServiceManager::init()
{
	bool init;
	LSError lserror;
	LSErrorInit(&lserror);

	String id("com.palm.luna-");
	id.append(String::number(getpid()));
	init = LSRegisterPalmService(id.utf8().data(), &palmServiceHandle, &lserror);
	if (!init) goto error;

	init = LSGmainAttachPalmService(palmServiceHandle,
			Palm::WebGlobal::mainLoop(), &lserror); 
	if (!init) goto error;

	private_bus = LSPalmServiceGetPrivateConnection(palmServiceHandle);
	public_bus = LSPalmServiceGetPublicConnection(palmServiceHandle);

	if (private_bus) {
		init = LSGmainSetPriority(private_bus, G_PRIORITY_DEFAULT, &lserror);
		if (!init)
			goto error;
	}

	if (public_bus) {
		init = LSGmainSetPriority(public_bus, G_PRIORITY_DEFAULT, &lserror);
		if (!init)
			goto error;
	}


	init = LSRegisterPalmService((id + "-phone").utf8().data(), &palmServiceHandleHighPriority, &lserror);
	if (!init) goto error;

	init = LSGmainAttachPalmService(palmServiceHandleHighPriority,
			Palm::WebGlobal::mainLoop(), &lserror); 
	if (!init) goto error;

	privateBusHighPriority = LSPalmServiceGetPrivateConnection(palmServiceHandleHighPriority);
	publicBusHighPriority = LSPalmServiceGetPublicConnection(palmServiceHandleHighPriority);

	if (privateBusHighPriority) {
		init = LSGmainSetPriority(privateBusHighPriority, G_PRIORITY_HIGH, &lserror);
		if (!init)
			goto error;
	}

	if (publicBusHighPriority) {
		init = LSGmainSetPriority(publicBusHighPriority, G_PRIORITY_HIGH, &lserror);
		if (!init)
			goto error;
	}


	init = LSRegisterPalmService((id + "-active").utf8().data(), &palmServiceHandleMediumPriority, &lserror);
    if (!init) goto error;

    init = LSGmainAttachPalmService(palmServiceHandleMediumPriority,
            Palm::WebGlobal::mainLoop(), &lserror);
    if (!init) goto error;

    privateBusMediumPriority = LSPalmServiceGetPrivateConnection(palmServiceHandleMediumPriority);
    publicBusMediumPriority = LSPalmServiceGetPublicConnection(palmServiceHandleMediumPriority);

    if (privateBusMediumPriority) {
        init = LSGmainSetPriority(privateBusMediumPriority, G_PRIORITY_HIGH + 50, &lserror);
        if (!init)
            goto error;
    }

    if (publicBusMediumPriority) {
        init = LSGmainSetPriority(publicBusMediumPriority, G_PRIORITY_HIGH + 50, &lserror);
        if (!init)
            goto error;
    }

error:
	if (!init)
	{
		g_warning("Cannot initialize LunaServiceManager ERROR %d: %s (%s @ %s:%d)",
			lserror.error_code, lserror.message,
			lserror.func, lserror.file, lserror.line);
		LSErrorFree(&lserror);
	}

	return init;
}

/** 
* @brief This method will make the async call to DBUS.
* 
* @param  uri 
* @param  payload 
* @param  inListener 
* 
* @retval 0 if message could not be sent.
* @retval >0 serial number for the message.
*/
unsigned long LunaServiceManager::call( const char* uri, const char* payload, LunaServiceManagerListener* inListener,
									    const char* callerId,bool usePrivateBus )
{
	bool retVal;
	LSError lserror;
	LSErrorInit(&lserror);
	LSMessageToken token = 0;
	LSHandle *serviceHandle = NULL;
	
	if (callerId && (0 == *callerId) )
	{
		callerId = NULL;
	}

	static int phoneAppIdLen = strlen("com.palm.app.phone");
	const char* activeAppId = Palm::WebGlobal::getActiveApplicationId().c_str();
	if (callerId && strncmp(callerId, "com.palm.app.phone", phoneAppIdLen) == 0) {

		if (!usePrivateBus)
			serviceHandle = publicBusHighPriority;
		else
			serviceHandle = privateBusHighPriority;

	}
	else if (callerId && activeAppId && strncmp(callerId, activeAppId, strlen(activeAppId)) == 0) {


	    if (!usePrivateBus)
            serviceHandle = publicBusMediumPriority;
        else
            serviceHandle = privateBusMediumPriority;

	}
	else {

		if (!usePrivateBus) {
			//g_warning("LunaServiceManager: Caller [%s] will use the PUBLIC bus",callerId);
			serviceHandle = public_bus;
		}
		else { 
			//g_warning("LunaServiceManager: Caller [%s] will use the PRIVATE bus",callerId);
			serviceHandle = private_bus;
		}
	}
	
	if (NULL == inListener)
	{
		retVal = LSCallFromApplication(serviceHandle, uri, payload, callerId, NULL, NULL, &token, &lserror);
	}
	else
	{
		retVal = LSCallFromApplication(serviceHandle, uri, payload, callerId, message_filter, inListener, &token, &lserror);
        if (retVal) {
            ASSERT(token != 0);
            inListener->_token = token;
            inListener->sh = serviceHandle;
        }
	}

	if (!retVal)
	{
		g_warning("LSCallFromApplication ERROR %d: %s (%s @ %s:%d)",
			lserror.error_code, lserror.message,
			lserror.func, lserror.file, lserror.line);
		LSErrorFree (&lserror);
		token = 0;
		goto error;
	}

	//g_debug("%s called with token %ld: '%s':'%s', listener:%c, private:%c",
    //        callerId, token, uri, payload, inListener ? 'Y' : 'N', usePrivateBus ? 'Y' : 'N');

error:
	return token;
}
	
/** 
 * @brief Terminates a call causing any subscription for responses to end.
 *        This is also called by garbage collector's collect()
 *        when no more references to inListener exist.
 *
 * @param  inListener 
 */
void LunaServiceManager::cancel( LunaServiceManagerListener* inListener )
{
    bool retVal;
    LSError lserror;

	if (!inListener || inListener->_token == 0)
		return;
	
    LSErrorInit(&lserror);
	
	//g_debug("cancelling token %ld", inListener->_token );
    if (!LSCallCancel(inListener->sh, inListener->_token, &lserror)) {
		g_warning("LSCallCancel ERROR %d: %s (%s @ %s:%d)",
			lserror.error_code, lserror.message,
			lserror.func, lserror.file, lserror.line);
        LSErrorFree (&lserror);
    }

	// set the token to zero to indicate we have been canceled
	inListener->_token = 0;
}

};

