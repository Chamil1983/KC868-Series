/**
 * InterruptManager.cpp - Interrupt handling for KC868-A16
 * Created by Your Name, Date
 * Released into the public domain.
 */

#include "InterruptManager.h"
#include <EEPROM.h>
#include <ArduinoJson.h>

InterruptManager::InterruptManager(HardwareManager& hardwareManager, ScheduleManager& scheduleManager) :
    _hardwareManager(hardwareManager),
    _scheduleManager(scheduleManager),
    _interruptsEnabled(false),
    _lastInputReadTime(0)
{
    // Initialize input state changed flags
    for (int i = 0; i < 16; i++) {
        _inputStateChanged[i] = false;
    }
}

void InterruptManager::begin() {
    // Initialize interrupt configurations
    initInterruptConfigs();

    // Load saved configurations
    loadInterruptConfigs();

    // Set up interrupts if any are enabled
    bool anyEnabled = false;
    for (int i = 0; i < 16; i++) {
        if (_interruptConfigs[i].enabled) {
            anyEnabled = true;
            break;
        }
    }

    if (anyEnabled) {
        setupInputInterrupts();
    }

    Serial.println("Interrupt manager initialized");
}

void InterruptManager::initInterruptConfigs() {
    for (int i = 0; i < 16; i++) {
        _interruptConfigs[i].enabled = false;
        _interruptConfigs[i].priority = INPUT_PRIORITY_MEDIUM;  // Default medium priority
        _interruptConfigs[i].inputIndex = i;
        _interruptConfigs[i].triggerType = INTERRUPT_TRIGGER_CHANGE;  // Default to change (both edges)
        snprintf(_interruptConfigs[i].name, 32, "Input %d", i + 1);
    }
}

void InterruptManager::saveInterruptConfigs() {
    DynamicJsonDocument doc(2048);
    JsonArray configArray = doc.createNestedArray("interrupts");

    for (int i = 0; i < 16; i++) {
        JsonObject config = configArray.createNestedObject();
        config["enabled"] = _interruptConfigs[i].enabled;
        config["priority"] = _interruptConfigs[i].priority;
        config["inputIndex"] = _interruptConfigs[i].inputIndex;
        config["triggerType"] = _interruptConfigs[i].triggerType;
        config["name"] = _interruptConfigs[i].name;
    }

    // Serialize to buffer
    char jsonBuffer[2048];
    size_t n = serializeJson(doc, jsonBuffer);

    // Store in EEPROM
    for (size_t i = 0; i < n && i < 1024; i++) {
        EEPROM.write(EEPROM_INTERRUPT_CONFIG_ADDR + i, jsonBuffer[i]);
    }

    // Write null terminator
    EEPROM.write(EEPROM_INTERRUPT_CONFIG_ADDR + n, 0);

    // Commit changes
    EEPROM.commit();

    Serial.println("Interrupt configurations saved");
}

void InterruptManager::loadInterruptConfigs() {
    // Create a buffer to read JSON data
    char jsonBuffer[2048];
    size_t i = 0;

    // Read data until null terminator or max buffer size
    while (i < 2047) {
        jsonBuffer[i] = EEPROM.read(EEPROM_INTERRUPT_CONFIG_ADDR + i);
        if (jsonBuffer[i] == 0) break;
        i++;
    }

    // Add null terminator if buffer is full
    jsonBuffer[i] = 0;

    // If we read something, try to parse it
    if (i > 0) {
        DynamicJsonDocument doc(2048);
        DeserializationError error = deserializeJson(doc, jsonBuffer);

        if (!error && doc.containsKey("interrupts")) {
            JsonArray configArray = doc["interrupts"];

            int index = 0;
            for (JsonObject config : configArray) {
                if (index >= 16) break;

                _interruptConfigs[index].enabled = config["enabled"] | false;
                _interruptConfigs[index].priority = config["priority"] | INPUT_PRIORITY_MEDIUM;
                _interruptConfigs[index].inputIndex = config["inputIndex"] | index;
                _interruptConfigs[index].triggerType = config["triggerType"] | INTERRUPT_TRIGGER_CHANGE;

                const char* name = config["name"];
                if (name) {
                    strlcpy(_interruptConfigs[index].name, name, 32);
                }

                index++;
            }

            Serial.println("Interrupt configurations loaded");
        }
        else {
            Serial.println("No valid interrupt configurations found, using defaults");
        }
    }
    else {
        Serial.println("No interrupt configurations found, using defaults");
    }
}

void InterruptManager::setupInputInterrupts() {
    // Disable any existing interrupts first
    disableInputInterrupts();

    // Check if any interrupt is enabled
    bool anyEnabled = false;
    for (int i = 0; i < 16; i++) {
        if (_interruptConfigs[i].enabled && _interruptConfigs[i].priority != INPUT_PRIORITY_NONE) {
            anyEnabled = true;
            break;
        }
    }

    if (!anyEnabled) {
        Serial.println("No input interrupts enabled");
        return;
    }

    // Setup task notifications for input interrupts
    Serial.println("Setting up input interrupts");

    // For I2C expanders, we use a polling approach with priorities determining the order of checking

    // Reset interrupt flags
    for (int i = 0; i < 16; i++) {
        _inputStateChanged[i] = false;
    }

    _interruptsEnabled = true;
}

void InterruptManager::disableInputInterrupts() {
    _interruptsEnabled = false;

    // Reset interrupt flags
    for (int i = 0; i < 16; i++) {
        _inputStateChanged[i] = false;
    }

    Serial.println("Input interrupts disabled");
}

