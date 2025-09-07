/**
 * NetworkManager.h - Network management for KC868-A16
 * Created by Your Name, Date
 * Released into the public domain.
 */

#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <ETH.h>
#include <DNSServer.h>
#include <ArduinoJson.h> // Added this include to fix the JsonObject error

 // ETH PHY pin definitions
#define ETH_PHY_ADDR        0       // Default PHY address
#define ETH_PHY_MDC         23      // GPIO pin for MDC
#define ETH_PHY_MDIO        18      // GPIO pin for MDIO
#define ETH_PHY_POWER       -1      // No power pin, use external power
#define ETH_CLOCK_GPIO17_OUT ETH_CLOCK_GPIO17_OUT // Clock on GPIO17 (Use the enum value from ETH library)

// Renamed to KC868NetworkManager to avoid conflicts with WiFi library
class KC868NetworkManager {
public:
    KC868NetworkManager();

    // Initialize Ethernet
    void initEthernet();

    // Initialize WiFi
    void initWiFi();

    // Start Access Point mode
    void startAPMode();

    // Start DNS server for captive portal
    void startDNSServer();

    // Process DNS requests
    void processDNSRequests();

    // Reset Ethernet controller
    void resetEthernet();

    // Check network status periodically
    void checkNetworkStatus();

    // Print current network status
    void printNetworkStatus();

    // Save network settings
    void saveNetworkSettings();

    // Load network settings
    void loadNetworkSettings();

    // Save WiFi credentials
    void saveWiFiCredentials(String ssid, String password);

    // Load WiFi credentials
    void loadWiFiCredentials();

    // Get network status
    bool isEthernetConnected() { return _ethConnected; }
    bool isWiFiConnected() { return _wifiConnected; }
    bool isAPMode() { return _apMode; }
    bool isWiFiClientMode() { return _wifiClientMode; }
    bool isWiredMode() { return _wiredMode; }

    // Get network configuration
    bool isDHCPMode() { return _dhcpMode; }
    String getWiFiSSID() { return _wifiSSID; }
    String getWiFiPassword() { return _wifiPassword; }
    String getMACAddress();
    String getIPAddress();
    String getGateway();
    String getSubnet();
    String getDNS1();
    String getDNS2();

    // Set DHCP/static mode
    void setDHCPMode(bool mode) { _dhcpMode = mode; }

    // Set static IP configuration
    void setStaticIP(String ip, String gateway, String subnet, String dns1, String dns2);

    // WiFi event handler
    static void WiFiEvent(WiFiEvent_t event);

    // Ethernet event handler
    static void EthEvent(WiFiEvent_t event);

    // Get network info for JSON responses
    void getNetworkInfo(JsonObject& networkObject);

private:

    
    // Network status
    bool _ethConnected;        // Ethernet connection status
    bool _wifiConnected;       // WiFi connection status
    bool _apMode;              // Whether device is in AP mode
    bool _wifiClientMode;      // Whether device is connected to WiFi as client
    bool _wiredMode;           // Whether device is using Ethernet

    // Network configuration
    bool _dhcpMode;            // DHCP or static IP mode
    IPAddress _ip;             // Static IP address
    IPAddress _gateway;        // Gateway
    IPAddress _subnet;         // Subnet mask
    IPAddress _dns1;           // Primary DNS
    IPAddress _dns2;           // Secondary DNS
    String _mac;               // MAC address
    String _wifiSSID;          // WiFi SSID
    String _wifiPassword;      // WiFi password

    // WiFi configuration
    String _wifiSecurity;          // WPA, WPA2, WEP, OPEN
    bool _wifiHidden;              // Whether SSID is hidden
    String _wifiMacFilter;         // MAC filtering
    bool _wifiAutoUpdate;          // Auto firmware updates
    String _wifiRadioMode;         // 802.11b, 802.11g, 802.11n
    int _wifiChannel;              // WiFi channel (1-13)
    int _wifiChannelWidth;         // Channel width (20 or 40 MHz)
    unsigned long _wifiDhcpLeaseTime; // DHCP lease time in seconds
    bool _wifiWmmEnabled;          // WiFi Multimedia (WMM)

    // AP configuration
    const char* _ap_ssid = "KC868-A16";      // AP SSID
    const char* _ap_password = "admin";      // AP Password

    // DNS server for captive portal
    DNSServer _dnsServer;
};

#endif // NETWORK_MANAGER_H