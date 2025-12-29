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

#ifndef SensorObjects_h
#define SensorObjects_h

#include "palmwebtypes.h"
#include <limits.h>
#include <pbnjson.hpp>

/**
 * Indicates a Invalid value. Since 0 cannot be considered as an invalid value
 */
extern const int INVALID_VALUE;

/**
 * Base class for all the sensor Object parsers
 */
class SensorObjectBase {
public:
    SensorObjectBase(Palm::SensorType sensorType);

    /**
     * Default virtual destructor
     */
    virtual ~SensorObjectBase() {}

    /**
     * Function creates an sensor Object & parses the sensor data.
     *
     * @param sensorType    - type of the sensor
     * @param sensorData    - sensor data in json format
     * @return Valid Sensor Object, NULL otherwise
     */
    static SensorObjectBase* parse(Palm::SensorType sensorType, const char* sensorData);

    /**
     * Function takes std::string and converts it to a number
     *
     * @param sensorData - number as string
     * @return string converted to a number
     */
    inline double sensorDataAsNumber(std::string sensorData) const
    {
        return atof(sensorData.c_str());
    }

    /**
     * Function parses the sensor data.
     *
     * @param sensorData    - sensor data in json format
     * @return true if successful, false otherwise
     */
    virtual bool parseSensorData(const char* sensorData) = 0;

    /**
     * Function parses the logical & physical sensor data and returns appropriate sensor object
     *
     * @param sensorData    - Sensor data
     * @param sensorObjName - Sensor Name
     */
    virtual pbnjson::JValue getJsonObject(const char* sensorData, const char* sensorObjName);

private:
    Palm::SensorType m_sensorType;
};

/**
 * Rotation Sensor Object
 */
class RotationSensorObject : public SensorObjectBase {
public:
    RotationSensorObject();

    /**
     * Convenient methods to retrieve sensor values
     */
    const double* rotationMatrix()  { return m_rotationMatrix; }
    double quaternionW() const      { return m_quaternionW; }
    double quaternionX() const      { return m_quaternionX; }
    double quaternionY() const      { return m_quaternionY; }
    double quaternionZ() const      { return m_quaternionZ; }
    double roll() const             { return m_roll; }
    double pitch() const            { return m_pitch; }
    double yaw() const              { return m_yaw; }

    /**
     * In Cartesian co-ordinate this is x value = Pitch from rotation
     */
    double beta() const             { return m_pitch; }

    /**
     * In Cartesian co-ordinate this is y value = roll from rotation
     */
    double gamma() const            { return m_roll; }

     /**
      * Function parses the sensor data.
      *
      * @param sensorData    - sensor data in json format
      * @return true if successful, false otherwise
      */
     virtual bool parseSensorData(const char* sensorData);

private:
     double m_rotationMatrix[9];
     double m_quaternionW;
     double m_quaternionX;
     double m_quaternionY;
     double m_quaternionZ;
     double m_roll;
     double m_pitch;
     double m_yaw;
};

/**
 * Bearing Sensor Object
 */
class BearingSensorObject : public SensorObjectBase {
public:
    BearingSensorObject();

    /**
     * Convenient methods to retrieve sensor values
     */
    double magneticHeading() const   { return m_magneticHeading; }
    double trueBearing() const       { return m_trueBearing; }
    double confidence() const        { return m_confidence; }

    /**
     * Function checks whether compass needs calibration
     *
     * @return true if compass needs calibration, false otherwise
     */
    bool doesCompassNeedsCalibration() const;

    /**
     * do we have alpha value (or compass value)
     * In Cartesian co-ordinate this is z value
     */
    bool hasAlpha() const;

     /**
      * Function parses the sensor data.
      *
      * @param sensorData    - sensor data in json format
      * @return true if successful, false otherwise
      */
     virtual bool parseSensorData(const char* sensorData);

private:
     double m_magneticHeading;
     double m_trueBearing;
     double m_confidence;
};

/**
 * Accelerometer Sensor Object
 */
class AccelerometerSensorObject : public SensorObjectBase {
public:
    AccelerometerSensorObject();

