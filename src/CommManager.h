/**
 * CommManager.h - Communication protocols for KC868-A16
 * Created by Your Name, Date
 * Released into the public domain.
 */

#ifndef COMM_MANAGER_H
#define COMM_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HardwareSerial.h>

class CommManager {
public:
    CommManager();
    
    // Initialize communication manager
    void begin();
    
    // Process commands received via active protocol
    void processCommands();
    
    // Execute command and return response
    String processCommand(String command);
    
    // Get the active communication protocol
    String getActiveProtocol();
    
    // Set the active communication protocol
    void setActiveProtocol(String protocol);
    
    // Set up USB serial communication
    void initUSB(int baudRate = 115200, int dataBits = 8, int parity = 0, int stopBits = 1);
    
    // Set up RS485 communication
    void initRS485(int baudRate = 9600, int dataBits = 8, int parity = 0, int stopBits = 1);
    
    // Get protocol configuration for web interface
    void getProtocolConfig(String protocol, JsonDocument& doc);
    
    // Update protocol configuration
    bool updateProtocolConfig(String protocol, JsonObject& config);
    
    // Save protocol configuration to EEPROM
    void saveProtocolConfig();
    
    // Load protocol configuration from EEPROM
    void loadProtocolConfig();
    
private:
    // Currently active protocol: "usb", "rs485", "wifi", "ethernet"
    String _activeProtocol;
    
    // USB configuration
    int _usbBaudRate;
    int _usbDataBits;
    int _usbParity;
    int _usbStopBits;
    
    // RS485 configuration
    int _rs485BaudRate;
    int _rs485DataBits;
    int _rs485Parity;
    int _rs485StopBits;
    String _rs485Protocol;
    String _rs485Mode;
    int _rs485DeviceAddress;
    bool _rs485FlowControl;
    bool _rs485NightMode;
    
    // Hardware serial for RS485
    HardwareSerial* _rs485Serial;
    
    // Pin definitions - renamed to avoid conflicts
    // Using pins directly instead of macros
    const int RS485_TX_PIN_NUM = 13;
    const int RS485_RX_PIN_NUM = 16;
    
    // EEPROM address for configuration
    const int EEPROM_COMM_ADDR = 384;
    const int EEPROM_COMM_CONFIG_ADDR = 3072;
    
    // Process commands via USB
    void processUSBCommands();
    
    // Process commands via RS485
    void processRS485Commands();
    
    // Helper functions for command processing
    String handleRelayCommand(String command);
    String handleInputStatusCommand();
    String handleAnalogStatusCommand();
    String handleSystemStatusCommand();
    String handleI2CScanCommand();
    String handleHelpCommand();
};

#endif // COMM_MANAGER_H