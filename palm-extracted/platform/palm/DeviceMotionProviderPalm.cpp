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

#include "config.h"
#include "DeviceMotionProviderPalm.h"

#include "DeviceMotionController.h"
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
void DeviceMotionProviderPalm::SensorDataCallback(Palm::SensorType sensorType, std::string& jsonData, void *userData)
{
    // Check whether we still have a valid object
    DeviceMotionProviderPalm* sensorObj = static_cast<DeviceMotionProviderPalm *>(userData);
    if ((sensorObj) && (!jsonData.empty()))
        sensorObj->sendData(jsonData.c_str());
}

// Sensor Data Callback
void DeviceMotionProviderPalm::SensorErrCallback(Palm::SensorType sensorType, std::string& errorDescription, void *userData)
{
    // Check whether we still have a valid object
    DeviceMotionProviderPalm* sensorObj = static_cast<DeviceMotionProviderPalm *>(userData);
    if ((sensorObj) && (!errorDescription.empty()))
        sensorObj->sendError(errorDescription.c_str());
}

DeviceMotionProviderPalm::DeviceMotionProviderPalm(Palm::WebPage* page)
    : m_SensorHandle(0)
    , m_SensorDeletefn(0)
    , m_WebPage(page)
    , m_controller(0)
    , m_SensorRunning(false)
    , m_motion(DeviceMotionData::create())
{
}

DeviceMotionProviderPalm::~DeviceMotionProviderPalm()
{
    destroySensor();
}

void DeviceMotionProviderPalm::setController(DeviceMotionController* controller)
{
    m_controller = controller;
}

void DeviceMotionProviderPalm::start()
{
    if (getClient()) {
        if (!m_SensorHandle)
            m_SensorHandle = getClient()->createSensor(Palm::SensorLogicalDeviceMotion, SensorDataCallback, SensorErrCallback, &m_SensorDeletefn, this);

        if (m_SensorHandle) {
            getClient()->startSensor(m_SensorHandle, true);
            m_SensorRunning = true;
        }
    }
}

void DeviceMotionProviderPalm::stop()
{
    if ((getClient()) && (m_SensorHandle)) {
        getClient()->startSensor(m_SensorHandle, false);
        m_SensorRunning = false;
    }
}

Palm::WebViewClient* DeviceMotionProviderPalm::getClient()
{
    Palm::WebViewClient *webViewClient = 0;

    if (m_WebPage && m_WebPage->view()) {
        Palm::WebView *webView = m_WebPage->view();
        if (webView && webView->client())
            webViewClient = webView->client();
    }

    return webViewClient;
}

void DeviceMotionProviderPalm::sendData(const char* data)
{
    if ((data) && (m_controller) && (m_SensorRunning)) {
        LogicalDeviceMotionSensorObject* sensorObj = static_cast<LogicalDeviceMotionSensorObject *>(SensorObjectBase::parse(Palm::SensorLogicalDeviceMotion, data));
        if (sensorObj) {
            const double earthGravity = 9.81;
            RefPtr<DeviceMotionData::Acceleration> accel = DeviceMotionData::Acceleration::create(true,
                                                                                                  sensorObj->accelerationWithoutG().x(),
                                                                                                  true,
                                                                                                  sensorObj->accelerationWithoutG().y(),
                                                                                                  true,
                                                                                                  sensorObj->accelerationWithoutG().z());

            RefPtr<DeviceMotionData::Acceleration> accelG= DeviceMotionData::Acceleration::create(true,
                                                                                                  sensorObj->accelerationIncludingG().x() * earthGravity,
                                                                                                  true,
                                                                                                  sensorObj->accelerationIncludingG().y() * earthGravity,
                                                                                                  true,
                                                                                                  sensorObj->accelerationIncludingG().z() * earthGravity);

            RefPtr<DeviceMotionData::RotationRate> rotation = DeviceMotionData::RotationRate::create(sensorObj->hasAlpha(),
                                                                                                     sensorObj->alpha(),
                                                                                                     true,
                                                                                                     sensorObj->beta(),
                                                                                                     true,
                                                                                                     sensorObj->gamma());

            m_motion = DeviceMotionData::create(accel,  /* Acceleration without Gravity */
                                                accelG, /* Acceleration including Gravity */
                                                rotation,
                                                false, // FIXME : provide timestamps
                                                0);

            m_controller->didChangeDeviceMotion(m_motion.get());

            delete sensorObj;
        }
    }
}

void DeviceMotionProviderPalm::sendError(const char* error)
{
}


void DeviceMotionProviderPalm::destroySensor()
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

