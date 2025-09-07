/**
 * ConfigManager.h - Configuration storage/retrieval for KC868-A16
 * Created by Your Name, Date
 * Released into the public domain.
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <EEPROM.h>
#include <ArduinoJson.h>

class ConfigManager {
public:
    ConfigManager();

    // Initialize EEPROM and load configuration
    void begin();

    // Save configuration to EEPROM
    void saveConfiguration();

    // Load configuration from EEPROM
    void loadConfiguration();

    // Initialize default configuration
    void initializeDefaultConfig();

    // Get/Set device name - Remove inline implementations
    String getDeviceName();
    void setDeviceName(String name);

    // Get/Set debug mode - Remove inline implementations
    bool isDebugMode();
    void setDebugMode(bool mode);

    // Get/Set DHCP mode - Remove inline implementations
    bool isDHCPMode();
    void setDHCPMode(bool mode);

private:
    // Device settings
    String _deviceName;
    bool _debugMode;
    bool _dhcpMode;

    // EEPROM addresses
    const int EEPROM_SIZE = 4096;
    const int EEPROM_CONFIG_ADDR = 256;
};

#endif // CONFIG_MANAGER_H