/**
 * WebServerManager.cpp - Web server and WebSocket management for KC868-A16
 * Created by Your Name, Date
 * Released into the public domain.
 */

#include "WebServerManager.h"
#include "GlobalConstants.h"

WebServerManager::WebServerManager(HardwareManager& hardwareManager, KC868NetworkManager& networkManager,
    SensorManager& sensorManager, ScheduleManager& scheduleManager,
    ConfigManager& configManager, CommManager& commManager,
    InterruptManager& interruptManager) :
    _hardwareManager(hardwareManager),
    _networkManager(networkManager),
    _sensorManager(sensorManager),
    _scheduleManager(scheduleManager),
    _configManager(configManager),
    _commManager(commManager),
    _interruptManager(interruptManager),
    _server(80),
    _webSocket(81)
{
    // Initialize WebSocket client array
    for (int i = 0; i < WEBSOCKETS_SERVER_CLIENT_MAX; i++) {
        _webSocketClients[i] = false;
    }
}

bool WebServerManager::initFileSystem() {
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS mount failed");
        return false;
    }
    Serial.println("SPIFFS mounted successfully");
    return true;
}

void WebServerManager::beginWebSocketServer() {
    _webSocket.begin();
    _webSocket.onEvent([this](uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
        this->webSocketEvent(num, type, payload, length);
        });
    Serial.println("WebSocket server started");
}

void WebServerManager::setupWebServer() {
    // Serve static files from SPIFFS
    _server.serveStatic("/", SPIFFS, "/index.html");
    _server.serveStatic("/style.css", SPIFFS, "/style.css");
    _server.serveStatic("/script.js", SPIFFS, "/script.js");

    // API endpoints
    _server.on("/", HTTP_GET, [this]() { this->handleWebRoot(); });
    _server.on("/api/status", HTTP_GET, [this]() { this->handleSystemStatus(); });
    _server.on("/api/relay", HTTP_POST, [this]() { this->handleRelayControl(); });
    _server.on("/api/schedules", HTTP_GET, [this]() { this->handleSchedules(); });
    _server.on("/api/schedules", HTTP_POST, [this]() { this->handleUpdateSchedule(); });
    _server.on("/api/evaluate-input-schedules", HTTP_GET, [this]() { this->handleEvaluateInputSchedules(); });
    _server.on("/api/analog-triggers", HTTP_GET, [this]() { this->handleAnalogTriggers(); });
    _server.on("/api/analog-triggers", HTTP_POST, [this]() { this->handleUpdateAnalogTriggers(); });
    _server.on("/api/ht-sensors", HTTP_GET, [this]() { this->handleHTSensors(); });
    _server.on("/api/ht-sensors", HTTP_POST, [this]() { this->handleUpdateHTSensor(); });
    _server.on("/api/config", HTTP_GET, [this]() { this->handleConfig(); });
    _server.on("/api/config", HTTP_POST, [this]() { this->handleUpdateConfig(); });
    _server.on("/api/debug", HTTP_GET, [this]() { this->handleDebug(); });
    _server.on("/api/debug", HTTP_POST, [this]() { this->handleDebugCommand(); });
    _server.on("/api/reboot", HTTP_POST, [this]() { this->handleReboot(); });

    // Communication endpoints
    _server.on("/api/communication", HTTP_GET, [this]() { this->handleCommunicationStatus(); });
    _server.on("/api/communication", HTTP_POST, [this]() { this->handleSetCommunication(); });
    _server.on("/api/communication/config", HTTP_GET, [this]() { this->handleCommunicationConfig(); });
    _server.on("/api/communication/config", HTTP_POST, [this]() { this->handleUpdateCommunicationConfig(); });

    // Time endpoints
    _server.on("/api/time", HTTP_GET, [this]() { this->handleGetTime(); });
    _server.on("/api/time", HTTP_POST, [this]() { this->handleSetTime(); });

    // Diagnostic endpoints
    _server.on("/api/i2c/scan", HTTP_GET, [this]() { this->handleI2CScan(); });

    // Interrupt configuration endpoint
    _server.on("/api/interrupts", HTTP_GET, [this]() { this->handleInterrupts(); });
    _server.on("/api/interrupts", HTTP_POST, [this]() { this->handleUpdateInterrupts(); });

    // Network settings endpoint
    _server.on("/api/network", HTTP_GET, [this]() { this->handleNetworkSettings(); });
    _server.on("/api/network", HTTP_POST, [this]() { this->handleUpdateNetworkSettings(); });

    // File upload handler
    _server.on("/api/upload", HTTP_POST,
        [this]() { _server.send(200, "text/plain", "File upload complete"); },
        [this]() { this->handleFileUpload(); }
    );

    // Not found handler
    _server.onNotFound([this]() { this->handleNotFound(); });

    // Start server
    _server.begin();
    Serial.println("Web server started");
}