    /**
     * Convenient methods to retrieve sensor values
     */
    double x() const { return m_x; }
    double y() const { return m_y; }
    double z() const { return m_z; }

     /**
      * Function parses the sensor data.
      *
      * @param sensorData    - sensor data in json format
      * @return true if successful, false otherwise
      */
     virtual bool parseSensorData(const char* sensorData);

private:
     double m_x;
     double m_y;
     double m_z;
};

/**
 * Linear Acceleration Sensor Object
 */

class LinearAccelerationSensorObject : public SensorObjectBase {
public:
    LinearAccelerationSensorObject();

    /**
     * Convenient methods to retrieve sensor values
     */
    double x() const      { return m_x; }
    double y() const      { return m_y; }
    double z() const      { return m_z; }
    double worldX() const { return m_worldX; }
    double worldY() const { return m_worldY; }
    double worldZ() const { return m_worldZ; }

     /**
      * Function parses the sensor data.
      *
      * @param sensorData    - sensor data in json format
      * @return true if successful, false otherwise
      */
     virtual bool parseSensorData(const char* sensorData);

private:
     double m_x;
     double m_y;
     double m_z;
     double m_worldX;
     double m_worldY;
     double m_worldZ;
};

/**
 * Logical Device Orientation Sensor Object
 *
 * - This sensor is a combination of rotation & compass (bearing) sensors
 */
class LogicalOrientationSensorObject : public SensorObjectBase {
public:
     LogicalOrientationSensorObject();

     /**
      * do we have alpha value (or compass value)
      * In Cartesian co-ordinate this is z value
      */
     bool hasAlpha() const { return m_bearingSensorObject.hasAlpha(); }

     /**
      * Magnetic heading from bearing
      */
     double alpha() const { return m_bearingSensorObject.magneticHeading(); }

     /**
      * In Cartesian co-ordinate this is x value = Pitch from rotation
      */
     double beta() const  { return m_rotationSensorObject.beta(); }

     /**
      * In Cartesian co-ordinate this is y value = roll from rotation
      */
     double gamma() const { return m_rotationSensorObject.gamma(); }

     /**
      * Function parses the sensor data.
      *
      * @param sensorData    - sensor data in json format
      * @return true if successful, false otherwise
      */
     virtual bool parseSensorData(const char* sensorData);

private:
     RotationSensorObject m_rotationSensorObject;
     BearingSensorObject m_bearingSensorObject;
};

/**
 * Logical Device Motion Sensor Object
 *
 * - This sensor is a combination of Accelerometer, Linear Acceleration & Logical Orientation sensors
 */
class LogicalDeviceMotionSensorObject : public SensorObjectBase {
public:
    LogicalDeviceMotionSensorObject();

    /**
     * Acceleration of the device
     */
    LinearAccelerationSensorObject& accelerationWithoutG() { return m_linearAccelerationSensorObject; }

    /**
     * Acceleration Including Gravity
     */
    AccelerometerSensorObject& accelerationIncludingG() { return m_accelerometerSensorObject; }

    /**
     * do we have alpha value (or compass value)
     * In Cartesian co-ordinate this is z value
     */
    bool hasAlpha() const { return m_bearingSensorObject.hasAlpha(); }

    /**
     * Magnetic heading from bearing
     */
    double alpha() const { return m_bearingSensorObject.magneticHeading(); }

    /**
     * In Cartesian co-ordinate this is x value = Pitch from rotation
     */
    double beta() const  { return m_rotationSensorObject.beta(); }

    /**
     * In Cartesian co-ordinate this is y value = roll from rotation
     */
    double gamma() const { return m_rotationSensorObject.gamma(); }

     /**
      * Function parses the sensor data.
      *
      * @param sensorData    - sensor data in json format
      * @return true if successful, false otherwise
      */
     virtual bool parseSensorData(const char* sensorData);

private:
     BearingSensorObject m_bearingSensorObject;
     RotationSensorObject m_rotationSensorObject;
     AccelerometerSensorObject m_accelerometerSensorObject;
     LinearAccelerationSensorObject m_linearAccelerationSensorObject;
};
#endif /* SensorObjects_h */
