/**
 * KC868_A16_Controller.ino
 * Complete control system for Kincony KC868-A16 Smart Home Controller
 * Using Renzo Mischianti's PCF8574 Library
 *
 * Features:
 * - 16 relay outputs via PCF8574 I2C expanders
 * - 16 digital inputs via PCF8574 I2C expanders
 * - 4 analog inputs
 * - 3 direct GPIO inputs (HT1-HT3)
 * - Multiple communication interfaces (USB, WiFi, Ethernet, RS-485)
 * - Web interface for control with visual representation
 * - Scheduling system with RTC
 * - Analog threshold triggering
 * - Comprehensive diagnostics
 * - WebSocket support for real-time updates
 *
 * Hardware:
 * - ESP32 microcontroller
 * - PCF8574 I2C expanders (addresses 0x21, 0x22, 0x24, 0x25)
 * - LAN8720 Ethernet PHY
 * - DS3231 RTC module (optional)
 *
 * Pin mapping from ESPHome configuration and schematic:
 * - I2C: SDA=GPIO4, SCL=GPIO5
 * - RS485: TX=GPIO13, RX=GPIO16
 * - Analog inputs: A1=GPIO36, A2=GPIO34, A3=GPIO35, A4=GPIO39
 * - Direct inputs: HT1=GPIO32, HT2=GPIO33, HT3=GPIO14
 * - RF: RX=GPIO2, TX=GPIO15
 * - Ethernet: MDC=GPIO23, MDIO=GPIO18, CLK=GPIO17
 */

#include <WiFi.h>
#include <ETH.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ESPmDNS.h>
#include <Update.h>
#include <Wire.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <time.h>
#include <FS.h>
#include <SPIFFS.h>
#include <HardwareSerial.h>
#include <RCSwitch.h>
#include <RTClib.h>
#include <DNSServer.h>
#include <PCF8574.h>  // Using Renzo Mischianti's library

 // I2C PCF8574 addresses
#define PCF8574_INPUTS_1_8    0x22
#define PCF8574_INPUTS_9_16   0x21
#define PCF8574_OUTPUTS_1_8   0x24
#define PCF8574_OUTPUTS_9_16  0x25

// GPIO definitions
#define SDA_PIN               4
#define SCL_PIN               5
#define HT1_PIN               32
#define HT2_PIN               33
#define HT3_PIN               14
#define RF_RX_PIN             2
#define RF_TX_PIN             15
#define RS485_TX_PIN          13
#define RS485_RX_PIN          16
#define ANALOG_PIN_1          36
#define ANALOG_PIN_2          34
#define ANALOG_PIN_3          35
#define ANALOG_PIN_4          39

// Ethernet settings
#define ETH_PHY_ADDR          0
#define ETH_PHY_MDC           23
#define ETH_PHY_MDIO          18
#define ETH_PHY_POWER         -1
#define ETH_PHY_TYPE          ETH_PHY_LAN8720
#define ETH_CLK_MODE          ETH_CLOCK_GPIO17_OUT

// EEPROM size and address definitions
#define EEPROM_SIZE           4096
#define EEPROM_WIFI_SSID_ADDR 0
#define EEPROM_WIFI_PASS_ADDR 64
#define EEPROM_CONFIG_ADDR    256
#define EEPROM_COMM_ADDR      384
#define EEPROM_SCHEDULE_ADDR  512
#define EEPROM_TRIGGER_ADDR   2048
#define EEPROM_COMM_CONFIG_ADDR 3072

// Maximum number of schedules and triggers
#define MAX_SCHEDULES         30
#define MAX_ANALOG_TRIGGERS   16

// Default WiFi credentials (can be changed via web interface)
const char* default_ssid = "KC868-A16";
const char* default_password = "12345678";

// Create PCF8574 objects using the Mischianti library
PCF8574 inputIC1(PCF8574_INPUTS_1_8);    // Digital Inputs 1-8
PCF8574 inputIC2(PCF8574_INPUTS_9_16);   // Digital Inputs 9-16
PCF8574 outputIC3(PCF8574_OUTPUTS_9_16); // Digital Outputs 9-16
PCF8574 outputIC4(PCF8574_OUTPUTS_1_8);  // Digital Outputs 1-8

// DNS server
DNSServer dnsServer;

// Web server port
WebServer server(80);

// WebSocket server
WebSocketsServer webSocket = WebSocketsServer(81);
bool webSocketClients[WEBSOCKETS_SERVER_CLIENT_MAX];

// RS485 serial
HardwareSerial rs485(1);

// RF receiver/transmitter
RCSwitch rfReceiver = RCSwitch();
RCSwitch rfTransmitter = RCSwitch();

// RTC object
RTC_DS3231 rtc;

// Firmware version
const String firmwareVersion = "2.1.1";

// System state variables
bool outputStates[16] = { false };        // Current output states
bool inputStates[16] = { false };         // Current input states
bool directInputStates[3] = { false };    // Current HT1-HT3 states
int analogValues[4] = { 0 };              // Current analog input values
bool ethConnected = false;              // Ethernet connection status
bool wifiConnected = false;             // WiFi connection status
String deviceName = "KC868-A16";        // Device name (changeable)
bool debugMode = true;                  // Debug mode flag
unsigned long lastTimeCheck = 0;        // Last time the schedules were checked
bool rtcInitialized = false;            // Whether RTC is available and initialized
String currentCommunicationProtocol = "wifi"; // Current communication protocol

// Network configuration
IPAddress ip;
IPAddress gateway;
IPAddress subnet(255, 255, 255, 0);
IPAddress dns1(8, 8, 8, 8);
IPAddress dns2(8, 8, 4, 4);
String mac = "";
String wifiSSID = "Google NODE";
String wifiPassword = "36002016";

// WiFi configuration
String wifiSecurity = "WPA2";          // WPA, WPA2, WEP, OPEN
bool wifiHidden = false;               // Whether SSID is hidden
String wifiMacFilter = "";             // MAC filtering
bool wifiAutoUpdate = true;            // Auto firmware updates
String wifiRadioMode = "802.11n";      // 802.11b, 802.11g, 802.11n
int wifiChannel = 6;                   // WiFi channel (1-13)
int wifiChannelWidth = 20;             // Channel width (20 or 40 MHz)
unsigned long wifiDhcpLeaseTime = 86400; // DHCP lease time in seconds
bool wifiWmmEnabled = true;            // WiFi Multimedia (WMM)

// USB configuration
int usbComPort = 0;                    // COM port number
int usbBaudRate = 115200;              // Baud rate
int usbDataBits = 8;                   // Data bits (7 or 8)
int usbParity = 0;                     // Parity (0=none, 1=odd, 2=even)
int usbStopBits = 1;                   // Stop bits (1 or 2)

// RS485 configuration
int rs485BaudRate = 9600;              // Baud rate
int rs485Parity = 0;                   // Parity (0=none, 1=odd, 2=even)
int rs485DataBits = 8;                 // Data bits (7 or 8)
int rs485StopBits = 1;                 // Stop bits (1 or 2)
String rs485Protocol = "Modbus RTU";   // Protocol type
String rs485Mode = "Half-duplex";      // Communication mode
int rs485DeviceAddress = 1;            // Device address
bool rs485FlowControl = false;         // Flow control
bool rs485NightMode = false;           // Night communication settings

// DHCP or static IP mode
bool dhcpMode = true;

// File upload
File fsUploadFile;

// Flag for device restart
bool restartRequired = false;

// Scheduling structures
struct TimeSchedule {
    bool enabled;
    uint8_t days;       // Bit field: bit 0=Sunday, bit 1=Monday, ..., bit 6=Saturday
    uint8_t hour;
    uint8_t minute;
    uint8_t action;     // 0=OFF, 1=ON, 2=TOGGLE
    uint8_t targetType; // 0=Output, 1=Multiple outputs
    uint16_t targetId;   // Output number (0-15) or bitmask for multiple outputs
    char name[32];      // Name/description of the schedule
};

TimeSchedule schedules[MAX_SCHEDULES];

// Analog trigger structure
struct AnalogTrigger {
    bool enabled;
    uint8_t analogInput;    // 0-3 (A1-A4)
    uint16_t threshold;     // Analog threshold value (0-4095)
    uint8_t condition;      // 0=Above, 1=Below, 2=Equal
    uint8_t action;         // 0=OFF, 1=ON, 2=TOGGLE
    uint8_t targetType;     // 0=Output, 1=Multiple outputs
    uint16_t targetId;       // Output number (0-15) or bitmask
    char name[32];          // Name/description of trigger
};

AnalogTrigger analogTriggers[MAX_ANALOG_TRIGGERS];

// Diagnostics
unsigned long i2cErrorCount = 0;
unsigned long lastSystemUptime = 0;
String lastErrorMessage = "";

// Function prototypes
void initI2C();
void initWiFi();
void initEthernet();
void initRTC();
void setupWebServer();
void handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length);
void broadcastUpdate();
void initRS485();
void initRF();
void saveConfiguration();
void loadConfiguration();
void saveWiFiCredentials(String ssid, String password);
void loadWiFiCredentials();
void saveCommunicationSettings();
void loadCommunicationSettings();
void saveCommunicationConfig();
void loadCommunicationConfig();
bool readInputs();
bool writeOutputs();
void handleWebRoot();
void handleNotFound();
void handleFileUpload();
void handleRelayControl();
void handleSystemStatus();
void handleSchedules();
void handleUpdateSchedule();
void handleConfig();
void handleUpdateConfig();
void handleAnalogTriggers();
void handleUpdateAnalogTriggers();
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
void checkSchedules();
void checkAnalogTriggers();
void processRS485Commands();
void processSerialCommands();
void WiFiEvent(WiFiEvent_t event);
void EthEvent(WiFiEvent_t event);
void debugPrintln(String message);
void syncTimeFromNTP();
void syncTimeFromClient(int year, int month, int day, int hour, int minute, int second);
String getTimeString();
String processCommand(String command);
int readAnalogInput(uint8_t index);
void printIOStates(); // New function to print I/O states for debugging

// Setup function
void setup() {
    // Initialize serial communication for debugging
    Serial.begin(115200);
    Serial.println("\nKC868-A16 Controller starting up...");

    // Initialize EEPROM
    EEPROM.begin(EEPROM_SIZE);

    // Initialize file system
    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS mount failed");
    }
    else {
        Serial.println("SPIFFS Mounted SUCCESSFULLY...");
    }

    // Initialize I2C with custom pins
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(50000);  // Lower to 50kHz for more reliable communication

    // Initialize PCF8574 expanders
    initI2C();

    // Initialize direct GPIO inputs
    pinMode(HT1_PIN, INPUT_PULLUP);
    pinMode(HT2_PIN, INPUT_PULLUP);
    pinMode(HT3_PIN, INPUT_PULLUP);

    // Load configuration from EEPROM
    loadConfiguration();
    loadWiFiCredentials();
    loadCommunicationSettings();
    loadCommunicationConfig();

    // Initialize RTC
    initRTC();

    // Initialize WiFi
    initWiFi();

    // Initialize Ethernet
    initEthernet();

    // Initialize RS485 serial with current configuration
    initRS485();

    // Initialize RF receiver and transmitter
    initRF();

    // Start DNS server for captive portal if in AP mode
    if (WiFi.getMode() == WIFI_AP) {
        dnsServer.start(53, "*", WiFi.softAPIP());
    }

    // Initialize WebSocket server
    webSocket.begin();
    webSocket.onEvent(handleWebSocketEvent);
    Serial.println("WebSocket server started");

    // Setup web server endpoints
    setupWebServer();

    // Initialize output states (All relays OFF)
    writeOutputs();

    // Read initial input states
    readInputs();

    // Read initial analog values
    for (int i = 0; i < 4; i++) {
        analogValues[i] = readAnalogInput(i);
    }

    // Initialize WebSocket client array
    for (int i = 0; i < WEBSOCKETS_SERVER_CLIENT_MAX; i++) {
        webSocketClients[i] = false;
    }

    // Print initial I/O states for debugging
    printIOStates();

    Serial.println("KC868-A16 Controller initialization complete");
}

