/**
 * ScheduleManager.cpp - Schedule and trigger handling for KC868-A16
 * Created by Your Name, Date
 * Released into the public domain.
 */

#include "ScheduleManager.h"
#include <EEPROM.h>

ScheduleManager::ScheduleManager(HardwareManager& hardwareManager, SensorManager& sensorManager) :
    _hardwareManager(hardwareManager),
    _sensorManager(sensorManager) // Removed the extra parenthesis
{
    // Initialize default schedules
    for (int i = 0; i < MAX_SCHEDULES; i++) {
        _schedules[i].enabled = false;
        _schedules[i].triggerType = 0;     // Default to time-based
        _schedules[i].days = 0;
        _schedules[i].hour = 0;
        _schedules[i].minute = 0;
        _schedules[i].inputMask = 0;       // No inputs selected
        _schedules[i].inputStates = 0;     // All LOW
        _schedules[i].logic = 0;           // AND logic
        _schedules[i].action = 0;
        _schedules[i].targetType = 0;
        _schedules[i].targetId = 0;
        _schedules[i].targetIdLow = 0;
        _schedules[i].sensorIndex = 0;
        _schedules[i].sensorTriggerType = 0;
        _schedules[i].sensorCondition = 0;
        _schedules[i].sensorThreshold = 25.0;
        snprintf(_schedules[i].name, 32, "Schedule %d", i + 1);
    }

    // Initialize default analog triggers
    for (int i = 0; i < MAX_ANALOG_TRIGGERS; i++) {
        _analogTriggers[i].enabled = false;
        _analogTriggers[i].analogInput = 0;
        _analogTriggers[i].threshold = 2048;  // Middle value
        _analogTriggers[i].condition = 0;
        _analogTriggers[i].action = 0;
        _analogTriggers[i].targetType = 0;
        _analogTriggers[i].targetId = 0;
        snprintf(_analogTriggers[i].name, 32, "Trigger %d", i + 1);
    }
}

void ScheduleManager::begin() {
    loadSchedules();
    loadAnalogTriggers();
    Serial.println("Schedule manager initialized");
}

void ScheduleManager::saveSchedules() {
    DynamicJsonDocument doc(8192);
    JsonArray schedulesArray = doc.createNestedArray("schedules");

    try {
        // Add each schedule to the array
        for (int i = 0; i < MAX_SCHEDULES; i++) {
            JsonObject schedule = schedulesArray.createNestedObject();
            schedule["enabled"] = _schedules[i].enabled;
            schedule["name"] = _schedules[i].name;
            schedule["triggerType"] = _schedules[i].triggerType;
            schedule["days"] = _schedules[i].days;
            schedule["hour"] = _schedules[i].hour;
            schedule["minute"] = _schedules[i].minute;
            schedule["inputMask"] = _schedules[i].inputMask;
            schedule["inputStates"] = _schedules[i].inputStates;
            schedule["logic"] = _schedules[i].logic;
            schedule["action"] = _schedules[i].action;
            schedule["targetType"] = _schedules[i].targetType;
            schedule["targetId"] = _schedules[i].targetId;
            schedule["targetIdLow"] = _schedules[i].targetIdLow;
            schedule["sensorIndex"] = _schedules[i].sensorIndex;
            schedule["sensorTriggerType"] = _schedules[i].sensorTriggerType;
            schedule["sensorCondition"] = _schedules[i].sensorCondition;
            schedule["sensorThreshold"] = _schedules[i].sensorThreshold;
        }

        // Serialize to buffer with error handling
        char jsonBuffer[8192];
        size_t n = serializeJson(doc, jsonBuffer);

        // Store in EEPROM
        const int EEPROM_SCHEDULE_ADDR = 512;

        for (size_t i = 0; i < n && i < 1536; i++) {
            EEPROM.write(EEPROM_SCHEDULE_ADDR + i, jsonBuffer[i]);
        }

        // Write null terminator
        EEPROM.write(EEPROM_SCHEDULE_ADDR + n, 0);

        // Commit changes
        EEPROM.commit();

        Serial.println("Schedules saved to EEPROM");
    }
    catch (const std::exception& e) {
        Serial.print("Exception in saveSchedules: ");
        Serial.println(e.what());
    }
    catch (...) {
        Serial.println("Unknown exception in saveSchedules");
    }
}

