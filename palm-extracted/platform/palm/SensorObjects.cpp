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
#include "SensorObjects.h"

#include <stdlib.h>

const int INVALID_VALUE = (INT_MIN);

/**
 * Base class for all the sensor Object parsers
 */
SensorObjectBase::SensorObjectBase(Palm::SensorType sensorType)
    : m_sensorType(sensorType)
{
}

SensorObjectBase* SensorObjectBase::parse(Palm::SensorType sensorType, const char* sensorData)
{
    SensorObjectBase* sensorObj = 0;
    if ((Palm::SensorInvalid != sensorType) && (sensorData)) {
        switch (sensorType) {
        case Palm::SensorRotation:
            sensorObj = new RotationSensorObject();
            break;

        case Palm::SensorBearing:
            sensorObj = new BearingSensorObject();
            break;

        case Palm::SensorAcceleration:
            sensorObj = new AccelerometerSensorObject();
            break;

        case Palm::SensorLinearAcceleration:
            sensorObj = new LinearAccelerationSensorObject();
            break;

        case Palm::SensorLogicalDeviceOrientation:
            sensorObj = new LogicalOrientationSensorObject();
            break;

        case Palm::SensorLogicalDeviceMotion:
            sensorObj = new LogicalDeviceMotionSensorObject();
            break;

        default:
            break;
        }

        if ((sensorObj) && (!(sensorObj->parseSensorData(sensorData)))) {
            g_critical("SensorObjectBase::parse : Error : [%s]", sensorData);
            // unable to parse the sensor data
            delete sensorObj;
            sensorObj = 0;
        }
    }

    return sensorObj;
}

pbnjson::JValue SensorObjectBase::getJsonObject(const char* sensorData, const char* sensorName)
{
    pbnjson::JValue retValue;

    if ((sensorData) && (sensorName)) {
        pbnjson::JSchema inputSchema = pbnjson::JSchemaFragment("{}");

        pbnjson::JDomParser parser;
        std::string jsonRaw(sensorData);
        if (parser.parse(jsonRaw, inputSchema)) {
            std::string objectName;
            std::string sensorObjName(sensorName);

            pbnjson::JValue root = parser.getDom();

            for (pbnjson::JValue::ObjectIterator pair = root.begin(); pair != root.end(); ++pair) {
                if ((!((*pair).second.isNull())) && ((*pair).second.isArray())) { // if it is part of a logical sensor
                    pbnjson::JValue objArray = (*pair).second;
                    for (int nCounter = 0; nCounter < objArray.arraySize(); ++nCounter) {
                        pbnjson::JValue arrayData = objArray[nCounter];

                        if ((!(arrayData[sensorObjName].isNull())) && (arrayData[sensorObjName].isObject())) {
                            retValue = arrayData[sensorObjName];
                            break;
                        }
                    }
                } else if ((!((*pair).first.isNull())) && (CONV_OK == (*pair).first.asString(objectName)) && (objectName == sensorObjName)) {
                    if ((!((*pair).second.isNull())) && ((*pair).second.isObject()))
                        retValue = (*pair).second; // Bearing sensor object

                    break;
                }
            }
        }
    }

    return retValue;
}


/**
 * Rotation Sensor Object
 */
RotationSensorObject::RotationSensorObject()
    : SensorObjectBase(Palm::SensorRotation)
    , m_quaternionW(0)
    , m_quaternionX(0)
    , m_quaternionY(0)
    , m_quaternionZ(0)
    , m_roll(0)
    , m_pitch(0)
    , m_yaw(0)
{
}

