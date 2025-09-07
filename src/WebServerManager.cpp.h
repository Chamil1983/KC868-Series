/**
 * WebServerManager.cpp.h - Implementation for WebServerManager functions
 * Created by Your Name, Date
 * Released into the public domain.
 */

#ifndef WEBSERVER_MANAGER_CPP_H
#define WEBSERVER_MANAGER_CPP_H

#include "WebServerManager.h"

void WebServerManager::handleSchedules() {
    DynamicJsonDocument doc(8192);
    JsonArray schedulesArray = doc.createNestedArray("schedules");

    // Get schedules from scheduler
    _scheduleManager.getSchedulesJson(schedulesArray);

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    _server.send(200, "application/json", jsonResponse);
}

void WebServerManager::handleUpdateSchedule() {
    String response = "{\"status\":\"error\",\"message\":\"Invalid request\"}";

    if (_server.hasArg("plain")) {
        String body = _server.arg("plain");
        DynamicJsonDocument doc(4096);
        DeserializationError error = deserializeJson(doc, body);

        if (!error) {
            // Check if this is a deletion request
            if (doc.containsKey("id") && doc.containsKey("delete") && doc["delete"].as<bool>()) {
                int id = doc["id"];
                TimeSchedule* schedule = _scheduleManager.getSchedule(id);
                if (schedule) {
                    schedule->enabled = false; // Disable the schedule
                    _scheduleManager.saveSchedules();
                    response = "{\"status\":\"success\",\"message\":\"Schedule deleted\"}";
                }
            }
            // Check if this is an enable/disable request
            else if (doc.containsKey("id") && doc.containsKey("enabled")) {
                int id = doc["id"];
                bool enabled = doc["enabled"];
                TimeSchedule* schedule = _scheduleManager.getSchedule(id);
                if (schedule) {
                    schedule->enabled = enabled;
                    _scheduleManager.saveSchedules();
                    response = "{\"status\":\"success\"}";
                }
            }
            // Otherwise, treat it as a full schedule update/creation
            else if (doc.containsKey("schedule")) {
                JsonObject scheduleJson = doc["schedule"];
                if (_scheduleManager.updateSchedule(scheduleJson)) {
                    response = "{\"status\":\"success\"}";
                }
            }
        }
    }

    _server.send(200, "application/json", response);
}

void WebServerManager::handleEvaluateInputSchedules() {
    // Force a check of all input-based schedules
    _scheduleManager.checkInputBasedSchedules();
    _server.send(200, "application/json", "{\"status\":\"success\"}");
}

void WebServerManager::handleAnalogTriggers() {
    DynamicJsonDocument doc(4096);

    // Check if a specific trigger ID was requested
    if (_server.hasArg("id")) {
        int triggerId = _server.arg("id").toInt();
        AnalogTrigger* trigger = _scheduleManager.getAnalogTrigger(triggerId);

        if (trigger) {
            JsonObject triggerJson = doc.createNestedObject("trigger");
            triggerJson["id"] = triggerId;
            triggerJson["enabled"] = trigger->enabled;
            triggerJson["name"] = trigger->name;
            triggerJson["analogInput"] = trigger->analogInput;
            triggerJson["threshold"] = trigger->threshold;
            triggerJson["condition"] = trigger->condition;
            triggerJson["action"] = trigger->action;
            triggerJson["targetType"] = trigger->targetType;
            triggerJson["targetId"] = trigger->targetId;
        }
    }
    else {
        // Return all triggers
        JsonArray triggersArray = doc.createNestedArray("triggers");
        _scheduleManager.getAnalogTriggersJson(triggersArray);
    }

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    _server.send(200, "application/json", jsonResponse);
}

void WebServerManager::handleUpdateAnalogTriggers() {
    String response = "{\"status\":\"error\",\"message\":\"Invalid request\"}";

    if (_server.hasArg("plain")) {
        String body = _server.arg("plain");
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, body);

        if (!error) {
            // Check if this is a deletion request
            if (doc.containsKey("id") && doc.containsKey("delete") && doc["delete"].as<bool>()) {
                int id = doc["id"];
                AnalogTrigger* trigger = _scheduleManager.getAnalogTrigger(id);
                if (trigger) {
                    trigger->enabled = false; // Disable the trigger
                    _scheduleManager.saveAnalogTriggers();
                    response = "{\"status\":\"success\",\"message\":\"Trigger deleted\"}";
                }
            }
            // Check if this is an enable/disable request
            else if (doc.containsKey("id") && doc.containsKey("enabled")) {
                int id = doc["id"];
                bool enabled = doc["enabled"];
                AnalogTrigger* trigger = _scheduleManager.getAnalogTrigger(id);
                if (trigger) {
                    trigger->enabled = enabled;
                    _scheduleManager.saveAnalogTriggers();
                    response = "{\"status\":\"success\"}";
                }
            }
            // Otherwise, treat it as a full trigger update/creation
            else if (doc.containsKey("trigger")) {
                JsonObject triggerJson = doc["trigger"];
                if (_scheduleManager.updateAnalogTrigger(triggerJson)) {
                    response = "{\"status\":\"success\"}";
                }
            }
        }
    }

    _server.send(200, "application/json", response);
}