void ScheduleManager::loadSchedules() {
    // Create a buffer to read JSON data
    char jsonBuffer[8192];
    size_t i = 0;

    // Define address for schedules
    const int EEPROM_SCHEDULE_ADDR = 512;

    // Read data until null terminator or max buffer size
    while (i < 8191) {
        jsonBuffer[i] = EEPROM.read(EEPROM_SCHEDULE_ADDR + i);
        if (jsonBuffer[i] == 0) break;
        i++;
    }

    // Add null terminator if buffer is full
    jsonBuffer[i] = 0;

    // If we read something, try to parse it
    if (i > 0) {
        DynamicJsonDocument doc(8192);
        DeserializationError error = deserializeJson(doc, jsonBuffer);

        if (!error && doc.containsKey("schedules")) {
            JsonArray schedulesArray = doc["schedules"];

            int index = 0;
            for (JsonObject scheduleJson : schedulesArray) {
                if (index >= MAX_SCHEDULES) break;

                _schedules[index].enabled = scheduleJson["enabled"] | false;
                strlcpy(_schedules[index].name, scheduleJson["name"] | "Schedule", 32);
                _schedules[index].triggerType = scheduleJson["triggerType"] | 0;
                _schedules[index].days = scheduleJson["days"] | 0;
                _schedules[index].hour = scheduleJson["hour"] | 0;
                _schedules[index].minute = scheduleJson["minute"] | 0;
                _schedules[index].inputMask = scheduleJson["inputMask"] | 0;
                _schedules[index].inputStates = scheduleJson["inputStates"] | 0;
                _schedules[index].logic = scheduleJson["logic"] | 0;
                _schedules[index].action = scheduleJson["action"] | 0;
                _schedules[index].targetType = scheduleJson["targetType"] | 0;
                _schedules[index].targetId = scheduleJson["targetId"] | 0;
                _schedules[index].targetIdLow = scheduleJson["targetIdLow"] | 0;
                _schedules[index].sensorIndex = scheduleJson["sensorIndex"] | 0;
                _schedules[index].sensorTriggerType = scheduleJson["sensorTriggerType"] | 0;
                _schedules[index].sensorCondition = scheduleJson["sensorCondition"] | 0;
                _schedules[index].sensorThreshold = scheduleJson["sensorThreshold"] | 25.0f;

                index++;
            }

            Serial.println("Schedules loaded from EEPROM");
        }
        else {
            Serial.println("Error parsing schedules JSON, using defaults");
        }
    }
    else {
        Serial.println("No schedules found in EEPROM, using defaults");
    }
}


void ScheduleManager::saveAnalogTriggers() {
    DynamicJsonDocument doc(4096);
    JsonArray triggersArray = doc.createNestedArray("triggers");

    try {
        for (int i = 0; i < MAX_ANALOG_TRIGGERS; i++) {
            JsonObject trigger = triggersArray.createNestedObject();
            trigger["enabled"] = _analogTriggers[i].enabled;
            trigger["name"] = _analogTriggers[i].name;
            trigger["analogInput"] = _analogTriggers[i].analogInput;
            trigger["threshold"] = _analogTriggers[i].threshold;
            trigger["condition"] = _analogTriggers[i].condition;
            trigger["action"] = _analogTriggers[i].action;
            trigger["targetType"] = _analogTriggers[i].targetType;
            trigger["targetId"] = _analogTriggers[i].targetId;
        }

        // Serialize to buffer
        char jsonBuffer[4096];
        size_t n = serializeJson(doc, jsonBuffer);

        // Store in EEPROM
        const int EEPROM_TRIGGER_ADDR = 2048;

        for (size_t i = 0; i < n && i < 1024; i++) {
            EEPROM.write(EEPROM_TRIGGER_ADDR + i, jsonBuffer[i]);
        }

        // Write null terminator
        EEPROM.write(EEPROM_TRIGGER_ADDR + n, 0);

        // Commit changes
        EEPROM.commit();

        Serial.println("Analog triggers saved to EEPROM");
    }
    catch (const std::exception& e) {
        Serial.print("Exception in saveAnalogTriggers: ");
        Serial.println(e.what());
    }
    catch (...) {
        Serial.println("Unknown exception in saveAnalogTriggers");
    }
}

