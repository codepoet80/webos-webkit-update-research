
#include "config.h"
#include "DOMWindow.h"

#include "V8Binding.h"
#include "V8PalmServiceBridge.h"
#include "V8CustomEventListener.h"
#include "V8Proxy.h"
#include "V8Utilities.h"

#include "Base64.h"
#include "ExceptionCode.h"
#include "DOMTimer.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameView.h"
#include "HTMLCollection.h"
#include "Page.h"
#include "PlatformScreen.h"
#include "ScheduledAction.h"
#include "ScriptSourceCode.h"
#include "PalmServiceBridge.h"
#include "V8PalmServiceBridge.h"
#include "Settings.h"
#include "WorkerContext.h"
#include "WorkerContextExecutionProxy.h"

namespace WebCore {
	
v8::Handle<v8::Value> V8PalmServiceBridge::constructorCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.PalmServiceBridge.Constructor");

    if (!args.IsConstructCall())
        return throwError("DOM object constructor cannot be called as a function.", V8Proxy::TypeError);

    // Expect no parameters.
    // Allocate a PalmServiceBridge object as its internal field.
    ScriptExecutionContext* scriptContext = getScriptExecutionContext();

    bool subscribe = true;
    if (args.Length() > 0)
    	subscribe = args[0]->BooleanValue();

    RefPtr<PalmServiceBridge> bridge = PalmServiceBridge::create(scriptContext, subscribe);
    V8DOMWrapper::setDOMWrapper(args.Holder(), &info, bridge.get() ); 

    // Add object to the wrapper map.
    bridge->ref();
    V8DOMWrapper::setJSWrapperForActiveDOMObject(bridge.get(), v8::Persistent<v8::Object>::New(args.Holder()));
    return args.Holder();
}


//ACCESSOR_GETTER(PalmServiceBridgeOnservicecallback)
v8::Handle<v8::Value> V8PalmServiceBridge::onservicecallbackAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.PalmServiceBridge.Onservicecallback._get");
    PalmServiceBridge* bridge = V8PalmServiceBridge::toNative(info.Holder());
    
	// TODO 
	
    return v8::Null();
}

//ACCESSOR_SETTER(PalmServiceBridgeOnservicecallback)
void V8PalmServiceBridge::onservicecallbackAccessorSetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.PalmServiceBridge.Onservicecallback._set");
    PalmServiceBridge* bridge = V8PalmServiceBridge::toNative(info.Holder());
	// If the bindings have been trimmed this cached wrapper value may be set to NULL
	if( !bridge )
		return;

    if (value->IsNull()) {
        //if (xmlHttpRequest->onreadystatechange()) {
        //    V8ObjectEventListener* listener = static_cast<V8ObjectEventListener*>(xmlHttpRequest->onreadystatechange());
        //    v8::Local<v8::Object> v8Listener = listener->getListenerObject();
        //    removeHiddenDependency(info.Holder(), v8Listener, V8Custom::kXMLHttpRequestCacheIndex);
        //}

        // Clear the listener.
        //bridge->setOnservicecallback(v8::Null());
    } else 
	{
        if( value->IsFunction() )
			bridge->setOnservicecallback( v8::Handle<v8::Function>::Cast(value) );
    }
}


} // WebCore