void InterruptManager::processInputInterrupts() {
    if (!_interruptsEnabled) return;

    // Keep track of previous states for edge detection
    static bool prevInputStates[16] = { false };

    unsigned long currentMillis = millis();

    // Read all digital inputs into a temporary array to avoid multiple I2C transactions
    bool currentInputs[16];
    bool anyChange = false;

    // Read inputs from hardware manager (will be implemented to read from I2C expanders)
    for (int i = 0; i < 16; i++) {
        currentInputs[i] = _hardwareManager.getInputState(i);

        // Determine if this input should be processed based on its trigger type
        bool shouldProcess = false;

        if (_interruptConfigs[i].enabled) {
            switch (_interruptConfigs[i].triggerType) {
            case INTERRUPT_TRIGGER_RISING:
                // Process on rising edge (LOW to HIGH)
                shouldProcess = !prevInputStates[i] && currentInputs[i];
                break;

            case INTERRUPT_TRIGGER_FALLING:
                // Process on falling edge (HIGH to LOW)
                shouldProcess = prevInputStates[i] && !currentInputs[i];
                break;

            case INTERRUPT_TRIGGER_CHANGE:
                // Process on any edge (change)
                shouldProcess = prevInputStates[i] != currentInputs[i];
                break;

            case INTERRUPT_TRIGGER_HIGH_LEVEL:
                // Process when the input is HIGH
                shouldProcess = currentInputs[i];
                break;

            case INTERRUPT_TRIGGER_LOW_LEVEL:
                // Process when the input is LOW
                shouldProcess = !currentInputs[i];
                break;
            }

            if (shouldProcess) {
                anyChange = true;
                _inputStateChanged[i] = true;
            }
        }

        // Update previous state for next iteration
        prevInputStates[i] = currentInputs[i];
    }

    // If no changes detected, nothing to do
    if (!anyChange) return;

    // Process changes based on priority levels
    // First HIGH priority
    for (int i = 0; i < 16; i++) {
        if (_interruptConfigs[i].enabled &&
            _interruptConfigs[i].priority == INPUT_PRIORITY_HIGH &&
            _inputStateChanged[i]) {

            // Process this input change
            processInputChange(i, currentInputs[i]);
            _inputStateChanged[i] = false;
        }
    }

    // Then MEDIUM priority
    for (int i = 0; i < 16; i++) {
        if (_interruptConfigs[i].enabled &&
            _interruptConfigs[i].priority == INPUT_PRIORITY_MEDIUM &&
            _inputStateChanged[i]) {

            // Process this input change
            processInputChange(i, currentInputs[i]);
            _inputStateChanged[i] = false;
        }
    }

    // Finally LOW priority
    for (int i = 0; i < 16; i++) {
        if (_interruptConfigs[i].enabled &&
            _interruptConfigs[i].priority == INPUT_PRIORITY_LOW &&
            _inputStateChanged[i]) {

            // Process this input change
            processInputChange(i, currentInputs[i]);
            _inputStateChanged[i] = false;
        }
    }
}

void InterruptManager::processInputChange(int inputIndex, bool newState) {
    Serial.println("Input " + String(inputIndex + 1) + " changed to " + String(newState ? "HIGH" : "LOW"));

    // Check for any schedules that use this input and evaluate if they should be triggered
    _scheduleManager.checkInputBasedSchedules(inputIndex, newState);
}

void InterruptManager::pollNonInterruptInputs() {
    unsigned long currentMillis = millis();

    // Only poll at the specified interval
    if (currentMillis - _lastInputReadTime < INPUT_READ_INTERVAL) {
        return;
    }

    _lastInputReadTime = currentMillis;

    // Identify which inputs need polling (priority NONE)
    bool needsPolling[16] = { false };
    bool anyNeedPolling = false;

    for (int i = 0; i < 16; i++) {
        if (_interruptConfigs[i].priority == INPUT_PRIORITY_NONE) {
            needsPolling[i] = true;
            anyNeedPolling = true;
        }
    }

    // If no inputs need polling, exit
    if (!anyNeedPolling) return;

    // Poll only the required inputs
    for (int i = 0; i < 16; i++) {
        if (needsPolling[i]) {
            bool newState = _hardwareManager.getInputState(i);

            // Get previous state (would be tracked by hardware manager)
            bool oldState = false; // This should be retrieved from hardware manager

            if (newState != oldState) {
                processInputChange(i, newState);
            }
        }
    }
}

InterruptConfig* InterruptManager::getInterruptConfig(int index) {
    if (index >= 0 && index < 16) {
        return &_interruptConfigs[index];
    }
    return nullptr;
}

bool InterruptManager::updateInterruptConfig(int index, InterruptConfig& config) {
    if (index >= 0 && index < 16) {
        _interruptConfigs[index] = config;
        saveInterruptConfigs();

        // Reconfigure interrupts if needed
        if (_interruptsEnabled) {
            setupInputInterrupts();
        }

        return true;
    }
    return false;
}

bool InterruptManager::enableInterrupt(int index, bool enable) {
    if (index >= 0 && index < 16) {
        _interruptConfigs[index].enabled = enable;
        saveInterruptConfigs();

        // Check if any interrupts are still enabled
        bool anyEnabled = false;
        for (int i = 0; i < 16; i++) {
            if (_interruptConfigs[i].enabled) {
                anyEnabled = true;
                break;
            }
        }

        if (anyEnabled && _interruptsEnabled) {
            setupInputInterrupts(); // Reconfigure interrupts
        }
        else if (!anyEnabled) {
            disableInputInterrupts(); // Disable the entire interrupt system
        }

        return true;
    }
    return false;
}

void InterruptManager::enableAllInterrupts(bool enable) {
    for (int i = 0; i < 16; i++) {
        _interruptConfigs[i].enabled = enable;
    }

    saveInterruptConfigs();

    if (enable) {
        setupInputInterrupts();
    }
    else {
        disableInputInterrupts();
    }
}