void WebServerManager::handleClients() {
    _server.handleClient();
}

void WebServerManager::handleWebSocketEvents() {
    _webSocket.loop();
}

void WebServerManager::webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
    case WStype_DISCONNECTED:
        Serial.printf("[WebSocket] #%u disconnected\n", num);
        _webSocketClients[num] = false;
        break;
    case WStype_CONNECTED:
    {
        IPAddress ip = _webSocket.remoteIP(num);
        Serial.printf("[WebSocket] #%u connected from %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);

        // Mark client as subscribed
        _webSocketClients[num] = true;

        // Send initial status update
        DynamicJsonDocument doc(1024);
        doc["type"] = "status";
        doc["connected"] = true;

        String message;
        serializeJson(doc, message);
        _webSocket.sendTXT(num, message);

        // Send current state of all relays and inputs
        broadcastUpdate();
    }
    break;
    case WStype_TEXT:
    {
        String text = String((char*)payload);
        Serial.printf("[WebSocket] #%u received: %s\n", num, text.c_str());

        // Process WebSocket command
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, text);

        if (!error) {
            String cmd = doc["command"];

            if (cmd == "subscribe") {
                // Subscribe to real-time updates
                _webSocketClients[num] = true;
                Serial.println("Client subscribed to updates");
            }
            else if (cmd == "unsubscribe") {
                // Unsubscribe from updates
                _webSocketClients[num] = false;
                Serial.println("Client unsubscribed from updates");
            }
            else if (cmd == "toggle_relay") {
                // Toggle relay command
                int relay = doc["relay"];
                bool state = doc["state"];

                Serial.printf("WebSocket: Toggling relay %d to %s\n", relay, state ? "ON" : "OFF");

                if (relay >= 0 && relay < 16) {
                    _hardwareManager.setOutputState(relay, state);

                    if (_hardwareManager.writeOutputs()) {
                        Serial.println("Relay toggled successfully via WebSocket");

                        // Send response
                        DynamicJsonDocument responseDoc(512);
                        responseDoc["type"] = "relay_update";
                        responseDoc["relay"] = relay;
                        responseDoc["state"] = _hardwareManager.getOutputState(relay);

                        String response;
                        serializeJson(responseDoc, response);
                        _webSocket.sendTXT(num, response);

                        // Broadcast update to all subscribed clients
                        broadcastUpdate();
                    }
                    else {
                        // Send error response
                        DynamicJsonDocument errorDoc(512);
                        errorDoc["type"] = "error";
                        errorDoc["message"] = "Failed to write to relay";

                        String errorResponse;
                        serializeJson(errorDoc, errorResponse);
                        _webSocket.sendTXT(num, errorResponse);

                        Serial.println("ERROR: Failed to toggle relay via WebSocket");
                    }
                }
                else {
                    Serial.printf("ERROR: Invalid relay index: %d\n", relay);
                }
            }
            else if (cmd == "get_protocol_config") {
                // Get protocol-specific configuration
                String protocol = doc["protocol"];

                DynamicJsonDocument responseDoc(1024);
                responseDoc["type"] = "protocol_config";
                responseDoc["protocol"] = protocol;

                // Add protocol-specific configuration from the appropriate manager
                _commManager.getProtocolConfig(protocol, responseDoc);

                String response;
                serializeJson(responseDoc, response);
                _webSocket.sendTXT(num, response);
            }
        }
        else {
            Serial.println("ERROR: Invalid JSON in WebSocket message");
        }
    }
    break;
    }
}

