/**
 * NetworkManager.cpp - Network management for KC868-A16
 * Created by Your Name, Date
 * Released into the public domain.
 */

#include "NetworkManager.h"
#include <EEPROM.h>
#include <ArduinoJson.h>


 // Add this global pointer at the top of the file, after includes
KC868NetworkManager* globalNetworkManagerPtr = nullptr;

KC868NetworkManager::KC868NetworkManager() :
    _ethConnected(false),
    _wifiConnected(false),
    _apMode(false),
    _wifiClientMode(false),
    _wiredMode(false),
    _dhcpMode(true),
    _wifiSecurity("WPA2"),
    _wifiHidden(false),
    _wifiMacFilter(""),
    _wifiAutoUpdate(true),
    _wifiRadioMode("802.11n"),
    _wifiChannel(6),
    _wifiChannelWidth(20),
    _wifiDhcpLeaseTime(86400),
    _wifiWmmEnabled(true)
{
    // Set the global pointer to this instance for static callbacks
    globalNetworkManagerPtr = this;

    // Default subnet mask
    _subnet = IPAddress(255, 255, 255, 0);

    // Default DNS servers
    _dns1 = IPAddress(8, 8, 8, 8);
    _dns2 = IPAddress(8, 8, 4, 4);
}

void KC868NetworkManager::initEthernet() {
    // Register Ethernet event handler
    WiFi.onEvent(EthEvent);

    // Initialize Ethernet with ETH_PHY_POWER set to -1 (no power pin control)
    // The LAN8720 is likely permanently powered on the board
    Serial.println("Starting Ethernet initialization...");

    // Try a reset sequence using delay timing instead of GPIO control
    delay(200); // Allow time for system stabilization

    // Initialize with default PHY address first
    ETH.begin(ETH_PHY_LAN8720, ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER, ETH_CLOCK_GPIO17_OUT);

    // If that doesn't work after a short wait, try alternative PHY addresses
    delay(1000);
    if (!ETH.linkUp()) {
        Serial.println("Trying alternative PHY address 0...");
        ETH.begin(ETH_PHY_LAN8720, 0, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER, ETH_CLOCK_GPIO17_OUT);
    }

    delay(1000);
    if (!ETH.linkUp()) {
        Serial.println("Trying alternative PHY address 1...");
        ETH.begin(ETH_PHY_LAN8720, 1, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER, ETH_CLOCK_GPIO17_OUT);
    }

    // Configure in DHCP or static mode
    if (!_dhcpMode) {
        ETH.config(_ip, _gateway, _subnet, _dns1, _dns2);
    }

    extern String deviceName;
    // Set hostname
    ETH.setHostname(deviceName.c_str());

    // Wait for Ethernet to establish link
    Serial.println("Waiting for Ethernet link...");
    int ethAttempts = 0;
    const int maxAttempts = 10;  // Increased timeout
    while (!ETH.linkUp() && ethAttempts < maxAttempts) {
        delay(1000);
        ethAttempts++;
        Serial.println("Waiting for Ethernet link... attempt " + String(ethAttempts) + "/" + String(maxAttempts));
    }

    // Check if Ethernet is connected after attempts
    if (ETH.linkUp()) {
        _wiredMode = true;
        _ethConnected = true;
        Serial.println("Ethernet link is UP!");

        // Wait for IP address
        int ipAttempts = 0;
        while (ETH.localIP().toString() == "0.0.0.0" && ipAttempts < maxAttempts) {
            delay(1000);
            ipAttempts++;
            Serial.println("Waiting for IP address... attempt " + String(ipAttempts) + "/" + String(maxAttempts));
        }

        if (ETH.localIP().toString() != "0.0.0.0") {
            Serial.println("Ethernet MAC: " + ETH.macAddress());
            Serial.println("Ethernet IP: " + ETH.localIP().toString());

            // If Ethernet is connected, prefer it over WiFi
            if (_wifiClientMode && !_apMode) {
                WiFi.disconnect();
                _wifiClientMode = false;
                _wifiConnected = false;
                Serial.println("WiFi client mode disabled since Ethernet is connected");
            }
        }
        else {
            Serial.println("Failed to get IP address via Ethernet");
            _ethConnected = false;
            _wiredMode = false;
        }
    }
    else {
        Serial.println("Ethernet link is DOWN. Check cable connection or LAN8720 initialization.");
        _wiredMode = false;
        _ethConnected = false;
    }
}

