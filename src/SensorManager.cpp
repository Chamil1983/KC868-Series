/**
 * SensorManager.cpp - Temperature, humidity and sensor management for KC868-A16
 * Created by Your Name, Date
 * Released into the public domain.
 */

#include "SensorManager.h"
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <time.h>

SensorManager::SensorManager() :
    _rtcInitialized(false)
{
    // Initialize sensor configuration
    for (int i = 0; i < 3; i++) {
        _htSensorConfig[i].sensorType = SENSOR_TYPE_DIGITAL;
        _htSensorConfig[i].temperature = 0;
        _htSensorConfig[i].humidity = 0;
        _htSensorConfig[i].configured = false;
        _htSensorConfig[i].lastReadTime = 0;

        // Initialize sensor pointers to NULL
        _dhtSensors[i] = NULL;
        _oneWireBuses[i] = NULL;
        _ds18b20Sensors[i] = NULL;
    }
}

void SensorManager::begin() {
    // Load sensor configurations from EEPROM
    loadSensorConfigs();

    // Initialize each sensor based on configuration
    for (int i = 0; i < 3; i++) {
        initializeSensor(i);
    }

    Serial.println("Sensor manager initialized");
}

void SensorManager::initRTC() {
    _rtcInitialized = _rtc.begin();
    if (!_rtcInitialized) {
        Serial.println("Couldn't find RTC, using ESP32 internal time");
        // Use NTP for time sync if RTC not available
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");
        syncTimeFromNTP();
    }
    else {
        Serial.println("RTC found");

        if (_rtc.lostPower()) {
            Serial.println("RTC lost power, setting to compile time");
            // Set RTC to compile time if power was lost
            _rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
            syncTimeFromNTP();  // Also try to get more accurate time
        }

        // Print current time from RTC
        DateTime now = _rtc.now();
        String timeStr = String(now.year()) + "-" +
            String(now.month()) + "-" +
            String(now.day()) + " " +
            String(now.hour()) + ":" +
            String(now.minute()) + ":" +
            String(now.second());
        Serial.println("RTC time: " + timeStr);
    }
}

void SensorManager::readAllSensors() {
    // Read all three HT sensors
    for (int i = 0; i < 3; i++) {
        readSensor(i);
    }
}

void SensorManager::readSensor(int htIndex) {
    // Only read at appropriate intervals based on sensor type
    unsigned long currentMillis = millis();

    // Define minimum read intervals for different sensor types (in ms)
    const unsigned long DHT_READ_INTERVAL = 2000;  // DHT sensors should be read max once every 2 seconds
    const unsigned long DS18B20_READ_INTERVAL = 1000;  // DS18B20 every 1 second
    const unsigned long DIGITAL_READ_INTERVAL = 100;  // Digital inputs every 100ms

    unsigned long minInterval;
    switch (_htSensorConfig[htIndex].sensorType) {
    case SENSOR_TYPE_DHT11:
    case SENSOR_TYPE_DHT22:
        minInterval = DHT_READ_INTERVAL;
        break;
    case SENSOR_TYPE_DS18B20:
        minInterval = DS18B20_READ_INTERVAL;
        break;
    case SENSOR_TYPE_DIGITAL:
    default:
        minInterval = DIGITAL_READ_INTERVAL;
        break;
    }

    // Return if it's not time to read yet
    if (currentMillis - _htSensorConfig[htIndex].lastReadTime < minInterval) {
        return;
    }

    // Update last read time
    _htSensorConfig[htIndex].lastReadTime = currentMillis;

    // Read sensor based on type
    switch (_htSensorConfig[htIndex].sensorType) {
    case SENSOR_TYPE_DIGITAL:
        // For digital input, just read the pin value (hardware handles this)
        // _htSensorConfig[htIndex].value would be updated by hardware manager
        break;

    case SENSOR_TYPE_DHT11:
    case SENSOR_TYPE_DHT22:
        if (_dhtSensors[htIndex] != NULL) {
            // Read humidity and temperature
            float newHumidity = _dhtSensors[htIndex]->readHumidity();
            float newTemperature = _dhtSensors[htIndex]->readTemperature();

            // Check if readings are valid
            if (!isnan(newHumidity) && !isnan(newTemperature)) {
                _htSensorConfig[htIndex].humidity = newHumidity;
                _htSensorConfig[htIndex].temperature = newTemperature;
                Serial.println("HT" + String(htIndex + 1) + " DHT: " +
                    String(newTemperature, 1) + "°C, " +
                    String(newHumidity, 1) + "%");
            }
            else {
                Serial.println("HT" + String(htIndex + 1) + " DHT read error");
            }
        }
        break;

    case SENSOR_TYPE_DS18B20:
        if (_ds18b20Sensors[htIndex] != NULL) {
            // Request temperature reading
            _ds18b20Sensors[htIndex]->requestTemperatures();

            // Get temperature in Celsius
            float newTemperature = _ds18b20Sensors[htIndex]->getTempCByIndex(0);

            // Check if reading is valid
            if (newTemperature != DEVICE_DISCONNECTED_C) {
                _htSensorConfig[htIndex].temperature = newTemperature;
                Serial.println("HT" + String(htIndex + 1) + " DS18B20: " +
                    String(newTemperature, 1) + "°C");
            }
            else {
                Serial.println("HT" + String(htIndex + 1) + " DS18B20 read error");
            }
        }
        break;
    }
}