void ScheduleManager::loadAnalogTriggers() {
    // Create a buffer to read JSON data
    char jsonBuffer[4096];
    size_t i = 0;

    // Define address for triggers
    const int EEPROM_TRIGGER_ADDR = 2048;

    // Read data until null terminator or max buffer size
    while (i < 4095) {
        jsonBuffer[i] = EEPROM.read(EEPROM_TRIGGER_ADDR + i);
        if (jsonBuffer[i] == 0) break;
        i++;
    }

    // Add null terminator if buffer is full
    jsonBuffer[i] = 0;

    // If we read something, try to parse it
    if (i > 0) {
        DynamicJsonDocument doc(4096);
        DeserializationError error = deserializeJson(doc, jsonBuffer);

        if (!error && doc.containsKey("triggers")) {
            JsonArray triggersArray = doc["triggers"];

            int index = 0;
            for (JsonObject triggerJson : triggersArray) {
                if (index >= MAX_ANALOG_TRIGGERS) break;

                _analogTriggers[index].enabled = triggerJson["enabled"] | false;
                strlcpy(_analogTriggers[index].name, triggerJson["name"] | "Trigger", 32);
                _analogTriggers[index].analogInput = triggerJson["analogInput"] | 0;
                _analogTriggers[index].threshold = triggerJson["threshold"] | 2048;
                _analogTriggers[index].condition = triggerJson["condition"] | 0;
                _analogTriggers[index].action = triggerJson["action"] | 0;
                _analogTriggers[index].targetType = triggerJson["targetType"] | 0;
                _analogTriggers[index].targetId = triggerJson["targetId"] | 0;

                index++;
            }

            Serial.println("Analog triggers loaded from EEPROM");
        }
        else {
            Serial.println("Error parsing analog triggers JSON, using defaults");
        }
    }
    else {
        Serial.println("No analog triggers found in EEPROM, using defaults");
    }
}

void ScheduleManager::checkSchedules() {
    // Get current time
    DateTime now = _sensorManager.getCurrentTime();
    
    // Calculate day of week bit (1=Sunday, 2=Monday, 4=Tuesday, etc.)
    uint8_t currentDayOfWeek = now.dayOfTheWeek();  // 0=Sunday, 1=Monday, etc.
    uint8_t currentDayBit = (1 << currentDayOfWeek);
    
    // Print current time info every minute for debugging
    static int lastMinutePrinted = -1;
    if (now.minute() != lastMinutePrinted) {
        lastMinutePrinted = now.minute();
        Serial.printf("Current time: %d-%d-%d %d:%d:%d\n", 
                     now.year(), now.month(), now.day(), 
                     now.hour(), now.minute(), now.second());
        Serial.printf("Day of week: %d, Day bit: %d\n", currentDayOfWeek, currentDayBit);
    }
    
    // Check each schedule
    for (int i = 0; i < MAX_SCHEDULES; i++) {
        if (!_schedules[i].enabled) {
            continue;
        }
        
        // We only care about time-based schedules here (type 0) or combined schedules (type 2)
        if (_schedules[i].triggerType == 0 || _schedules[i].triggerType == 2) {
            // Check if schedule should run today
            if (_schedules[i].days & currentDayBit) {
                // Check if it's time to run (only check the first 5 seconds of the minute)
                if (now.hour() == _schedules[i].hour && now.minute() == _schedules[i].minute && now.second() < 5) {
                    Serial.printf("Time trigger met for schedule %d: %s\n", i, _schedules[i].name);
                    
                    // For time-only schedules, execute directly
                    if (_schedules[i].triggerType == 0) {
                        executeScheduleAction(i);
                    }
                    // For combined schedules, check input conditions too
                    else if (_schedules[i].triggerType == 2) {
                        // Call checkInputBasedSchedules which handles combined schedules
                        checkInputBasedSchedules();
                    }
                }
            }
        }
    }
}

uint32_t ScheduleManager::calculateInputStateMask() {
    // Calculate current state of all inputs as a single 32-bit value
    uint32_t currentInputState = 0;
    
    // Add digital inputs (bits 0-15)
    for (int i = 0; i < 16; i++) {
        if (_hardwareManager.getInputState(i)) {
            currentInputState |= (1UL << i);
        }
    }
    
    // Add direct inputs HT1-HT3 (bits 16-18)
    for (int i = 0; i < 3; i++) {
        if (_hardwareManager.getDirectInputState(i)) {
            currentInputState |= (1UL << (16 + i));
        }
    }
    
    return currentInputState;
}