void KC868NetworkManager::initWiFi() {
    // Register event handler
    WiFi.onEvent(WiFiEvent);

    // Load WiFi credentials
    loadWiFiCredentials();

    // Configure in DHCP or static mode
    if (_dhcpMode) {
        WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);
    }
    else {
        WiFi.config(_ip, _gateway, _subnet, _dns1, _dns2);
    }

    // Set hostname
    WiFi.setHostname("KC868-A16");

    // Attempt to connect if credentials exist
    if (_wifiSSID.length() > 0) {
        WiFi.begin(_wifiSSID.c_str(), _wifiPassword.c_str());
        Serial.println("Connecting to WiFi SSID: " + _wifiSSID);

        // Wait for connection with timeout
        int connectionAttempts = 0;
        while (WiFi.status() != WL_CONNECTED && connectionAttempts < 20) {
            delay(500);
            Serial.print(".");
            connectionAttempts++;
        }

        if (WiFi.status() == WL_CONNECTED) {
            _wifiConnected = true;
            _wifiClientMode = true;
            _apMode = false;

            Serial.println("");
            Serial.print("Connected to WiFi. IP address: ");
            Serial.println(WiFi.localIP());
            _mac = WiFi.macAddress();

            // After successful connection, update saved credentials
            saveWiFiCredentials(_wifiSSID, _wifiPassword);
            return;
        }
    }

    // If we reach here, connection failed or no credentials - start AP
    startAPMode();
}

void KC868NetworkManager::startAPMode() {
    WiFi.disconnect();
    delay(100);

    // Start AP mode
    WiFi.mode(WIFI_AP);
    WiFi.softAP(_ap_ssid, _ap_password);

    _apMode = true;
    _wifiClientMode = false;
    _wifiConnected = true; // Device is "connected" in AP mode

    Serial.println("Failed to connect as client. Starting AP Mode");
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
}

void KC868NetworkManager::startDNSServer() {
    if (_apMode) {
        _dnsServer.start(53, "*", WiFi.softAPIP());
        Serial.println("DNS Server started for captive portal");
    }
}

void KC868NetworkManager::processDNSRequests() {
    if (_apMode) {
        _dnsServer.processNextRequest();
    }
}

void KC868NetworkManager::resetEthernet() {
    // Software reset for Ethernet module
    Serial.println("Performing software reset of Ethernet module...");

    // For ESP32, we can try the following software approach:
    // 1. Delay to ensure system stability
    delay(500);

    // 2. Initialize Ethernet with different timing
    WiFi.mode(WIFI_OFF);  // Turn off WiFi to reduce interference
    delay(200);

    // Set ESP32 to higher CPU frequency during initialization
    setCpuFrequencyMhz(240);

    // Try to initialize Ethernet
    ETH.begin(ETH_PHY_LAN8720, ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER, ETH_CLOCK_GPIO17_OUT);

    // Return to normal CPU frequency if needed
    // setCpuFrequencyMhz(160);

    Serial.println("Ethernet software reset complete");
}

void KC868NetworkManager::checkNetworkStatus() {
    // If Ethernet was connected but now disconnected, try to reconnect WiFi
    if (_wiredMode && !ETH.linkUp()) {
        _wiredMode = false;
        _ethConnected = false;

        // Try to reconnect WiFi if we have credentials
        if (_wifiSSID.length() > 0 && !_wifiClientMode && !_apMode) {
            WiFi.begin(_wifiSSID.c_str(), _wifiPassword.c_str());
            Serial.println("Ethernet disconnected, trying WiFi...");
        }
    }

    // If WiFi client was connected but now disconnected, try to reconnect
    if (_wifiClientMode && WiFi.status() != WL_CONNECTED) {
        _wifiClientMode = false;
        _wifiConnected = false;

        // Try reconnecting to WiFi
        if (!_ethConnected && !_apMode) {
            WiFi.reconnect();
            Serial.println("WiFi disconnected, trying to reconnect...");
        }
    }
}

void KC868NetworkManager::printNetworkStatus() {
    if (_ethConnected) {
        Serial.println("Using Ethernet connection");
        Serial.print("IP: ");
        Serial.println(ETH.localIP());
    }
    else if (_wifiClientMode) {
        Serial.println("Using WiFi Client connection");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
    }
    else if (_apMode) {
        Serial.println("Running in Access Point mode");
        Serial.print("AP IP: ");
        Serial.println(WiFi.softAPIP());
    }
}