// Modified loop function to broadcast more frequently
void loop() {
    // Handle DNS requests for captive portal if in AP mode
    if (WiFi.getMode() == WIFI_AP) {
        dnsServer.processNextRequest();
    }

    // Handle web server clients
    server.handleClient();

    // Handle WebSocket events
    webSocket.loop();

    // Static variables to track last updates
    static unsigned long lastWebSocketUpdate = 0;
    static unsigned long lastInputsCheck = 0;
    static unsigned long lastAnalogCheck = 0;

    unsigned long currentMillis = millis();

    // Read digital inputs more frequently (every 100ms)
    if (currentMillis - lastInputsCheck >= 100) {
        lastInputsCheck = currentMillis;
        bool inputsChanged = readInputs();

        // If inputs changed, broadcast immediately
        if (inputsChanged) {
            broadcastUpdate();
            lastWebSocketUpdate = currentMillis;
        }
    }

    // Read analog inputs every 500ms
    if (currentMillis - lastAnalogCheck >= 500) {
        lastAnalogCheck = currentMillis;
        bool analogChanged = false;

        for (int i = 0; i < 4; i++) {
            int newValue = readAnalogInput(i);
            if (abs(newValue - analogValues[i]) > 20) { // Apply some hysteresis
                analogValues[i] = newValue;
                analogChanged = true;
            }
        }

        // If analog values changed significantly, check triggers
        if (analogChanged) {
            checkAnalogTriggers();

            // Broadcast immediately if analog values changed
            broadcastUpdate();
            lastWebSocketUpdate = currentMillis;
        }
    }

    // Broadcast periodic updates every 2 seconds even if no changes
    if (currentMillis - lastWebSocketUpdate >= 2000) {
        broadcastUpdate();
        lastWebSocketUpdate = currentMillis;
    }

    // Process commands based on active communication protocol
    if (currentCommunicationProtocol == "usb") {
        processSerialCommands();
    }
    else if (currentCommunicationProtocol == "rs485") {
        processRS485Commands();
    }

    // Check RF receiver for any signals
    if (rfReceiver.available()) {
        unsigned long rfCode = rfReceiver.getReceivedValue();
        debugPrintln("RF code received: " + String(rfCode));
        rfReceiver.resetAvailable();
    }

    // Check schedules every second
    if (currentMillis - lastTimeCheck >= 1000) {
        lastTimeCheck = currentMillis;
        checkSchedules();
    }

    // Update system uptime (for diagnostics)
    if (currentMillis - lastSystemUptime >= 60000) {
        lastSystemUptime = currentMillis;
        debugPrintln("System uptime: " + String(millis() / 60000) + " minutes");
    }
}

// Print I/O states for debugging
void printIOStates() {
    Serial.println("--- Current I/O States ---");

    // Print input states
    Serial.println("Input States (1=HIGH/OFF, 0=LOW/ON):");
    Serial.print("Inputs 1-8:  ");
    for (int i = 7; i >= 0; i--) {
        Serial.print(inputStates[i] ? "1" : "0");
    }
    Serial.println();

    Serial.print("Inputs 9-16: ");
    for (int i = 15; i >= 8; i--) {
        Serial.print(inputStates[i] ? "1" : "0");
    }
    Serial.println();

    // Print output states
    Serial.println("Output States (1=HIGH/ON, 0=LOW/OFF):");
    Serial.print("Outputs 1-8:  ");
    for (int i = 7; i >= 0; i--) {
        Serial.print(outputStates[i] ? "1" : "0");
    }
    Serial.println();

    Serial.print("Outputs 9-16: ");
    for (int i = 15; i >= 8; i--) {
        Serial.print(outputStates[i] ? "1" : "0");
    }
    Serial.println();

    Serial.println("----------------------------");
}

// Initialize I2C communication with PCF8574 chips
void initI2C() {

    // Configure input pins (set as inputs with pull-ups)
    for (int i = 0; i < 8; i++) {
        inputIC1.pinMode(i, INPUT);
        inputIC2.pinMode(i, INPUT);
    }

    // Configure output pins (set as outputs)
    for (int i = 0; i < 8; i++) {
        outputIC3.pinMode(i, OUTPUT);
        outputIC4.pinMode(i, OUTPUT);
    }


    // Initialize PCF8574 ICs
    if (!inputIC1.begin()) {
        Serial.println("Error: Could not initialize Input IC1 (0x22)");
        i2cErrorCount++;
        lastErrorMessage = "Failed to initialize Input IC1";
    }

    if (!inputIC2.begin()) {
        Serial.println("Error: Could not initialize Input IC2 (0x21)");
        i2cErrorCount++;
        lastErrorMessage = "Failed to initialize Input IC2";
    }

    if (!outputIC3.begin()) {
        Serial.println("Error: Could not initialize Output IC3 (0x25)");
        i2cErrorCount++;
        lastErrorMessage = "Failed to initialize Output IC3";
    }

    if (!outputIC4.begin()) {
        Serial.println("Error: Could not initialize Output IC4 (0x24)");
        i2cErrorCount++;
        lastErrorMessage = "Failed to initialize Output IC4";
    }


    // Initialize all outputs to HIGH (OFF state due to inverted logic)
    for (int i = 0; i < 8; i++) {
        outputIC3.digitalWrite(i, HIGH);
        outputIC4.digitalWrite(i, HIGH);
    }

    // Initialize input state arrays
    for (int i = 0; i < 16; i++) {
        inputStates[i] = true;   // Default HIGH (pull-up)
    }

    debugPrintln("I2C and PCF8574 expanders initialized successfully");
}

// Initialize WiFi
void initWiFi() {
    // Register event handler
    WiFi.onEvent(WiFiEvent);

    if (dhcpMode) {
        WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    }
    else {
        WiFi.config(ip, gateway, subnet, dns1, dns2);
    }

    // Set hostname
    WiFi.setHostname(deviceName.c_str());

    // Start WiFi with loaded credentials
    if (wifiSSID.length() > 0) {
        WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
        debugPrintln("Connecting to WiFi SSID: " + wifiSSID);
    }
    else {
        WiFi.begin();
        debugPrintln("Connecting to last used WiFi network");
    }

    // Wait for connection
    int connectionAttempts = 0;
    while (WiFi.status() != WL_CONNECTED && connectionAttempts < 20) {
        delay(500);
        Serial.print(".");
        connectionAttempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.println("");
        Serial.print("Connected to WiFi. IP address: ");
        Serial.println(WiFi.localIP());
        mac = WiFi.macAddress();
    }
    else {
        wifiConnected = false;
        Serial.println("WiFi connection failed, starting AP mode");

        // Start access point mode if client connection fails
        WiFi.softAP(default_ssid, default_password);
        Serial.print("AP IP address: ");
        Serial.println(WiFi.softAPIP());
    }
}

// Ethernet event handler
void EthEvent(WiFiEvent_t event) {
    switch (event) {
    case ARDUINO_EVENT_ETH_START:
        Serial.println("ETH Started");
        ETH.setHostname(deviceName.c_str());
        break;
    case ARDUINO_EVENT_ETH_CONNECTED:
        Serial.println("ETH Connected");
        break;
    case ARDUINO_EVENT_ETH_GOT_IP:
        Serial.print("ETH MAC: ");
        Serial.print(ETH.macAddress());
        Serial.print(", IPv4: ");
        Serial.print(ETH.localIP());
        if (ETH.fullDuplex()) {
            Serial.print(", FULL_DUPLEX");
        }
        Serial.print(", ");
        Serial.print(ETH.linkSpeed());
        Serial.println("Mbps");
        ethConnected = true;
        break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
        Serial.println("ETH Disconnected");
        ethConnected = false;
        break;
    case ARDUINO_EVENT_ETH_STOP:
        Serial.println("ETH Stopped");
        ethConnected = false;
        break;
    default:
        break;
    }
}

// Initialize Ethernet
void initEthernet() {
    // Register Ethernet event handler
    WiFi.onEvent(EthEvent, ARDUINO_EVENT_ETH_START);
    WiFi.onEvent(EthEvent, ARDUINO_EVENT_ETH_CONNECTED);
    WiFi.onEvent(EthEvent, ARDUINO_EVENT_ETH_GOT_IP);
    WiFi.onEvent(EthEvent, ARDUINO_EVENT_ETH_DISCONNECTED);
    WiFi.onEvent(EthEvent, ARDUINO_EVENT_ETH_STOP);

    // Initialize Ethernet
    ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER, ETH_CLK_MODE);

    if (!dhcpMode) {
        // Static IP
        ETH.config(ip, gateway, subnet, dns1, dns2);
    }
}

// Initialize RTC with time sync options
void initRTC() {
    rtcInitialized = rtc.begin();
    if (!rtcInitialized) {
        debugPrintln("Couldn't find RTC, using ESP32 internal time");
        // Use NTP for time sync if RTC not available
        configTime(0, 0, "pool.ntp.org", "time.nist.gov");
        syncTimeFromNTP();
    }
    else {
        debugPrintln("RTC found");

        if (rtc.lostPower()) {
            debugPrintln("RTC lost power, setting to compile time");
            // Set RTC to compile time if power was lost
            rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
            syncTimeFromNTP();  // Also try to get more accurate time
        }

        // Print current time from RTC
        DateTime now = rtc.now();
        String timeStr = String(now.year()) + "-" +
            String(now.month()) + "-" +
            String(now.day()) + " " +
            String(now.hour()) + ":" +
            String(now.minute()) + ":" +
            String(now.second());
        debugPrintln("RTC time: " + timeStr);
    }
}

// Sync time from NTP servers
void syncTimeFromNTP() {
    debugPrintln("Syncing time from NTP...");
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");

    time_t now = time(nullptr);
    int retry = 0;
    const int maxRetries = 10;

    // Wait for time to be set
    while (now < 24 * 3600 && retry < maxRetries) {
        debugPrintln("Waiting for NTP time sync...");
        delay(500);
        now = time(nullptr);
        retry++;
    }

    if (now > 24 * 3600) {
        struct tm timeinfo;
        gmtime_r(&now, &timeinfo);

        debugPrintln("NTP time sync successful");

        // If RTC available, update it with NTP time
        if (rtcInitialized) {
            rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
            debugPrintln("Updated RTC with NTP time");
        }
    }
    else {
        debugPrintln("NTP time sync failed");
    }
}

// Sync time from client (called when receiving time from web UI)
void syncTimeFromClient(int year, int month, int day, int hour, int minute, int second) {
    if (rtcInitialized) {
        rtc.adjust(DateTime(year, month, day, hour, minute, second));
        debugPrintln("Updated RTC with client time");
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
        debugPrintln("Updated system time with client time");
    }
}

// Get current time as formatted string
String getTimeString() {
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    char timeString[30];
    strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(timeString);
}