void ScheduleManager::checkInputBasedSchedules() {
    // Calculate current state of all inputs
    uint32_t currentInputState = calculateInputStateMask();
    
    // Check each schedule
    for (int i = 0; i < MAX_SCHEDULES; i++) {
        if (!_schedules[i].enabled) continue;
        
        // We now care about input-based, combined, and sensor-based schedules
        if (_schedules[i].triggerType != 1 && _schedules[i].triggerType != 2 && _schedules[i].triggerType != 3) continue;
        
        // Skip if this is an input-based schedule with no inputs
        if ((_schedules[i].triggerType == 1 || _schedules[i].triggerType == 2) && _schedules[i].inputMask == 0) continue;
        
        bool conditionMet = false;
        bool timeConditionMet = true;  // Default true for input-based/sensor-based, will check for combined
        
        if (_schedules[i].triggerType == 2) { // Combined type
            // Get current time
            DateTime now = _sensorManager.getCurrentTime();
            
            // Calculate day of week bit (1=Sunday, 2=Monday, 4=Tuesday, etc.)
            uint8_t currentDayOfWeek = now.dayOfTheWeek();  // 0=Sunday, 1=Monday, etc.
            uint8_t currentDayBit = (1 << currentDayOfWeek);
            
            // Check if schedule should run today
            if (!(_schedules[i].days & currentDayBit)) {
                timeConditionMet = false;
            }
            else {
                // Check if it's the right hour and minute
                timeConditionMet = (now.hour() == _schedules[i].hour && now.minute() == _schedules[i].minute);
            }
            
            if (!timeConditionMet) {
                continue; // Skip to next schedule if time condition not met for combined type
            }
        }
        
        // Track inputs with TRUE and FALSE matches to handle both conditions
        uint16_t highMatchingInputs = 0;
        uint16_t lowMatchingInputs = 0;
        
        // Check conditions based on trigger type
        if (_schedules[i].triggerType == 3) { // Sensor-based
            // Get the sensor index and ensure it's valid
            uint8_t sensorIndex = _schedules[i].sensorIndex;
            if (sensorIndex >= 3) continue; // Invalid sensor index
            
            // Skip if sensor is configured as digital input
            if (_sensorManager.getSensorType(sensorIndex) == 0) continue;
            
            bool sensorConditionMet = false;
            
            // Check temperature threshold
            if (_schedules[i].sensorTriggerType == 0) { // Temperature
                float currentTemp = _sensorManager.getTemperature(sensorIndex);
                float threshold = _schedules[i].sensorThreshold;
                
                switch (_schedules[i].sensorCondition) {
                    case 0: // Above
                        sensorConditionMet = (currentTemp > threshold);
                        break;
                    case 1: // Below
                        sensorConditionMet = (currentTemp < threshold);
                        break;
                    case 2: // Equal (with tolerance)
                        sensorConditionMet = (abs(currentTemp - threshold) < 0.5f);
                        break;
                }
            }
            // Check humidity threshold (only for DHT sensors)
            else if (_schedules[i].sensorTriggerType == 1 && 
                    (_sensorManager.getSensorType(sensorIndex) == 1 || 
                     _sensorManager.getSensorType(sensorIndex) == 2)) {
                
                float currentHumidity = _sensorManager.getHumidity(sensorIndex);
                float threshold = _schedules[i].sensorThreshold;
                
                switch (_schedules[i].sensorCondition) {
                    case 0: // Above
                        sensorConditionMet = (currentHumidity > threshold);
                        break;
                    case 1: // Below
                        sensorConditionMet = (currentHumidity < threshold);
                        break;
                    case 2: // Equal (with tolerance)
                        sensorConditionMet = (abs(currentHumidity - threshold) < 2.0f);
                        break;
                }
            }
            
            conditionMet = sensorConditionMet;
            
            // For sensor conditions, we'll use the targetId for the true condition
            // and targetIdLow for the false condition (like high and low for digital inputs)
            if (sensorConditionMet) {
                highMatchingInputs = 1; // Just a non-zero value to trigger the action
            } else {
                lowMatchingInputs = 1;
            }
        }
        else { // Input-based or combined with input
            // Evaluate input conditions based on logic type
            if (_schedules[i].logic == 0) {  // AND logic
                // All conditions must be met
                conditionMet = true;  // Start with true for AND logic
                
                for (int bitPos = 0; bitPos < 19; bitPos++) {
                    uint32_t bitMask = 1UL << bitPos;
                    
                    // If this bit is part of our input mask, check its state
                    if (_schedules[i].inputMask & bitMask) {
                        bool desiredState = (_schedules[i].inputStates & bitMask) != 0;
                        bool currentState = (currentInputState & bitMask) != 0;
                        
                        if (currentState != desiredState) {
                            conditionMet = false;
                            break; // Break early for AND logic if one condition fails
                        }
                        
                        // Track which inputs match which state for relay control
                        if (currentState) {
                            highMatchingInputs |= bitMask;
                        }
                        else {
                            lowMatchingInputs |= bitMask;
                        }
                    }
                }
            }
            else {  // OR logic
                // Any condition can trigger
                conditionMet = false;  // Start with false for OR logic
                
                for (int bitPos = 0; bitPos < 19; bitPos++) {
                    uint32_t bitMask = 1UL << bitPos;
                    
                    // If this bit is part of our input mask, check its state
                    if (_schedules[i].inputMask & bitMask) {
                        bool desiredState = (_schedules[i].inputStates & bitMask) != 0;
                        bool currentState = (currentInputState & bitMask) != 0;
                        
                        // Track which inputs match which state for relay control
                        if (currentState) {
                            highMatchingInputs |= bitMask;
                        }
                        else {
                            lowMatchingInputs |= bitMask;
                        }
                        
                        if (currentState == desiredState) {
                            conditionMet = true;
                            // Don't break early for OR logic - we need to track all matching inputs
                        }
                    }
                }
            }
        }
        
        // If all conditions are met, execute the schedule
        if (conditionMet && timeConditionMet) {
            Serial.printf("Trigger conditions met for schedule %d: %s\n", i, _schedules[i].name);
            
            // Execute actions for HIGH inputs if we have any inputs in HIGH state
            if (highMatchingInputs && _schedules[i].targetId > 0) {
                executeScheduleAction(i, _schedules[i].targetId);
            }
            
            // Execute actions for LOW inputs if we have any inputs in LOW state
            if (lowMatchingInputs && _schedules[i].targetIdLow > 0) {
                executeScheduleAction(i, _schedules[i].targetIdLow);
            }
        }
    }
}

