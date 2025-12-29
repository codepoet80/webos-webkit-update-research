/**
 *******************************************************************************
 *
 * Copyright (c) 2011 Hewlett-Packard Development Company, L.P.
 * All rights reserved.
 *
 * Created on: Oct 27, 2011
 *
 *******************************************************************************
 */
#ifndef DeviceOrientationProviderPalm_h
#define DeviceOrientationProviderPalm_h

#include "DeviceOrientation.h"
#include "RefPtr.h"
#include "palmwebframe.h"
#include "palmwebframeclient.h"
#include "palmwebframeprivate.h"
#include "palmwebpage.h"
#include "palmwebview.h"
#include "webkitpalmsettings.h"

using namespace Palm;

namespace WebCore {

class DeviceOrientationController;
class DeviceOrientationClientPalm;

class DeviceOrientationProviderPalm {
public:
    DeviceOrientationProviderPalm(Palm::WebPage* page);
    ~DeviceOrientationProviderPalm();

    /**
     * Sensor Control APIs
     */
    void start();
    void stop();

    /**
     * Get the last orientation
     */
    DeviceOrientation* orientation() const { return m_orientation.get(); }
    virtual void setController(DeviceOrientationController*);

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
    RefPtr<DeviceOrientation> m_orientation;
    SensorHandle m_SensorHandle;
    fnSensorHandleDelete m_SensorDeletefn;
    Palm::WebPage* m_WebPage;
    DeviceOrientationController* m_controller;
    bool m_SensorRunning;
};

}


#endif /* DeviceOrientationProviderPalm_h */
