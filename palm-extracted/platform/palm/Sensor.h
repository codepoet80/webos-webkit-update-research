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


#ifndef Sensor_h
#define Sensor_h

#include "ActiveDOMObject.h"
#include "Event.h"
#include "EventListener.h"
#include "EventTarget.h"
#include "palmwebtypes.h"
#include "palmwebviewclient.h"
#include "v8.h"
#include <glib.h>
#include <list>
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

#if ENABLE(WEBOS_SENSORS)

using namespace Palm;

namespace WebCore {

// Forward Declaration
class Document;

class Sensor : public RefCounted<Sensor>,
               public ActiveDOMObject   {
public:
    /**
     * @brief Defines all valid poll rates for sensors.
     *
     * Each sensor module that polls for events defines a corresponding rate (in samples per second)
     * for each of these values.
     */
    enum SensorRate {
        RATE_DEFAULT    = Palm::RATE_DEFAULT,
        RATE_LOW        = Palm::RATE_LOW,
        RATE_MEDIUM     = Palm::RATE_MEDIUM,
        RATE_HIGH       = Palm::RATE_HIGH,
        RATE_HIGHEST    = Palm::RATE_HIGHEST
    };

    /**
     * static constructor with sensor name as argument
     */
    static PassRefPtr<Sensor> create(ScriptExecutionContext* context, ExceptionCode& ec, String& sensorName, Palm::SensorType sensorType)
    {
        return adoptRef(new Sensor(context, ec, sensorName, sensorType));
    }

    /**
     * default destructor
     */
    virtual ~Sensor();

    /**
     * Get the current sensor rate
     */
    int currentRate()  { return m_SensorRate; }

    /**
     * Set the current sensor rate
     *
     * @param aRequestedRate    - Requested rate
     */
    void setCurrentRate(int requestedRate);

    /**
     * Get the current sensor name
     */
    String sensorName() const { return m_SensorName; }

    /**
     * Sensor Management APIs
     */

    /**
     * Switch on the sensor
     */
    void on();

    /**
     * Switch off the sensor
     */
    void off();

    /**
     * Return the script Execution Context
     */
    ScriptExecutionContext* context() const { return ActiveDOMObject::scriptExecutionContext(); }

    /**
     * Set ondata callback
     */
    void setOnDataCallback(v8::Handle<v8::Function> func);

    /**
     * Set onerror callback
     */
    void setOnErrorCallback(v8::Handle<v8::Function> func);

    /**
     * From ActiveDOMObject:
     */
    virtual void contextDestroyed();
    virtual bool canSuspend() const;
    virtual void stop();

protected:
    /**
     * Callback from Garbage collector to destroy Persistent handles
     *
     * Persistent handles are not held on a stack and are deleted only when you specifically remove them.
     * Just like a local handle, a persistent handle provides a reference to a heap-allocated object.
     * Use a persistent handle when you need to keep a reference to an object for more than one function call,
     * or when handle lifetimes do not correspond to C++ scopes. Google Chrome, for example, uses persistent
     * handles to refer to Document Object Model (DOM) nodes. Create a persistent handle with Persistent::New
     * and dispose with Persistent::Dispose A persistent handle can be made weak, using Persistent::MakeWeak,
     * to trigger a callback from the garbage collector when the only references to an object are from weak
     * persistent handles.
     */
    static void destroyOnDataCallback(v8::Persistent<v8::Value> object, void* parameter);
    static void destroyOnErrorCallback(v8::Persistent<v8::Value> object, void* parameter);

    /**
     * Callbacks from SysMgr-HAL Bindings
     */
    static void SensorDataCallback(Palm::SensorType type, std::string& jsonData, void *userData);
    static void SensorErrCallback(Palm::SensorType type, std::string& errorDescription, void *userData);
private:
    /**
     * Private Constructor takes the script execution context and sensor name as arguments
     */
    Sensor(ScriptExecutionContext *context, ExceptionCode& ec, String& sensorName, Palm::SensorType sensorType);

    /**
     * send Data
     */
    void sendData(String& data);

    /**
     * send Error
     */
    void sendError(String& err);

    /**
     * destroy the sensor
     */
    void destroySensor();

    /**
     * Get Document
     */
    Document* document() const;

    /**
     * Get webView Client
     */
    Palm::WebViewClient* getClient() const;

    // Data
    String m_SensorName;
    Palm::SensorType m_SensorType;
    SensorRate m_SensorRate;
    SensorHandle m_SensorHandle;
    bool m_InDataCallback;
    bool m_InErrorCallback;
    fnSensorHandleDelete m_SensorDeletefn;
    v8::Persistent<v8::Function> m_OnDataCallbackFunction;
    v8::Persistent<v8::Function> m_OnErrorCallbackFunction;
}; // class Sensor

}
#endif // #if ENABLE(WEBOS_SENSORS)

#endif /* Sensor_h */