void ScheduleManager::checkInputBasedSchedules(int changedInputIndex, bool newState) {
    // Calculate the bit mask for this input
    uint16_t changedInputMask = (1UL << changedInputIndex);
    
    // Get full input state mask
    uint32_t currentInputState = calculateInputStateMask();
    
    Serial.printf("Checking input-based schedules for input %d (state: %s)\n",
                 changedInputIndex, newState ? "HIGH" : "LOW");
    
    // Check each schedule
    for (int i = 0; i < MAX_SCHEDULES; i++) {
        if (!_schedules[i].enabled) continue;
        
        // We only care about input-based or combined schedules
        if (_schedules[i].triggerType != 1 && _schedules[i].triggerType != 2) continue;
        
        // Skip if this input isn't part of the schedule's input mask
        if (!(_schedules[i].inputMask & changedInputMask)) continue;
        
        Serial.printf("Evaluating schedule %d: %s\n", i, _schedules[i].name);
        
        bool inputConditionMet = false;
        
        // For combined schedules, we also need to check the time condition
        bool timeConditionMet = true;  // Default true, will be overridden if it's a combined schedule
        
        if (_schedules[i].triggerType == 2) { // Combined type
            DateTime now = _sensorManager.getCurrentTime();
            
            // Calculate day of week bit (1=Sunday, 2=Monday, 4=Tuesday, etc.)
            uint8_t currentDayOfWeek = now.dayOfTheWeek();  // 0=Sunday, 1=Monday, etc.
            uint8_t currentDayBit = (1 << currentDayOfWeek);
            
            // Check if schedule should run today
            if (!(_schedules[i].days & currentDayBit)) {
                timeConditionMet = false;
            }
            else {
                // Check if it's time to run
                timeConditionMet = (now.hour() == _schedules[i].hour && now.minute() == _schedules[i].minute);
            }
            
            if (!timeConditionMet) {
                Serial.printf("Time condition not met for combined schedule %d\n", i);
                continue; // Skip to next schedule if time condition not met for combined type
            }
        }
        
        // Check input conditions
        if (_schedules[i].inputMask != 0) {
            // Evaluate input conditions based on logic type
            if (_schedules[i].logic == 0) {  // AND logic
                // All conditions must be met
                inputConditionMet = true;  // Start with true for AND logic
                
                for (int bitPos = 0; bitPos < 19; bitPos++) {
                    uint32_t bitMask = 1UL << bitPos;
                    
                    // If this bit is part of our input mask, check its state
                    if (_schedules[i].inputMask & bitMask) {
                        bool desiredState = (_schedules[i].inputStates & bitMask) != 0;
                        bool currentState = (currentInputState & bitMask) != 0;
                        
                        if (currentState != desiredState) {
                            inputConditionMet = false;
                            break; // Break early for AND logic if one condition fails
                        }
                    }
                }
            }
            else {  // OR logic
                // Any condition can trigger
                inputConditionMet = false;  // Start with false for OR logic
                
                for (int bitPos = 0; bitPos < 19; bitPos++) {
                    uint32_t bitMask = 1UL << bitPos;
                    
                    // If this bit is part of our input mask, check its state
                    if (_schedules[i].inputMask & bitMask) {
                        bool desiredState = (_schedules[i].inputStates & bitMask) != 0;
                        bool currentState = (currentInputState & bitMask) != 0;
                        
                        if (currentState == desiredState) {
                            inputConditionMet = true;
                            break; // Break early for OR logic if one condition is met
                        }
                    }
                }
            }
        }
        
        Serial.printf("Input condition %s for schedule %d\n", 
                     inputConditionMet ? "met" : "not met", i);
        
        // For input-based trigger, we need the input condition to be met
        if (inputConditionMet) {
            executeSchedule(i);
        }
    }
}