void KC868NetworkManager::saveNetworkSettings() {
    DynamicJsonDocument doc(512);

    doc["dhcp_mode"] = _dhcpMode;

    if (!_dhcpMode) {
        doc["ip"] = _ip.toString();
        doc["gateway"] = _gateway.toString();
        doc["subnet"] = _subnet.toString();
        doc["dns1"] = _dns1.toString();
        doc["dns2"] = _dns2.toString();
    }

    // Serialize to buffer
    char jsonBuffer[512];
    size_t n = serializeJson(doc, jsonBuffer);

    // Define a specific address for network settings
    const int NETWORK_SETTINGS_ADDR = 3700;

    // Store in EEPROM
    for (size_t i = 0; i < n && i < 256; i++) {
        EEPROM.write(NETWORK_SETTINGS_ADDR + i, jsonBuffer[i]);
    }

    // Write null terminator
    EEPROM.write(NETWORK_SETTINGS_ADDR + n, 0);

    // Commit changes
    EEPROM.commit();

    Serial.println("Network settings saved to EEPROM");
}

void KC868NetworkManager::loadNetworkSettings() {
    // Create a buffer to read JSON data
    char jsonBuffer[512];
    size_t i = 0;

    // Define a specific address for network settings
    const int NETWORK_SETTINGS_ADDR = 3700;

    // Read data until null terminator or max buffer size
    while (i < 511) {
        jsonBuffer[i] = EEPROM.read(NETWORK_SETTINGS_ADDR + i);
        if (jsonBuffer[i] == 0) break;
        i++;
    }

    // Add null terminator if buffer is full
    jsonBuffer[i] = 0;

    // If we read something, try to parse it
    if (i > 0) {
        DynamicJsonDocument doc(512);
        DeserializationError error = deserializeJson(doc, jsonBuffer);

        if (!error) {
            _dhcpMode = doc["dhcp_mode"] | true;

            if (!_dhcpMode) {
                String ipStr = doc["ip"] | "192.168.1.100";
                String gatewayStr = doc["gateway"] | "192.168.1.1";
                String subnetStr = doc["subnet"] | "255.255.255.0";
                String dns1Str = doc["dns1"] | "8.8.8.8";
                String dns2Str = doc["dns2"] | "8.8.4.4";

                _ip.fromString(ipStr);
                _gateway.fromString(gatewayStr);
                _subnet.fromString(subnetStr);
                _dns1.fromString(dns1Str);
                _dns2.fromString(dns2Str);
            }

            Serial.println("Network settings loaded from EEPROM");
        }
        else {
            Serial.println("Error parsing network settings, using defaults");
            _dhcpMode = true;
        }
    }
    else {
        Serial.println("No network settings found, using defaults");
        _dhcpMode = true;
    }
}

void KC868NetworkManager::saveWiFiCredentials(String ssid, String password) {
    // Define EEPROM addresses
    const int EEPROM_WIFI_SSID_ADDR = 0;
    const int EEPROM_WIFI_PASS_ADDR = 64;

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
    _wifiSSID = ssid;
    _wifiPassword = password;

    Serial.println("WiFi credentials saved to EEPROM");
}

void KC868NetworkManager::loadWiFiCredentials() {
    // Define EEPROM addresses
    const int EEPROM_WIFI_SSID_ADDR = 0;
    const int EEPROM_WIFI_PASS_ADDR = 64;

    _wifiSSID = "";
    _wifiPassword = "";

    // Read SSID
    for (int i = 0; i < 64; i++) {
        char c = EEPROM.read(EEPROM_WIFI_SSID_ADDR + i);
        if (c != 0) {
            _wifiSSID += c;
        }
        else {
            break;
        }
    }

    // Read password
    for (int i = 0; i < 64; i++) {
        char c = EEPROM.read(EEPROM_WIFI_PASS_ADDR + i);
        if (c != 0) {
            _wifiPassword += c;
        }
        else {
            break;
        }
    }

    Serial.println("Loaded WiFi SSID: " + _wifiSSID);
}