void WebServerManager::broadcastUpdate() {
    DynamicJsonDocument doc(4096);
    doc["type"] = "status_update";
    doc["time"] = _sensorManager.getTimeString();
    doc["timestamp"] = millis(); // Add timestamp for freshness checking

    // Add output states
    JsonArray outputs = doc.createNestedArray("outputs");
    for (int i = 0; i < 16; i++) {
        JsonObject output = outputs.createNestedObject();
        output["id"] = i;
        output["state"] = _hardwareManager.getOutputState(i);
    }

    // Add input states
    JsonArray inputs = doc.createNestedArray("inputs");
    for (int i = 0; i < 16; i++) {
        JsonObject input = inputs.createNestedObject();
        input["id"] = i;
        input["state"] = _hardwareManager.getInputState(i);
    }

    // Add direct input states (HT1-HT3)
    JsonArray directInputs = doc.createNestedArray("direct_inputs");
    for (int i = 0; i < 3; i++) {
        JsonObject input = directInputs.createNestedObject();
        input["id"] = i;
        input["state"] = _hardwareManager.getDirectInputState(i);
    }

    // Add HT sensors data
    JsonArray htSensors = doc.createNestedArray("htSensors");
    for (int i = 0; i < 3; i++) {
        HTSensorConfig* config = _sensorManager.getSensorConfig(i);
        if (config) {
            JsonObject sensor = htSensors.createNestedObject();
            sensor["index"] = i;
            sensor["pin"] = "HT" + String(i + 1);
            sensor["sensorType"] = config->sensorType;

            const char* sensorTypeNames[] = {
                "Digital Input", "DHT11", "DHT22", "DS18B20"
            };
            sensor["sensorTypeName"] = sensorTypeNames[config->sensorType];

            switch (config->sensorType) {
            case SENSOR_TYPE_DIGITAL:
                sensor["value"] = _hardwareManager.getDirectInputState(i) ? "HIGH" : "LOW";
                break;

            case SENSOR_TYPE_DHT11:
            case SENSOR_TYPE_DHT22:
                sensor["temperature"] = config->temperature;
                sensor["humidity"] = config->humidity;
                break;

            case SENSOR_TYPE_DS18B20:
                sensor["temperature"] = config->temperature;
                break;
            }
        }
    }

    // Add analog inputs
    JsonArray analog = doc.createNestedArray("analog");
    for (int i = 0; i < 4; i++) {
        JsonObject analogInput = analog.createNestedObject();
        analogInput["id"] = i;
        analogInput["value"] = _hardwareManager.getAnalogValue(i);
        analogInput["voltage"] = _hardwareManager.getAnalogVoltage(i);
        analogInput["percentage"] = _hardwareManager.calculatePercentage(_hardwareManager.getAnalogVoltage(i));
    }

    // Add system information
    doc["device"] = _configManager.getDeviceName();
    doc["wifi_connected"] = _networkManager.isWiFiConnected();
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["wifi_ip"] = _networkManager.isWiFiConnected() ? WiFi.localIP().toString() : "0.0.0.0";
    doc["eth_connected"] = _networkManager.isEthernetConnected();
    doc["eth_ip"] = _networkManager.isEthernetConnected() ? ETH.localIP().toString() : "0.0.0.0";
    doc["mac"] = _networkManager.getMACAddress();
    doc["uptime"] = getUptimeString();
    doc["active_protocol"] = getActiveProtocolName();
    doc["firmware_version"] = FIRMWARE_VERSION; // Use directly without scope
    doc["i2c_errors"] = _hardwareManager.getI2CErrorCount();
    doc["free_heap"] = ESP.getFreeHeap();
    doc["cpu_freq"] = ESP.getCpuFreqMHz();
    doc["last_error"] = _hardwareManager.getLastErrorMessage();

    String jsonString;
    serializeJson(doc, jsonString);

    // Send to all WebSocket clients
    _webSocket.broadcastTXT(jsonString);
}

String WebServerManager::getUptimeString() {
    unsigned long uptime = millis() / 1000; // Convert to seconds

    unsigned long days = uptime / 86400;
    uptime %= 86400;

    unsigned long hours = uptime / 3600;
    uptime %= 3600;

    unsigned long minutes = uptime / 60;
    unsigned long seconds = uptime % 60;

    char uptimeStr[30];
    if (days > 0) {
        sprintf(uptimeStr, "%ld days, %02ld:%02ld:%02ld", days, hours, minutes, seconds);
    }
    else {
        sprintf(uptimeStr, "%02ld:%02ld:%02ld", hours, minutes, seconds);
    }

    return String(uptimeStr);
}

String WebServerManager::getActiveProtocolName() {
    String protocolName = _commManager.getActiveProtocol();

    if (protocolName == "wifi") {
        return "WiFi";
    }
    else if (protocolName == "ethernet") {
        return "Ethernet";
    }
    else if (protocolName == "rs485") {
        return "RS-485";
    }
    else if (protocolName == "usb") {
        return "USB";
    }

    // Return as-is if unknown
    return protocolName;
}

void WebServerManager::handleWebRoot() {
    _server.sendHeader("Location", "/index.html", true);
    _server.send(302, "text/plain", "");
}

