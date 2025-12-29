/* ============================================================
 * Date  : 2008-02-21
 * Copyright 2008 Palm, Inc. All rights reserved.
 * ============================================================ */

#include "config.h"
 
#include "SingletonTimerPalm.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <sys/time.h>

#include "CurrentTime.h"

//#define USE_SYS_MALLOC 1

#ifdef USE_SYS_MALLOC
#define New(x)       (x*) malloc(sizeof(x))
#define New0(x)      (x*) calloc(1, sizeof(x))
#define Free(type,x) free(x)
#else
#define New(x)       g_slice_new(x)
#define New0(x)      g_slice_new0(x)
#define Free(type,x) g_slice_free(type,x)
#endif

struct SingletonTimer {
    int64_t                fireTime;
    SingletonTimerCallback callback;
    void*                  userArg;
};

static gboolean PrvTimerPrepare(GSource* source, gint* timeout);
static gboolean PrvTimerCheck(GSource* source);
static gboolean PrvTimerDispatch(GSource* source, GSourceFunc callback, gpointer userData);

static GMainLoop*      sLoop          = 0;
static GSource*        sSource        = 0;
static GList*          sActiveList    = 0;
static GSourceFuncs    sTimerFuncs    =
{
    PrvTimerPrepare,
    PrvTimerCheck,
    PrvTimerDispatch,
    NULL,
    NULL,
    NULL
};

extern "C" void
SingletonTimerInit(GMainLoop* loop)
{
    if (sLoop) {
        // Already initialized
        return;
    }
    
    sLoop   = loop;
    sSource = g_source_new(&sTimerFuncs, sizeof(GSource));
    g_source_set_priority(sSource, G_PRIORITY_DEFAULT);
    g_source_set_can_recurse(sSource, TRUE);
    g_source_attach(sSource, g_main_loop_get_context(loop));

    sActiveList = 0;
}

extern "C" SingletonTimer*
SingletonTimerCreate(void (*callback)(void*), void* userArg)
{
    SingletonTimer* timer = New0(SingletonTimer);
    timer->callback = callback;
    timer->userArg  = userArg;

    return timer;
}

extern "C" void
SingletonTimerFire(SingletonTimer* timer, uint64_t timeInMs)
{
    if (!timer)
        return;
    timer->fireTime = timeInMs > 0 ? timeInMs : 0;

	sActiveList = g_list_remove(sActiveList, timer);

	// re-insert sorted based on
	// 1. ascending based on firetime
	// 2. firetime being equal, order of this call.
	if (!sActiveList) {
		sActiveList = g_list_append(sActiveList, timer);
	}
	else {

		bool inserted = false;
		for (GList* iter = g_list_first(sActiveList); iter;
			 iter = g_list_next(iter)) {

			SingletonTimer* t = (SingletonTimer*) iter->data;
			if (t->fireTime > timer->fireTime) {
				sActiveList = g_list_insert_before(sActiveList, iter, timer);
				inserted = true;
				break;
			}
		}

		if (!inserted)
			sActiveList = g_list_append(sActiveList, timer);
	}

    // wake up main loop if it is suspended in a poll
    g_main_context_wakeup(g_main_loop_get_context(sLoop));
}

extern "C" void
SingletonTimerDelete(SingletonTimer* timer)
{
    if (!timer)
        return;

	sActiveList = g_list_remove(sActiveList, timer);
    Free(SingletonTimer, timer);
}

extern "C" void
SingletonTimerFreeze()
{
	assert(sLoop);

	// already frozen?
	if (!sSource)
		return;
		
	g_source_destroy(sSource);
	g_source_unref(sSource);
	sSource = 0;
}

extern "C" void
SingletonTimerThaw()
{
	assert(sLoop);

	// already thawed?
	if (sSource)
		return;
	
	sSource = g_source_new(&sTimerFuncs, sizeof(GSource));
    g_source_set_priority(sSource, G_PRIORITY_DEFAULT);
    g_source_set_can_recurse(sSource, TRUE);
    g_source_attach(sSource, g_main_loop_get_context(sLoop));

    // wake up main loop if it is suspended in a poll
    g_main_context_wakeup(g_main_loop_get_context(sLoop));	
}

static SingletonTimer*
PrvFindMinTimer()
{
	if (!sActiveList || g_list_length(sActiveList) == 0)
        return 0;

	GList* iter = g_list_first(sActiveList);
	return (SingletonTimer*) iter->data;
}	

static gboolean
PrvTimerPrepare(GSource* source, gint* timeout)
{
	SingletonTimer* minTimer = PrvFindMinTimer();
	if (!minTimer) {
		*timeout = -1;
		return FALSE;
	}
	
    struct timespec currTime;
    clock_gettime(CLOCK_MONOTONIC, &currTime);

    int64_t diff = minTimer->fireTime -
                   (int64_t) currTime.tv_sec * 1000 -
                   (int64_t) currTime.tv_nsec / 1000000;
    if (diff <= 0) {
        return TRUE;
    }

    *timeout  = (gint) (diff);
    return FALSE;    
}

static gboolean
PrvTimerCheck(GSource* source)
{
	SingletonTimer* minTimer = PrvFindMinTimer();
	if (!minTimer)
		return FALSE;
	
	

    struct timespec currTime;
    clock_gettime(CLOCK_MONOTONIC, &currTime);

    int64_t diff = minTimer->fireTime -
                   (int64_t) currTime.tv_sec * 1000 -
                   (int64_t) currTime.tv_nsec / 1000000;
				   
				   
    return (diff <= 0);    
}

static gboolean
PrvTimerDispatch(GSource* source, GSourceFunc callback, gpointer userData)
{
	SingletonTimer* minTimer = PrvFindMinTimer();
	if (!minTimer)
		return TRUE;

	// Currently in active list. Remove from there
	sActiveList = g_list_remove(sActiveList, minTimer);
	
    if (minTimer->callback) {
        minTimer->callback(minTimer->userArg);
    }	

    return TRUE;
}

