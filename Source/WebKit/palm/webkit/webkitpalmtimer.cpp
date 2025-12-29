/* ============================================================
 * Date  : 2008-02-22
 * Copyright 2008 Palm, Inc. All rights reserved.
 * ============================================================ */

#include "config.h"
#include "webkitpalmtimer.h"
#include "palmwebglobal.h"
#include "Timer.h"

// Post common use pattern is single instance so let's save on creation
static WebKitPalmTimer* sCachedTimer = 0;

 struct WebKitPalmTimer
 {
	WebKitPalmTimer() 
		:  m_webkitTimer(this, &WebKitPalmTimer::callbackTimerFired )
		, m_userContext(0)
	{ 
	}
	~WebKitPalmTimer() 
	{
	}
 	
	WebCore::Timer<WebKitPalmTimer> m_webkitTimer;
	void callbackTimerFired(WebCore::Timer<WebKitPalmTimer>*)
	{
		(m_userCallback)( m_userContext );
	}
	
	void* m_userContext;
	WebKitPalmTimerCallback m_userCallback;
 };


extern "C" WebKitPalmTimer* webkit_timer_new(WebKitPalmTimerCallback cb, void* userArg)
{
	WebKitPalmTimer* timer = 0;
 	
	if( sCachedTimer ) {
		timer = sCachedTimer;
		sCachedTimer = 0;
	} 
	else 
	{
		timer = new WebKitPalmTimer;
	}
 	
	timer->m_userContext = userArg;
	timer->m_userCallback = cb;
	
 	return timer;
}

extern "C" void
webkit_timer_fire(WebKitPalmTimer* timer, uint64_t timeInMs)
{
	timer->m_webkitTimer.startOneShot( timeInMs / 1000.0 );
}

extern "C" void
webkit_timer_delete(WebKitPalmTimer* timer)
{
	if( !sCachedTimer ) {
		timer->m_webkitTimer.stop();
		sCachedTimer = timer;
	} 
	else 
		delete timer;
}

extern "C" void
webkit_timer_freeze()
{
}

extern "C" void
webkit_timer_thaw()
{
}