void ScheduleManager::checkAnalogTriggers() {
    for (int i = 0; i < MAX_ANALOG_TRIGGERS; i++) {
        if (_analogTriggers[i].enabled) {
            uint8_t analogInput = _analogTriggers[i].analogInput;
            
            if (analogInput < 4) {
                int value = _hardwareManager.getAnalogValue(analogInput);
                bool triggerConditionMet = false;
                
                // Check condition
                if (_analogTriggers[i].condition == 0) {     // Above
                    triggerConditionMet = (value > _analogTriggers[i].threshold);
                }
                else if (_analogTriggers[i].condition == 1) { // Below
                    triggerConditionMet = (value < _analogTriggers[i].threshold);
                }
                else if (_analogTriggers[i].condition == 2) { // Equal (with some tolerance)
                    triggerConditionMet = (abs(value - _analogTriggers[i].threshold) < 50);
                }
                
                if (triggerConditionMet) {
                    Serial.printf("Analog trigger activated: %s\n", _analogTriggers[i].name);
                    
                    // Perform the trigger action
                    if (_analogTriggers[i].targetType == 0) {
                        // Single output
                        uint8_t relay = _analogTriggers[i].targetId;
                        if (relay < 16) {
                            if (_analogTriggers[i].action == 0) {        // OFF
                                _hardwareManager.setOutputState(relay, false);
                            }
                            else if (_analogTriggers[i].action == 1) { // ON
                                _hardwareManager.setOutputState(relay, true);
                            }
                            else if (_analogTriggers[i].action == 2) { // TOGGLE
                                _hardwareManager.setOutputState(relay, !_hardwareManager.getOutputState(relay));
                            }
                        }
                    }
                    else if (_analogTriggers[i].targetType == 1) {
                        // Multiple outputs (using bitmask)
                        for (int j = 0; j < 16; j++) {
                            if (_analogTriggers[i].targetId & (1 << j)) {
                                if (_analogTriggers[i].action == 0) {        // OFF
                                    _hardwareManager.setOutputState(j, false);
                                }
                                else if (_analogTriggers[i].action == 1) { // ON
                                    _hardwareManager.setOutputState(j, true);
                                }
                                else if (_analogTriggers[i].action == 2) { // TOGGLE
                                    _hardwareManager.setOutputState(j, !_hardwareManager.getOutputState(j));
                                }
                            }
                        }
                    }
                    
                    // Update outputs
                    _hardwareManager.writeOutputs();
                }
            }
        }
    }
}

void ScheduleManager::executeSchedule(int scheduleIndex) {
    if (scheduleIndex < 0 || scheduleIndex >= MAX_SCHEDULES || !_schedules[scheduleIndex].enabled) {
        return;
    }
    
    Serial.printf("Executing schedule: %s\n", _schedules[scheduleIndex].name);
    
    // Execute default action
    executeScheduleAction(scheduleIndex);
}