// Initialize RS485
void initRS485() {
    // Configure with current RS485 settings
    int configParity = SERIAL_8N1; // Default

    // Set correct parity
    if (rs485DataBits == 8) {
        if (rs485Parity == 0) {
            if (rs485StopBits == 1) configParity = SERIAL_8N1;
            else configParity = SERIAL_8N2;
        }
        else if (rs485Parity == 1) {
            if (rs485StopBits == 1) configParity = SERIAL_8O1;
            else configParity = SERIAL_8O2;
        }
        else if (rs485Parity == 2) {
            if (rs485StopBits == 1) configParity = SERIAL_8E1;
            else configParity = SERIAL_8E2;
        }
    }
    else if (rs485DataBits == 7) {
        if (rs485Parity == 0) {
            if (rs485StopBits == 1) configParity = SERIAL_7N1;
            else configParity = SERIAL_7N2;
        }
        else if (rs485Parity == 1) {
            if (rs485StopBits == 1) configParity = SERIAL_7O1;
            else configParity = SERIAL_7O2;
        }
        else if (rs485Parity == 2) {
            if (rs485StopBits == 1) configParity = SERIAL_7E1;
            else configParity = SERIAL_7E2;
        }
    }

    rs485.begin(rs485BaudRate, configParity, RS485_RX_PIN, RS485_TX_PIN);
    debugPrintln("RS485 initialized with baud rate: " + String(rs485BaudRate));
}

// Initialize RF receiver and transmitter
void initRF() {
    rfReceiver.enableReceive(RF_RX_PIN);
    rfTransmitter.enableTransmit(RF_TX_PIN);
    debugPrintln("RF receiver/transmitter initialized");
}

// Setup web server endpoints
void setupWebServer() {
    // Serve static files from SPIFFS
    server.serveStatic("/", SPIFFS, "/index.html");
    server.serveStatic("/style.css", SPIFFS, "/style.css");
    server.serveStatic("/script.js", SPIFFS, "/script.js");

    // API endpoints
    server.on("/", HTTP_GET, handleWebRoot);
    server.on("/api/status", HTTP_GET, handleSystemStatus);
    server.on("/api/relay", HTTP_POST, handleRelayControl);
    server.on("/api/schedules", HTTP_GET, handleSchedules);
    server.on("/api/schedules", HTTP_POST, handleUpdateSchedule);
    server.on("/api/analog-triggers", HTTP_GET, handleAnalogTriggers);
    server.on("/api/analog-triggers", HTTP_POST, handleUpdateAnalogTriggers);
    server.on("/api/config", HTTP_GET, handleConfig);
    server.on("/api/config", HTTP_POST, handleUpdateConfig);
    server.on("/api/debug", HTTP_GET, handleDebug);
    server.on("/api/debug", HTTP_POST, handleDebugCommand);
    server.on("/api/reboot", HTTP_POST, handleReboot);

    // Communication endpoints
    server.on("/api/communication", HTTP_GET, handleCommunicationStatus);
    server.on("/api/communication", HTTP_POST, handleSetCommunication);
    server.on("/api/communication/config", HTTP_GET, handleCommunicationConfig);
    server.on("/api/communication/config", HTTP_POST, handleUpdateCommunicationConfig);

    // Time endpoints
    server.on("/api/time", HTTP_GET, handleGetTime);
    server.on("/api/time", HTTP_POST, handleSetTime);

    // Diagnostic endpoints
    server.on("/api/i2c/scan", HTTP_GET, handleI2CScan);

    // File upload handler
    server.on("/api/upload", HTTP_POST, []() {
        server.send(200, "text/plain", "File upload complete");
        }, handleFileUpload);

    // Not found handler
    server.onNotFound(handleNotFound);

    // Start server
    server.begin();
    debugPrintln("Web server started");
}

// Handle WebSocket events
void handleWebSocketEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
    case WStype_DISCONNECTED:
        debugPrintln("WebSocket client disconnected");
        webSocketClients[num] = false;
        break;
    case WStype_CONNECTED:
    {
        IPAddress ip = webSocket.remoteIP(num);
        debugPrintln("WebSocket client connected: " + ip.toString());

        // Mark client as subscribed
        webSocketClients[num] = true;

        // Send initial status update
        DynamicJsonDocument doc(1024);
        doc["type"] = "status";
        doc["connected"] = true;

        String message;
        serializeJson(doc, message);
        webSocket.sendTXT(num, message);

        // Send current state of all relays and inputs
        broadcastUpdate();
    }
    break;
    case WStype_TEXT:
    {
        String text = String((char*)payload);
        debugPrintln("WebSocket received: " + text);

        // Process WebSocket command
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, text);

        if (!error) {
            String cmd = doc["command"];

            if (cmd == "subscribe") {
                // Subscribe to real-time updates
                webSocketClients[num] = true;
                debugPrintln("Client subscribed to updates");
            }
            else if (cmd == "unsubscribe") {
                // Unsubscribe from updates
                webSocketClients[num] = false;
                debugPrintln("Client unsubscribed from updates");
            }
            else if (cmd == "toggle_relay") {
                // Toggle relay command
                int relay = doc["relay"];
                bool state = doc["state"];

                debugPrintln("WebSocket: Toggling relay " + String(relay) + " to " + String(state ? "ON" : "OFF"));

                if (relay >= 0 && relay < 16) {
                    outputStates[relay] = state;

                    if (writeOutputs()) {
                        debugPrintln("Relay toggled successfully via WebSocket");

                        // Send response
                        DynamicJsonDocument responseDoc(512);
                        responseDoc["type"] = "relay_update";
                        responseDoc["relay"] = relay;
                        responseDoc["state"] = outputStates[relay];

                        String response;
                        serializeJson(responseDoc, response);
                        webSocket.sendTXT(num, response);

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
                        webSocket.sendTXT(num, errorResponse);

                        debugPrintln("ERROR: Failed to toggle relay via WebSocket");
                    }
                }
                else {
                    debugPrintln("ERROR: Invalid relay index: " + String(relay));
                }
            }
            else if (cmd == "get_protocol_config") {
                // Get protocol-specific configuration
                String protocol = doc["protocol"];

                DynamicJsonDocument responseDoc(1024);
                responseDoc["type"] = "protocol_config";
                responseDoc["protocol"] = protocol;

                if (protocol == "wifi") {
                    responseDoc["security"] = wifiSecurity;
                    responseDoc["hidden"] = wifiHidden;
                    responseDoc["mac_filter"] = wifiMacFilter;
                    responseDoc["auto_update"] = wifiAutoUpdate;
                    responseDoc["radio_mode"] = wifiRadioMode;
                    responseDoc["channel"] = wifiChannel;
                    responseDoc["channel_width"] = wifiChannelWidth;
                    responseDoc["dhcp_lease_time"] = wifiDhcpLeaseTime;
                    responseDoc["wmm_enabled"] = wifiWmmEnabled;
                    responseDoc["ssid"] = wifiSSID;
                }
                else if (protocol == "ethernet") {
                    responseDoc["dhcp_mode"] = dhcpMode;
                    if (!dhcpMode) {
                        responseDoc["ip"] = ip.toString();
                        responseDoc["gateway"] = gateway.toString();
                        responseDoc["subnet"] = subnet.toString();
                        responseDoc["dns1"] = dns1.toString();
                        responseDoc["dns2"] = dns2.toString();
                    }
                    if (ethConnected) {
                        responseDoc["eth_mac"] = ETH.macAddress();
                        responseDoc["eth_ip"] = ETH.localIP().toString();
                        responseDoc["eth_duplex"] = ETH.fullDuplex() ? "Full" : "Half";
                        responseDoc["eth_speed"] = String(ETH.linkSpeed()) + " Mbps";
                    }
                }
                else if (protocol == "usb") {
                    responseDoc["com_port"] = usbComPort;
                    responseDoc["baud_rate"] = usbBaudRate;
                    responseDoc["data_bits"] = usbDataBits;
                    responseDoc["parity"] = usbParity;
                    responseDoc["stop_bits"] = usbStopBits;
                }
                else if (protocol == "rs485") {
                    responseDoc["baud_rate"] = rs485BaudRate;
                    responseDoc["parity"] = rs485Parity;
                    responseDoc["data_bits"] = rs485DataBits;
                    responseDoc["stop_bits"] = rs485StopBits;
                    responseDoc["protocol_type"] = rs485Protocol;
                    responseDoc["comm_mode"] = rs485Mode;
                    responseDoc["device_address"] = rs485DeviceAddress;
                    responseDoc["flow_control"] = rs485FlowControl;
                    responseDoc["night_mode"] = rs485NightMode;
                }

                String response;
                serializeJson(responseDoc, response);
                webSocket.sendTXT(num, response);
            }
        }
        else {
            debugPrintln("ERROR: Invalid JSON in WebSocket message");
        }
    }
    break;
    }
}

// Broadcast updates to all subscribed WebSocket clients
void broadcastUpdate() {
    DynamicJsonDocument doc(4096);
    doc["type"] = "status_update";
    doc["time"] = getTimeString();
    doc["timestamp"] = millis(); // Add timestamp for freshness checking

    // Add outputs
    JsonArray outputs = doc.createNestedArray("outputs");
    for (int i = 0; i < 16; i++) {
        JsonObject output = outputs.createNestedObject();
        output["id"] = i;
        output["state"] = outputStates[i];
    }

    // Add inputs
    JsonArray inputs = doc.createNestedArray("inputs");
    for (int i = 0; i < 16; i++) {
        JsonObject input = inputs.createNestedObject();
        input["id"] = i;
        input["state"] = inputStates[i];
    }

    // Add direct inputs
    JsonArray directInputs = doc.createNestedArray("direct_inputs");
    for (int i = 0; i < 3; i++) {
        JsonObject input = directInputs.createNestedObject();
        input["id"] = i;
        input["state"] = directInputStates[i];
    }

    // Add analog inputs
    JsonArray analog = doc.createNestedArray("analog");
    for (int i = 0; i < 4; i++) {
        JsonObject an = analog.createNestedObject();
        an["id"] = i;
        an["value"] = analogValues[i];
        an["percentage"] = map(analogValues[i], 0, 4095, 0, 100);
    }

    // Add system info
    doc["device"] = deviceName;
    doc["uptime"] = millis() / 1000;
    doc["wifi_connected"] = wifiConnected;
    doc["eth_connected"] = ethConnected;
    doc["active_protocol"] = currentCommunicationProtocol;
    doc["firmware_version"] = firmwareVersion;

    if (wifiConnected) {
        doc["wifi_ip"] = WiFi.localIP().toString();
        doc["wifi_rssi"] = WiFi.RSSI();
    }

    if (ethConnected) {
        doc["eth_ip"] = ETH.localIP().toString();
    }

    // Add MAC address
    doc["mac"] = mac;

    String message;
    serializeJson(doc, message);

    // Send to all subscribed clients
    int clientCount = 0;
    for (int i = 0; i < WEBSOCKETS_SERVER_CLIENT_MAX; i++) {
        if (webSocketClients[i]) {
            webSocket.sendTXT(i, message);
            clientCount++;
        }
    }

    if (clientCount > 0) {
        debugPrintln("Broadcast update sent to " + String(clientCount) + " clients");
    }
}

// Save WiFi credentials to EEPROM
void saveWiFiCredentials(String ssid, String password) {
    // Store SSID
    for (int i = 0; i < 64; i++) {
        if (i < ssid.length()) {
            EEPROM.write(EEPROM_WIFI_SSID_ADDR + i, ssid[i]);
        }
        else {
            EEPROM.write(EEPROM_WIFI_SSID_ADDR + i, 0);
        }
    }

    // Store password
    for (int i = 0; i < 64; i++) {
        if (i < password.length()) {
            EEPROM.write(EEPROM_WIFI_PASS_ADDR + i, password[i]);
        }
        else {
            EEPROM.write(EEPROM_WIFI_PASS_ADDR + i, 0);
        }
    }

    EEPROM.commit();

    // Update global variables
    wifiSSID = ssid;
    wifiPassword = password;
}