void WebServerManager::handleHTSensors() {
    DynamicJsonDocument doc(1024);
    JsonArray sensorsArray = doc.createNestedArray("htSensors");

    const char* sensorTypeNames[] = {
        "Digital Input", "DHT11", "DHT22", "DS18B20"
    };

    for (int i = 0; i < 3; i++) {
        HTSensorConfig* config = _sensorManager.getSensorConfig(i);
        if (config) {
            JsonObject sensor = sensorsArray.createNestedObject();
            sensor["index"] = i;
            sensor["pin"] = "HT" + String(i + 1);
            sensor["sensorType"] = config->sensorType;
            sensor["sensorTypeName"] = sensorTypeNames[config->sensorType];

            // Add appropriate readings based on sensor type
            if (config->sensorType == SENSOR_TYPE_DIGITAL) {
                sensor["value"] = _hardwareManager.getDirectInputState(i) ? "HIGH" : "LOW";
            }
            else if (config->sensorType == SENSOR_TYPE_DHT11 ||
                config->sensorType == SENSOR_TYPE_DHT22) {
                sensor["temperature"] = config->temperature;
                sensor["humidity"] = config->humidity;
            }
            else if (config->sensorType == SENSOR_TYPE_DS18B20) {
                sensor["temperature"] = config->temperature;
            }
        }
    }

    String response;
    serializeJson(doc, response);
    _server.send(200, "application/json", response);
}

void WebServerManager::handleUpdateHTSensor() {
    String response = "{\"status\":\"error\",\"message\":\"Invalid request\"}";

    if (_server.hasArg("plain")) {
        String body = _server.arg("plain");
        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, body);

        if (!error && doc.containsKey("sensor")) {
            JsonObject sensorJson = doc["sensor"];

            if (sensorJson.containsKey("index") && sensorJson.containsKey("sensorType")) {
                int index = sensorJson["index"];
                int sensorType = sensorJson["sensorType"];

                if (index >= 0 && index < 3 &&
                    sensorType >= 0 && sensorType <= SENSOR_TYPE_DS18B20) {

                    // Update sensor type in SensorManager
                    if (_sensorManager.updateSensorConfig(index, sensorType)) {
                        response = "{\"status\":\"success\",\"message\":\"Sensor configuration updated\"}";
                    }
                    else {
                        response = "{\"status\":\"error\",\"message\":\"Failed to update sensor configuration\"}";
                    }
                }
            }
        }
    }

    _server.send(200, "application/json", response);
}

void WebServerManager::handleConfig() {
    DynamicJsonDocument doc(1024);

    // Device settings
    doc["device_name"] = _configManager.getDeviceName();
    doc["debug_mode"] = _configManager.isDebugMode();
    doc["dhcp_mode"] = _networkManager.isDHCPMode();
    doc["wifi_ssid"] = _networkManager.getWiFiSSID();

    // Don't send password for security
    doc["wifi_password"] = "";

    // If not using DHCP, include network settings
    if (!_networkManager.isDHCPMode()) {
        doc["ip"] = _networkManager.getIPAddress();
        doc["gateway"] = _networkManager.getGateway();
        doc["subnet"] = _networkManager.getSubnet();
        doc["dns1"] = _networkManager.getDNS1();
        doc["dns2"] = _networkManager.getDNS2();
    }

    // Firmware version
    doc["firmware_version"] = FIRMWARE_VERSION;

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    _server.send(200, "application/json", jsonResponse);
}

