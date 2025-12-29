
//	Copyright 2008,2009,2010 Palm Inc.

#ifndef __PalmServiceBridge_h__
#define __PalmServiceBridge_h__

#include "EventListener.h"
#include "EventTarget.h"
#include "LunaServiceMgr.h"
#include "Event.h"
#include "ActiveDOMObject.h"
#include <wtf/OwnPtr.h>

#if USE(V8)
#include "v8.h"
#endif

#include <list>
#include <glib.h>


namespace WebCore {

class Document;


class PalmServiceBridge : public RefCounted<PalmServiceBridge>,
							public LunaServiceManagerListener,
							public ActiveDOMObject
{
	public:
		static PassRefPtr<PalmServiceBridge> create( ScriptExecutionContext* context, bool subscribe = false ) { return adoptRef(new PalmServiceBridge(context, subscribe) ); }
		bool init(Document* d, bool subscribed = false);
		~PalmServiceBridge();
		
		static int numHandlesForUrl( const char* appId );
		static void handlesForUrl( const char* appId, std::list<PalmServiceBridge*>& outHandles );
		
		virtual PalmServiceBridge* toPalmServiceBridge() { return this; }

		static void detachServices(Document* doc);
		static void cancelServices(Document* doc);
			
		String version();
		
		int token();
		
		int call( const String& uri, const String& payload, ExceptionCode& ec );
		void cancel( ExceptionCode& ec );
		
		// callback from LunaServiceManagerListener
		virtual void serviceResponse( const char* body );
		
		Document* document() const;
	
#if USE(V8)		
		static void WeakRefCallback( v8::Persistent<v8::Value> object, void* parameter );
		void setOnservicecallback( v8::Handle<v8::Function> func );
#else
		void setOnservicecallback(PassRefPtr<EventListener> eventListener) { m_callbackFunction = eventListener; }
		EventListener* onservicecallback() const { return m_callbackFunction.get(); }
#endif		
		const String* uri() const { return m_debug_uri; }
		const String* payload() const { return m_debug_params; }
		int returnCount() const { return m_debug_returnCount; }
		
		
		virtual ScriptExecutionContext* scriptExecutionContext() const;
		
		// ActiveDOMObject:
		virtual void contextDestroyed();
		virtual bool canSuspend() const;
		virtual void stop();
		
	private:
#if USE(V8)		
		v8::Persistent<v8::Function>	m_callbackFunction;
#else
		RefPtr<EventListener>			m_callbackFunction;
#endif
		bool							m_canceled;						// From JS
		bool							m_subscribed;
		bool							m_inServiceCallback;
		
		String							*m_debug_uri;
		String							*m_debug_params;
		int								m_debug_returnCount;
		
		PalmServiceBridge( ScriptExecutionContext*, bool );
		PalmServiceBridge();
};

}

#endif

