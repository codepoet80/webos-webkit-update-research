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

#ifndef SensorManager_h
#define SensorManager_h

#include "ActiveDOMObject.h"
#include "Event.h"
#include "EventListener.h"
#include "EventTarget.h"
#include "palmwebtypes.h"
#include "palmwebviewclient.h"
#include "v8.h"
#include <glib.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

#if ENABLE(WEBOS_SENSORS)

namespace WebCore {

// Forward Declaration
class Document;
class Sensor;


class SensorManager : public RefCounted<SensorManager>,
                      public ActiveDOMObject {
public:
    /**
     * static constructor with only script execution context
     */
    static PassRefPtr<SensorManager> create(ScriptExecutionContext* context)
    {
        return adoptRef(new SensorManager(context) );
    }

    /**
     * default destructor
     */
    virtual ~SensorManager();

    /**
     * Available Sensors
     */
    String aCCELEROMETER()      const { return m_strAccelerometer;      }
    String oRIENTATION()        const { return m_strOrientation;        }
    String sHAKE()              const { return m_strShake;              }
    String aLS()                const { return m_strALS;                }
    String aNGULAR_VELOCITY()   const { return m_strAngularVelocity;    }
    String cOMPASS()            const { return m_strBearing;            }
    String gRAVITY()            const { return m_strGravity;            }
    String lINEAR_ACCELERATION()const { return m_strLinearAcceleration; }
    String mAGNETIC_FIELD()     const { return m_strMagneticField;      }
    String sCREEN_PROXIMITY()   const { return m_strScreenProximity;    }
    String rOTATION()           const { return m_strRotation;           }

    /**
     * Get the list of supported sensors in a json format
     */
    String getSupportedSensors() const;

    /**
     * Create the requested Sensor
     *
     * @sensorName  - name of the sensor
     * @ec          - ExceptionCode to be set, if any error
     */
    PassRefPtr<Sensor> createSensor(String sensorName, ExceptionCode& ec);

    /**
     * Clear the context
     */
    void disconnectFrame();

    /**
     * Return the script Execution Context
     */
    ScriptExecutionContext* context() const { return ActiveDOMObject::scriptExecutionContext(); }

    /**
     * Validate the sensor name
     */
    static bool validateSensorName(String& sensorName);

    /**
     * Map Sensor Name to sensor Type
     */
    static Palm::SensorType toSensorType(String& sensorName);

public:
    /* Static Constant Data */
    static const String        m_strAccelerometer;
    static const String        m_strOrientation;
    static const String        m_strShake;
    static const String        m_strALS;
    static const String        m_strAngularVelocity;
    static const String        m_strBearing;
    static const String        m_strGravity;
    static const String        m_strLinearAcceleration;
    static const String        m_strMagneticField;
    static const String        m_strScreenProximity;
    static const String        m_strRotation;

private:
    /**
     * Private Constructor with only script execution context
     */
    SensorManager(ScriptExecutionContext *context);

    /**
     * Get Document
     */
    Document* document() const;

    /**
     * Get webView Client
     */
    Palm::WebViewClient* getClient() const;
}; // class SensorManager

} // namespace WebCore

#endif // #if ENABLE(WEBOS_SENSORS)
#endif /* SensorManager_h */