void ScheduleManager::executeScheduleAction(int scheduleIndex, uint16_t targetId) {
    if (scheduleIndex < 0 || scheduleIndex >= MAX_SCHEDULES || !_schedules[scheduleIndex].enabled) {
        return;
    }
    
    Serial.printf("Executing schedule action: %s with targetId %u\n", 
                 _schedules[scheduleIndex].name, targetId);
    
    // Perform the scheduled action
    if (_schedules[scheduleIndex].targetType == 0) {
        // Single output
        uint8_t relay = targetId;
        if (relay < 16) {
            Serial.printf("Setting single relay %u to %s\n", 
                         relay, 
                         _schedules[scheduleIndex].action == 0 ? "OFF" :
                         _schedules[scheduleIndex].action == 1 ? "ON" : "TOGGLE");
            
            if (_schedules[scheduleIndex].action == 0) {        // OFF
                _hardwareManager.setOutputState(relay, false);
            }
            else if (_schedules[scheduleIndex].action == 1) {   // ON
                _hardwareManager.setOutputState(relay, true);
            }
            else if (_schedules[scheduleIndex].action == 2) {   // TOGGLE
                _hardwareManager.setOutputState(relay, !_hardwareManager.getOutputState(relay));
            }
        }
    }
    else if (_schedules[scheduleIndex].targetType == 1) {
        // Multiple outputs (using bitmask)
        Serial.printf("Setting multiple relays with mask: %u\n", targetId);
        
        for (int j = 0; j < 16; j++) {
            if (targetId & (1 << j)) {
                Serial.printf("Setting relay %d to %s\n", 
                             j, 
                             _schedules[scheduleIndex].action == 0 ? "OFF" :
                             _schedules[scheduleIndex].action == 1 ? "ON" : "TOGGLE");
                
                if (_schedules[scheduleIndex].action == 0) {        // OFF
                    _hardwareManager.setOutputState(j, false);
                }
                else if (_schedules[scheduleIndex].action == 1) {   // ON
                    _hardwareManager.setOutputState(j, true);
                }
                else if (_schedules[scheduleIndex].action == 2) {   // TOGGLE
                    _hardwareManager.setOutputState(j, !_hardwareManager.getOutputState(j));
                }
            }
        }
    }
    
    // Update outputs
    if (!_hardwareManager.writeOutputs()) {
        Serial.println("ERROR: Failed to write outputs when executing schedule");
    }
}

// Helper for original API
void ScheduleManager::executeScheduleAction(int scheduleIndex) {
    executeScheduleAction(scheduleIndex, _schedules[scheduleIndex].targetId);
}

// Get schedule by index
TimeSchedule* ScheduleManager::getSchedule(int index) {
    if (index < 0 || index >= MAX_SCHEDULES) {
        return nullptr;
    }
    return &_schedules[index];
}

// Get analog trigger by index
AnalogTrigger* ScheduleManager::getAnalogTrigger(int index) {
    if (index < 0 || index >= MAX_ANALOG_TRIGGERS) {
        return nullptr;
    }
    return &_analogTriggers[index];
}

// Get schedules for JSON response
void ScheduleManager::getSchedulesJson(JsonArray& schedulesArray) {
    for (int i = 0; i < MAX_SCHEDULES; i++) {
        JsonObject schedule = schedulesArray.createNestedObject();
        schedule["id"] = i;
        schedule["enabled"] = _schedules[i].enabled;
        schedule["name"] = _schedules[i].name;
        schedule["triggerType"] = _schedules[i].triggerType;
        schedule["days"] = _schedules[i].days;
        schedule["hour"] = _schedules[i].hour;
        schedule["minute"] = _schedules[i].minute;
        schedule["inputMask"] = _schedules[i].inputMask;
        schedule["inputStates"] = _schedules[i].inputStates;
        schedule["logic"] = _schedules[i].logic;
        schedule["action"] = _schedules[i].action;
        schedule["targetType"] = _schedules[i].targetType;
        schedule["targetId"] = _schedules[i].targetId;
        schedule["targetIdLow"] = _schedules[i].targetIdLow;
        schedule["sensorIndex"] = _schedules[i].sensorIndex;
        schedule["sensorTriggerType"] = _schedules[i].sensorTriggerType;
        schedule["sensorCondition"] = _schedules[i].sensorCondition;
        schedule["sensorThreshold"] = _schedules[i].sensorThreshold;
    }
}

