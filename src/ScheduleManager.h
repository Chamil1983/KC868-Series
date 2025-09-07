/**
 * ScheduleManager.h - Schedule and trigger handling for KC868-A16
 * Created by Your Name, Date
 * Released into the public domain.
 */

#ifndef SCHEDULE_MANAGER_H
#define SCHEDULE_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include "HardwareManager.h"
#include "SensorManager.h"

// Forward declarations
class HardwareManager;
class SensorManager;

#define MAX_SCHEDULES 30
#define MAX_ANALOG_TRIGGERS 16

// Time schedule structure
struct TimeSchedule {
    bool enabled;
    uint8_t triggerType;  // 0=Time-based, 1=Input-based, 2=Combined, 3=Sensor-based
    uint8_t days;         // Bit field: bit 0=Sunday, bit 1=Monday, ..., bit 6=Saturday (for time-based)
    uint8_t hour;         // Hour for time-based trigger
    uint8_t minute;       // Minute for time-based trigger
    uint16_t inputMask;   // Bit mask for inputs (bits 0-15 for digital inputs, bits 16-18 for HT1-HT3)
    uint16_t inputStates; // Required state for each input (0=LOW, 1=HIGH)
    uint8_t logic;        // 0=AND (all conditions must be met), 1=OR (any condition can trigger)
    uint8_t action;       // 0=OFF, 1=ON, 2=TOGGLE
    uint8_t targetType;   // 0=Output, 1=Multiple outputs
    uint16_t targetId;    // Output number (0-15) or bitmask for multiple outputs
    uint16_t targetIdLow; // Additional target for LOW state (when input is FALSE)
    char name[32];        // Name/description of the schedule
    
    // Fields for sensor triggers
    uint8_t sensorIndex;      // HT sensor index (0-2 for HT1-HT3)
    uint8_t sensorTriggerType; // 0=Temperature, 1=Humidity
    uint8_t sensorCondition;   // 0=Above, 1=Below, 2=Equal
    float sensorThreshold;     // Temperature or humidity threshold value
};

// Analog trigger structure
struct AnalogTrigger {
    bool enabled;
    uint8_t analogInput;    // 0-3 (A1-A4)
    uint16_t threshold;     // Analog threshold value (0-4095)
    uint8_t condition;      // 0=Above, 1=Below, 2=Equal
    uint8_t action;         // 0=OFF, 1=ON, 2=TOGGLE
    uint8_t targetType;     // 0=Output, 1=Multiple outputs
    uint16_t targetId;      // Output number (0-15) or bitmask
    char name[32];          // Name/description of trigger
};

class ScheduleManager {
public:
    ScheduleManager(HardwareManager& hardwareManager, SensorManager& sensorManager);
    
    // Initialize schedules
    void begin();
    
    // Save schedules to EEPROM
    void saveSchedules();
    
    // Load schedules from EEPROM
    void loadSchedules();
    
    // Save analog triggers to EEPROM
    void saveAnalogTriggers();
    
    // Load analog triggers from EEPROM
    void loadAnalogTriggers();
    
    // Check time-based schedules
    void checkSchedules();
    
    // Check input-based schedules
    void checkInputBasedSchedules();
    
    // Check input-based schedule for a specific input
    void checkInputBasedSchedules(int changedInputIndex, bool newState);
    
    // Check analog triggers
    void checkAnalogTriggers();
    
    // Execute a schedule action
    void executeSchedule(int scheduleIndex);
    
    // Execute a schedule action with specific target
    void executeScheduleAction(int scheduleIndex, uint16_t targetId);
    
    // Get schedule by index
    TimeSchedule* getSchedule(int index);
    
    // Get analog trigger by index
    AnalogTrigger* getAnalogTrigger(int index);
    
    // Get schedules for JSON response
    void getSchedulesJson(JsonArray& schedulesArray);
    
    // Get analog triggers for JSON response
    void getAnalogTriggersJson(JsonArray& triggersArray);
    
    // Update schedule from JSON
    bool updateSchedule(JsonObject& scheduleJson);
    
    // Update analog trigger from JSON
    bool updateAnalogTrigger(JsonObject& triggerJson);
    
private:
    // References to other managers
    HardwareManager& _hardwareManager;
    SensorManager& _sensorManager;
    
    // Schedules array
    TimeSchedule _schedules[MAX_SCHEDULES];
    
    // Analog triggers array
    AnalogTrigger _analogTriggers[MAX_ANALOG_TRIGGERS];
    
    // Calculate current input state mask
    uint32_t calculateInputStateMask();
    
    // Helper for original API
    void executeScheduleAction(int scheduleIndex);
};

#endif // SCHEDULE_MANAGER_H