// Load WiFi credentials from EEPROM
void loadWiFiCredentials() {
    wifiSSID = "";
    wifiPassword = "";

    // Read SSID
    for (int i = 0; i < 64; i++) {
        char c = EEPROM.read(EEPROM_WIFI_SSID_ADDR + i);
        if (c != 0) {
            wifiSSID += c;
        }
        else {
            break;
        }
    }

    // Read password
    for (int i = 0; i < 64; i++) {
        char c = EEPROM.read(EEPROM_WIFI_PASS_ADDR + i);
        if (c != 0) {
            wifiPassword += c;
        }
        else {
            break;
        }
    }

    debugPrintln("Loaded WiFi SSID: " + wifiSSID);
}

// Save communication protocol setting
void saveCommunicationSettings() {
    // Store protocol in EEPROM
    for (int i = 0; i < 10; i++) {
        if (i < currentCommunicationProtocol.length()) {
            EEPROM.write(EEPROM_COMM_ADDR + i, currentCommunicationProtocol[i]);
        }
        else {
            EEPROM.write(EEPROM_COMM_ADDR + i, 0);
        }
    }

    EEPROM.commit();
    debugPrintln("Saved communication protocol: " + currentCommunicationProtocol);
}

// Load communication protocol setting
void loadCommunicationSettings() {
    String protocol = "";

    // Read protocol
    for (int i = 0; i < 10; i++) {
        char c = EEPROM.read(EEPROM_COMM_ADDR + i);
        if (c != 0) {
            protocol += c;
        }
        else {
            break;
        }
    }

    // Validate protocol
    if (protocol == "wifi" || protocol == "ethernet" || protocol == "usb" || protocol == "rs485") {
        currentCommunicationProtocol = protocol;
    }
    else {
        // Default to WiFi if invalid
        currentCommunicationProtocol = "wifi";
    }

    debugPrintln("Loaded communication protocol: " + currentCommunicationProtocol);
}

// Save communication protocol specific configuration
void saveCommunicationConfig() {
    DynamicJsonDocument doc(2048);

    // WiFi config
    JsonObject wifi = doc.createNestedObject("wifi");
    wifi["security"] = wifiSecurity;
    wifi["hidden"] = wifiHidden;
    wifi["mac_filter"] = wifiMacFilter;
    wifi["auto_update"] = wifiAutoUpdate;
    wifi["radio_mode"] = wifiRadioMode;
    wifi["channel"] = wifiChannel;
    wifi["channel_width"] = wifiChannelWidth;
    wifi["dhcp_lease_time"] = wifiDhcpLeaseTime;
    wifi["wmm_enabled"] = wifiWmmEnabled;

    // USB config
    JsonObject usb = doc.createNestedObject("usb");
    usb["com_port"] = usbComPort;
    usb["baud_rate"] = usbBaudRate;
    usb["data_bits"] = usbDataBits;
    usb["parity"] = usbParity;
    usb["stop_bits"] = usbStopBits;

    // RS485 config
    JsonObject rs485Config = doc.createNestedObject("rs485");
    rs485Config["baud_rate"] = rs485BaudRate;
    rs485Config["parity"] = rs485Parity;
    rs485Config["data_bits"] = rs485DataBits;
    rs485Config["stop_bits"] = rs485StopBits;
    rs485Config["protocol"] = rs485Protocol;
    rs485Config["mode"] = rs485Mode;
    rs485Config["device_address"] = rs485DeviceAddress;
    rs485Config["flow_control"] = rs485FlowControl;
    rs485Config["night_mode"] = rs485NightMode;

    // Serialize JSON to a buffer
    char jsonBuffer[2048];
    size_t n = serializeJson(doc, jsonBuffer);

    // Store in EEPROM
    for (size_t i = 0; i < n && i < 1024; i++) {
        EEPROM.write(EEPROM_COMM_CONFIG_ADDR + i, jsonBuffer[i]);
    }

    // Write null terminator
    EEPROM.write(EEPROM_COMM_CONFIG_ADDR + n, 0);

    // Commit changes
    EEPROM.commit();

    debugPrintln("Saved communication protocol configuration");
}

// Load communication protocol specific configuration
void loadCommunicationConfig() {
    // Create a buffer to read JSON data
    char jsonBuffer[2048];
    size_t i = 0;

    // Read data until null terminator or max buffer size
    while (i < 2047) {
        jsonBuffer[i] = EEPROM.read(EEPROM_COMM_CONFIG_ADDR + i);
        if (jsonBuffer[i] == 0) break;
        i++;
    }

    // Add null terminator if buffer is full
    jsonBuffer[i] = 0;

    // If we read something, try to parse it
    if (i > 0) {
        DynamicJsonDocument doc(2048);
        DeserializationError error = deserializeJson(doc, jsonBuffer);

        if (!error) {
            // WiFi config
            if (doc.containsKey("wifi")) {
                wifiSecurity = doc["wifi"]["security"] | "WPA2";
                wifiHidden = doc["wifi"]["hidden"] | false;
                wifiMacFilter = doc["wifi"]["mac_filter"] | "";
                wifiAutoUpdate = doc["wifi"]["auto_update"] | true;
                wifiRadioMode = doc["wifi"]["radio_mode"] | "802.11n";
                wifiChannel = doc["wifi"]["channel"] | 6;
                wifiChannelWidth = doc["wifi"]["channel_width"] | 20;
                wifiDhcpLeaseTime = doc["wifi"]["dhcp_lease_time"] | 86400;
                wifiWmmEnabled = doc["wifi"]["wmm_enabled"] | true;
            }

            // USB config
            if (doc.containsKey("usb")) {
                usbComPort = doc["usb"]["com_port"] | 0;
                usbBaudRate = doc["usb"]["baud_rate"] | 115200;
                usbDataBits = doc["usb"]["data_bits"] | 8;
                usbParity = doc["usb"]["parity"] | 0;
                usbStopBits = doc["usb"]["stop_bits"] | 1;
            }

            // RS485 config
            if (doc.containsKey("rs485")) {
                rs485BaudRate = doc["rs485"]["baud_rate"] | 9600;
                rs485Parity = doc["rs485"]["parity"] | 0;
                rs485DataBits = doc["rs485"]["data_bits"] | 8;
                rs485StopBits = doc["rs485"]["stop_bits"] | 1;
                rs485Protocol = doc["rs485"]["protocol"] | "Modbus RTU";
                rs485Mode = doc["rs485"]["mode"] | "Half-duplex";
                rs485DeviceAddress = doc["rs485"]["device_address"] | 1;
                rs485FlowControl = doc["rs485"]["flow_control"] | false;
                rs485NightMode = doc["rs485"]["night_mode"] | false;
            }

            debugPrintln("Communication configuration loaded from EEPROM");
        }
        else {
            debugPrintln("Failed to parse communication configuration JSON");
            // Keep defaults
        }
    }
    else {
        debugPrintln("No communication configuration found, using defaults");
    }
}

// Read inputs from PCF8574 chips and direct GPIO inputs
bool readInputs() {
    bool anyChanged = false;
    bool success = true;

    // Read from PCF8574 input expanders using the PCF8574 library
    // Inputs 1-8 (IC1)
    for (int i = 0; i < 8; i++) {
        bool newState = false;
        try {
            newState = inputIC1.digitalRead(i);
        }
        catch (const std::exception& e) {
            i2cErrorCount++;
            lastErrorMessage = "Error reading from Input IC1";
            success = false;
            debugPrintln("Error reading from Input IC1: " + String(e.what()));
            continue;
        }

        // Invert because of the pull-up configuration (LOW = active/true)
        newState = !newState;

        if (inputStates[i] != newState) {
            inputStates[i] = newState;
            anyChanged = true;
            debugPrintln("Input " + String(i + 1) + " changed to " + String(newState ? "HIGH" : "LOW"));
        }
    }

    // Inputs 9-16 (IC2)
    for (int i = 0; i < 8; i++) {
        bool newState = false;
        try {
            newState = inputIC2.digitalRead(i);
        }
        catch (const std::exception& e) {
            i2cErrorCount++;
            lastErrorMessage = "Error reading from Input IC2";
            success = false;
            debugPrintln("Error reading from Input IC2: " + String(e.what()));
            continue;
        }

        // Invert because of the pull-up configuration (LOW = active/true)
        newState = !newState;

        if (inputStates[i + 8] != newState) {
            inputStates[i + 8] = newState;
            anyChanged = true;
            debugPrintln("Input " + String(i + 9) + " changed to " + String(newState ? "HIGH" : "LOW"));
        }
    }

    // Read direct GPIO inputs with inversion (LOW = active/true)
    bool ht1 = !digitalRead(HT1_PIN);
    bool ht2 = !digitalRead(HT2_PIN);
    bool ht3 = !digitalRead(HT3_PIN);

    if (directInputStates[0] != ht1) {
        directInputStates[0] = ht1;
        anyChanged = true;
        debugPrintln("HT1 changed to " + String(ht1 ? "HIGH" : "LOW"));
    }

    if (directInputStates[1] != ht2) {
        directInputStates[1] = ht2;
        anyChanged = true;
        debugPrintln("HT2 changed to " + String(ht2 ? "HIGH" : "LOW"));
    }

    if (directInputStates[2] != ht3) {
        directInputStates[2] = ht3;
        anyChanged = true;
        debugPrintln("HT3 changed to " + String(ht3 ? "HIGH" : "LOW"));
    }

    // If any changes, print the current I/O states for debugging
    if (anyChanged && debugMode) {
        printIOStates();
    }

    return anyChanged;
}

// Write outputs to PCF8574 chips using the PCF8574 library
bool writeOutputs() {
    bool success = true;

    // Set outputs 1-8 (IC4)
    for (int i = 0; i < 8; i++) {
        try {
            // Write HIGH when output state is false (relays are active LOW)
            outputIC4.digitalWrite(i, outputStates[i] ? LOW : HIGH);
        }
        catch (const std::exception& e) {
            i2cErrorCount++;
            lastErrorMessage = "Failed to write to Output IC4";
            success = false;
            debugPrintln("Error writing to Output IC4: " + String(e.what()));
        }
    }

    // Set outputs 9-16 (IC3)
    for (int i = 0; i < 8; i++) {
        try {
            // Write HIGH when output state is false (relays are active LOW)
            outputIC3.digitalWrite(i, outputStates[i + 8] ? LOW : HIGH);
        }
        catch (const std::exception& e) {
            i2cErrorCount++;
            lastErrorMessage = "Failed to write to Output IC3";
            success = false;
            debugPrintln("Error writing to Output IC3: " + String(e.what()));
        }
    }

    if (success) {
        debugPrintln("Successfully updated all relays");
        if (debugMode) {
            printIOStates();
        }
    }
    else {
        debugPrintln("ERROR: Failed to write to some output expanders");
        // Try to recover I2C bus
        Wire.flush();
        delay(50);
    }

    return success;
}

// Read analog input
int readAnalogInput(uint8_t index) {
    int pinMapping[] = { ANALOG_PIN_1, ANALOG_PIN_2, ANALOG_PIN_3, ANALOG_PIN_4 };

    if (index < 4) {
        return analogRead(pinMapping[index]);
    }

    return 0;
}