// Get analog triggers for JSON response
void ScheduleManager::getAnalogTriggersJson(JsonArray& triggersArray) {
    for (int i = 0; i < MAX_ANALOG_TRIGGERS; i++) {
        JsonObject trigger = triggersArray.createNestedObject();
        trigger["id"] = i;
        trigger["enabled"] = _analogTriggers[i].enabled;
        trigger["name"] = _analogTriggers[i].name;
        trigger["analogInput"] = _analogTriggers[i].analogInput;
        trigger["threshold"] = _analogTriggers[i].threshold;
        trigger["condition"] = _analogTriggers[i].condition;
        trigger["action"] = _analogTriggers[i].action;
        trigger["targetType"] = _analogTriggers[i].targetType;
        trigger["targetId"] = _analogTriggers[i].targetId;
    }
}

// Update schedule from JSON
bool ScheduleManager::updateSchedule(JsonObject& scheduleJson) {
    int id = scheduleJson.containsKey("id") ? scheduleJson["id"].as<int>() : -1;

    if (id >= 0 && id < MAX_SCHEDULES) {
        try {
            // Use defaults for all properties if not provided
            _schedules[id].enabled = scheduleJson["enabled"] | false;

            // For string values, ensure we don't exceed buffer size
            const char* nameStr = scheduleJson["name"] | "Schedule";
            strncpy(_schedules[id].name, nameStr, 31);
            _schedules[id].name[31] = '\0'; // Ensure null termination

            _schedules[id].triggerType = scheduleJson["triggerType"] | 0;
            _schedules[id].days = scheduleJson["days"] | 0;
            _schedules[id].hour = scheduleJson["hour"] | 0;
            _schedules[id].minute = scheduleJson["minute"] | 0;
            _schedules[id].inputMask = scheduleJson["inputMask"] | 0;
            _schedules[id].inputStates = scheduleJson["inputStates"] | 0;
            _schedules[id].logic = scheduleJson["logic"] | 0;
            _schedules[id].action = scheduleJson["action"] | 0;
            _schedules[id].targetType = scheduleJson["targetType"] | 0;
            _schedules[id].targetId = scheduleJson["targetId"] | 0;
            _schedules[id].targetIdLow = scheduleJson["targetIdLow"] | 0;
            _schedules[id].sensorIndex = scheduleJson["sensorIndex"] | 0;
            _schedules[id].sensorTriggerType = scheduleJson["sensorTriggerType"] | 0;
            _schedules[id].sensorCondition = scheduleJson["sensorCondition"] | 0;
            _schedules[id].sensorThreshold = scheduleJson["sensorThreshold"] | 25.0f;

            saveSchedules();
            return true;
        }
        catch (const std::exception& e) {
            Serial.print("Exception in updateSchedule: ");
            Serial.println(e.what());
            return false;
        }
        catch (...) {
            Serial.println("Unknown exception in updateSchedule");
            return false;
        }
    }

    return false;
}

// Update analog trigger from JSON
bool ScheduleManager::updateAnalogTrigger(JsonObject& triggerJson) {
    int id = triggerJson.containsKey("id") ? triggerJson["id"].as<int>() : -1;

    if (id >= 0 && id < MAX_ANALOG_TRIGGERS) {
        try {
            // Use defaults for all properties if not provided
            _analogTriggers[id].enabled = triggerJson["enabled"] | false;

            // For string values, ensure we don't exceed buffer size
            const char* nameStr = triggerJson["name"] | "Trigger";
            strncpy(_analogTriggers[id].name, nameStr, 31);
            _analogTriggers[id].name[31] = '\0'; // Ensure null termination

            _analogTriggers[id].analogInput = triggerJson["analogInput"] | 0;
            _analogTriggers[id].threshold = triggerJson["threshold"] | 2048;
            _analogTriggers[id].condition = triggerJson["condition"] | 0;
            _analogTriggers[id].action = triggerJson["action"] | 0;
            _analogTriggers[id].targetType = triggerJson["targetType"] | 0;
            _analogTriggers[id].targetId = triggerJson["targetId"] | 0;

            saveAnalogTriggers();
            return true;
        }
        catch (const std::exception& e) {
            Serial.print("Exception in updateAnalogTrigger: ");
            Serial.println(e.what());
            return false;
        }
        catch (...) {
            Serial.println("Unknown exception in updateAnalogTrigger");
            return false;
        }
    }

    return false;
}