void WebServerManager::handleUpdateConfig() {
    String response = "{\"status\":\"error\",\"message\":\"Invalid request\"}";

    if (_server.hasArg("plain")) {
        String body = _server.arg("plain");
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, body);

        if (!error) {
            // Check if this is a reset request
            if (doc.containsKey("reset") && doc["reset"].as<bool>()) {
                _configManager.initializeDefaultConfig();
                _configManager.saveConfiguration();
                response = "{\"status\":\"success\",\"message\":\"Settings reset to default\"}";
            }
            else {
                // Update device settings
                if (doc.containsKey("device_name")) {
                    _configManager.setDeviceName(doc["device_name"].as<String>());
                }

                if (doc.containsKey("debug_mode")) {
                    _configManager.setDebugMode(doc["debug_mode"].as<bool>());
                }

                if (doc.containsKey("dhcp_mode")) {
                    _configManager.setDHCPMode(doc["dhcp_mode"].as<bool>());
                }

                // Update WiFi settings if provided
                bool wifiUpdated = false;
                String ssid = "";
                String password = "";

                if (doc.containsKey("wifi_ssid")) {
                    ssid = doc["wifi_ssid"].as<String>();
                    wifiUpdated = true;
                }

                if (doc.containsKey("wifi_password")) {
                    password = doc["wifi_password"].as<String>();
                    wifiUpdated = true;
                }

                // Update static IP if DHCP is disabled
                if (!_configManager.isDHCPMode()) {
                    if (doc.containsKey("ip") && doc.containsKey("gateway") &&
                        doc.containsKey("subnet") && doc.containsKey("dns1") &&
                        doc.containsKey("dns2")) {

                        _networkManager.setStaticIP(
                            doc["ip"].as<String>(),
                            doc["gateway"].as<String>(),
                            doc["subnet"].as<String>(),
                            doc["dns1"].as<String>(),
                            doc["dns2"].as<String>()
                        );
                    }
                }

                // Save configuration
                _configManager.saveConfiguration();
                response = "{\"status\":\"success\"}";
            }
        }
    }

    _server.send(200, "application/json", response);
}

void WebServerManager::handleDebug() {
    DynamicJsonDocument doc(1024);

    // System information
    doc["cpu_freq"] = ESP.getCpuFreqMHz();
    doc["free_heap"] = ESP.getFreeHeap();
    doc["i2c_errors"] = _hardwareManager.getI2CErrorCount();
    doc["last_error"] = _hardwareManager.getLastErrorMessage();
    doc["firmware_version"] = FIRMWARE_VERSION;

    // Network diagnostics
    doc["internet_connected"] = true; // Placeholder - would need actual check

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    _server.send(200, "application/json", jsonResponse);
}

void WebServerManager::handleDebugCommand() {
    String response = "{\"status\":\"error\",\"message\":\"Invalid request\"}";

    if (_server.hasArg("plain")) {
        String body = _server.arg("plain");
        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, body);

        if (!error && doc.containsKey("command")) {
            String command = doc["command"];
            String commandResponse = processCommand(command);

            DynamicJsonDocument responseDoc(1024);
            responseDoc["status"] = "success";
            responseDoc["response"] = commandResponse;

            String jsonResponse;
            serializeJson(responseDoc, jsonResponse);
            _server.send(200, "application/json", jsonResponse);
            return;
        }
    }

    _server.send(200, "application/json", response);
}

void WebServerManager::handleReboot() {
    _server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Rebooting device\"}");
    delay(500);
    ESP.restart();
}

void WebServerManager::handleCommunicationStatus() {
    DynamicJsonDocument doc(1024);

    // Add active protocol
    doc["active_protocol"] = _commManager.getActiveProtocol();

    // Add USB status (always available in ESP32)
    doc["usb_available"] = true;

    // Add WiFi status
    doc["wifi_connected"] = _networkManager.isWiFiConnected();

    // Add Ethernet status
    doc["eth_connected"] = _networkManager.isEthernetConnected();

    // Add RS485 status (always available in this hardware)
    doc["rs485_available"] = true;

    // Add I2C status
    doc["i2c_error_count"] = _hardwareManager.getI2CErrorCount();

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    _server.send(200, "application/json", jsonResponse);
}

void WebServerManager::handleSetCommunication() {
    String response = "{\"status\":\"error\",\"message\":\"Invalid request\"}";

    if (_server.hasArg("plain")) {
        String body = _server.arg("plain");
        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, body);

        if (!error && doc.containsKey("protocol")) {
            String protocol = doc["protocol"];

            if (protocol == "usb" || protocol == "rs485" ||
                protocol == "wifi" || protocol == "ethernet") {

                _commManager.setActiveProtocol(protocol);
                response = "{\"status\":\"success\"}";
            }
        }
    }

    _server.send(200, "application/json", response);
}

