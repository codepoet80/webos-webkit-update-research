
//	Copyright 2008,2009,2010 Palm Inc.
//	All Rights Reserved Worldwide

#include "config.h"
#include "PlatformString.h"
#include "Logging.h"
#include "Document.h"
#include "Event.h"
#include "EventException.h"
#include "EventListener.h"
#include "EventNames.h"
#include "CString.h"
#include "ExceptionCode.h"
#include <wtf/RefCountedLeakCounter.h>

#include "Frame.h"
#include "Page.h"

#if USE(V8)
	#include "V8Proxy.h"
	#include "ScriptController.h"
#else
	#include "ScriptController.h"
	#include <runtime/JSLock.h>
	#include "JSDOMWindow.h"	
	using namespace JSC;
#endif

#include "palmwebframe.h"
#include "palmwebframeprivate.h"
#include "palmwebframeclient.h"
#include "webkitpalmsettings.h"
#include <webkitstats.h>
#include <SimpleStats/SimpleStats.h>

#include "PalmServiceBridge.h"

#include <set>
#include <map>

namespace WebCore {

typedef std::set<PalmServiceBridge*> ServicesSet;
typedef std::map<Document*, ServicesSet*> ServicesSetMap;

static bool sDebugServices = ( getenv("DEBUG_SERVICES") ? true : false );

#ifndef NDEBUG
static WTF::RefCountedLeakCounter serviceBridgeCounter("PalmServiceBridge");
#endif

static ServicesSetMap* servicesByDocument()
{
	static  ServicesSetMap map;
	return &map;
}

/* static */ int PalmServiceBridge::numHandlesForUrl( const char* appId )
{
	for( ServicesSetMap::iterator setIt=servicesByDocument()->begin(); setIt !=servicesByDocument()->end(); ++setIt )
	{
		if( !strcmp(appId,setIt->first->url().string().utf8().data()) )
		{
			return setIt->second->size();
		}
	}
	
	return 0;
}

/* static */ void PalmServiceBridge::handlesForUrl( const char* appId, std::list<PalmServiceBridge*>& outHandles )
{
	outHandles.clear();
	for( ServicesSetMap::iterator setIt=servicesByDocument()->begin(); setIt !=servicesByDocument()->end(); ++setIt )
	{
		if( !strcmp(appId,setIt->first->url().string().utf8().data()) )
		{
			ServicesSet* set = setIt->second;
			
			for( ServicesSet::iterator s = set->begin(); s != set->end(); ++s )
			{
				outHandles.push_back( *s );
			}
			
			return;
		}
	}
}

static void addToServicesByDocument(Document* doc, PalmServiceBridge* svc)
{
	if( !doc || !svc ) 
		return;

	ServicesSet* set = 0;
	ServicesSetMap::iterator it = servicesByDocument()->find(doc);
	if( it == servicesByDocument()->end() )
	{
		set = new ServicesSet();
		(*servicesByDocument())[doc] = set;
	}
	else
		set = it->second;
		
	set->insert(svc);
}

static void removeFromServicesByDocument(Document* doc, PalmServiceBridge* svc)
{
	if( !doc || !svc ) 
		return;

	ServicesSetMap::iterator it = servicesByDocument()->find(doc);
	if( it == servicesByDocument()->end() ) 
		return;
	
	ServicesSet* set = it->second;
	if( !set )
		return;
	
	set->erase( svc );
	if( !set->size() )
	{
		// remove from the hash map
		delete set;
		servicesByDocument()->erase(it);
	}
}	



PalmServiceBridge::PalmServiceBridge( ScriptExecutionContext* context, bool subscribe )
	: ActiveDOMObject(context, this)
	, m_canceled(false)
	, m_subscribed(subscribe)
	, m_inServiceCallback(false)
	, m_debug_uri(0)
	, m_debug_params(0)
	, m_debug_returnCount(0)
{
	addToServicesByDocument(document(), this);
	
#ifndef NDEBUG
	serviceBridgeCounter.increment();
#endif

    //WEBOS_OBJECT_NEW(this,PalmServiceBridge);
}

bool PalmServiceBridge::init( Document* d, bool subscribe )
{
	m_subscribed = subscribe;
	return true;
}

PalmServiceBridge::~PalmServiceBridge()
{
	//printf("~PalmServiceBridge %p\n", this );
	ExceptionCode ec;
	cancel( ec );
	
#if USE(V8)		
	if (!m_callbackFunction.IsEmpty()) {
		m_callbackFunction.Dispose();
		m_callbackFunction.Clear();
	}
#endif // USE(V8)

	if (scriptExecutionContext() && document())
		removeFromServicesByDocument(document(), this);

#ifndef NDEBUG
	serviceBridgeCounter.decrement();
#endif
    //WEBOS_OBJECT_DELETE(this,PalmServiceBridge);
    
    delete m_debug_uri;
    delete m_debug_params;
}

void PalmServiceBridge::detachServices(Document* doc)
{
	ServicesSetMap::iterator it = servicesByDocument()->find(doc);
    if ( it == servicesByDocument()->end() )
        return;
	
    ServicesSet* services = it->second;
	servicesByDocument()->erase(it);
	
	if( services )
	{
		while( services->size() )
		{
			ServicesSet::iterator sit= services->begin();
			ExceptionCode ec;
			(*sit)->cancel(ec);
			services->erase( sit );
		}
		
		delete services;
	}

}

void PalmServiceBridge::cancelServices(Document* doc)
{
	ServicesSetMap::iterator it = servicesByDocument()->find(doc);
    if ( it == servicesByDocument()->end() )
        return;
	
    ServicesSet* services = it->second;
	
	if( services )
	{
		for( ServicesSet::iterator sit= services->begin(); sit != services->end(); ++sit )
		{
			PalmServiceBridge* br = *sit;
			ExceptionCode ec;
			br->cancel(ec);
		}
	}
}

String PalmServiceBridge::version()
{
	return String("1.1");
}

#if USE(V8)
void PalmServiceBridge::setOnservicecallback( v8::Handle<v8::Function> func )
{
	if (!m_callbackFunction.IsEmpty()) {
		m_callbackFunction.Dispose();
		m_callbackFunction.Clear();
	}
	m_callbackFunction = v8::Persistent<v8::Function>::New(func);
	
	// Comment out below to use weak references on functions.
	m_callbackFunction.MakeWeak(this, (void (*)(v8::Persistent<v8::Value>, void*)) PalmServiceBridge::WeakRefCallback);
}

// We get this callback from v8 when the only reference to our callback function
// are also weak. We will therefore release our reference to it as well.
void PalmServiceBridge::WeakRefCallback( v8::Persistent<v8::Value> object, void* parameter )
{
	PalmServiceBridge* pThis = (PalmServiceBridge*)parameter;
	
	// In the case when we loose the function refernce during a callback, do nothing.
	if( pThis->m_inServiceCallback )
		return;

	ExceptionCode ec;	
	pThis->cancel(ec);

	if (!pThis->m_callbackFunction.IsEmpty()) {
		pThis->m_callbackFunction.Dispose();
		pThis->m_callbackFunction.Clear();
	}
	
	// Set m_canceled=true here in case this callback fn is being disposed 
	// while we're executing the callback function itself NOV-35217
	pThis->m_canceled = true; 
}
#else // USE(V8)

#endif // USE(V8)
	
int PalmServiceBridge::token()
{
	return (int)_token;
}

int PalmServiceBridge::call( const String& uri, const String& payload, ExceptionCode&  ec )
{
	const char* callerId = 0;
	bool usePrivateBus = false;
	
	if( ::PalmBrowserSettings()->debugServiceHandles ) {
		m_debug_uri = new String( uri );
		m_debug_params = new String( payload );
	}

	if (document()->frame()) {

		Palm::WebFrame* webFrame = kit(document()->frame());
		if (webFrame && webFrame->client()) {
			callerId = webFrame->client()->getIdentifier();
			usePrivateBus = webFrame->client()->isBusPriviledged();
		}
	}
	
	/*
	 * 	Determine here whether the caller is privileged or not (public or private bus access. This is because 
	 *  LunaServiceManagerListener doesn't expose a way to get to the document() fn of this object, and there's no RTTI
	 *  on this platform to do a safe cast back up to PalmServiceBridge.
	 * 
	 * Instead, overloading LunaServiceManager::call() with a parameter to specify bus type...
	 */
	
	
	LunaServiceManager::instance()->call( uri.utf8().data(), payload.utf8().data(), this, callerId, usePrivateBus );
	if( LSMESSAGE_TOKEN_INVALID == _token ) {
		ExceptionCode ec;
		cancel(ec);
	}
	
	return (int)_token;
}

void PalmServiceBridge::serviceResponse( const char* body )
{
	if( m_canceled || !document() )
		return;
	
	if(!body)
		body = "";

	Frame* frame = document()->frame();
	
#if USE(V8)

	
	// Protect this object so it does not get collected when we call into V8.
	// V8 is free to do garbage collection anytime we asked it to allocate anything,
	// which will be first when the argument string is allocated below.
	ref();
	ScriptController* script = frame->script();
	
	script->gcProtectJSWrapper(this);
	m_inServiceCallback=true;
	
	if( !body )
		body = "";

	if( !m_callbackFunction.IsEmpty() && m_callbackFunction->IsFunction() ) 
	{
        Palm::SimpleStats::Event evtStats(Palm::WebKitStats::getServiceResponseEventData());

        WTF::OwnPtr<webOS::Reporter::Event> evt;

        if (webOS::Reporter::isEnabled()) {
            // Large responses take too long to log and skew the results
            const size_t k_maxResponseLogSize = 512-3-1;
            if (strlen(body) >= k_maxResponseLogSize) {
                char* tmp = new char[k_maxResponseLogSize+3+1];
                strncpy(tmp, body, k_maxResponseLogSize);
                tmp[k_maxResponseLogSize] = '\0';
                strcat(tmp, "...");
                evt = new webOS::Reporter::Event(frame->document(), "v8.luna-service.callback", "Response: %s", tmp);
                delete [] tmp;
            }
            else {
                evt = new webOS::Reporter::Event(frame->document(), "v8.luna-service.callback", "Response: %s", body);
            }
        }

		if( script->canExecuteScripts(AboutToExecuteScript) )
		{
			{
				v8::HandleScope handle_scope;
				v8::Local<v8::Context> context = V8Proxy::context(frame);
				if( !context.IsEmpty()) 
				{
					v8::Context::Scope scope(context);
					V8Proxy* proxy = V8Proxy::retrieve(frame);
		
					v8::Local<v8::Value> args = v8::String::New(body);
					
					// Script timeout is handled in the proxy.
					if( !m_callbackFunction.IsEmpty() )
							proxy->callFunction( v8::Persistent<v8::Function>::Cast(m_callbackFunction),
										context->Global(), 1, &args );
				}
			}
			
			if( !m_subscribed )
			{
				ExceptionCode ec;
				cancel(ec);
			}
			
			//if (Document* doc = frame->document())
			//	doc->updateRendering();			
		}
	}
	
	m_inServiceCallback=false;
	script->gcUnprotectJSWrapper(this);
	deref();
	
	Document::updateStyleForAllDocuments();
	
#else
	JSGlobalObject* globalObject = frame->script()->globalObject();
	ExecState* exec = globalObject->globalExec();
	
	JSC::JSLock(false);
	
    JSValue function = m_callbackFunction->jsFunction();
    CallData callData;
    CallType callType = function.getCallData(callData);
	
    if (callType == CallTypeNone) {
		return;
    }
        
    MarkedArgumentBuffer args;
	args.append( jsString(exec,String::fromUTF8(body) ) );
    
    globalObject->globalData()->timeoutChecker.start();
	JSC::call(exec, function, callType, callData, function, args);
    globalObject->globalData()->timeoutChecker.stop();
        
    if (exec->hadException())
        reportCurrentException(exec);
	
	Document::updateStyleForAllDocuments();
#endif
}

void PalmServiceBridge::cancel( ExceptionCode& ec )
{
	if( m_canceled )
		return;
	
	m_canceled = true;
	if( _token )
		LunaServiceManager::instance()->cancel( this );

#if USE(V8)	
	if (!m_callbackFunction.IsEmpty()) {
		m_callbackFunction.Dispose();
		m_callbackFunction.Clear();
	}
#endif // USE(V8)	
}

void PalmServiceBridge::stop()
{
	ExceptionCode ec;
    cancel(ec);
}

bool PalmServiceBridge::canSuspend() const
{
    return false;
}

void PalmServiceBridge::contextDestroyed()
{
#if USE(V8)
	ASSERT( m_callbackFunction.IsEmpty() );
#endif
    ActiveDOMObject::contextDestroyed();
}

ScriptExecutionContext* PalmServiceBridge::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

Document* PalmServiceBridge::document() const
{
    ASSERT(scriptExecutionContext()->isDocument());
    return static_cast<Document*>(scriptExecutionContext());
}


}
