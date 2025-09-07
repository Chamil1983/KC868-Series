/**
 * CommManager.cpp - Communication protocols for KC868-A16
 * Created by Your Name, Date
 * Released into the public domain.
 */

#include "CommManager.h"
#include <EEPROM.h>
#include <Wire.h>

CommManager::CommManager() :
    _activeProtocol("wifi"),
    _usbBaudRate(115200),
    _usbDataBits(8),
    _usbParity(0),
    _usbStopBits(1),
    _rs485BaudRate(9600),
    _rs485DataBits(8),
    _rs485Parity(0),
    _rs485StopBits(1),
    _rs485Protocol("Modbus RTU"),
    _rs485Mode("Half-duplex"),
    _rs485DeviceAddress(1),
    _rs485FlowControl(false),
    _rs485NightMode(false)
{
    _rs485Serial = new HardwareSerial(1);
}

void CommManager::begin() {
    // Initialize Serial for USB
    Serial.begin(_usbBaudRate);
    
    // Initialize RS485
    initRS485(_rs485BaudRate, _rs485DataBits, _rs485Parity, _rs485StopBits);
    
    // Load saved protocol settings
    loadProtocolConfig();
    
    Serial.println("Communication manager initialized");
}

void CommManager::initUSB(int baudRate, int dataBits, int parity, int stopBits) {
    _usbBaudRate = baudRate;
    _usbDataBits = dataBits;
    _usbParity = parity;
    _usbStopBits = stopBits;
    
    // Re-initialize serial with new settings
    Serial.end();
    Serial.begin(baudRate);
    
    Serial.println("USB communication initialized");
}

void CommManager::initRS485(int baudRate, int dataBits, int parity, int stopBits) {
    _rs485BaudRate = baudRate;
    _rs485DataBits = dataBits;
    _rs485Parity = parity;
    _rs485StopBits = stopBits;
    
    // Configure with current RS485 settings
    int configParity = SERIAL_8N1; // Default
    
    // Set correct parity
    if (dataBits == 8) {
        if (parity == 0) {
            if (stopBits == 1) configParity = SERIAL_8N1;
            else configParity = SERIAL_8N2;
        }
        else if (parity == 1) {
            if (stopBits == 1) configParity = SERIAL_8O1;
            else configParity = SERIAL_8O2;
        }
        else if (parity == 2) {
            if (stopBits == 1) configParity = SERIAL_8E1;
            else configParity = SERIAL_8E2;
        }
    }
    else if (dataBits == 7) {
        if (parity == 0) {
            if (stopBits == 1) configParity = SERIAL_7N1;
            else configParity = SERIAL_7N2;
        }
        else if (parity == 1) {
            if (stopBits == 1) configParity = SERIAL_7O1;
            else configParity = SERIAL_7O2;
        }
        else if (parity == 2) {
            if (stopBits == 1) configParity = SERIAL_7E1;
            else configParity = SERIAL_7E2;
        }
    }
    
    // Using the class variables for pins instead of macros
    _rs485Serial->begin(baudRate, configParity, RS485_RX_PIN_NUM, RS485_TX_PIN_NUM);
    Serial.println("RS485 initialized with baud rate: " + String(baudRate));
}


void CommManager::processCommands() {
    if (_activeProtocol == "usb") {
        processUSBCommands();
    }
    else if (_activeProtocol == "rs485") {
        processRS485Commands();
    }
    // Note: WiFi and Ethernet commands are handled by WebServerManager
}

void CommManager::processUSBCommands() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        
        String response = processCommand(command);
        Serial.println(response);
    }
}

void CommManager::processRS485Commands() {
    if (_rs485Serial->available()) {
        String command = _rs485Serial->readStringUntil('\n');
        command.trim();
        
        String response = processCommand(command);
        _rs485Serial->println(response);
    }
}

String CommManager::processCommand(String command) {
    // This is a simplified implementation - the actual implementation would interact
    // with HardwareManager, SensorManager, etc.
    
    if (command.startsWith("RELAY ")) {
        return handleRelayCommand(command.substring(6));
    }
    else if (command.startsWith("INPUT STATUS")) {
        return handleInputStatusCommand();
    }
    else if (command.startsWith("ANALOG STATUS")) {
        return handleAnalogStatusCommand();
    }
    else if (command == "STATUS") {
        return handleSystemStatusCommand();
    }
    else if (command.startsWith("SCAN I2C")) {
        return handleI2CScanCommand();
    }
    else if (command == "HELP") {
        return handleHelpCommand();
    }
    
    return "ERROR: Unknown command. Type HELP for commands.";
}