void KC868NetworkManager::WiFiEvent(WiFiEvent_t event) {
    // Static pointer to store the instance - needs to be set in the constructor
    static KC868NetworkManager* instance = globalNetworkManagerPtr;

    switch (event) {
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
        Serial.print("WiFi connected with IP: ");
        Serial.println(WiFi.localIP());

        if (instance) {
            instance->_wifiConnected = true;
            instance->_wifiClientMode = true;
        }
        else {
            // Fallback to global variables if instance not found
            // FIX: Use instance member variables with underscore prefix
            // wifiConnected = true;
            // wifiClientMode = true;

            // Since we can't access the instance, we need to create global variables in main sketch
            extern bool wifiConnected;
            extern bool wifiClientMode;
            wifiConnected = true;
            wifiClientMode = true;
        }
        break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
        Serial.println("WiFi lost connection");

        if (instance) {
            instance->_wifiConnected = false;
            instance->_wifiClientMode = false;
        }
        else {
            // Fallback to global variables if instance not found
            // FIX: Use instance member variables with underscore prefix
            // wifiConnected = false;
            // wifiClientMode = false;

            // Since we can't access the instance, we need to create global variables in main sketch
            extern bool wifiConnected;
            extern bool wifiClientMode;
            wifiConnected = false;
            wifiClientMode = false;
        }
        break;
    }
}

void KC868NetworkManager::EthEvent(WiFiEvent_t event) {
    // Get the instance pointer that was set in the constructor
    KC868NetworkManager* instance = globalNetworkManagerPtr;

    switch (event) {
    case ARDUINO_EVENT_ETH_START:
        Serial.println("ETH Started");
        ETH.setHostname("KC868-A16");
        break;
    case ARDUINO_EVENT_ETH_CONNECTED:
        Serial.println("ETH Connected");
        break;
    case ARDUINO_EVENT_ETH_GOT_IP:
        Serial.println("ETH MAC: " + ETH.macAddress());
        Serial.println("ETH IPv4: " + ETH.localIP().toString());
        if (ETH.fullDuplex()) {
            Serial.println("ETH Link: FULL_DUPLEX");
        }
        else {
            Serial.println("ETH Link: HALF_DUPLEX");
        }
        Serial.println("ETH Speed: " + String(ETH.linkSpeed()) + " Mbps");

        // Update instance variables if instance is available
        if (instance) {
            instance->_ethConnected = true;
            instance->_wiredMode = true;

            // If Ethernet connected, disable WiFi client mode (but keep AP if active)
            if (instance->_wifiClientMode && !instance->_apMode) {
                WiFi.disconnect();
                instance->_wifiClientMode = false;
                instance->_wifiConnected = false;
            }

            // Update MAC address
            instance->_mac = ETH.macAddress();
        }
        else {
            // Fallback to global variables
            // FIX: Use instance member variables with underscore prefix
            // ethConnected = true;
            // wiredMode = true;

            // Since we can't access the instance, we need to create global variables in main sketch
            extern bool ethConnected;
            extern bool wiredMode;
            extern bool wifiClientMode;
            extern bool apMode;
            extern bool wifiConnected;
            extern String mac;

            ethConnected = true;
            wiredMode = true;

            if (wifiClientMode && !apMode) {
                WiFi.disconnect();
                wifiClientMode = false;
                wifiConnected = false;
            }

            mac = ETH.macAddress();
        }
        break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
        Serial.println("ETH Disconnected");

        // Update instance variables if instance is available
        if (instance) {
            instance->_ethConnected = false;
            instance->_wiredMode = false;

            // If Ethernet disconnects and we're not in AP mode, try to reconnect WiFi
            if (!instance->_apMode && !instance->_wifiClientMode && instance->_wifiSSID.length() > 0) {
                Serial.println("Ethernet disconnected, trying WiFi reconnection");
                WiFi.begin(instance->_wifiSSID.c_str(), instance->_wifiPassword.c_str());
            }
        }
        else {
            // Fallback to global variables
            // FIX: Use instance member variables with underscore prefix
            extern bool ethConnected;
            extern bool wiredMode;
            extern bool apMode;
            extern bool wifiClientMode;
            extern String wifiSSID;
            extern String wifiPassword;

            ethConnected = false;
            wiredMode = false;

            if (!apMode && !wifiClientMode && wifiSSID.length() > 0) {
                Serial.println("Ethernet disconnected, trying WiFi reconnection");
                WiFi.begin(wifiSSID.c_str(), wifiPassword.c_str());
            }
        }
        break;
    case ARDUINO_EVENT_ETH_STOP:
        Serial.println("ETH Stopped");

        // Update instance variables if instance is available
        if (instance) {
            instance->_ethConnected = false;
            instance->_wiredMode = false;
        }
        else {
            // Fallback to global variables
            // FIX: Use instance member variables with underscore prefix
            extern bool ethConnected;
            extern bool wiredMode;

            ethConnected = false;
            wiredMode = false;
        }
        break;
    default:
        break;
    }
}

