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

#include "config.h"
#include "DeviceOrientationProviderPalm.h"

#include "DeviceOrientationController.h"
#include "Page.h"
#include "PlatformString.h"
#include "ScriptController.h"
#include "SensorManager.h"
#include "SensorObjects.h"

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
void DeviceOrientationProviderPalm::SensorDataCallback(Palm::SensorType sensorType, std::string& jsonData, void *userData)
{
    // Check whether we still have a valid object
    DeviceOrientationProviderPalm* sensorObj = static_cast<DeviceOrientationProviderPalm *>(userData);
    if ((sensorObj) && (!jsonData.empty()))
        sensorObj->sendData(jsonData.c_str());
}

// Sensor Data Callback
void DeviceOrientationProviderPalm::SensorErrCallback(Palm::SensorType sensorType, std::string& errorDescription, void *userData)
{
    // Check whether we still have a valid object
    DeviceOrientationProviderPalm* sensorObj = static_cast<DeviceOrientationProviderPalm *>(userData);
    if ((sensorObj) && (!errorDescription.empty()))
        sensorObj->sendError(errorDescription.c_str());
}

DeviceOrientationProviderPalm::DeviceOrientationProviderPalm(Palm::WebPage* page)
    : m_SensorHandle(0)
    , m_SensorDeletefn(0)
    , m_WebPage(page)
    , m_controller(0)
    , m_SensorRunning(false)
    , m_orientation(DeviceOrientation::create())
{
}

DeviceOrientationProviderPalm::~DeviceOrientationProviderPalm()
{
    destroySensor();
}

void DeviceOrientationProviderPalm::setController(DeviceOrientationController* controller)
{
    m_controller = controller;
}

void DeviceOrientationProviderPalm::start()
{
    if (getClient()) {
        if (!m_SensorHandle)
            m_SensorHandle = getClient()->createSensor(Palm::SensorLogicalDeviceOrientation, SensorDataCallback, SensorErrCallback, &m_SensorDeletefn, this);

        if (m_SensorHandle) {
            getClient()->startSensor(m_SensorHandle, true);
            m_SensorRunning = true;
        }
    }
}

void DeviceOrientationProviderPalm::stop()
{
    if ((getClient()) && (m_SensorHandle)) {
        getClient()->startSensor(m_SensorHandle, false);
        m_SensorRunning = false;
    }
}

Palm::WebViewClient* DeviceOrientationProviderPalm::getClient()
{
    Palm::WebViewClient *webViewClient = 0;

    if (m_WebPage && m_WebPage->view()) {
        Palm::WebView *webView = m_WebPage->view();
        if (webView && webView->client())
            webViewClient = webView->client();
    }

    return webViewClient;
}

void DeviceOrientationProviderPalm::sendData(const char* data)
{
    if (data && m_controller && m_SensorRunning) {
        LogicalOrientationSensorObject* sensorObj = static_cast<LogicalOrientationSensorObject *>(SensorObjectBase::parse(Palm::SensorLogicalDeviceOrientation, data));
        if (sensorObj) {
            m_orientation = DeviceOrientation::create(sensorObj->hasAlpha(),
                                                      sensorObj->alpha(),
                                                      true,
                                                      sensorObj->beta(),
                                                      true,
                                                      sensorObj->gamma());

            m_controller->didChangeDeviceOrientation(m_orientation.get());

            delete sensorObj;
        }
    }
}

void DeviceOrientationProviderPalm::sendError(const char* error)
{
}


void DeviceOrientationProviderPalm::destroySensor()
{
    if (m_SensorHandle) {
        if (getClient()) {
            getClient()->destroySensor(&m_SensorHandle);
        } else if (m_SensorDeletefn) {
            m_SensorDeletefn(&m_SensorHandle);
            m_SensorDeletefn = 0;
        }
        m_SensorHandle = 0;
    }
}

}
