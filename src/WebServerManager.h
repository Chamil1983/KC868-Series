/**
 * WebServerManager.h - Web server and WebSocket management for KC868-A16
 * Created by Your Name, Date
 * Released into the public domain.
 */

#ifndef WEBSERVER_MANAGER_H
#define WEBSERVER_MANAGER_H

#include <Arduino.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <SPIFFS.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <ArduinoJson.h>
#include "HardwareManager.h"
#include "NetworkManager.h"
#include "SensorManager.h"
#include "ScheduleManager.h"
#include "ConfigManager.h"
#include "CommManager.h"
#include "InterruptManager.h"

 // Forward declarations
class HardwareManager;
class KC868NetworkManager;
class SensorManager;
class ScheduleManager;
class ConfigManager;
class CommManager;
class InterruptManager;
class KC868_A16;  // Added forward declaration for KC868_A16

class WebServerManager {
public:
    WebServerManager(HardwareManager& hardwareManager, KC868NetworkManager& networkManager,
        SensorManager& sensorManager, ScheduleManager& scheduleManager,
        ConfigManager& configManager, CommManager& commManager,
        InterruptManager& interruptManager);

    // Initialize file system
    bool initFileSystem();

    // Initialize WebSocket server
    void beginWebSocketServer();

    // Setup web server endpoints
    void setupWebServer();

    // Handle web server clients
    void handleClients();

    // Handle WebSocket events
    void handleWebSocketEvents();

    // Broadcast update to all WebSocket clients
    void broadcastUpdate();

    // Get uptime string
    String getUptimeString();

    // Get active protocol name
    String getActiveProtocolName();

    // WebSocket event handler
    void webSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
    void handleHTSensors();
    void handleUpdateHTSensor();

private:
    // References to other managers
    HardwareManager& _hardwareManager;
    KC868NetworkManager& _networkManager; // Updated to renamed class
    SensorManager& _sensorManager;
    ScheduleManager& _scheduleManager;
    ConfigManager& _configManager;
    CommManager& _commManager;
    InterruptManager& _interruptManager;

    // Web server
    WebServer _server;

    // WebSocket server
    WebSocketsServer _webSocket;

    // WebSocket client status
    bool _webSocketClients[WEBSOCKETS_SERVER_CLIENT_MAX];

    // File upload
    File _fsUploadFile;

    // API endpoint handlers
    void handleWebRoot();
    void handleNotFound();
    void handleFileUpload();
    void handleRelayControl();
    void handleSystemStatus();
    void handleSchedules();
    void handleUpdateSchedule();
    void handleEvaluateInputSchedules();
    void handleAnalogTriggers();
    void handleUpdateAnalogTriggers();
    void handleConfig();
    void handleUpdateConfig();
    void handleDebug();
    void handleDebugCommand();
    void handleReboot();
    void handleCommunicationStatus();
    void handleSetCommunication();
    void handleCommunicationConfig();
    void handleUpdateCommunicationConfig();
    void handleGetTime();
    void handleSetTime();
    void handleI2CScan();
    void handleInterrupts();
    void handleUpdateInterrupts();
    void handleNetworkSettings();
    void handleUpdateNetworkSettings();

    // Process command received via WebSocket or API
    String processCommand(String command);

    // Toast notification (send message to UI)
    void sendToastNotification(String message, String type = "info");
};

#endif // WEBSERVER_MANAGER_H