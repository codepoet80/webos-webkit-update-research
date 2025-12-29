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
#include "Sensor.h"

#include "CString.h"
#include "Document.h"
#include "Event.h"
#include "EventException.h"
#include "EventListener.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "Frame.h"
#include "Logging.h"
#include "Page.h"
#include "PlatformString.h"
#include "ScriptController.h"
#include "SensorManager.h"
#include "V8Proxy.h"
#include "palmwebframe.h"
#include "palmwebframeclient.h"
#include "palmwebframeprivate.h"
#include "palmwebpage.h"
#include "palmwebview.h"
#include "webkitpalmsettings.h"

#include <webkitstats.h>
#include <wtf/RefCountedLeakCounter.h>

namespace WebCore {

// Sensor Data Callback
void Sensor::SensorDataCallback(Palm::SensorType sensorType, std::string& jsonData, void *userData)
{
    // Check whether we still have a valid object
    Sensor* sensorObj = static_cast<Sensor *>(userData);
    if ((sensorObj) && (!jsonData.empty())) {
        String data(jsonData.c_str());
        sensorObj->sendData(data);
    }
}

// Sensor Data Callback
void Sensor::SensorErrCallback(Palm::SensorType sensorType, std::string& errorDescription, void *userData)
{
    // Check whether we still have a valid object
    Sensor* sensorObj = static_cast<Sensor *>(userData);
    if ((sensorObj) && (!errorDescription.empty())) {
        String error(errorDescription.c_str());
        sensorObj->sendError(error);
    }
}

Sensor::Sensor(ScriptExecutionContext *context, ExceptionCode& ec, String& sensorName, Palm::SensorType sensorType)
    : ActiveDOMObject(context, this)
    , m_SensorName(sensorName)
    , m_SensorType(sensorType)
    , m_SensorRate(RATE_DEFAULT)
    , m_SensorHandle(0)
    , m_InDataCallback(false)
    , m_InErrorCallback(false)
    , m_SensorDeletefn(0)
{
    if ((SensorManager::validateSensorName(sensorName)) && (Palm::SensorInvalid != sensorType)) {
        // Create the sensor
        if (getClient()) {
            m_SensorHandle = getClient()->createSensor(m_SensorType, SensorDataCallback, SensorErrCallback, &m_SensorDeletefn, this);
            if (!m_SensorHandle) {
                ec = NOT_FOUND_ERR;
                return;
            }
        }
    } else {
        ec = NOT_SUPPORTED_ERR;
        return;
    }
}

Sensor::~Sensor()
{
    destroySensor();
}

void Sensor::destroySensor()
{
    if (m_SensorHandle) {
        if (getClient()) {
            getClient()->destroySensor(&m_SensorHandle);
        } else if (m_SensorDeletefn) {
            m_SensorDeletefn(&m_SensorHandle);
            m_SensorDeletefn = 0;
        }
    }

    if (!m_OnDataCallbackFunction.IsEmpty()) {
        m_OnDataCallbackFunction.Dispose();
        m_OnDataCallbackFunction.Clear();
    }

    if (!m_OnErrorCallbackFunction.IsEmpty()) {
        m_OnErrorCallbackFunction.Dispose();
        m_OnErrorCallbackFunction.Clear();
    }
}

void Sensor::destroyOnDataCallback(v8::Persistent<v8::Value> object, void* parameter )
{
    Sensor* sensorObj = static_cast<Sensor*>(parameter);

    // In the case when we loose the function reference during a callback, do nothing.
    if ((sensorObj) && (!(sensorObj->m_InDataCallback))) {
        if (!sensorObj->m_OnDataCallbackFunction.IsEmpty()) {
            sensorObj->m_OnDataCallbackFunction.Dispose();
            sensorObj->m_OnDataCallbackFunction.Clear();
        }
    }
}

void Sensor::setOnDataCallback(v8::Handle<v8::Function> func)
{
    // Dispose the earlier ondata callback before setting a new one
    if (!m_OnDataCallbackFunction.IsEmpty()) {
        m_OnDataCallbackFunction.Dispose();
        m_OnDataCallbackFunction.Clear();
    }

    // Create a persistent handle onto the V8 heap
    m_OnDataCallbackFunction = v8::Persistent<v8::Function>::New(func);

    // Set a weak ref destruction callback from GC
    m_OnDataCallbackFunction.MakeWeak(this, (void (*)(v8::Persistent<v8::Value>, void*)) Sensor::destroyOnDataCallback);
}

void Sensor::destroyOnErrorCallback(v8::Persistent<v8::Value> object, void* parameter)
{
    Sensor* sensorObj = static_cast<Sensor*>(parameter);

    // In the case when we loose the function reference during a callback, do nothing.
    if ((sensorObj) && (!(sensorObj->m_InErrorCallback))) {
        if (!sensorObj->m_OnErrorCallbackFunction.IsEmpty()) {
            sensorObj->m_OnErrorCallbackFunction.Dispose();
            sensorObj->m_OnErrorCallbackFunction.Clear();
        }
    }
}

void Sensor::setOnErrorCallback(v8::Handle<v8::Function> func)
{
    // Dispose the earlier onerror callback before setting a new one
    if (!m_OnErrorCallbackFunction.IsEmpty()) {
        m_OnErrorCallbackFunction.Dispose();
        m_OnErrorCallbackFunction.Clear();
    }

    // Create a persistent handle onto the V8 heap
    m_OnErrorCallbackFunction = v8::Persistent<v8::Function>::New(func);

    // Set a weak ref destruction callback from GC
    m_OnErrorCallbackFunction.MakeWeak(this, (void (*)(v8::Persistent<v8::Value>, void*)) Sensor::destroyOnErrorCallback);
}

void Sensor::setCurrentRate(int requestedRate)
{
    if (m_SensorHandle) {
        if (requestedRate >= Palm::RATE_DEFAULT && requestedRate <= Palm::RATE_HIGHEST) {
            if (!(getClient()->setSensorRate(m_SensorHandle, static_cast<Palm::SensorRate>(requestedRate)))) {
                String strError;
                strError.format("Error Occured : Unable to set sensor rate for [%s] sensor.", m_SensorName.utf8().data());
                sendError(strError);
            } else {
                m_SensorRate = (SensorRate)requestedRate;
            }
        } else {
            String strError;
            strError.format("Invalid Sensor Rate for [%s] sensor.", m_SensorName.utf8().data());
            sendError(strError);
        }
    }
}

void Sensor::on()
{
    if (m_SensorHandle) {
        if (!(getClient()->startSensor(m_SensorHandle, true))) {
            String strError;
            strError.format("Error Occured : Unable to start [%s] sensor.", m_SensorName.utf8().data());
            sendError(strError);
        }
    }
}

void Sensor::off()
{
    if (m_SensorHandle) {
        if (!(getClient()->startSensor(m_SensorHandle, false))) {
            String strError;
            strError.format("Error Occured : Unable to stop [%s] sensor.", m_SensorName.utf8().data());
            sendError(strError);
        }
    }
}

void Sensor::sendData(String& data)
{
    if ((!data.isEmpty()) && (document()) && (!m_SensorName.isEmpty())) {
        // Protect this object so it does not get collected when we call into V8.
        // V8 is free to do garbage collection anytime we asked it to allocate anything,
        // which will be first when the argument string is allocated below.
        ref();

        m_InDataCallback = true;

        if ((!m_OnDataCallbackFunction.IsEmpty()) && (m_OnDataCallbackFunction->IsFunction()) && (document()->frame())) {
            v8::HandleScope hScope;
            v8::Local<v8::Context> context = V8Proxy::context(document()->frame());
            if (!context.IsEmpty()) {
                v8::Context::Scope scope(context);
                V8Proxy* proxy = V8Proxy::retrieve(document()->frame());

                v8::Local<v8::Value> argv[2];
                argv[0] = v8::String::New(m_SensorName.utf8().data());
                argv[1] = v8::String::New(data.utf8().data());

                // Call ondata callback
                proxy->callFunction(v8::Persistent<v8::Function>::Cast(m_OnDataCallbackFunction), context->Global(), 2, argv);
            }
        }

        m_InDataCallback = false;

        deref();
    }
}

void Sensor::sendError(String& error)
{
    if ((!error.isEmpty()) && (document()) && (!m_SensorName.isEmpty())) {
        // Protect this object so it does not get collected when we call into V8.
        // V8 is free to do garbage collection anytime we asked it to allocate anything,
        // which will be first when the argument string is allocated below.
        ref();

        m_InErrorCallback = true;

        if ((!m_OnErrorCallbackFunction.IsEmpty()) && (m_OnErrorCallbackFunction->IsFunction())) {
            v8::HandleScope hScope;
            v8::Local<v8::Context> context = V8Proxy::context(document()->frame());
            if (!context.IsEmpty()) {
                v8::Context::Scope scope(context);
                V8Proxy* proxy = V8Proxy::retrieve(document()->frame());

                v8::Handle<v8::Value> argv[2];
                argv[0] = v8::String::New(m_SensorName.utf8().data());
                argv[1] = v8::String::New(error.utf8().data());

                // Call onerror callback
                proxy->callFunction(v8::Persistent<v8::Function>::Cast(m_OnErrorCallbackFunction), context->Global(), 2, argv);
            }
        }

        m_InErrorCallback = false;

        deref();
    }
}

Palm::WebViewClient* Sensor::getClient() const
{
    Palm::WebViewClient *webViewClient = 0;

    if (document() && document()->frame()) {
        Palm::WebFrame *webFrame = kit(document()->frame());
        if (webFrame && webFrame->page()) {
            Palm::WebPage *webPage = webFrame->page();
            if (webPage && webPage->view()) {
                Palm::WebView *webView = webPage->view();
                if (webView && webView->client())
                    webViewClient = webView->client();
            }
        }
    }

    return webViewClient;
}

Document* Sensor::document() const
{
    ASSERT(ActiveDOMObject::scriptExecutionContext()->isDocument());
    return static_cast<Document*>(ActiveDOMObject::scriptExecutionContext());
}

void Sensor::stop()
{
    destroySensor();
}

bool Sensor::canSuspend() const
{
    return false;
}

void Sensor::contextDestroyed()
{
    ActiveDOMObject::contextDestroyed();
}

}

#endif // #if ENABLE(WEBOS_SENSORS)