HTSensorConfig* SensorManager::getSensorConfig(int index) {
    if (index >= 0 && index < 3) {
        return &_htSensorConfig[index];
    }
    return NULL;
}

// Add this method to get sensor type - resolves first error
uint8_t SensorManager::getSensorType(int index) {
    if (index >= 0 && index < 3) {
        return _htSensorConfig[index].sensorType;
    }
    return SENSOR_TYPE_DIGITAL;  // Default to digital
}

// Add this method to get temperature - resolves second error
float SensorManager::getTemperature(int index) {
    if (index >= 0 && index < 3) {
        return _htSensorConfig[index].temperature;
    }
    return 0.0f;  // Default to 0
}

// Add this method to get humidity - resolves third error
float SensorManager::getHumidity(int index) {
    if (index >= 0 && index < 3) {
        return _htSensorConfig[index].humidity;
    }
    return 0.0f;  // Default to 0
}

bool SensorManager::updateSensorConfig(int index, uint8_t sensorType) {
    if (index < 0 || index > 2 || sensorType > SENSOR_TYPE_DS18B20) {
        return false;
    }

    // Only update if the type is changing
    if (_htSensorConfig[index].sensorType != sensorType) {
        _htSensorConfig[index].sensorType = sensorType;
        _htSensorConfig[index].temperature = 0;
        _htSensorConfig[index].humidity = 0;
        _htSensorConfig[index].configured = false;
        _htSensorConfig[index].lastReadTime = 0;

        // Initialize with new settings
        initializeSensor(index);

        // Save the configuration
        saveSensorConfigs();

        return true;
    }

    return false;
}

void SensorManager::saveSensorConfigs() {
    DynamicJsonDocument doc(512);
    JsonArray configArray = doc.createNestedArray("htConfig");

    for (int i = 0; i < 3; i++) {
        JsonObject config = configArray.createNestedObject();
        config["sensorType"] = _htSensorConfig[i].sensorType;
    }

    // Serialize to buffer
    char jsonBuffer[512];
    size_t n = serializeJson(doc, jsonBuffer);

    // Store in EEPROM
    for (size_t i = 0; i < n && i < 256; i++) {
        EEPROM.write(HT_CONFIG_ADDR + i, jsonBuffer[i]);
    }

    // Write null terminator
    EEPROM.write(HT_CONFIG_ADDR + n, 0);

    // Commit changes
    EEPROM.commit();

    Serial.println("HT sensor configuration saved");
}

void SensorManager::loadSensorConfigs() {
    // Create a buffer to read JSON data
    char jsonBuffer[512];
    size_t i = 0;

    // Read data until null terminator or max buffer size
    while (i < 511) {
        jsonBuffer[i] = EEPROM.read(HT_CONFIG_ADDR + i);
        if (jsonBuffer[i] == 0) break;
        i++;
    }

    // Add null terminator if buffer is full
    jsonBuffer[i] = 0;

    // If we read something, try to parse it
    if (i > 0) {
        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, jsonBuffer);

        if (!error && doc.containsKey("htConfig")) {
            JsonArray configArray = doc["htConfig"];

            int index = 0;
            for (JsonObject config : configArray) {
                if (index >= 3) break;

                _htSensorConfig[index].sensorType = config["sensorType"] | SENSOR_TYPE_DIGITAL;
                index++;
            }

            Serial.println("HT sensor configuration loaded");
        }
        else {
            Serial.println("No valid HT sensor configuration found, using defaults");
        }
    }
    else {
        Serial.println("No HT sensor configuration found, using defaults");
    }
}

