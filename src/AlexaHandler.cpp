/**
 * AlexaHandler.cpp
 * Alexa integration for KC868-A16 Smart Home Controller using fauxmoESP
 * 
 * This module provides Amazon Alexa integration for the KC868-A16 controller,
 * allowing voice control of relays and other functions through Alexa devices.
 */

#include "AlexaHandler.h"
#include "GlobalConstants.h"
#include <WiFi.h>

// Initialize the fauxmoESP instance
fauxmoESP fauxmo;

// Device names for Alexa integration
String deviceNames[16] = {
    "Relay 1", "Relay 2", "Relay 3", "Relay 4",
    "Relay 5", "Relay 6", "Relay 7", "Relay 8", 
    "Relay 9", "Relay 10", "Relay 11", "Relay 12",
    "Relay 13", "Relay 14", "Relay 15", "Relay 16"
};

// Global instance definition
AlexaHandler alexaHandler;

// Constructor
AlexaHandler::AlexaHandler() {
    _initialized = false;
}

// Initialize Alexa integration
bool AlexaHandler::begin() {
    if (!WiFi.isConnected()) {
        Serial.println("WiFi not connected. Cannot initialize Alexa.");
        return false;
    }

    // Initialize fauxmoESP
    fauxmo.createServer(true);
    fauxmo.setPort(80); // Default port for fauxmoESP

    // Add all relay devices
    for (int i = 0; i < 16; i++) {
        fauxmo.addDevice(deviceNames[i].c_str());
    }

    // Set up the callback for device state changes
    fauxmo.onSetState([](unsigned char device_id, const char* device_name, bool state, unsigned char value) {
        Serial.printf("[ALEXA] Device #%d (%s) state: %s value: %d\n", device_id, device_name, state ? "ON" : "OFF", value);
        
        // Handle relay control based on device_id
        if (device_id < 16) {
            // Update the relay state - this should be connected to your relay control system
            // For now, we'll just print the action
            Serial.printf("Setting relay %d to %s\n", device_id + 1, state ? "ON" : "OFF");
            
            // TODO: Connect this to the actual relay control in your main system
            // Example: setRelayState(device_id, state);
        }
    });

    _initialized = true;
    Serial.println("Alexa integration initialized successfully");
    return true;
}

// Handle Alexa discovery and communication
void AlexaHandler::handle() {
    if (_initialized) {
        fauxmo.handle();
    }
}

// Set device names for Alexa devices
void AlexaHandler::setDeviceNames(String names[16]) {
    if (!_initialized) {
        Serial.println("AlexaHandler not initialized");
        return;
    }

    // Store the new device names
    for (int i = 0; i < 16; i++) {
        deviceNames[i] = names[i];
    }

    // Clear all existing devices by removing them one by one
    // FIXED: Original code used fauxmo.removeDeviceByName("*") which doesn't exist
    // The fauxmoESP library only has removeDevice(int index), not removeDeviceByName()
    // We iterate backwards to avoid index shifting issues when removing devices
    for (int i = fauxmo.countDevices() - 1; i >= 0; i--) {
        fauxmo.removeDevice(i);  // This is the correct method that exists in fauxmoESP
    }

    // Re-add devices with new names
    for (int i = 0; i < 16; i++) {
        fauxmo.addDevice(deviceNames[i].c_str());
    }

    Serial.println("Device names updated in Alexa integration");
}

// Set a single device name
void AlexaHandler::setDeviceName(int deviceIndex, const String& name) {
    if (deviceIndex < 0 || deviceIndex >= 16) {
        Serial.println("Invalid device index");
        return;
    }

    if (!_initialized) {
        Serial.println("AlexaHandler not initialized");
        return;
    }

    // Update the device name in our array
    deviceNames[deviceIndex] = name;

    // Remove the specific device and re-add it with the new name
    // We need to rebuild all devices to maintain proper indexing
    String tempNames[16];
    for (int i = 0; i < 16; i++) {
        tempNames[i] = deviceNames[i];
    }

    // Remove all devices and re-add them to maintain proper indexing
    // FIXED: The original problematic line 73 was: fauxmo.removeDeviceByName("*")
    // This method doesn't exist in fauxmoESP. The correct approach is to remove
    // each device by index using removeDevice(int index)
    for (int i = fauxmo.countDevices() - 1; i >= 0; i--) {
        fauxmo.removeDevice(i);  // Use the correct method that actually exists
    }

    // Re-add all devices with current names
    for (int i = 0; i < 16; i++) {
        fauxmo.addDevice(tempNames[i].c_str());
    }

    Serial.printf("Device %d name updated to: %s\n", deviceIndex + 1, name.c_str());
}

// Get device name
String AlexaHandler::getDeviceName(int deviceIndex) {
    if (deviceIndex < 0 || deviceIndex >= 16) {
        return "";
    }
    return deviceNames[deviceIndex];
}

// Set relay state (called from main system)
void AlexaHandler::setRelayState(int relayIndex, bool state) {
    if (relayIndex < 0 || relayIndex >= 16 || !_initialized) {
        return;
    }

    // Update the fauxmoESP device state to reflect the actual relay state
    fauxmo.setState(relayIndex, state, 255);
}

// Check if Alexa integration is initialized
bool AlexaHandler::isInitialized() {
    return _initialized;
}

// Get number of devices
int AlexaHandler::getDeviceCount() {
    if (_initialized) {
        return fauxmo.countDevices();
    }
    return 0;
}