// Save configuration to EEPROM
void saveConfiguration() {
    DynamicJsonDocument doc(2048);

    // Device settings
    doc["device_name"] = deviceName;
    doc["debug_mode"] = debugMode;
    doc["dhcp_mode"] = dhcpMode;

    // Network settings if static IP
    if (!dhcpMode) {
        doc["ip"] = ip.toString();
        doc["gateway"] = gateway.toString();
        doc["subnet"] = subnet.toString();
        doc["dns1"] = dns1.toString();
        doc["dns2"] = dns2.toString();
    }

    // Firmware version for tracking
    doc["firmware_version"] = firmwareVersion;

    // Serialize JSON to a buffer
    char jsonBuffer[2048];
    size_t n = serializeJson(doc, jsonBuffer);

    // Store in EEPROM
    for (size_t i = 0; i < n && i < 1536; i++) {
        EEPROM.write(EEPROM_CONFIG_ADDR + i, jsonBuffer[i]);
    }

    // Write null terminator
    EEPROM.write(EEPROM_CONFIG_ADDR + n, 0);

    // Commit changes
    EEPROM.commit();

    debugPrintln("Configuration saved to EEPROM");
}

// Load configuration from EEPROM
void loadConfiguration() {
    // Create a buffer to read JSON data
    char jsonBuffer[2048];
    size_t i = 0;

    // Read data until null terminator or max buffer size
    while (i < 2047) {
        jsonBuffer[i] = EEPROM.read(EEPROM_CONFIG_ADDR + i);
        if (jsonBuffer[i] == 0) break;
        i++;
    }

    // Add null terminator if buffer is full
    jsonBuffer[i] = 0;

    // If we read something, try to parse it
    if (i > 0) {
        DynamicJsonDocument doc(2048);
        DeserializationError error = deserializeJson(doc, jsonBuffer);

        if (!error) {
            // Device settings
            deviceName = doc["device_name"] | "KC868-A16";
            debugMode = doc["debug_mode"] | true;
            dhcpMode = doc["dhcp_mode"] | true;

            // Network settings
            if (!dhcpMode && doc.containsKey("ip") && doc.containsKey("gateway")) {
                ip.fromString(doc["ip"].as<String>());
                gateway.fromString(doc["gateway"].as<String>());
                subnet.fromString(doc["subnet"] | "255.255.255.0");
                dns1.fromString(doc["dns1"] | "8.8.8.8");
                dns2.fromString(doc["dns2"] | "8.8.4.4");
            }

            debugPrintln("Configuration loaded from EEPROM");
        }
        else {
            debugPrintln("Failed to parse configuration JSON");
            // Use defaults
            initializeDefaultConfig();
        }
    }
    else {
        // No data found, use defaults
        initializeDefaultConfig();
    }

    // Initialize default schedules
    for (int i = 0; i < MAX_SCHEDULES; i++) {
        schedules[i].enabled = false;
        schedules[i].days = 0;
        schedules[i].hour = 0;
        schedules[i].minute = 0;
        schedules[i].action = 0;
        schedules[i].targetType = 0;
        schedules[i].targetId = 0;
        snprintf(schedules[i].name, 32, "Schedule %d", i + 1);
    }

    // Initialize default analog triggers
    for (int i = 0; i < MAX_ANALOG_TRIGGERS; i++) {
        analogTriggers[i].enabled = false;
        analogTriggers[i].analogInput = 0;
        analogTriggers[i].threshold = 2048;  // Middle value
        analogTriggers[i].condition = 0;
        analogTriggers[i].action = 0;
        analogTriggers[i].targetType = 0;
        analogTriggers[i].targetId = 0;
        snprintf(analogTriggers[i].name, 32, "Trigger %d", i + 1);
    }
}

// Initialize default configuration
void initializeDefaultConfig() {
    deviceName = "KC868-A16";
    debugMode = true;
    dhcpMode = true;

    debugPrintln("Using default configuration");
}

// Web server root handler
void handleWebRoot() {
    server.sendHeader("Location", "/index.html", true);
    server.send(302, "text/plain", "");
}

// Handle 404 not found
void handleNotFound() {
    // If in captive portal mode and request is for a domain, redirect to configuration page
    if (WiFi.getMode() == WIFI_AP && !server.hostHeader().startsWith("192.168.")) {
        server.sendHeader("Location", "/", true);
        server.send(302, "text/plain", "");
        return;
    }

    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";

    for (uint8_t i = 0; i < server.args(); i++) {
        message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }

    server.send(404, "text/plain", message);
}

// Handle file upload
void handleFileUpload() {
    HTTPUpload& upload = server.upload();

    if (upload.status == UPLOAD_FILE_START) {
        String filename = upload.filename;
        if (!filename.startsWith("/")) {
            filename = "/" + filename;
        }
        debugPrintln("File upload start: " + filename);
        fsUploadFile = SPIFFS.open(filename, FILE_WRITE);
    }
    else if (upload.status == UPLOAD_FILE_WRITE) {
        if (fsUploadFile) {
            fsUploadFile.write(upload.buf, upload.currentSize);
        }
    }
    else if (upload.status == UPLOAD_FILE_END) {
        if (fsUploadFile) {
            fsUploadFile.close();
            debugPrintln("File upload complete: " + String(upload.totalSize) + " bytes");
        }
    }
}

// Handle relay control requests
void handleRelayControl() {
    String response = "{\"status\":\"error\",\"message\":\"Invalid request\"}";

    if (server.hasArg("plain")) {
        String body = server.arg("plain");
        debugPrintln("Relay control request body: " + body);

        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, body);

        if (!error) {
            if (doc.containsKey("relay") && doc.containsKey("state")) {
                int relay = doc["relay"];
                bool state = doc["state"];

                debugPrintln("Request to set relay " + String(relay) + " to " + String(state ? "ON" : "OFF"));

                if (relay >= 0 && relay < 16) {
                    outputStates[relay] = state;
                    if (writeOutputs()) {
                        debugPrintln("Relay control successful");
                        response = "{\"status\":\"success\",\"relay\":" + String(relay) +
                            ",\"state\":" + String(state ? "true" : "false") + "}";

                        // Broadcast update
                        broadcastUpdate();
                    }
                    else {
                        debugPrintln("Failed to write to relay");
                        response = "{\"status\":\"error\",\"message\":\"Failed to write to relay\"}";
                    }
                }
                else if (relay == 99) {  // Special case for all relays
                    debugPrintln("Setting all relays to " + String(state ? "ON" : "OFF"));

                    for (int i = 0; i < 16; i++) {
                        outputStates[i] = state;
                    }
                    if (writeOutputs()) {
                        response = "{\"status\":\"success\",\"relay\":\"all\",\"state\":" +
                            String(state ? "true" : "false") + "}";

                        // Broadcast update
                        broadcastUpdate();
                    }
                    else {
                        debugPrintln("Failed to write to relays");
                        response = "{\"status\":\"error\",\"message\":\"Failed to write to relays\"}";
                    }
                }
                else {
                    debugPrintln("Invalid relay number: " + String(relay));
                }
            }
            else {
                debugPrintln("Missing relay or state in request");
            }
        }
        else {
            debugPrintln("Invalid JSON in request: " + String(error.c_str()));
        }
    }
    else {
        debugPrintln("No plain body in request");
    }

    server.send(200, "application/json", response);
}

