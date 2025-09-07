/**
 * SensorManager.h - Sensor management for KC868-A16
 * Created by Your Name, Date
 * Released into the public domain.
 */

#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include <RTClib.h>
#include <Wire.h>

 // Sensor type definitions for HT1-HT3 pins
#define SENSOR_TYPE_DIGITAL  0  // General digital input
#define SENSOR_TYPE_DHT11    1  // DHT11 temperature/humidity sensor
#define SENSOR_TYPE_DHT22    2  // DHT22/AM2302 temperature/humidity sensor
#define SENSOR_TYPE_DS18B20  3  // DS18B20 temperature sensor

// GPIO definitions for HT pins
#define HT1_PIN              32
#define HT2_PIN              33
#define HT3_PIN              14

// Structure for HT pin configuration
struct HTSensorConfig {
    uint8_t sensorType;     // 0=Digital, 1=DHT11, 2=DHT22, 3=DS18B20
    float temperature;      // Last temperature reading (°C)
    float humidity;         // Last humidity reading (% - only for DHT sensors)
    bool configured;        // Whether sensor has been configured
    unsigned long lastReadTime; // Last time sensor was read
};

class SensorManager {
public:
    SensorManager();

    // Initialize sensors
    void begin();

    // Initialize RTC (Real Time Clock)
    void initRTC();

    // Read all sensors
    void readAllSensors();

    // Read a specific HT sensor
    void readSensor(int htIndex);

    // Get HT sensor configuration
    HTSensorConfig* getSensorConfig(int index);

    // Update sensor configuration
    bool updateSensorConfig(int index, uint8_t sensorType);

    // Save sensor configurations
    void saveSensorConfigs();

    // Load sensor configurations
    void loadSensorConfigs();

    // RTC functions
    bool isRTCAvailable() { return _rtcInitialized; }
    DateTime getCurrentTime();
    bool setCurrentTime(int year, int month, int day, int hour, int minute, int second);
    bool syncTimeFromNTP();
    String getTimeString();

    // Added methods to fix compilation errors
    uint8_t getSensorType(int index);
    float getTemperature(int index);
    float getHumidity(int index);

private:
    // GPIO definitions for HT pins
    const uint8_t HT_PINS[3] = { HT1_PIN, HT2_PIN, HT3_PIN }; // HT1, HT2, HT3

    // Sensor objects
    DHT* _dhtSensors[3];
    OneWire* _oneWireBuses[3];
    DallasTemperature* _ds18b20Sensors[3];

    // Sensor configurations
    HTSensorConfig _htSensorConfig[3];

    // RTC object
    RTC_DS3231 _rtc;
    bool _rtcInitialized;

    // EEPROM address for sensor configurations
    static const int HT_CONFIG_ADDR = 3900;

    // Initialize a sensor based on its configuration
    void initializeSensor(int htIndex);
};

#endif // SENSOR_MANAGER_H