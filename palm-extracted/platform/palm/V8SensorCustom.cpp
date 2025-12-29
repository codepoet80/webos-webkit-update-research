/**
 *******************************************************************************
 *
 * Copyright (c) 2011 Hewlett-Packard Development Company, L.P.
 * All rights reserved.
 *
 * Created on: Oct 5, 2011
 *
 *******************************************************************************
 */


#include "config.h"

#if ENABLE(WEBOS_SENSORS)

#include "V8Sensor.h"

#include "Base64.h"
#include "DOMTimer.h"
#include "DOMWindow.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameView.h"
#include "HTMLCollection.h"
#include "Page.h"
#include "PlatformScreen.h"
#include "ScheduledAction.h"
#include "ScriptSourceCode.h"
#include "Sensor.h"
#include "Settings.h"
#include "V8Binding.h"
#include "V8CustomEventListener.h"
#include "V8PalmServiceBridge.h"
#include "V8Proxy.h"
#include "V8Utilities.h"

namespace WebCore {

v8::Handle<v8::Value> V8Sensor::ondataAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    return v8::Null();
}

void V8Sensor::ondataAccessorSetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.Sensor.ondata._set");

    Sensor* sensorObj = V8Sensor::toNative(info.Holder());
    if (sensorObj) {
        ScriptExecutionContext* pContext = sensorObj->context();
        if (pContext) {
            if ((!(value->IsNull())) && value->IsFunction())
                sensorObj->setOnDataCallback(v8::Handle<v8::Function>::Cast(value));
        }
    }
}

v8::Handle<v8::Value> V8Sensor::onerrorAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    return v8::Null();
}

void V8Sensor::onerrorAccessorSetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.Sensor.onerror._set");

    Sensor* sensorObj = V8Sensor::toNative(info.Holder());
    if (sensorObj) {
        ScriptExecutionContext* pContext = sensorObj->context();
        if (pContext) {
            if ((!(value->IsNull())) && value->IsFunction())
                sensorObj->setOnErrorCallback(v8::Handle<v8::Function>::Cast(value));
        }
    }
}

}
#endif // #if ENABLE(WEBOS_SENSORS)