bool RotationSensorObject::parseSensorData(const char* sensorData)
{
    bool successfullyParsed = false;

    pbnjson::JValue rotationData = SensorObjectBase::getJsonObject(sensorData, Palm::SensorNames::strRotation());

    if ((!rotationData.isNull()) && (rotationData.isObject())) {
        pbnjson::JValue rotationMatrix = rotationData[Palm::HALJsonStringConst::strRotationMatrix()];
        pbnjson::JValue quaternionVector = rotationData[Palm::HALJsonStringConst::strQuaternionVector()];
        pbnjson::JValue eulerangle = rotationData[Palm::HALJsonStringConst::strEulerAngle()];

        // parsing the rotation matrix
        if ((!rotationMatrix.isNull()) && (rotationMatrix.isArray())) {
            for (int nCounter = 0; nCounter < rotationMatrix.arraySize(); ++nCounter) {
                m_rotationMatrix[nCounter] = sensorDataAsNumber(rotationMatrix[nCounter].asNumber<std::string>());
                // g_critical("[kkk] : RotationSensorObject::parseSensorData : [Matrix = %f]", m_rotationMatrix[nCounter]);
            }
        }

        // parsing quaternion vector
        if (!quaternionVector.isNull()) {
            m_quaternionW = sensorDataAsNumber(quaternionVector[Palm::HALJsonStringConst::strW()].asNumber<std::string>());
            m_quaternionX = sensorDataAsNumber(quaternionVector[Palm::HALJsonStringConst::strX()].asNumber<std::string>());
            m_quaternionY = sensorDataAsNumber(quaternionVector[Palm::HALJsonStringConst::strY()].asNumber<std::string>());
            m_quaternionZ = sensorDataAsNumber(quaternionVector[Palm::HALJsonStringConst::strZ()].asNumber<std::string>());
            // g_critical("[kkk] : RotationSensorObject::parseSensorData : [Data : w= %f, x=%f, y=%f, z=%f]", m_quaternionW, m_quaternionX, m_quaternionY, m_quaternionZ);
        }

        // parsing euler angle
        if (!eulerangle.isNull()) {
            m_roll = sensorDataAsNumber(eulerangle[Palm::HALJsonStringConst::strRoll()].asNumber<std::string>());
            m_pitch = sensorDataAsNumber(eulerangle[Palm::HALJsonStringConst::strPitch()].asNumber<std::string>());
            m_yaw = sensorDataAsNumber(eulerangle[Palm::HALJsonStringConst::strYaw()].asNumber<std::string>());

            // g_critical("[kkk] : RotationSensorObject::parseSensorData : [Data : roll= %f, pitch=%f, yaw=%f]", m_roll, m_pitch, m_yaw);
        }

        successfullyParsed = true;
    }

    return successfullyParsed;
}

/**
 * Bearing Sensor Object
 */
BearingSensorObject::BearingSensorObject()
    : SensorObjectBase(Palm::SensorBearing)
    , m_magneticHeading(INVALID_VALUE)
    , m_trueBearing(INVALID_VALUE)
    , m_confidence(0)
{
}

bool BearingSensorObject::hasAlpha() const
{
    return (INVALID_VALUE != m_magneticHeading);
}

bool BearingSensorObject::doesCompassNeedsCalibration() const
{
    return ((INVALID_VALUE != m_confidence) && (m_confidence < 100.0));
}

bool BearingSensorObject::parseSensorData(const char* sensorData)
{
    bool successfullyParsed = false;

    pbnjson::JValue bearingData = SensorObjectBase::getJsonObject(sensorData, Palm::SensorNames::strBearing());

    if ((!bearingData.isNull()) && (bearingData.isObject())) {
        m_magneticHeading = sensorDataAsNumber(bearingData[Palm::HALJsonStringConst::strMagnetic()].asNumber<std::string>());
        m_trueBearing = sensorDataAsNumber(bearingData[Palm::HALJsonStringConst::strTrueBearing()].asNumber<std::string>());
        m_confidence = sensorDataAsNumber(bearingData[Palm::HALJsonStringConst::strConfidence()].asNumber<std::string>());

        // g_critical("[kkk] : BearingSensorObject::parseSensorData : [Data : mag_heading = %f, true=%f, heading=%f]", magneticHeading(), trueBearing(), confidence());
        successfullyParsed = true;
    }

    return successfullyParsed;
}

/**
 * Accelerometer Sensor Object
 */
AccelerometerSensorObject::AccelerometerSensorObject()
    : SensorObjectBase(Palm::SensorAcceleration)
    , m_x(0)
    , m_y(0)
    , m_z(0)
{
}

bool AccelerometerSensorObject::parseSensorData(const char* sensorData)
{
    bool successfullyParsed = false;

    pbnjson::JValue accelData = SensorObjectBase::getJsonObject(sensorData, Palm::SensorNames::strAccelerometer());

    if ((!accelData.isNull()) && (accelData.isObject())) {
        m_x = sensorDataAsNumber(accelData[Palm::HALJsonStringConst::strX()].asNumber<std::string>());
        m_y = sensorDataAsNumber(accelData[Palm::HALJsonStringConst::strY()].asNumber<std::string>());
        m_z = sensorDataAsNumber(accelData[Palm::HALJsonStringConst::strZ()].asNumber<std::string>());

        successfullyParsed = true;
    }

    return successfullyParsed;
}

