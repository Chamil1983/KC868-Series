/**
 * KC868_A16.cpp - Main implementation file for KC868-A16 Smart Home Controller
 * Created by Your Name, Date
 * Released into the public domain.
 */

#include "KC868_A16.h"
#include "GlobalConstants.h"

 // Initialize class-specific constant by referencing the global one
const String KC868_A16::FIRMWARE_VERSION = ::FIRMWARE_VERSION;

KC868_A16::KC868_A16() :
    _hardwareManager(),
    _networkManager(),
    _sensorManager(),
    _configManager(),
    _commManager(),
    _scheduleManager(_hardwareManager, _sensorManager),
    _interruptManager(_hardwareManager, _scheduleManager),
    _webServerManager(_hardwareManager, _networkManager, _sensorManager, _scheduleManager, _configManager, _commManager, _interruptManager),
    _lastWebSocketUpdate(0),
    _lastInputsCheck(0),
    _lastAnalogCheck(0),
    _lastSensorCheck(0),
    _lastNetworkCheck(0),
    _lastTimeCheck(0),
    _lastSystemUptime(0),
    _restartRequired(false)
{
    // Constructor - initialization lists used above
}

void KC868_A16::begin() {
    Serial.begin(115200);
    Serial.println("\nKC868-A16 Controller starting up...");
    Serial.println("Firmware Version: " + FIRMWARE_VERSION);

    // Increase the CPU frequency to improve performance
    setCpuFrequencyMhz(240);

    // Initialize EEPROM and load configuration
    _configManager.begin();

    // Initialize file system
    _webServerManager.initFileSystem();

    // Initialize hardware
    _hardwareManager.begin();

    // Initialize HT sensors
    _sensorManager.begin();

    // Initialize RTC
    _sensorManager.initRTC();

    // Reset Ethernet controller
    _networkManager.resetEthernet();

    // Initialize Ethernet first
    _networkManager.initEthernet();

    // Initialize WiFi only if Ethernet is not connected
    if (!_networkManager.isEthernetConnected()) {
        _networkManager.initWiFi();
    }

    // Initialize communication protocols
    _commManager.begin();

    // Start DNS server for captive portal if in AP mode
    if (_networkManager.isAPMode()) {
        _networkManager.startDNSServer();
    }

    // Initialize WebSocket server
    _webServerManager.beginWebSocketServer();

    // Setup web server endpoints
    _webServerManager.setupWebServer();

    // Initialize output states (All relays OFF)
    _hardwareManager.writeOutputs();

    // Read initial input states
    _hardwareManager.readInputs();

    // Read initial analog values
    _hardwareManager.readAllAnalogInputs();

    // Initialize interrupts
    _interruptManager.begin();

    // Print status info
    _hardwareManager.printIOStates();

    Serial.println("KC868-A16 Controller initialization complete");

    // Print network status
    _networkManager.printNetworkStatus();
}

void KC868_A16::loop() {
    // Handle DNS requests for captive portal if in AP mode
    if (_networkManager.isAPMode()) {
        _networkManager.processDNSRequests();
    }

    // Handle web server clients
    _webServerManager.handleClients();

    // Handle WebSocket events
    _webServerManager.handleWebSocketEvents();

    unsigned long currentMillis = millis();

    // Process any input interrupts with priorities
    _interruptManager.processInputInterrupts();

    // Poll any non-interrupt inputs
    _interruptManager.pollNonInterruptInputs();

    // Read digital inputs more frequently if interrupts are not enabled
    if (!_interruptManager.areInterruptsEnabled() && (currentMillis - _lastInputsCheck >= 100)) {
        _lastInputsCheck = currentMillis;
        bool inputsChanged = _hardwareManager.readInputs();

        // If inputs changed, broadcast immediately
        if (inputsChanged) {
            _webServerManager.broadcastUpdate();
            _lastWebSocketUpdate = currentMillis;
        }
    }

    // Read HT sensors periodically
    if (currentMillis - _lastSensorCheck >= 1000) { // Check sensors every second
        _lastSensorCheck = currentMillis;
        _sensorManager.readAllSensors();
    }

    // Read analog inputs more frequently for better responsiveness
    if (currentMillis - _lastAnalogCheck >= 100) {
        _lastAnalogCheck = currentMillis;
        bool analogChanged = _hardwareManager.readAllAnalogInputs();

        // If analog values changed significantly, check triggers
        if (analogChanged) {
            _scheduleManager.checkAnalogTriggers();

            // Broadcast immediately if analog values changed
            _webServerManager.broadcastUpdate();
            _lastWebSocketUpdate = currentMillis;
        }
    }

    // Periodically check network status
    if (currentMillis - _lastNetworkCheck >= 5000) {
        _lastNetworkCheck = currentMillis;
        _networkManager.checkNetworkStatus();
    }

    // Broadcast periodic updates even if no changes
    if (currentMillis - _lastWebSocketUpdate >= 1000) {
        _webServerManager.broadcastUpdate();
        _lastWebSocketUpdate = currentMillis;
    }

    // Process commands based on active communication protocol
    _commManager.processCommands();

    // Check schedules every second
    if (currentMillis - _lastTimeCheck >= 1000) {
        _lastTimeCheck = currentMillis;
        _scheduleManager.checkSchedules();
    }

    // Update system uptime (for diagnostics)
    if (currentMillis - _lastSystemUptime >= 60000) {
        _lastSystemUptime = currentMillis;
        Serial.println("System uptime: " + String(millis() / 60000) + " minutes");

        // Print free heap for monitoring memory usage
        Serial.println("Free heap: " + String(ESP.getFreeHeap()) + " bytes");
        Serial.println("Min free heap: " + String(ESP.getMinFreeHeap()) + " bytes");
        Serial.println("Max alloc heap: " + String(ESP.getMaxAllocHeap()) + " bytes");
    }

    // Check for restart if required
    if (_restartRequired) {
        Serial.println("Restart required, rebooting...");
        delay(1000); // Allow time for any pending operations to complete
        ESP.restart();
    }
}