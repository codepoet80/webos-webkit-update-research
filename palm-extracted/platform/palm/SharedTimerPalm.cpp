

#include "config.h"
#include "SharedTimer.h"

#include <wtf/Assertions.h>
#include <wtf/CurrentTime.h>
#include "palmwebglobal.h"
#include <glib.h>

namespace WebCore {
	
extern double currentMonotonicTime();

static guint srcId;
static GSource* sharedTimerSrc = 0;
static void (*sharedTimerFiredFunction)();

void setSharedTimerFiredFunction(void (*f)())
{
	//printf("setSharedTimerFiredFunction -> %p\n", f );
    sharedTimerFiredFunction = f;
}

static gboolean timeout_cb(gpointer p)
{
    // Note: the sharedTimerFiredFunction() may call setSharedTimerFireTime()
    // to set the next timer value.  Hence, it may call s_source_destroy() on
    // sharedTimerSrc, and set it to the next new timer we are expecting to
    // fire.  For these 2 reasons, we should destroy the old timer here and
    // nullify it here, and not after the call to sharedTimerFiredFunction().
    GSource* src = (GSource*)p;
    g_source_destroy(src);
    sharedTimerSrc=0;

    //printf(" timeout_cb srcId=%d callback=%p\n",srcId,sharedTimerFiredFunction);
    if (sharedTimerFiredFunction)
        sharedTimerFiredFunction();
    
    return FALSE;
}

void setSharedTimerFireTime(double fireTime)
{
    ASSERT(sharedTimerFiredFunction);

    double interval = fireTime - currentMonotonicTime();
    guint intervalInMS;
    if (interval < 0)
        intervalInMS = 0;
    else {
        interval *= 1000;
        intervalInMS = (guint)interval;
    }

    if( sharedTimerSrc )
    	g_source_destroy( sharedTimerSrc );

    GMainContext* ctxt =  g_main_loop_get_context( Palm::WebGlobal::mainLoop() );
    
    sharedTimerSrc = g_timeout_source_new( intervalInMS );
    if( !sharedTimerSrc )
    	g_error("error creating sharedTimerSrc");
    //g_source_set_can_recurse( sharedTimerSrc, TRUE );
    g_source_set_callback( sharedTimerSrc, timeout_cb, sharedTimerSrc, 0 );
    srcId = g_source_attach( sharedTimerSrc, ctxt );
    g_source_unref( sharedTimerSrc );
    g_main_context_wakeup( ctxt );
    //printf(".... setSharedTimer sourceId=%d timeout=%d ms\n", srcId, intervalInMS );
}

void stopSharedTimer()
{
    if (!sharedTimerSrc)
        return;

    //printf(" >>>> stopSharedTimer %d\n", srcId );
    g_source_destroy( sharedTimerSrc );
    srcId=0;
    sharedTimerSrc = 0;
}

}