String CommManager::handleRelayCommand(String command) {
    // Placeholder - would interact with HardwareManager in full implementation
    return "Relay command processed: " + command;
}

String CommManager::handleInputStatusCommand() {
    // Placeholder - would interact with HardwareManager in full implementation
    return "INPUT STATUS:\nReading input states...";
}

String CommManager::handleAnalogStatusCommand() {
    // Placeholder - would interact with HardwareManager in full implementation
    return "ANALOG STATUS:\nReading analog inputs...";
}

String CommManager::handleSystemStatusCommand() {
    // Placeholder - would collect data from multiple managers in full implementation
    return "KC868-A16 System Status\n---------------------\nDevice: KC868-A16";
}

String CommManager::handleI2CScanCommand() {
    // Simple I2C scanner that actually works
    String response = "I2C DEVICES:\n";
    int deviceCount = 0;
    
    for (uint8_t address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        uint8_t error = Wire.endTransmission();
        
        if (error == 0) {
            deviceCount++;
            response += "0x" + String(address, HEX) + "\n";
        }
    }
    
    response += "Found " + String(deviceCount) + " device(s)\n";
    return response;
}

String CommManager::handleHelpCommand() {
    String response = "KC868-A16 Controller Command Help\n";
    response += "---------------------\n";
    response += "RELAY STATUS - Show all relay states\n";
    response += "RELAY ALL ON - Turn all relays on\n";
    response += "RELAY ALL OFF - Turn all relays off\n";
    response += "RELAY <num> ON - Turn relay on (1-16)\n";
    response += "RELAY <num> OFF - Turn relay off (1-16)\n";
    response += "INPUT STATUS - Show all input states\n";
    response += "ANALOG STATUS - Show all analog input values\n";
    response += "SCAN I2C - Scan for I2C devices\n";
    response += "STATUS - Show system status\n";
    response += "VERSION - Show firmware version\n";
    
    return response;
}

String CommManager::getActiveProtocol() {
    return _activeProtocol;
}

void CommManager::setActiveProtocol(String protocol) {
    if (protocol == "usb" || protocol == "rs485" || protocol == "wifi" || protocol == "ethernet") {
        _activeProtocol = protocol;
    }
}

void CommManager::getProtocolConfig(String protocol, JsonDocument& doc) {
    // Add protocol-specific settings to the document
    if (protocol == "usb") {
        doc["com_port"] = 0; // Default COM port
        doc["baud_rate"] = _usbBaudRate;
        doc["data_bits"] = _usbDataBits;
        doc["parity"] = _usbParity;
        doc["stop_bits"] = _usbStopBits;
    }
    else if (protocol == "rs485") {
        doc["baud_rate"] = _rs485BaudRate;
        doc["parity"] = _rs485Parity;
        doc["data_bits"] = _rs485DataBits;
        doc["stop_bits"] = _rs485StopBits;
        doc["protocol_type"] = _rs485Protocol;
        doc["comm_mode"] = _rs485Mode;
        doc["device_address"] = _rs485DeviceAddress;
        doc["flow_control"] = _rs485FlowControl;
        doc["night_mode"] = _rs485NightMode;
    }
    // Note: WiFi and Ethernet settings are handled by NetworkManager
}