// Handle system status requests
void handleSystemStatus() {
    DynamicJsonDocument doc(4096);

    // Device info
    doc["device"] = deviceName;
    doc["uptime"] = millis() / 1000;
    doc["wifi_connected"] = wifiConnected;
    doc["eth_connected"] = ethConnected;
    doc["active_protocol"] = currentCommunicationProtocol;
    doc["firmware_version"] = firmwareVersion;

    if (wifiConnected) {
        doc["wifi_ip"] = WiFi.localIP().toString();
        doc["wifi_rssi"] = WiFi.RSSI();
    }

    if (ethConnected) {
        doc["eth_ip"] = ETH.localIP().toString();
    }

    // MAC address
    doc["mac"] = mac;

    // Get current time
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    char timeString[30];
    strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M:%S", &timeinfo);
    doc["time"] = timeString;

    // Digital inputs
    JsonArray inputs = doc.createNestedArray("inputs");
    for (int i = 0; i < 16; i++) {
        JsonObject input = inputs.createNestedObject();
        input["id"] = i;
        input["name"] = "Input " + String(i + 1);
        input["state"] = inputStates[i];
    }

    // Direct inputs
    JsonArray directInputs = doc.createNestedArray("direct_inputs");
    for (int i = 0; i < 3; i++) {
        JsonObject input = directInputs.createNestedObject();
        input["id"] = i;
        input["name"] = "HT" + String(i + 1);
        input["state"] = directInputStates[i];
    }

    // Relays
    JsonArray outputs = doc.createNestedArray("outputs");
    for (int i = 0; i < 16; i++) {
        JsonObject output = outputs.createNestedObject();
        output["id"] = i;
        output["name"] = "Relay " + String(i + 1);
        output["state"] = outputStates[i];
    }

    // Analog inputs
    JsonArray analog = doc.createNestedArray("analog");
    for (int i = 0; i < 4; i++) {
        JsonObject an = analog.createNestedObject();
        an["id"] = i;
        an["name"] = "A" + String(i + 1);
        an["value"] = analogValues[i];
        an["percentage"] = map(analogValues[i], 0, 4095, 0, 100);
    }

    // Diagnostics
    doc["i2c_errors"] = i2cErrorCount;
    doc["last_error"] = lastErrorMessage;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["cpu_freq"] = ESP.getCpuFreqMHz();

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

// Handle schedules API
void handleSchedules() {
    DynamicJsonDocument doc(4096);
    JsonArray schedulesArray = doc.createNestedArray("schedules");

    for (int i = 0; i < MAX_SCHEDULES; i++) {
        JsonObject schedule = schedulesArray.createNestedObject();
        schedule["id"] = i;
        schedule["enabled"] = schedules[i].enabled;
        schedule["name"] = schedules[i].name;
        schedule["days"] = schedules[i].days;
        schedule["hour"] = schedules[i].hour;
        schedule["minute"] = schedules[i].minute;
        schedule["action"] = schedules[i].action;
        schedule["targetType"] = schedules[i].targetType;
        schedule["targetId"] = schedules[i].targetId;
    }

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

// Update schedule handler
void handleUpdateSchedule() {
    String response = "{\"status\":\"error\",\"message\":\"Invalid request\"}";

    if (server.hasArg("plain")) {
        String body = server.arg("plain");
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, body);

        if (!error && doc.containsKey("schedule")) {
            JsonObject scheduleJson = doc["schedule"];

            // Extract schedule data
            int id = scheduleJson.containsKey("id") ? scheduleJson["id"].as<int>() : -1;

            if (id >= 0 && id < MAX_SCHEDULES) {
                schedules[id].enabled = scheduleJson["enabled"];
                strlcpy(schedules[id].name, scheduleJson["name"] | "Schedule", 32);
                schedules[id].days = scheduleJson["days"];
                schedules[id].hour = scheduleJson["hour"];
                schedules[id].minute = scheduleJson["minute"];
                schedules[id].action = scheduleJson["action"];
                schedules[id].targetType = scheduleJson["targetType"];
                schedules[id].targetId = scheduleJson["targetId"];

                saveConfiguration();
                response = "{\"status\":\"success\"}";
            }
        }
        else if (!error && doc.containsKey("id") && doc.containsKey("enabled")) {
            // Handle simple enable/disable
            int id = doc["id"].as<int>();
            bool enabled = doc["enabled"];

            if (id >= 0 && id < MAX_SCHEDULES) {
                schedules[id].enabled = enabled;
                saveConfiguration();
                response = "{\"status\":\"success\"}";
            }
        }
    }

    server.send(200, "application/json", response);
}

// Handle analog triggers API
void handleAnalogTriggers() {
    DynamicJsonDocument doc(4096);
    JsonArray triggersArray = doc.createNestedArray("triggers");

    for (int i = 0; i < MAX_ANALOG_TRIGGERS; i++) {
        JsonObject trigger = triggersArray.createNestedObject();
        trigger["id"] = i;
        trigger["enabled"] = analogTriggers[i].enabled;
        trigger["name"] = analogTriggers[i].name;
        trigger["analogInput"] = analogTriggers[i].analogInput;
        trigger["threshold"] = analogTriggers[i].threshold;
        trigger["condition"] = analogTriggers[i].condition;
        trigger["action"] = analogTriggers[i].action;
        trigger["targetType"] = analogTriggers[i].targetType;
        trigger["targetId"] = analogTriggers[i].targetId;
    }

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

// Update analog triggers handler
void handleUpdateAnalogTriggers() {
    String response = "{\"status\":\"error\",\"message\":\"Invalid request\"}";

    if (server.hasArg("plain")) {
        String body = server.arg("plain");
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, body);

        if (!error && doc.containsKey("trigger")) {
            JsonObject triggerJson = doc["trigger"];

            // Extract trigger data
            int id = triggerJson.containsKey("id") ? triggerJson["id"].as<int>() : -1;

            if (id >= 0 && id < MAX_ANALOG_TRIGGERS) {
                analogTriggers[id].enabled = triggerJson["enabled"];
                strlcpy(analogTriggers[id].name, triggerJson["name"] | "Trigger", 32);
                analogTriggers[id].analogInput = triggerJson["analogInput"];
                analogTriggers[id].threshold = triggerJson["threshold"];
                analogTriggers[id].condition = triggerJson["condition"];
                analogTriggers[id].action = triggerJson["action"];
                analogTriggers[id].targetType = triggerJson["targetType"];
                analogTriggers[id].targetId = triggerJson["targetId"];

                saveConfiguration();
                response = "{\"status\":\"success\"}";
            }
        }
        else if (!error && doc.containsKey("id") && doc.containsKey("enabled")) {
            // Handle simple enable/disable
            int id = doc["id"].as<int>();
            bool enabled = doc["enabled"];

            if (id >= 0 && id < MAX_ANALOG_TRIGGERS) {
                analogTriggers[id].enabled = enabled;
                saveConfiguration();
                response = "{\"status\":\"success\"}";
            }
        }
    }

    server.send(200, "application/json", response);
}

// Handle configuration API
void handleConfig() {
    DynamicJsonDocument doc(1024);

    doc["device_name"] = deviceName;
    doc["dhcp_mode"] = dhcpMode;
    doc["debug_mode"] = debugMode;
    doc["wifi_ssid"] = wifiSSID;  // Only send SSID, not password
    doc["firmware_version"] = firmwareVersion;

    if (!dhcpMode) {
        doc["ip"] = ip.toString();
        doc["gateway"] = gateway.toString();
        doc["subnet"] = subnet.toString();
        doc["dns1"] = dns1.toString();
        doc["dns2"] = dns2.toString();
    }

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

// Update configuration handler
void handleUpdateConfig() {
    String response = "{\"status\":\"error\",\"message\":\"Invalid request\"}";

    if (server.hasArg("plain")) {
        String body = server.arg("plain");
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, body);

        if (!error) {
            bool changed = false;

            if (doc.containsKey("device_name")) {
                deviceName = doc["device_name"].as<String>();
                changed = true;
            }

            if (doc.containsKey("debug_mode")) {
                debugMode = doc["debug_mode"];
                changed = true;
            }

            if (doc.containsKey("dhcp_mode")) {
                dhcpMode = doc["dhcp_mode"];

                // Static IP settings
                if (!dhcpMode) {
                    if (doc.containsKey("ip") && doc.containsKey("gateway") &&
                        doc.containsKey("subnet") && doc.containsKey("dns1")) {

                        ip.fromString(doc["ip"].as<String>());
                        gateway.fromString(doc["gateway"].as<String>());
                        subnet.fromString(doc["subnet"].as<String>());
                        dns1.fromString(doc["dns1"].as<String>());

                        if (doc.containsKey("dns2")) {
                            dns2.fromString(doc["dns2"].as<String>());
                        }
                    }
                }

                changed = true;
            }

            if (doc.containsKey("wifi_ssid") && doc.containsKey("wifi_password")) {
                String newSSID = doc["wifi_ssid"].as<String>();
                String newPass = doc["wifi_password"].as<String>();

                // Only update if values provided
                if (newSSID.length() > 0) {
                    // Store WiFi credentials in EEPROM
                    saveWiFiCredentials(newSSID, newPass);
                    changed = true;
                }
            }

            if (changed) {
                saveConfiguration();
                response = "{\"status\":\"success\",\"msg\":\"Configuration updated. Device will restart.\"}";

                // Set flag to restart device after response is sent
                restartRequired = true;
            }
        }
    }

    server.send(200, "application/json", response);

    // Restart if needed
    if (restartRequired) {
        delay(1000);
        ESP.restart();
    }
}

// Handle debug API
void handleDebug() {
    DynamicJsonDocument doc(4096);

    doc["i2c_errors"] = i2cErrorCount;
    doc["last_error"] = lastErrorMessage;
    doc["uptime_ms"] = millis();
    doc["free_heap"] = ESP.getFreeHeap();
    doc["cpu_freq"] = ESP.getCpuFreqMHz();
    doc["firmware_version"] = firmwareVersion;

    // Check internet connectivity
    time_t now;
    time(&now);
    doc["internet_connected"] = (now > 1600000000);  // Reasonable timestamp indicates NTP sync worked

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

// Handle debug command
void handleDebugCommand() {
    String response = "{\"status\":\"error\",\"message\":\"Invalid request\"}";

    if (server.hasArg("plain")) {
        String body = server.arg("plain");
        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, body);

        if (!error && doc.containsKey("command")) {
            String command = doc["command"].as<String>();
            String commandResponse = processCommand(command);

            DynamicJsonDocument responseDoc(1024);
            responseDoc["status"] = "success";
            responseDoc["response"] = commandResponse;

            String jsonResponse;
            serializeJson(responseDoc, jsonResponse);

            response = jsonResponse;
        }
    }

    server.send(200, "application/json", response);
}

// Handle reboot endpoint
void handleReboot() {
    server.send(200, "application/json", "{\"status\":\"success\",\"message\":\"Rebooting device...\"}");
    delay(500);
    ESP.restart();
}

// Handle communication status endpoint
void handleCommunicationStatus() {
    DynamicJsonDocument doc(1024);

    doc["usb_available"] = true;  // Serial is always available on ESP32
    doc["wifi_connected"] = wifiConnected;
    doc["wifi_rssi"] = WiFi.RSSI();
    doc["eth_connected"] = ethConnected;
    doc["rs485_available"] = true;

    doc["active_protocol"] = currentCommunicationProtocol;

    // I2C status
    doc["i2c_status"] = (i2cErrorCount == 0) ? "OK" : "Issues detected";
    doc["i2c_error_count"] = i2cErrorCount;

    // Protocol-specific details
    if (currentCommunicationProtocol == "wifi") {
        doc["wifi_security"] = wifiSecurity;
        doc["wifi_ssid"] = wifiSSID;
        doc["wifi_channel"] = wifiChannel;
    }
    else if (currentCommunicationProtocol == "ethernet") {
        doc["eth_mac"] = ethConnected ? ETH.macAddress() : "Disconnected";
        doc["eth_ip"] = ethConnected ? ETH.localIP().toString() : "Unknown";
        doc["eth_speed"] = ethConnected ? String(ETH.linkSpeed()) + " Mbps" : "Unknown";
    }
    else if (currentCommunicationProtocol == "rs485") {
        doc["rs485_baud"] = rs485BaudRate;
        doc["rs485_protocol"] = rs485Protocol;
        doc["rs485_address"] = rs485DeviceAddress;
    }
    else if (currentCommunicationProtocol == "usb") {
        doc["usb_baud"] = usbBaudRate;
        doc["usb_com"] = usbComPort;
    }

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

// Handle set communication protocol
void handleSetCommunication() {
    String response = "{\"status\":\"error\",\"message\":\"Invalid request\"}";

    if (server.hasArg("plain")) {
        String body = server.arg("plain");
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, body);

        if (!error) {
            if (doc.containsKey("protocol")) {
                String protocol = doc["protocol"];

                // Update the communication protocol
                if (protocol == "usb" || protocol == "wifi" || protocol == "ethernet" || protocol == "rs485") {
                    currentCommunicationProtocol = protocol;

                    // Apply protocol-specific settings
                    if (protocol == "rs485") {
                        // Initialize RS485 with current settings
                        initRS485();
                    }

                    // Save to EEPROM
                    saveCommunicationSettings();

                    response = "{\"status\":\"success\",\"protocol\":\"" + protocol + "\"}";
                }
            }
        }
    }

    server.send(200, "application/json", response);
}

// Handle communication configuration GET request
void handleCommunicationConfig() {
    DynamicJsonDocument doc(2048);

    // Get protocol from query string
    String protocol = server.hasArg("protocol") ? server.arg("protocol") : currentCommunicationProtocol;
    doc["protocol"] = protocol;

    // Return configuration based on protocol
    if (protocol == "wifi") {
        doc["security"] = wifiSecurity;
        doc["hidden"] = wifiHidden;
        doc["mac_filter"] = wifiMacFilter;
        doc["auto_update"] = wifiAutoUpdate;
        doc["radio_mode"] = wifiRadioMode;
        doc["channel"] = wifiChannel;
        doc["channel_width"] = wifiChannelWidth;
        doc["dhcp_lease_time"] = wifiDhcpLeaseTime;
        doc["wmm_enabled"] = wifiWmmEnabled;
        doc["ssid"] = wifiSSID;

        // Extra network info
        if (wifiConnected) {
            doc["ip"] = WiFi.localIP().toString();
            doc["mac"] = WiFi.macAddress();
            doc["rssi"] = WiFi.RSSI();
            doc["hostname"] = WiFi.getHostname();
        }
    }
    else if (protocol == "ethernet") {
        doc["dhcp_mode"] = dhcpMode;

        if (!dhcpMode) {
            doc["ip"] = ip.toString();
            doc["gateway"] = gateway.toString();
            doc["subnet"] = subnet.toString();
            doc["dns1"] = dns1.toString();
            doc["dns2"] = dns2.toString();
        }

        // Add current Ethernet status if connected
        if (ethConnected) {
            doc["eth_mac"] = ETH.macAddress();
            doc["eth_ip"] = ETH.localIP().toString();
            doc["eth_subnet"] = ETH.subnetMask().toString();
            doc["eth_gateway"] = ETH.gatewayIP().toString();
            doc["eth_dns"] = ETH.dnsIP().toString();
            doc["eth_duplex"] = ETH.fullDuplex() ? "Full" : "Half";
            doc["eth_speed"] = String(ETH.linkSpeed()) + " Mbps";
            doc["eth_hostname"] = ETH.getHostname();
        }
    }
    else if (protocol == "usb") {
        doc["com_port"] = usbComPort;
        doc["baud_rate"] = usbBaudRate;
        doc["data_bits"] = usbDataBits;
        doc["parity"] = usbParity;
        doc["stop_bits"] = usbStopBits;

        // Available baud rates for selection
        JsonArray baudRates = doc.createNestedArray("available_baud_rates");
        baudRates.add(9600);
        baudRates.add(19200);
        baudRates.add(38400);
        baudRates.add(57600);
        baudRates.add(115200);

        // Available data bits for selection
        JsonArray dataBits = doc.createNestedArray("available_data_bits");
        dataBits.add(7);
        dataBits.add(8);

        // Available parity options for selection
        JsonArray parityOptions = doc.createNestedArray("available_parity");
        parityOptions.add(0);  // None
        parityOptions.add(1);  // Odd
        parityOptions.add(2);  // Even

        // Available stop bits for selection
        JsonArray stopBits = doc.createNestedArray("available_stop_bits");
        stopBits.add(1);
        stopBits.add(2);
    }
    else if (protocol == "rs485") {
        doc["baud_rate"] = rs485BaudRate;
        doc["parity"] = rs485Parity;
        doc["data_bits"] = rs485DataBits;
        doc["stop_bits"] = rs485StopBits;
        doc["protocol_type"] = rs485Protocol;
        doc["comm_mode"] = rs485Mode;
        doc["device_address"] = rs485DeviceAddress;
        doc["flow_control"] = rs485FlowControl;
        doc["night_mode"] = rs485NightMode;

        // Available protocol types for selection
        JsonArray protocolTypes = doc.createNestedArray("available_protocols");
        protocolTypes.add("Modbus RTU");
        protocolTypes.add("BACnet");
        protocolTypes.add("Custom ASCII");
        protocolTypes.add("Custom Binary");

        // Available communication modes for selection
        JsonArray commModes = doc.createNestedArray("available_modes");
        commModes.add("Half-duplex");
        commModes.add("Full-duplex");
        commModes.add("Log Mode");
        commModes.add("NMEA Mode");
        commModes.add("TCP ASCII");
        commModes.add("TCP Binary");

        // Available baud rates for selection
        JsonArray baudRates = doc.createNestedArray("available_baud_rates");
        baudRates.add(1200);
        baudRates.add(2400);
        baudRates.add(4800);
        baudRates.add(9600);
        baudRates.add(19200);
        baudRates.add(38400);
        baudRates.add(57600);
        baudRates.add(115200);
    }

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

// Handle update communication configuration
void handleUpdateCommunicationConfig() {
    String response = "{\"status\":\"error\",\"message\":\"Invalid request\"}";

    if (server.hasArg("plain")) {
        String body = server.arg("plain");
        DynamicJsonDocument doc(2048);
        DeserializationError error = deserializeJson(doc, body);

        if (!error && doc.containsKey("protocol")) {
            String protocol = doc["protocol"];
            bool changed = false;

            // Process protocol-specific settings
            if (protocol == "wifi") {
                if (doc.containsKey("security")) {
                    wifiSecurity = doc["security"].as<String>();
                    changed = true;
                }
                if (doc.containsKey("hidden")) {
                    wifiHidden = doc["hidden"].as<bool>();
                    changed = true;
                }
                if (doc.containsKey("mac_filter")) {
                    wifiMacFilter = doc["mac_filter"].as<String>();
                    changed = true;
                }
                if (doc.containsKey("auto_update")) {
                    wifiAutoUpdate = doc["auto_update"].as<bool>();
                    changed = true;
                }
                if (doc.containsKey("radio_mode")) {
                    wifiRadioMode = doc["radio_mode"].as<String>();
                    changed = true;
                }
                if (doc.containsKey("channel")) {
                    wifiChannel = doc["channel"].as<int>();
                    changed = true;
                }
                if (doc.containsKey("channel_width")) {
                    wifiChannelWidth = doc["channel_width"].as<int>();
                    changed = true;
                }
                if (doc.containsKey("dhcp_lease_time")) {
                    wifiDhcpLeaseTime = doc["dhcp_lease_time"].as<long>();
                    changed = true;
                }
                if (doc.containsKey("wmm_enabled")) {
                    wifiWmmEnabled = doc["wmm_enabled"].as<bool>();
                    changed = true;
                }
                // WiFi credentials are handled separately in handleUpdateConfig
            }
            else if (protocol == "usb") {
                if (doc.containsKey("com_port")) {
                    usbComPort = doc["com_port"].as<int>();
                    changed = true;
                }
                if (doc.containsKey("baud_rate")) {
                    usbBaudRate = doc["baud_rate"].as<int>();
                    changed = true;
                }
                if (doc.containsKey("data_bits")) {
                    usbDataBits = doc["data_bits"].as<int>();
                    changed = true;
                }
                if (doc.containsKey("parity")) {
                    usbParity = doc["parity"].as<int>();
                    changed = true;
                }
                if (doc.containsKey("stop_bits")) {
                    usbStopBits = doc["stop_bits"].as<int>();
                    changed = true;
                }
            }
            else if (protocol == "rs485") {
                if (doc.containsKey("baud_rate")) {
                    rs485BaudRate = doc["baud_rate"].as<int>();
                    changed = true;
                }
                if (doc.containsKey("parity")) {
                    rs485Parity = doc["parity"].as<int>();
                    changed = true;
                }
                if (doc.containsKey("data_bits")) {
                    rs485DataBits = doc["data_bits"].as<int>();
                    changed = true;
                }
                if (doc.containsKey("stop_bits")) {
                    rs485StopBits = doc["stop_bits"].as<int>();
                    changed = true;
                }
                if (doc.containsKey("protocol_type")) {
                    rs485Protocol = doc["protocol_type"].as<String>();
                    changed = true;
                }
                if (doc.containsKey("comm_mode")) {
                    rs485Mode = doc["comm_mode"].as<String>();
                    changed = true;
                }
                if (doc.containsKey("device_address")) {
                    rs485DeviceAddress = doc["device_address"].as<int>();
                    changed = true;
                }
                if (doc.containsKey("flow_control")) {
                    rs485FlowControl = doc["flow_control"].as<bool>();
                    changed = true;
                }
                if (doc.containsKey("night_mode")) {
                    rs485NightMode = doc["night_mode"].as<bool>();
                    changed = true;
                }
            }

            // If any settings changed, save and return success
            if (changed) {
                saveCommunicationConfig();

                // Re-initialize interface if necessary
                if (protocol == "rs485" && currentCommunicationProtocol == "rs485") {
                    initRS485();
                }

                response = "{\"status\":\"success\",\"message\":\"Communication settings updated\"}";
            }
        }
    }

    server.send(200, "application/json", response);
}

// Handle get time endpoint
void handleGetTime() {
    DynamicJsonDocument doc(256);

    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    doc["year"] = timeinfo.tm_year + 1900;
    doc["month"] = timeinfo.tm_mon + 1;
    doc["day"] = timeinfo.tm_mday;
    doc["hour"] = timeinfo.tm_hour;
    doc["minute"] = timeinfo.tm_min;
    doc["second"] = timeinfo.tm_sec;
    doc["formatted"] = getTimeString();
    doc["rtc_available"] = rtcInitialized;

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

// Handle set time endpoint
void handleSetTime() {
    String response = "{\"status\":\"error\",\"message\":\"Invalid request\"}";

    if (server.hasArg("plain")) {
        String body = server.arg("plain");
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
                syncTimeFromClient(year, month, day, hour, minute, second);

                response = "{\"status\":\"success\",\"message\":\"Time updated\"}";
            }
            else if (doc.containsKey("ntp_sync") && doc["ntp_sync"].as<bool>()) {
                // Sync time from NTP
                syncTimeFromNTP();
                response = "{\"status\":\"success\",\"message\":\"NTP sync initiated\"}";
            }
        }
    }

    server.send(200, "application/json", response);
}

// Handle I2C scan endpoint
void handleI2CScan() {
    DynamicJsonDocument doc(1024);
    JsonArray devices = doc.createNestedArray("devices");

    // Scan I2C bus
    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        uint8_t error = Wire.endTransmission();

        if (error == 0) {
            JsonObject device = devices.createNestedObject();
            device["address"] = "0x" + String(address, HEX);

            // Identify known devices
            if (address == PCF8574_INPUTS_1_8) {
                device["name"] = "PCF8574 Inputs 1-8";
            }
            else if (address == PCF8574_INPUTS_9_16) {
                device["name"] = "PCF8574 Inputs 9-16";
            }
            else if (address == PCF8574_OUTPUTS_1_8) {
                device["name"] = "PCF8574 Outputs 1-8";
            }
            else if (address == PCF8574_OUTPUTS_9_16) {
                device["name"] = "PCF8574 Outputs 9-16";
            }
            else if (address == 0x68) {
                device["name"] = "DS3231 RTC";
            }
            else {
                device["name"] = "Unknown device";
            }
        }
    }

    doc["total_devices"] = devices.size();

    String response;
    serializeJson(doc, response);
    server.send(200, "application/json", response);
}

