/**
 *******************************************************************************
 *
 * Copyright (c) 2011 Hewlett-Packard Development Company, L.P.
 * All rights reserved.
 *
 * Created on: Nov. 01, 2011
 *
 *******************************************************************************
 */
#ifndef DeviceMotionProviderPalm_h
#define DeviceMotionProviderPalm_h

#include "DeviceMotionData.h"
#include "RefPtr.h"
#include "palmwebframe.h"
#include "palmwebframeclient.h"
#include "palmwebframeprivate.h"
#include "palmwebpage.h"
#include "palmwebview.h"

namespace WebCore {

class DeviceMotionController;
class DeviceMotionClientPalm;

class DeviceMotionProviderPalm {
public:
    DeviceMotionProviderPalm(Palm::WebPage* page);
    ~DeviceMotionProviderPalm();

    /**
     * Start & Stop the connected sensor
     */
    void start();
    void stop();

    /**
     * Get the current device motion
     */
    DeviceMotionData* currentDeviceMotion() const { return m_motion.get(); }

    /**
     * Controller is used to send the data to JS code
     */
    virtual void setController(DeviceMotionController*);

    /**
     * send Data
     */
    void sendData(const char* data);

    /**
     * send Error
     */
    void sendError(const char* err);

protected:
    /**
     * Callbacks from SysMgr-HAL Bindings
     */
    static void SensorDataCallback(Palm::SensorType type, std::string& jsonData, void *userData);
    static void SensorErrCallback(Palm::SensorType type, std::string& errorDescription, void *userData);

    /**
     * Destroys the connected sensor
     */
    void destroySensor();

    /**
     * Get the WebViewClient
     */
    Palm::WebViewClient* getClient();

private:
    RefPtr<DeviceMotionData> m_motion;
    Palm::SensorHandle m_SensorHandle;
    Palm::fnSensorHandleDelete m_SensorDeletefn;
    Palm::WebPage* m_WebPage;
    DeviceMotionController* m_controller;
    bool m_SensorRunning;
};

} // namespace WebCore

#endif /* DeviceMotionProviderPalm_h */
