

#include "config.h"
#include "Threading.h"

#include "HashMap.h"
#include "Logging.h"

#include <glib.h>

#include "palmwebglobal.h"
#include "MainThread.h"

namespace WebCore {

struct FunctionWithContext {
    MainThreadFunction* function;
    void* context;
};

static gboolean callFunctionOnMainThread(gpointer data)
{
    FunctionWithContext* functionWithContext = static_cast<FunctionWithContext*>(data);
    functionWithContext->function(functionWithContext->context);
    delete functionWithContext;
    return FALSE;
}

void callOnMainThread(MainThreadFunction* function, void* context)
{
    ASSERT(function);
    FunctionWithContext* functionWithContext = new FunctionWithContext;
    functionWithContext->function = function;
    functionWithContext->context = context;

    GMainLoop* pMainLoop = Palm::WebGlobal::mainLoop();
    GSource*   pSource   = g_idle_source_new();
    g_source_set_callback(pSource, (GSourceFunc) callFunctionOnMainThread, functionWithContext, NULL);
    g_source_attach(pSource, g_main_loop_get_context(pMainLoop));
    g_source_unref(pSource);
}

// Rest of threading functionality comes from ../pthreads/ThreadingPthread.cpp

}