// Process commands received via Serial
void processSerialCommands() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();

        String response = processCommand(command);
        Serial.println(response);
    }
}

// Process commands received via RS485
void processRS485Commands() {
    if (rs485.available()) {
        String command = rs485.readStringUntil('\n');
        command.trim();

        String response = processCommand(command);
        rs485.println(response);
    }
}

// Process command and return response
String processCommand(String command) {
    if (command.startsWith("RELAY ")) {
        // Relay control command
        command = command.substring(6);

        if (command.startsWith("STATUS")) {
            // Return relay status
            String response = "RELAY STATUS:\n";
            for (int i = 0; i < 16; i++) {
                response += String(i + 1) + " (Relay " + String(i + 1) + "): ";
                response += outputStates[i] ? "ON" : "OFF";
                response += "\n";
            }
            return response;
        }
        else if (command.startsWith("ALL ON")) {
            // Turn all relays on
            for (int i = 0; i < 16; i++) {
                outputStates[i] = true;
            }
            if (writeOutputs()) {
                broadcastUpdate();
                return "All relays turned ON";
            }
            else {
                return "ERROR: Failed to turn all relays ON";
            }
        }
        else if (command.startsWith("ALL OFF")) {
            // Turn all relays off
            for (int i = 0; i < 16; i++) {
                outputStates[i] = false;
            }
            if (writeOutputs()) {
                broadcastUpdate();
                return "All relays turned OFF";
            }
            else {
                return "ERROR: Failed to turn all relays OFF";
            }
        }
        else {
            // Individual relay control: RELAY <number> <ON|OFF>
            int spacePos = command.indexOf(' ');
            if (spacePos > 0) {
                int relayNum = command.substring(0, spacePos).toInt();
                String action = command.substring(spacePos + 1);

                if (relayNum >= 1 && relayNum <= 16) {
                    int index = relayNum - 1;
                    if (action == "ON") {
                        outputStates[index] = true;
                        if (writeOutputs()) {
                            broadcastUpdate();
                            return "Relay " + String(relayNum) + " turned ON";
                        }
                        else {
                            return "ERROR: Failed to turn relay ON";
                        }
                    }
                    else if (action == "OFF") {
                        outputStates[index] = false;
                        if (writeOutputs()) {
                            broadcastUpdate();
                            return "Relay " + String(relayNum) + " turned OFF";
                        }
                        else {
                            return "ERROR: Failed to turn relay OFF";
                        }
                    }
                }
            }
        }

        return "ERROR: Invalid relay command";
    }
    else if (command.startsWith("INPUT STATUS")) {
        // Return input status
        String response = "INPUT STATUS:\n";
        for (int i = 0; i < 16; i++) {
            response += String(i + 1) + " (Input " + String(i + 1) + "): ";
            response += inputStates[i] ? "HIGH" : "LOW";
            response += "\n";
        }
        return response;
    }
    else if (command.startsWith("ANALOG STATUS")) {
        // Return analog input status
        String response = "ANALOG STATUS:\n";
        for (int i = 0; i < 4; i++) {
            response += String(i + 1) + " (Analog " + String(i + 1) + "): ";
            response += String(analogValues[i]) + " (" + String(map(analogValues[i], 0, 4095, 0, 100)) + "%)";
            response += "\n";
        }
        return response;
    }
    else if (command.startsWith("COMM STATUS")) {
        // Return communication status
        String response = "COMMUNICATION STATUS:\n";
        response += "Active Protocol: " + currentCommunicationProtocol + "\n";
        response += "WiFi Connected: " + String(wifiConnected ? "Yes" : "No") + "\n";
        response += "Ethernet Connected: " + String(ethConnected ? "Yes" : "No") + "\n";
        response += "RS485 Available: Yes\n";
        response += "USB Available: Yes\n";

        // Add protocol-specific details
        if (currentCommunicationProtocol == "wifi") {
            response += "\nWIFI DETAILS:\n";
            response += "SSID: " + wifiSSID + "\n";
            response += "Security: " + wifiSecurity + "\n";
            response += "Channel: " + String(wifiChannel) + "\n";
            if (wifiConnected) {
                response += "IP: " + WiFi.localIP().toString() + "\n";
                response += "Signal: " + String(WiFi.RSSI()) + " dBm\n";
            }
        }
        else if (currentCommunicationProtocol == "ethernet") {
            response += "\nETHERNET DETAILS:\n";
            if (ethConnected) {
                response += "MAC: " + ETH.macAddress() + "\n";
                response += "IP: " + ETH.localIP().toString() + "\n";
                response += "Speed: " + String(ETH.linkSpeed()) + " Mbps\n";
                response += "Duplex: " + String(ETH.fullDuplex() ? "Full" : "Half") + "\n";
            }
            else {
                response += "Status: Disconnected\n";
            }
        }
        else if (currentCommunicationProtocol == "rs485") {
            response += "\nRS485 DETAILS:\n";
            response += "Baud Rate: " + String(rs485BaudRate) + "\n";
            response += "Protocol: " + rs485Protocol + "\n";
            response += "Mode: " + rs485Mode + "\n";
            response += "Address: " + String(rs485DeviceAddress) + "\n";
        }
        else if (currentCommunicationProtocol == "usb") {
            response += "\nUSB DETAILS:\n";
            response += "COM Port: " + String(usbComPort) + "\n";
            response += "Baud Rate: " + String(usbBaudRate) + "\n";
            response += "Data Bits: " + String(usbDataBits) + "\n";
            response += "Parity: " + String(usbParity == 0 ? "None" : usbParity == 1 ? "Odd" : "Even") + "\n";
            response += "Stop Bits: " + String(usbStopBits) + "\n";
        }

        return response;
    }
    else if (command.startsWith("SCAN I2C")) {
        // Scan I2C bus
        String response = "I2C DEVICES:\n";
        int deviceCount = 0;

        for (uint8_t address = 1; address < 127; address++) {
            Wire.beginTransmission(address);
            uint8_t error = Wire.endTransmission();

            if (error == 0) {
                deviceCount++;
                response += "0x" + String(address, HEX) + " - ";

                // Identify known devices
                if (address == PCF8574_INPUTS_1_8) {
                    response += "PCF8574 Inputs 1-8";
                }
                else if (address == PCF8574_INPUTS_9_16) {
                    response += "PCF8574 Inputs 9-16";
                }
                else if (address == PCF8574_OUTPUTS_1_8) {
                    response += "PCF8574 Outputs 1-8";
                }
                else if (address == PCF8574_OUTPUTS_9_16) {
                    response += "PCF8574 Outputs 9-16";
                }
                else if (address == 0x68) {
                    response += "DS3231 RTC";
                }
                else {
                    response += "Unknown device";
                }

                response += "\n";
            }
        }

        response += "Found " + String(deviceCount) + " device(s)\n";
        return response;
    }
    else if (command == "STATUS") {
        // Return system status
        String response = "KC868-A16 System Status\n";
        response += "---------------------\n";
        response += "Device: " + deviceName + "\n";
        response += "Firmware: " + firmwareVersion + "\n";
        response += "Uptime: " + String(millis() / 1000) + " seconds\n";

        if (wifiConnected) {
            response += "WiFi: Connected, IP: " + WiFi.localIP().toString() + "\n";
        }
        else {
            response += "WiFi: Not connected\n";
        }

        if (ethConnected) {
            response += "Ethernet: Connected, IP: " + ETH.localIP().toString() + "\n";
        }
        else {
            response += "Ethernet: Not connected\n";
        }

        response += "Active Protocol: " + currentCommunicationProtocol + "\n";
        response += "I2C errors: " + String(i2cErrorCount) + "\n";
        response += "RTC available: " + String(rtcInitialized ? "Yes" : "No") + "\n";
        response += "Current time: " + getTimeString() + "\n";
        response += "Free heap: " + String(ESP.getFreeHeap()) + " bytes\n";

        return response;
    }
    else if (command == "HELP") {
        // Return help information
        String response = "KC868-A16 Command Help\n";
        response += "---------------------\n";
        response += "RELAY STATUS - Show all relay states\n";
        response += "RELAY ALL ON - Turn all relays on\n";
        response += "RELAY ALL OFF - Turn all relays off\n";
        response += "RELAY <num> ON - Turn relay on (1-16)\n";
        response += "RELAY <num> OFF - Turn relay off (1-16)\n";
        response += "INPUT STATUS - Show all input states\n";
        response += "ANALOG STATUS - Show all analog input values\n";
        response += "COMM STATUS - Show communication interface status\n";
        response += "SCAN I2C - Scan for I2C devices\n";
        response += "STATUS - Show system status\n";
        response += "DEBUG ON - Enable debug mode\n";
        response += "DEBUG OFF - Disable debug mode\n";
        response += "SET TIME <yyyy-mm-dd hh:mm:ss> - Set system time\n";
        response += "REBOOT - Restart the system\n";
        response += "VERSION - Show firmware version\n";

        return response;
    }
    else if (command.startsWith("SET TIME ")) {
        // Set time command
        String timeStr = command.substring(9);

        // Format should be "yyyy-mm-dd hh:mm:ss"
        if (timeStr.length() == 19) {
            int year = timeStr.substring(0, 4).toInt();
            int month = timeStr.substring(5, 7).toInt();
            int day = timeStr.substring(8, 10).toInt();
            int hour = timeStr.substring(11, 13).toInt();
            int minute = timeStr.substring(14, 16).toInt();
            int second = timeStr.substring(17, 19).toInt();

            if (year >= 2023 && month >= 1 && month <= 12 && day >= 1 && day <= 31 &&
                hour >= 0 && hour <= 23 && minute >= 0 && minute <= 59 && second >= 0 && second <= 59) {

                syncTimeFromClient(year, month, day, hour, minute, second);
                return "Time set successfully: " + getTimeString();
            }
        }

        return "ERROR: Invalid time format. Use SET TIME yyyy-mm-dd hh:mm:ss";
    }
    else if (command == "DEBUG ON") {
        debugMode = true;
        return "Debug mode enabled";
    }
    else if (command == "DEBUG OFF") {
        debugMode = false;
        return "Debug mode disabled";
    }
    else if (command == "VERSION") {
        return "KC868-A16 Controller firmware version " + firmwareVersion;
    }
    else if (command == "REBOOT") {
        String response = "Rebooting system...";
        delay(100);
        ESP.restart();
        return response;
    }

    return "ERROR: Unknown command. Type HELP for commands.";
}