void SensorManager::initializeSensor(int htIndex) {
    // Get the pin for this HT sensor
    int pin = HT_PINS[htIndex];

    // Clean up previous sensor objects if they exist
    if (_dhtSensors[htIndex] != NULL) {
        delete _dhtSensors[htIndex];
        _dhtSensors[htIndex] = NULL;
    }
    if (_oneWireBuses[htIndex] != NULL) {
        delete _oneWireBuses[htIndex];
        _oneWireBuses[htIndex] = NULL;
    }
    if (_ds18b20Sensors[htIndex] != NULL) {
        delete _ds18b20Sensors[htIndex];
        _ds18b20Sensors[htIndex] = NULL;
    }

    // Configure pin based on sensor type
    switch (_htSensorConfig[htIndex].sensorType) {
    case SENSOR_TYPE_DIGITAL:
        pinMode(pin, INPUT_PULLUP);
        break;

    case SENSOR_TYPE_DHT11:
        _dhtSensors[htIndex] = new DHT(pin, DHT11);
        _dhtSensors[htIndex]->begin();
        break;

    case SENSOR_TYPE_DHT22:
        _dhtSensors[htIndex] = new DHT(pin, DHT22);
        _dhtSensors[htIndex]->begin();
        break;

    case SENSOR_TYPE_DS18B20:
        _oneWireBuses[htIndex] = new OneWire(pin);
        _ds18b20Sensors[htIndex] = new DallasTemperature(_oneWireBuses[htIndex]);
        _ds18b20Sensors[htIndex]->begin();
        break;
    }

    _htSensorConfig[htIndex].configured = true;
    _htSensorConfig[htIndex].lastReadTime = 0;

    Serial.println("HT" + String(htIndex + 1) + " sensor initialized as type " +
        String(_htSensorConfig[htIndex].sensorType));
}

DateTime SensorManager::getCurrentTime() {
    if (_rtcInitialized) {
        return _rtc.now();
    }

    // Use ESP32 time if RTC not available
    time_t nowTime;
    struct tm timeinfo;
    time(&nowTime);
    localtime_r(&nowTime, &timeinfo);
    return DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
        timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
}

bool SensorManager::setCurrentTime(int year, int month, int day, int hour, int minute, int second) {
    if (_rtcInitialized) {
        _rtc.adjust(DateTime(year, month, day, hour, minute, second));
        Serial.println("Updated RTC with client time");
        return true;
    }
    else {
        struct tm tm;
        tm.tm_year = year - 1900;
        tm.tm_mon = month - 1;
        tm.tm_mday = day;
        tm.tm_hour = hour;
        tm.tm_min = minute;
        tm.tm_sec = second;
        time_t t = mktime(&tm);
        struct timeval now = { .tv_sec = t };
        settimeofday(&now, NULL);
        Serial.println("Updated system time with client time");
        return true;
    }
}

bool SensorManager::syncTimeFromNTP() {
    Serial.println("Syncing time from NTP...");
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    time_t now = time(nullptr);
    int retry = 0;
    const int maxRetries = 10;

    // Wait for time to be set
    while (now < 24 * 3600 && retry < maxRetries) {
        Serial.println("Waiting for NTP time sync...");
        delay(500);
        now = time(nullptr);
        retry++;
    }

    if (now > 24 * 3600) {
        struct tm timeinfo;
        gmtime_r(&now, &timeinfo);

        Serial.println("NTP time sync successful");

        // If RTC available, update it with NTP time
        if (_rtcInitialized) {
            _rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
            Serial.println("Updated RTC with NTP time");
        }

        return true;
    }
    else {
        Serial.println("NTP time sync failed");
        return false;
    }
}

String SensorManager::getTimeString() {
    DateTime now = getCurrentTime();

    char timeString[30];
    sprintf(timeString, "%04d-%02d-%02d %02d:%02d:%02d",
        now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());

    return String(timeString);
}