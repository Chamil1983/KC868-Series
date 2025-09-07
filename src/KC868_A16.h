/**
 * KC868_A16.h - Main header file for KC868-A16 Smart Home Controller
 * Created by Your Name, Date
 * Released into the public domain.
 */

#ifndef KC868_A16_H
#define KC868_A16_H

#include <Arduino.h>
#include "HardwareManager.h"
#include "NetworkManager.h" // Will be included as KC868NetworkManager
#include "WebServerManager.h"
#include "ScheduleManager.h"
#include "SensorManager.h"
#include "ConfigManager.h"
#include "CommManager.h"
#include "InterruptManager.h"
#include "Utilities.h"

class KC868_A16 {
public:
    KC868_A16();

    // Initialize the controller
    void begin();

    // Main loop function - call this in loop()
    void loop();

    // Access to managers
    HardwareManager* hardware() { return &_hardwareManager; }
    KC868NetworkManager* network() { return &_networkManager; }
    WebServerManager* server() { return &_webServerManager; }
    ScheduleManager* scheduler() { return &_scheduleManager; }
    SensorManager* sensors() { return &_sensorManager; }
    ConfigManager* config() { return &_configManager; }
    CommManager* comm() { return &_commManager; }
    // Renamed to avoid conflict with Arduino's interrupts() macro
    InterruptManager* interruptManager() { return &_interruptManager; }

    // Firmware version
    String getFirmwareVersion() { return FIRMWARE_VERSION; }

    // Restart management
    bool isRestartRequired() { return _restartRequired; }
    void setRestartRequired(bool required) { _restartRequired = required; }

private:
    // Manager instances
    HardwareManager _hardwareManager;
    KC868NetworkManager _networkManager; // Updated to use the renamed class
    SensorManager _sensorManager;
    ConfigManager _configManager;
    CommManager _commManager;
    ScheduleManager _scheduleManager; // Moved after its dependencies
    InterruptManager _interruptManager;
    WebServerManager _webServerManager; // Moved after all dependencies

    // Timer variables for periodic operations
    unsigned long _lastWebSocketUpdate;
    unsigned long _lastInputsCheck;
    unsigned long _lastAnalogCheck;
    unsigned long _lastSensorCheck;
    unsigned long _lastNetworkCheck;
    unsigned long _lastTimeCheck;
    unsigned long _lastSystemUptime;

    // Firmware version
    static const String FIRMWARE_VERSION;

    // Flag for device restart
    bool _restartRequired;
};

#endif // KC868_A16_H