// Check schedules against current time
void checkSchedules() {
    // Get current time
    DateTime now;
    if (rtcInitialized) {
        now = rtc.now();
    }
    else {
        // Use ESP32 time if RTC not available
        time_t nowTime;
        struct tm timeinfo;
        time(&nowTime);
        localtime_r(&nowTime, &timeinfo);

        now = DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
            timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }

    uint8_t currentDay = now.dayOfTheWeek();
    if (currentDay == 0) currentDay = 7;  // Convert Sunday from 0 to 7
    uint8_t currentDayBit = 1 << (currentDay - 1);

    // Check each schedule
    for (int i = 0; i < MAX_SCHEDULES; i++) {
        if (schedules[i].enabled) {
            // Check if this schedule should run today
            if (schedules[i].days & currentDayBit) {
                // Check if it's time to run this schedule
                if (now.hour() == schedules[i].hour && now.minute() == schedules[i].minute && now.second() < 2) {
                    debugPrintln("Running schedule: " + String(schedules[i].name));

                    // Perform the scheduled action
                    if (schedules[i].targetType == 0) {
                        // Single output
                        uint8_t relay = schedules[i].targetId;
                        if (relay < 16) {
                            if (schedules[i].action == 0) {        // OFF
                                outputStates[relay] = false;
                            }
                            else if (schedules[i].action == 1) { // ON
                                outputStates[relay] = true;
                            }
                            else if (schedules[i].action == 2) { // TOGGLE
                                outputStates[relay] = !outputStates[relay];
                            }
                        }
                    }
                    else if (schedules[i].targetType == 1) {
                        // Multiple outputs (using bitmask)
                        for (int j = 0; j < 16; j++) {
                            if (schedules[i].targetId & (1 << j)) {
                                if (schedules[i].action == 0) {        // OFF
                                    outputStates[j] = false;
                                }
                                else if (schedules[i].action == 1) { // ON
                                    outputStates[j] = true;
                                }
                                else if (schedules[i].action == 2) { // TOGGLE
                                    outputStates[j] = !outputStates[j];
                                }
                            }
                        }
                    }

                    // Update outputs
                    writeOutputs();

                    // Broadcast update
                    broadcastUpdate();
                }
            }
        }
    }
}

// Check analog triggers against current values
void checkAnalogTriggers() {
    for (int i = 0; i < MAX_ANALOG_TRIGGERS; i++) {
        if (analogTriggers[i].enabled) {
            uint8_t analogInput = analogTriggers[i].analogInput;

            if (analogInput < 4) {
                int value = analogValues[analogInput];
                bool triggerConditionMet = false;

                // Check condition
                if (analogTriggers[i].condition == 0) {     // Above
                    triggerConditionMet = (value > analogTriggers[i].threshold);
                }
                else if (analogTriggers[i].condition == 1) { // Below
                    triggerConditionMet = (value < analogTriggers[i].threshold);
                }
                else if (analogTriggers[i].condition == 2) { // Equal (with some tolerance)
                    triggerConditionMet = (abs(value - analogTriggers[i].threshold) < 50);
                }

                if (triggerConditionMet) {
                    debugPrintln("Analog trigger activated: " + String(analogTriggers[i].name));

                    // Perform the trigger action
                    if (analogTriggers[i].targetType == 0) {
                        // Single output
                        uint8_t relay = analogTriggers[i].targetId;
                        if (relay < 16) {
                            if (analogTriggers[i].action == 0) {        // OFF
                                outputStates[relay] = false;
                            }
                            else if (analogTriggers[i].action == 1) { // ON
                                outputStates[relay] = true;
                            }
                            else if (analogTriggers[i].action == 2) { // TOGGLE
                                outputStates[relay] = !outputStates[relay];
                            }
                        }
                    }
                    else if (analogTriggers[i].targetType == 1) {
                        // Multiple outputs (using bitmask)
                        for (int j = 0; j < 16; j++) {
                            if (analogTriggers[i].targetId & (1 << j)) {
                                if (analogTriggers[i].action == 0) {        // OFF
                                    outputStates[j] = false;
                                }
                                else if (analogTriggers[i].action == 1) { // ON
                                    outputStates[j] = true;
                                }
                                else if (analogTriggers[i].action == 2) { // TOGGLE
                                    outputStates[j] = !outputStates[j];
                                }
                            }
                        }
                    }

                    // Update outputs
                    writeOutputs();

                    // Broadcast update
                    broadcastUpdate();
                }
            }
        }
    }
}

// WiFi event handler
void WiFiEvent(WiFiEvent_t event) {
    switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        wifiConnected = true;
        debugPrintln("WiFi connected. IP: " + WiFi.localIP().toString());
        break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        wifiConnected = false;
        debugPrintln("WiFi connection lost");
        break;
    default:
        break;
    }
}

// Debug print function
void debugPrintln(String message) {
    if (debugMode) {
        Serial.println(message);
    }
}