void WebServerManager::handleNotFound() {
    // If in captive portal mode and request is for a domain, redirect to configuration page
    if (_networkManager.isAPMode() && !_server.hostHeader().startsWith("192.168.")) {
        _server.sendHeader("Location", "/", true);
        _server.send(302, "text/plain", "");
        return;
    }

    String message = "File Not Found\n\n";
    message += "URI: ";
    message += _server.uri();
    message += "\nMethod: ";
    message += (_server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += _server.args();
    message += "\n";

    for (uint8_t i = 0; i < _server.args(); i++) {
        message += " " + _server.argName(i) + ": " + _server.arg(i) + "\n";
    }

    _server.send(404, "text/plain", message);
}

void WebServerManager::handleFileUpload() {
    HTTPUpload& upload = _server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        String filename = upload.filename;
        if (!filename.startsWith("/")) {
            filename = "/" + filename;
        }
        Serial.printf("File upload start: %s\n", filename.c_str());
        _fsUploadFile = SPIFFS.open(filename, FILE_WRITE);
    }
    else if (upload.status == UPLOAD_FILE_WRITE) {
        if (_fsUploadFile) {
            _fsUploadFile.write(upload.buf, upload.currentSize);
        }
    }
    else if (upload.status == UPLOAD_FILE_END) {
        if (_fsUploadFile) {
            _fsUploadFile.close();
            Serial.printf("File upload complete: %u bytes\n", upload.totalSize);
        }
    }
}

void WebServerManager::handleRelayControl() {
    String response = "{\"status\":\"error\",\"message\":\"Invalid request\"}";

    if (_server.hasArg("plain")) {
        String body = _server.arg("plain");
        Serial.printf("Relay control request body: %s\n", body.c_str());

        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, body);

        if (!error) {
            if (doc.containsKey("relay") && doc.containsKey("state")) {
                int relay = doc["relay"];
                bool state = doc["state"];

                Serial.printf("Request to set relay %d to %s\n", relay, state ? "ON" : "OFF");

                if (relay >= 0 && relay < 16) {
                    _hardwareManager.setOutputState(relay, state);
                    if (_hardwareManager.writeOutputs()) {
                        Serial.println("Relay control successful");
                        response = "{\"status\":\"success\",\"relay\":" + String(relay) +
                            ",\"state\":" + String(state ? "true" : "false") + "}";

                        // Broadcast update
                        broadcastUpdate();
                    }
                    else {
                        Serial.println("Failed to write to relay");
                        response = "{\"status\":\"error\",\"message\":\"Failed to write to relay\"}";
                    }
                }
                else if (relay == 99) {  // Special case for all relays
                    Serial.printf("Setting all relays to %s\n", state ? "ON" : "OFF");

                    _hardwareManager.setAllOutputs(state);
                    if (_hardwareManager.writeOutputs()) {
                        response = "{\"status\":\"success\",\"relay\":\"all\",\"state\":" +
                            String(state ? "true" : "false") + "}";

                        // Broadcast update
                        broadcastUpdate();
                    }
                    else {
                        Serial.println("Failed to write to relays");
                        response = "{\"status\":\"error\",\"message\":\"Failed to write to relays\"}";
                    }
                }
                else {
                    Serial.printf("Invalid relay number: %d\n", relay);
                }
            }
            else {
                Serial.println("Missing relay or state in request");
            }
        }
        else {
            Serial.printf("Invalid JSON in request: %s\n", error.c_str());
        }
    }
    else {
        Serial.println("No plain body in request");
    }

    _server.send(200, "application/json", response);
}