bool CommManager::updateProtocolConfig(String protocol, JsonObject& config) {
    bool changed = false;
    
    if (protocol == "usb") {
        if (config.containsKey("baud_rate")) {
            _usbBaudRate = config["baud_rate"];
            changed = true;
        }
        if (config.containsKey("data_bits")) {
            _usbDataBits = config["data_bits"];
            changed = true;
        }
        if (config.containsKey("parity")) {
            _usbParity = config["parity"];
            changed = true;
        }
        if (config.containsKey("stop_bits")) {
            _usbStopBits = config["stop_bits"];
            changed = true;
        }
        
        if (changed) {
            initUSB(_usbBaudRate, _usbDataBits, _usbParity, _usbStopBits);
        }
    }
    else if (protocol == "rs485") {
        if (config.containsKey("baud_rate")) {
            _rs485BaudRate = config["baud_rate"];
            changed = true;
        }
        if (config.containsKey("data_bits")) {
            _rs485DataBits = config["data_bits"];
            changed = true;
        }
        if (config.containsKey("parity")) {
            _rs485Parity = config["parity"];
            changed = true;
        }
        if (config.containsKey("stop_bits")) {
            _rs485StopBits = config["stop_bits"];
            changed = true;
        }
        if (config.containsKey("protocol_type")) {
            _rs485Protocol = config["protocol_type"].as<String>();
            changed = true;
        }
        if (config.containsKey("comm_mode")) {
            _rs485Mode = config["comm_mode"].as<String>();
            changed = true;
        }
        if (config.containsKey("device_address")) {
            _rs485DeviceAddress = config["device_address"];
            changed = true;
        }
        if (config.containsKey("flow_control")) {
            _rs485FlowControl = config["flow_control"];
            changed = true;
        }
        if (config.containsKey("night_mode")) {
            _rs485NightMode = config["night_mode"];
            changed = true;
        }
        
        if (changed) {
            initRS485(_rs485BaudRate, _rs485DataBits, _rs485Parity, _rs485StopBits);
        }
    }
    
    if (changed) {
        saveProtocolConfig();
    }
    
    return changed;
}

void CommManager::saveProtocolConfig() {
    DynamicJsonDocument doc(2048);
    
    // Active protocol
    doc["active_protocol"] = _activeProtocol;
    
    // USB settings
    JsonObject usb = doc.createNestedObject("usb");
    usb["baud_rate"] = _usbBaudRate;
    usb["data_bits"] = _usbDataBits;
    usb["parity"] = _usbParity;
    usb["stop_bits"] = _usbStopBits;
    
    // RS485 settings
    JsonObject rs485 = doc.createNestedObject("rs485");
    rs485["baud_rate"] = _rs485BaudRate;
    rs485["data_bits"] = _rs485DataBits;
    rs485["parity"] = _rs485Parity;
    rs485["stop_bits"] = _rs485StopBits;
    rs485["protocol"] = _rs485Protocol;
    rs485["mode"] = _rs485Mode;
    rs485["device_address"] = _rs485DeviceAddress;
    rs485["flow_control"] = _rs485FlowControl;
    rs485["night_mode"] = _rs485NightMode;
    
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
    
    Serial.println("Communication settings saved to EEPROM");
}

void CommManager::loadProtocolConfig() {
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
            // Active protocol
            _activeProtocol = doc["active_protocol"] | "wifi";
            
            // USB settings
            if (doc.containsKey("usb")) {
                _usbBaudRate = doc["usb"]["baud_rate"] | 115200;
                _usbDataBits = doc["usb"]["data_bits"] | 8;
                _usbParity = doc["usb"]["parity"] | 0;
                _usbStopBits = doc["usb"]["stop_bits"] | 1;
            }
            
            // RS485 settings
            if (doc.containsKey("rs485")) {
                _rs485BaudRate = doc["rs485"]["baud_rate"] | 9600;
                _rs485DataBits = doc["rs485"]["data_bits"] | 8;
                _rs485Parity = doc["rs485"]["parity"] | 0;
                _rs485StopBits = doc["rs485"]["stop_bits"] | 1;
                _rs485Protocol = doc["rs485"]["protocol"] | "Modbus RTU";
                _rs485Mode = doc["rs485"]["mode"] | "Half-duplex";
                _rs485DeviceAddress = doc["rs485"]["device_address"] | 1;
                _rs485FlowControl = doc["rs485"]["flow_control"] | false;
                _rs485NightMode = doc["rs485"]["night_mode"] | false;
            }
            
            Serial.println("Communication settings loaded from EEPROM");
        }
        else {
            Serial.println("Error parsing communication settings, using defaults");
        }
    }
    else {
        Serial.println("No communication settings found, using defaults");
    }
    
    // Initialize USB and RS485 with loaded settings
    initUSB(_usbBaudRate, _usbDataBits, _usbParity, _usbStopBits);
    initRS485(_rs485BaudRate, _rs485DataBits, _rs485Parity, _rs485StopBits);
}