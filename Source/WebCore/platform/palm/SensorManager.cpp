/**
 *******************************************************************************
 *
 * Copyright (c) 2011 Hewlett-Packard Development Company, L.P.
 * All rights reserved.
 *
 * Created on: Sep 29, 2011
 *
 *******************************************************************************
 */
#include "config.h"

#if ENABLE(WEBOS_SENSORS)

#include "SensorManager.h"

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
#include "Sensor.h"
#include "V8Proxy.h"
#include "palmwebframe.h"
#include "palmwebframeclient.h"
#include "palmwebframeprivate.h"
#include "palmwebpage.h"
#include "palmwebview.h"
#include "webkitpalmsettings.h"

#include <map>
#include <string>
#include <utility>
#include <webkitstats.h>
#include <wtf/RefCountedLeakCounter.h>

namespace WebCore {

typedef std::map<std::string, Palm::SensorType> SensorMap;
static SensorMap sensorMap;

static void InitSensorMap()
{
    static bool isInitialized = false;
    if (!isInitialized) {
        // Add all the available sensors
        sensorMap[std::string(Palm::SensorNames::strAccelerometer())]      = Palm::SensorAcceleration;
        sensorMap[std::string(Palm::SensorNames::strOrientation())]        = Palm::SensorOrientation;
        sensorMap[std::string(Palm::SensorNames::strShake())]              = Palm::SensorShake;
        sensorMap[std::string(Palm::SensorNames::strALS())]                = Palm::SensorALS;
        sensorMap[std::string(Palm::SensorNames::strAngularVelocity())]    = Palm::SensorAngularVelocity;
        sensorMap[std::string(Palm::SensorNames::strBearing())]            = Palm::SensorBearing;
        sensorMap[std::string(Palm::SensorNames::strGravity())]            = Palm::SensorGravity;
        sensorMap[std::string(Palm::SensorNames::strLinearAcceleration())] = Palm::SensorLinearAcceleration;
        sensorMap[std::string(Palm::SensorNames::strMagneticField())]      = Palm::SensorMagneticField;
        sensorMap[std::string(Palm::SensorNames::strScreenProximity())]    = Palm::SensorScreenProximity;
        sensorMap[std::string(Palm::SensorNames::strRotation())]           = Palm::SensorRotation;

        isInitialized = true;
    }
}

const String SensorManager::m_strAccelerometer(Palm::SensorNames::strAccelerometer());
const String SensorManager::m_strOrientation(Palm::SensorNames::strOrientation());
const String SensorManager::m_strShake(Palm::SensorNames::strShake());
const String SensorManager::m_strALS(Palm::SensorNames::strALS());
const String SensorManager::m_strAngularVelocity(Palm::SensorNames::strAngularVelocity());
const String SensorManager::m_strBearing(Palm::SensorNames::strBearing());
const String SensorManager::m_strGravity(Palm::SensorNames::strGravity());
const String SensorManager::m_strLinearAcceleration(Palm::SensorNames::strLinearAcceleration());
const String SensorManager::m_strMagneticField(Palm::SensorNames::strMagneticField());
const String SensorManager::m_strScreenProximity(Palm::SensorNames::strScreenProximity());
const String SensorManager::m_strRotation(Palm::SensorNames::strRotation());

SensorManager::SensorManager(ScriptExecutionContext *context)
    : ActiveDOMObject(context, this)
{
    InitSensorMap();
}

SensorManager::~SensorManager()
{
}


Palm::SensorType SensorManager::toSensorType(String& sensorName)
{
    Palm::SensorType mappedSensor = Palm::SensorInvalid;

    SensorMap::const_iterator it = sensorMap.find(std::string(sensorName.utf8().data()));
    if (it != sensorMap.end())
        mappedSensor = it->second;

    return mappedSensor;
}

bool SensorManager::validateSensorName(String& sensorName)
{
    bool bValid = false;

    SensorMap::const_iterator it = sensorMap.find(std::string(sensorName.utf8().data()));
    if (it != sensorMap.end())
        bValid = true;

    return bValid;
}

PassRefPtr<Sensor> SensorManager::createSensor(String sensorName, ExceptionCode& ec)
{
    if (validateSensorName(sensorName))
        return Sensor::create(ActiveDOMObject::scriptExecutionContext(), ec, sensorName, toSensorType(sensorName));

    // Otherwise return the syntax error
    ec = SYNTAX_ERR;
    return 0;
}

String SensorManager::getSupportedSensors() const
{
    if (getClient())
        return String(getClient()->getSupportedSensors().c_str());

    return "";
}

void SensorManager::disconnectFrame()
{
    // Destroy the sensor here
}

Palm::WebViewClient* SensorManager::getClient() const
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

Document* SensorManager::document() const
{
    ASSERT(ActiveDOMObject::scriptExecutionContext()->isDocument());
    return static_cast<Document*>(ActiveDOMObject::scriptExecutionContext());
}

}

#endif // #if ENABLE(WEBOS_SENSORS)