void WebServerManager::handleCommunicationConfig() {
    String response = "{\"status\":\"error\",\"message\":\"Protocol not specified\"}";

    if (_server.hasArg("protocol")) {
        String protocol = _server.arg("protocol");

        DynamicJsonDocument doc(1024);
        doc["protocol"] = protocol;

        // Get protocol-specific configuration
        _commManager.getProtocolConfig(protocol, doc);

        String jsonResponse;
        serializeJson(doc, jsonResponse);
        _server.send(200, "application/json", jsonResponse);
        return;
    }

    _server.send(200, "application/json", response);
}

void WebServerManager::handleUpdateCommunicationConfig() {
    String response = "{\"status\":\"error\",\"message\":\"Invalid request\"}";

    if (_server.hasArg("plain")) {
        String body = _server.arg("plain");
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, body);

        if (!error && doc.containsKey("protocol")) {
            String protocol = doc["protocol"];

            // Extract the protocol-specific configuration
            JsonObject config = doc.as<JsonObject>();

            // Update the configuration
            if (_commManager.updateProtocolConfig(protocol, config)) {
                response = "{\"status\":\"success\"}";
            }
        }
    }

    _server.send(200, "application/json", response);
}

void WebServerManager::handleGetTime() {
    DynamicJsonDocument doc(512);

    // Get current time from RTC or system
    DateTime now = _sensorManager.getCurrentTime();

    // Format for response
    doc["year"] = now.year();
    doc["month"] = now.month();
    doc["day"] = now.day();
    doc["hour"] = now.hour();
    doc["minute"] = now.minute();
    doc["second"] = now.second();
    doc["day_of_week"] = now.dayOfTheWeek();
    doc["formatted"] = _sensorManager.getTimeString();
    doc["rtc_available"] = _sensorManager.isRTCInitialized();

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    _server.send(200, "application/json", jsonResponse);
}

void WebServerManager::handleSetTime() {
    String response = "{\"status\":\"error\",\"message\":\"Invalid request\"}";

    if (_server.hasArg("plain")) {
        String body = _server.arg("plain");
        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, body);

        if (!error) {
            if (doc.containsKey("year") && doc.containsKey("month") && doc.containsKey("day") &&
                doc.containsKey("hour") && doc.containsKey("minute") && doc.containsKey("second")) {

                int year = doc["year"];
                int month = doc["month"];
                int day = doc["day"];
                int hour = doc["hour"];
                int minute = doc["minute"];
                int second = doc["second"];

                // Update system time and RTC if available
                _sensorManager.syncTimeFromClient(year, month, day, hour, minute, second);
                response = "{\"status\":\"success\",\"message\":\"Time updated\"}";
            }
            else if (doc.containsKey("ntp_sync") && doc["ntp_sync"].as<bool>()) {
                // Sync time from NTP
                _sensorManager.syncTimeFromNTP();
                response = "{\"status\":\"success\",\"message\":\"NTP sync initiated\"}";
            }
        }
    }

    _server.send(200, "application/json", response);
}

void WebServerManager::handleI2CScan() {
    DynamicJsonDocument doc(1024);
    JsonArray devices = doc.createNestedArray("devices");

    int deviceCount = 0;

    // Scan I2C bus
    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        uint8_t error = Wire.endTransmission();

        if (error == 0) {
            JsonObject device = devices.createNestedObject();
            device["address"] = "0x" + String(address, HEX);

            // Try to identify common I2C devices
            String name = "Unknown";

            if (address == 0x21) name = "PCF8574 (Inputs 9-16)";
            else if (address == 0x22) name = "PCF8574 (Inputs 1-8)";
            else if (address == 0x24) name = "PCF8574 (Outputs 1-8)";
            else if (address == 0x25) name = "PCF8574 (Outputs 9-16)";
            else if (address == 0x68) name = "DS3231 RTC";
            else if (address == 0x3C || address == 0x3D) name = "OLED Display";
            else if (address == 0x76 || address == 0x77) name = "BMP280/BME280";

            device["name"] = name;
            deviceCount++;
        }
    }

    doc["total_devices"] = deviceCount;

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    _server.send(200, "application/json", jsonResponse);
}