void WebServerManager::handleSystemStatus() {
    DynamicJsonDocument doc(4096);

    // Add output states
    JsonArray outputs = doc.createNestedArray("outputs");
    for (int i = 0; i < 16; i++) {
        JsonObject output = outputs.createNestedObject();
        output["id"] = i;
        output["state"] = _hardwareManager.getOutputState(i);
    }

    // Add input states
    JsonArray inputs = doc.createNestedArray("inputs");
    for (int i = 0; i < 16; i++) {
        JsonObject input = inputs.createNestedObject();
        input["id"] = i;
        input["state"] = _hardwareManager.getInputState(i);
    }

    // Add direct input states (HT1-HT3)
    JsonArray directInputs = doc.createNestedArray("direct_inputs");
    for (int i = 0; i < 3; i++) {
        JsonObject input = directInputs.createNestedObject();
        input["id"] = i;
        input["state"] = _hardwareManager.getDirectInputState(i);
    }

    // Add HT sensors data
    JsonArray htSensorsData = doc.createNestedArray("ht_sensors");
    for (int i = 0; i < 3; i++) {
        JsonObject sensor = htSensorsData.createNestedObject();
        sensor["index"] = i;
        sensor["pin"] = "HT" + String(i + 1);
        sensor["type"] = _sensorManager.getSensorType(i);

        switch (_sensorManager.getSensorType(i)) {
        case 0: // Digital
            sensor["value"] = _hardwareManager.getDirectInputState(i) ? "HIGH" : "LOW";
            sensor["name"] = "Digital Input";
            break;

        case 1: // DHT11
            sensor["temperature"] = _sensorManager.getTemperature(i);
            sensor["humidity"] = _sensorManager.getHumidity(i);
            sensor["name"] = "DHT11";
            break;

        case 2: // DHT22
            sensor["temperature"] = _sensorManager.getTemperature(i);
            sensor["humidity"] = _sensorManager.getHumidity(i);
            sensor["name"] = "DHT22";
            break;

        case 3: // DS18B20
            sensor["temperature"] = _sensorManager.getTemperature(i);
            sensor["name"] = "DS18B20";
            break;
        }
    }

    // Add analog inputs
    JsonArray analog = doc.createNestedArray("analog");
    for (int i = 0; i < 4; i++) {
        JsonObject analogInput = analog.createNestedObject();
        analogInput["id"] = i;
        analogInput["value"] = _hardwareManager.getAnalogValue(i);
        analogInput["voltage"] = _hardwareManager.getAnalogVoltage(i);
        analogInput["percentage"] = _hardwareManager.calculatePercentage(_hardwareManager.getAnalogVoltage(i));
    }

    // Add system information
    doc["device"] = _configManager.getDeviceName();
    doc["wifi_connected"] = _networkManager.isWiFiConnected();
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["wifi_ip"] = _networkManager.getIPAddress();
    doc["eth_connected"] = _networkManager.isEthernetConnected();
    doc["eth_ip"] = _networkManager.getIPAddress();
    doc["mac"] = _networkManager.getMACAddress();
    doc["uptime"] = getUptimeString();
    doc["active_protocol"] = getActiveProtocolName();
    doc["firmware_version"] = FIRMWARE_VERSION; // Use directly
    doc["i2c_errors"] = _hardwareManager.getI2CErrorCount();
    doc["free_heap"] = ESP.getFreeHeap();
    doc["cpu_freq"] = ESP.getCpuFreqMHz();
    doc["last_error"] = _hardwareManager.getLastErrorMessage();

    // Add network status to display on dashboard
    doc["rtc_initialized"] = _sensorManager.isRTCInitialized();

    String jsonResponse;
    serializeJson(doc, jsonResponse);

    _server.send(200, "application/json", jsonResponse);
}

// Include stub implementations for the missing functions
#include "WebServerManager.cpp.h"

String WebServerManager::processCommand(String command) {
    return _commManager.processCommand(command);
}

void WebServerManager::sendToastNotification(String message, String type) {
    DynamicJsonDocument doc(512);
    doc["type"] = "toast";
    doc["message"] = message;
    doc["toastType"] = type;

    String jsonString;
    serializeJson(doc, jsonString);

    // Send to all WebSocket clients
    _webSocket.broadcastTXT(jsonString);
}

// Handle HT sensors API - Get sensor data
void WebServerManager::handleHTSensors() {
    DynamicJsonDocument doc(1024);
    JsonArray sensorsArray = doc.createNestedArray("htSensors");

    const char* sensorTypeNames[] = {
        "Digital Input", "DHT11", "DHT22", "DS18B20"
    };

    for (int i = 0; i < 3; i++) {
        HTSensorConfig* config = _sensorManager.getSensorConfig(i);
        if (!config) continue;

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

    String response;
    serializeJson(doc, response);
    _server.send(200, "application/json", response);
}


// Handle update HT sensor configuration
void WebServerManager::handleUpdateHTSensor() {
    String response = "{\"status\":\"error\",\"message\":\"Invalid request\"}";

    if (_server.hasArg("plain")) {
        String body = _server.arg("plain");
        Serial.println("Received HT sensor update: " + body);

        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, body);

        if (!error && doc.containsKey("sensor")) {
            JsonObject sensorJson = doc["sensor"];

            if (sensorJson.containsKey("index") && sensorJson.containsKey("sensorType")) {
                int index = sensorJson["index"];
                int sensorType = sensorJson["sensorType"];

                Serial.println("Updating HT sensor " + String(index) + " to type " + String(sensorType));

                if (index >= 0 && index < 3 &&
                    sensorType >= 0 && sensorType <= SENSOR_TYPE_DS18B20) {

                    // Update sensor configuration
                    if (_sensorManager.updateSensorConfig(index, sensorType)) {
                        response = "{\"status\":\"success\",\"message\":\"Sensor configuration updated\"}";
                    }
                    else {
                        response = "{\"status\":\"success\",\"message\":\"No changes needed\"}";
                    }
                }
            }
        }
    }

    _server.send(200, "application/json", response);
}