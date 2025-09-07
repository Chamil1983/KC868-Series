/**
 * Utilities.h - Helper functions for KC868-A16
 * Created by Your Name, Date
 * Released into the public domain.
 */

#ifndef UTILITIES_H
#define UTILITIES_H

#include <Arduino.h>

 // EEPROM size and address definitions
#define EEPROM_SIZE           4096
#define EEPROM_WIFI_SSID_ADDR 0
#define EEPROM_WIFI_PASS_ADDR 64
#define EEPROM_CONFIG_ADDR    256
#define EEPROM_COMM_ADDR      384
#define EEPROM_SCHEDULE_ADDR  512
#define EEPROM_TRIGGER_ADDR   2048
#define EEPROM_COMM_CONFIG_ADDR 3072
#define EEPROM_INTERRUPT_CONFIG_ADDR 3584

// Convert milliseconds to a formatted uptime string
inline String getUptimeString(unsigned long milliseconds) {
    unsigned long uptime = milliseconds / 1000; // Convert to seconds

    unsigned long days = uptime / 86400;
    uptime %= 86400;

    unsigned long hours = uptime / 3600;
    uptime %= 3600;

    unsigned long minutes = uptime / 60;
    unsigned long seconds = uptime % 60;

    char uptimeStr[30];
    if (days > 0) {
        sprintf(uptimeStr, "%ld days, %02ld:%02ld:%02ld", days, hours, minutes, seconds);
    }
    else {
        sprintf(uptimeStr, "%02ld:%02ld:%02ld", hours, minutes, seconds);
    }

    return String(uptimeStr);
}

// Debug print function with optional timestamp
inline void debugPrintln(String message, bool includeTimestamp = true) {
    if (includeTimestamp) {
        unsigned long ms = millis();
        unsigned long seconds = ms / 1000;
        unsigned long minutes = seconds / 60;
        unsigned long hours = minutes / 60;

        // Format: [HH:MM:SS.mmm]
        char timestamp[16];
        sprintf(timestamp, "[%02lu:%02lu:%02lu.%03lu] ",
            hours, minutes % 60, seconds % 60, ms % 1000);

        Serial.print(timestamp);
    }

    Serial.println(message);
}

// Convert analog value to voltage with calibration
inline float analogToVoltage(int analogValue) {
    // Calibration data: pairs of [ADC value, Actual Voltage]
    const int calADC[] = { 0, 820, 1640, 2460, 3270, 4095 };
    const float calVolts[] = { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f };
    const int numCalPoints = 6;

    // Bound input to valid range
    if (analogValue <= 0) return 0.0f;
    if (analogValue >= 4095) return 5.0f;

    // Find the right calibration segment
    int segment = 0;
    while (segment < numCalPoints - 1 && analogValue > calADC[segment + 1]) {
        segment++;
    }

    // Linear interpolation within the segment
    float fraction = (float)(analogValue - calADC[segment]) /
        (float)(calADC[segment + 1] - calADC[segment]);

    float voltage = calVolts[segment] + fraction * (calVolts[segment + 1] - calVolts[segment]);

    return voltage;
}

// Calculate percentage (0-100) from voltage (0-5V)
inline int voltageToPercentage(float voltage) {
    // Ensure voltage is in the correct range
    if (voltage > 5.0f) voltage = 5.0f;
    if (voltage < 0.0f) voltage = 0.0f;

    // Calculate percentage based on 0-5V range
    return (int)((voltage / 5.0f) * 100.0f);
}

// Validate IP address string
inline bool isValidIPAddress(String ip) {
    IPAddress result;
    return result.fromString(ip);
}

#endif // UTILITIES_H