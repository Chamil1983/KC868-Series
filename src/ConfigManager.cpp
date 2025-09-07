/**
 * ConfigManager.cpp - Configuration storage/retrieval for KC868-A16
 * Created by Your Name, Date
 * Released into the public domain.
 */

#include "ConfigManager.h"

ConfigManager::ConfigManager() :
    _deviceName("KC868-A16"),
    _debugMode(true),
    _dhcpMode(true)
{
    // Default values set in initializer list
}

void ConfigManager::begin() {
    // Initialize EEPROM
    EEPROM.begin(EEPROM_SIZE);
    
    // Load configuration
    loadConfiguration();
    
    Serial.println("Configuration manager initialized");
}

void ConfigManager::saveConfiguration() {
    DynamicJsonDocument doc(2048);
    
    // Device settings
    doc["device_name"] = _deviceName;
    doc["debug_mode"] = _debugMode;
    doc["dhcp_mode"] = _dhcpMode;
    
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
    
    Serial.println("Configuration saved to EEPROM");
}

void ConfigManager::loadConfiguration() {
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
            _deviceName = doc["device_name"] | "KC868-A16";
            _debugMode = doc["debug_mode"] | true;
            _dhcpMode = doc["dhcp_mode"] | true;
            
            Serial.println("Configuration loaded from EEPROM");
        }
        else {
            Serial.println("Failed to parse configuration JSON");
            // Use defaults
            initializeDefaultConfig();
        }
    }
    else {
        // No data found, use defaults
        initializeDefaultConfig();
    }
}

void ConfigManager::initializeDefaultConfig() {
    _deviceName = "KC868-A16";
    _debugMode = true;
    _dhcpMode = true;

    Serial.println("Using default configuration");
}

String ConfigManager::getDeviceName() {
    return _deviceName;
}

void ConfigManager::setDeviceName(String name) {
    _deviceName = name;
}

bool ConfigManager::isDebugMode() {
    return _debugMode;
}

void ConfigManager::setDebugMode(bool mode) {
    _debugMode = mode;
}

bool ConfigManager::isDHCPMode() {
    return _dhcpMode;
}

void ConfigManager::setDHCPMode(bool mode) {
    _dhcpMode = mode;
}