String KC868NetworkManager::getMACAddress() {
    if (_ethConnected) {
        return ETH.macAddress();
    }
    else {
        return WiFi.macAddress();
    }
}

String KC868NetworkManager::getIPAddress() {
    if (_ethConnected) {
        return ETH.localIP().toString();
    }
    else if (_wifiClientMode) {
        return WiFi.localIP().toString();
    }
    else if (_apMode) {
        return WiFi.softAPIP().toString();
    }
    return "0.0.0.0";
}

String KC868NetworkManager::getGateway() {
    if (_ethConnected) {
        return ETH.gatewayIP().toString();
    }
    else if (_wifiClientMode) {
        return WiFi.gatewayIP().toString();
    }
    return "0.0.0.0";
}

String KC868NetworkManager::getSubnet() {
    if (_ethConnected) {
        return ETH.subnetMask().toString();
    }
    else if (_wifiClientMode) {
        return WiFi.subnetMask().toString();
    }
    return "255.255.255.0";
}

String KC868NetworkManager::getDNS1() {
    if (_ethConnected) {
        return ETH.dnsIP(0).toString();
    }
    else if (_wifiClientMode) {
        return WiFi.dnsIP(0).toString();
    }
    return "0.0.0.0";
}

String KC868NetworkManager::getDNS2() {
    if (_ethConnected) {
        return ETH.dnsIP(1).toString();
    }
    else if (_wifiClientMode) {
        return WiFi.dnsIP(1).toString();
    }
    return "0.0.0.0";
}

void KC868NetworkManager::setStaticIP(String ip, String gateway, String subnet, String dns1, String dns2) {
    _ip.fromString(ip);
    _gateway.fromString(gateway);
    _subnet.fromString(subnet);
    _dns1.fromString(dns1);
    _dns2.fromString(dns2);
    _dhcpMode = false;
}

void KC868NetworkManager::getNetworkInfo(JsonObject& networkObject) {
    networkObject["dhcp_mode"] = _dhcpMode;

    // WiFi details
    if (_wifiConnected) {
        if (_wifiClientMode) {
            networkObject["wifi_ip"] = WiFi.localIP().toString();
            networkObject["wifi_gateway"] = WiFi.gatewayIP().toString();
            networkObject["wifi_subnet"] = WiFi.subnetMask().toString();
            networkObject["wifi_dns1"] = WiFi.dnsIP(0).toString();
            networkObject["wifi_dns2"] = WiFi.dnsIP(1).toString();
            networkObject["wifi_rssi"] = WiFi.RSSI();
            networkObject["wifi_mac"] = WiFi.macAddress();
            networkObject["wifi_ssid"] = _wifiSSID;
        }
        else if (_apMode) {
            networkObject["wifi_mode"] = "Access Point";
            networkObject["wifi_ap_ip"] = WiFi.softAPIP().toString();
            networkObject["wifi_ap_mac"] = WiFi.softAPmacAddress();
            networkObject["wifi_ap_ssid"] = _ap_ssid;
        }
    }

    // Ethernet details
    if (_ethConnected) {
        networkObject["eth_ip"] = ETH.localIP().toString();
        networkObject["eth_gateway"] = ETH.gatewayIP().toString();
        networkObject["eth_subnet"] = ETH.subnetMask().toString();
        networkObject["eth_dns1"] = ETH.dnsIP(0).toString();
        networkObject["eth_dns2"] = ETH.dnsIP(1).toString();
        networkObject["eth_mac"] = ETH.macAddress();
        networkObject["eth_speed"] = String(ETH.linkSpeed()) + " Mbps";
        networkObject["eth_duplex"] = ETH.fullDuplex() ? "Full" : "Half";
    }
}