/**
 * Linear Acceleration Sensor Object
 */

LinearAccelerationSensorObject::LinearAccelerationSensorObject()
    : SensorObjectBase(Palm::SensorLinearAcceleration)
    , m_x(0)
    , m_y(0)
    , m_z(0)
    , m_worldX(0)
    , m_worldY(0)
    , m_worldZ(0)
{
}

bool LinearAccelerationSensorObject::parseSensorData(const char* sensorData)
{
    bool successfullyParsed = false;

    pbnjson::JValue linearAccelData = SensorObjectBase::getJsonObject(sensorData, Palm::SensorNames::strLinearAcceleration());

    if ((!linearAccelData.isNull()) && (linearAccelData.isObject())) {
        m_x = sensorDataAsNumber(linearAccelData[Palm::HALJsonStringConst::strX()].asNumber<std::string>());
        m_y = sensorDataAsNumber(linearAccelData[Palm::HALJsonStringConst::strY()].asNumber<std::string>());
        m_z = sensorDataAsNumber(linearAccelData[Palm::HALJsonStringConst::strZ()].asNumber<std::string>());

        m_worldX = sensorDataAsNumber(linearAccelData[Palm::HALJsonStringConst::strWorldX()].asNumber<std::string>());
        m_worldY = sensorDataAsNumber(linearAccelData[Palm::HALJsonStringConst::strWorldY()].asNumber<std::string>());
        m_worldZ = sensorDataAsNumber(linearAccelData[Palm::HALJsonStringConst::strWorldZ()].asNumber<std::string>());

        // g_critical("[kkk] : LinearAccelerationSensorObject::parseSensorData : [Data : x=%f, y=%f, z=%f]", m_x, m_y, m_z);
        // g_critical("[kkk] : AccelerometerSensorObject::parseSensorData : [Data : wx=%f, wy=%f, wz=%f]", m_worldX, m_worldY, m_worldZ);
        successfullyParsed = true;
    }

    return successfullyParsed;
}

/**
 * Logical Device Orientation Sensor Object
 *
 * - This sensor is a combination of rotation & compass (bearing) sensors
 */
LogicalOrientationSensorObject::LogicalOrientationSensorObject()
    : SensorObjectBase(Palm::SensorLogicalDeviceOrientation)
{
}

bool LogicalOrientationSensorObject::parseSensorData(const char* sensorData)
{
    bool successfullyParsed = false;

    if (sensorData) {
        pbnjson::JSchema inputSchema = pbnjson::JSchemaFragment("{}");

        pbnjson::JDomParser parser;
        std::string jsonRaw(sensorData);
        if (parser.parse(jsonRaw, inputSchema)) {
            pbnjson::JValue json = parser.getDom();

            if (json[Palm::SensorNames::strLogicalDeviceOrientation()].isArray()) {
                successfullyParsed = m_bearingSensorObject.parseSensorData(sensorData);
                if (successfullyParsed)
                    successfullyParsed = m_rotationSensorObject.parseSensorData(sensorData);
            }
        }
    }

    return successfullyParsed;
}

/**
 * Logical Device Motion Sensor Object
 *
 * - This sensor is a combination of Accelerometer, Linear Acceleration & rotation sensors
 */
LogicalDeviceMotionSensorObject::LogicalDeviceMotionSensorObject()
    : SensorObjectBase(Palm::SensorLogicalDeviceMotion)
{
}

bool LogicalDeviceMotionSensorObject::parseSensorData(const char* sensorData)
{
    bool successfullyParsed = false;

    if (sensorData) {
        pbnjson::JSchema inputSchema = pbnjson::JSchemaFragment("{}");

        pbnjson::JDomParser parser;
        std::string jsonRaw(sensorData);
        if (parser.parse(jsonRaw, inputSchema)) {
            pbnjson::JValue json = parser.getDom();

            if (json[Palm::SensorNames::strLogicalDeviceMotion()].isArray()) {
                successfullyParsed = m_bearingSensorObject.parseSensorData(sensorData);
                if (successfullyParsed)
                    successfullyParsed = m_rotationSensorObject.parseSensorData(sensorData);
                if (successfullyParsed)
                    successfullyParsed = m_accelerometerSensorObject.parseSensorData(sensorData);
                if (successfullyParsed)
                    successfullyParsed = m_linearAccelerationSensorObject.parseSensorData(sensorData);
            }
        }
    }

    return successfullyParsed;
}
