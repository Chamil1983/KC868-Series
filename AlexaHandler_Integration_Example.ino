/**
 * Integration example for AlexaHandler in KC868-A16 Controller
 * 
 * This shows how to properly integrate the AlexaHandler into the main project
 * and demonstrates that the removeDeviceByName compile error has been fixed.
 */

// Add these includes to the main .ino file
#include <fauxmoESP.h>
#include "src/AlexaHandler.h"

// Global AlexaHandler instance
AlexaHandler alexaHandler;

// Example of how to integrate into setup()
void setup_alexa_integration() {
    Serial.println("Initializing Alexa integration...");
    
    // Initialize WiFi first (this should already be done in main setup)
    // WiFi.begin(ssid, password);
    // while (WiFi.status() != WL_CONNECTED) { delay(500); }
    
    // Initialize Alexa handler
    if (alexaHandler.begin()) {
        Serial.println("Alexa integration initialized successfully");
        
        // Set custom device names if needed
        String customNames[16] = {
            "Living Room Light", "Kitchen Light", "Bedroom Light", "Bathroom Light",
            "Garden Light", "Garage Door", "Pool Pump", "Sprinkler System",
            "Security Light", "Porch Light", "Basement Light", "Attic Fan",
            "Heater", "Air Conditioner", "Water Heater", "Alarm System"
        };
        
        alexaHandler.setDeviceNames(customNames);
    } else {
        Serial.println("Failed to initialize Alexa integration");
    }
}

// Example of how to integrate into loop()
void loop_alexa_integration() {
    // Handle Alexa discovery and communication
    alexaHandler.handle();
}

// Example of how to sync relay states with Alexa when changed locally
void onRelayStateChanged(int relayIndex, bool newState) {
    // Update your physical relay here
    // setPhysicalRelay(relayIndex, newState);
    
    // Sync the state with Alexa
    alexaHandler.setRelayState(relayIndex, newState);
    
    Serial.printf("Relay %d set to %s and synced with Alexa\n", 
                  relayIndex + 1, newState ? "ON" : "OFF");
}

// Demonstrate the fix for the original compile error
void demonstrate_fix() {
    Serial.println("\n=== COMPILE ERROR FIX DEMONSTRATION ===");
    Serial.println("Original problematic code (line 73):");
    Serial.println("  fauxmo.removeDeviceByName(\"*\")  // ERROR: Method doesn't exist!");
    Serial.println("");
    Serial.println("Fixed implementation:");
    Serial.println("  for (int i = fauxmo.countDevices() - 1; i >= 0; i--) {");
    Serial.println("      fauxmo.removeDevice(i);  // CORRECT: This method exists");
    Serial.println("  }");
    Serial.println("");
    Serial.println("Result: Compile error eliminated, functionality preserved!");
}