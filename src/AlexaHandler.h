/**
 * AlexaHandler.h
 * Header file for Alexa integration using fauxmoESP library
 * 
 * This class provides Amazon Alexa integration for the KC868-A16 controller,
 * allowing voice control of relays and other functions through Alexa devices.
 */

#ifndef ALEXA_HANDLER_H
#define ALEXA_HANDLER_H

#include <Arduino.h>
#include <fauxmoESP.h>

class AlexaHandler {
private:
    bool _initialized;

public:
    /**
     * Constructor
     */
    AlexaHandler();

    /**
     * Initialize Alexa integration
     * @return true if initialization successful, false otherwise
     */
    bool begin();

    /**
     * Handle Alexa discovery and communication (call this in main loop)
     */
    void handle();

    /**
     * Set device names for all 16 relay devices
     * @param names Array of 16 device names
     */
    void setDeviceNames(String names[16]);

    /**
     * Set a single device name
     * @param deviceIndex Index of the device (0-15)
     * @param name New name for the device
     */
    void setDeviceName(int deviceIndex, const String& name);

    /**
     * Get device name
     * @param deviceIndex Index of the device (0-15)
     * @return Device name or empty string if invalid index
     */
    String getDeviceName(int deviceIndex);

    /**
     * Set relay state (called from main system to sync with Alexa)
     * @param relayIndex Index of the relay (0-15)
     * @param state Relay state (true = ON, false = OFF)
     */
    void setRelayState(int relayIndex, bool state);

    /**
     * Check if Alexa integration is initialized
     * @return true if initialized, false otherwise
     */
    bool isInitialized();

    /**
     * Get number of devices registered with Alexa
     * @return Number of devices
     */
    int getDeviceCount();
};

// Global instance
extern AlexaHandler alexaHandler;

#endif // ALEXA_HANDLER_H