void WebServerManager::handleInterrupts() {
    DynamicJsonDocument doc(4096);
    JsonArray interruptsArray = doc.createNestedArray("interrupts");

    // Get interrupt configurations
    for (int i = 0; i < 16; i++) {
        InterruptConfig* config = _interruptManager.getInterruptConfig(i);
        if (config) {
            JsonObject interrupt = interruptsArray.createNestedObject();
            interrupt["id"] = i;
            interrupt["enabled"] = config->enabled;
            interrupt["name"] = config->name;
            interrupt["priority"] = config->priority;
            interrupt["inputIndex"] = config->inputIndex;
            interrupt["triggerType"] = config->triggerType;
        }
    }

    String response;
    serializeJson(doc, response);
    _server.send(200, "application/json", response);
}

void WebServerManager::handleUpdateInterrupts() {
    String response = "{\"status\":\"error\",\"message\":\"Invalid request\"}";

    if (_server.hasArg("plain")) {
        String body = _server.arg("plain");
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, body);

        if (!error && doc.containsKey("interrupt")) {
            JsonObject interruptJson = doc["interrupt"];

            // Extract interrupt data
            int id = interruptJson.containsKey("id") ? interruptJson["id"].as<int>() : -1;

            if (id >= 0 && id < 16) {
                InterruptConfig config;
                config.enabled = interruptJson["enabled"];
                strlcpy(config.name, interruptJson["name"] | "Input", 32);
                config.priority = interruptJson["priority"] | 2;  // Default medium priority
                config.inputIndex = interruptJson["inputIndex"] | id;
                config.triggerType = interruptJson["triggerType"] | 2;  // Default change trigger

                if (_interruptManager.updateInterruptConfig(id, config)) {
                    response = "{\"status\":\"success\"}";
                }
            }
        }
        else if (!error && doc.containsKey("id") && doc.containsKey("enabled")) {
            // Handle simple enable/disable
            int id = doc["id"].as<int>();
            bool enabled = doc["enabled"];

            if (_interruptManager.enableInterrupt(id, enabled)) {
                response = "{\"status\":\"success\"}";
            }
        }
        else if (!error && doc.containsKey("action")) {
            String action = doc["action"];

            if (action == "enable_all") {
                _interruptManager.enableAllInterrupts(true);
                response = "{\"status\":\"success\",\"message\":\"All interrupts enabled\"}";
            }
            else if (action == "disable_all") {
                _interruptManager.enableAllInterrupts(false);
                response = "{\"status\":\"success\",\"message\":\"All interrupts disabled\"}";
            }
        }
    }

    _server.send(200, "application/json", response);
}

void WebServerManager::handleNetworkSettings() {
    DynamicJsonDocument doc(1024);

    // Get current network settings
    doc["dhcp_mode"] = _networkManager.isDHCPMode();

    // Add network details
    JsonObject networkObject = doc.to<JsonObject>();
    _networkManager.getNetworkInfo(networkObject);

    String jsonResponse;
    serializeJson(doc, jsonResponse);
    _server.send(200, "application/json", jsonResponse);
}

void WebServerManager::handleUpdateNetworkSettings() {
    String response = "{\"status\":\"error\",\"message\":\"Invalid request\"}";

    if (_server.hasArg("plain")) {
        String body = _server.arg("plain");
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, body);

        if (!error) {
            bool requireRestart = false;

            // Handle WiFi settings update
            if (doc.containsKey("wifi_ssid") && doc.containsKey("wifi_password")) {
                String ssid = doc["wifi_ssid"];
                String password = doc["wifi_password"];

                // Save WiFi credentials and flag for restart
                _networkManager.saveWiFiCredentials(ssid, password);
                requireRestart = true;
            }

            // Handle network mode (DHCP vs Static)
            if (doc.containsKey("dhcp_mode")) {
                bool dhcpMode = doc["dhcp_mode"];
                _networkManager.setDHCPMode(dhcpMode);

                // If static IP, handle those settings
                if (!dhcpMode &&
                    doc.containsKey("ip") &&
                    doc.containsKey("gateway") &&
                    doc.containsKey("subnet") &&
                    doc.containsKey("dns1") &&
                    doc.containsKey("dns2")) {

                    _networkManager.setStaticIP(
                        doc["ip"].as<String>(),
                        doc["gateway"].as<String>(),
                        doc["subnet"].as<String>(),
                        doc["dns1"].as<String>(),
                        doc["dns2"].as<String>()
                    );
                }

                // Save network settings
                _networkManager.saveNetworkSettings();
                requireRestart = true;
            }

            if (requireRestart) {
                response = "{\"status\":\"success\",\"restart\":true}";
            }
            else {
                response = "{\"status\":\"success\"}";
            }
        }
    }

    _server.send(200, "application/json", response);
}

#endif // WEBSERVER_MANAGER_CPP_H