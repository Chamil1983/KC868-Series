/**
 * HardwareManager.cpp - Hardware I/O management for KC868-A16
 * Created by Your Name, Date
 * Released into the public domain.
 */

#include "HardwareManager.h"

HardwareManager::HardwareManager() :
    _inputIC1(PCF8574_INPUTS_1_8),
    _inputIC2(PCF8574_INPUTS_9_16),
    _outputIC3(PCF8574_OUTPUTS_9_16),
    _outputIC4(PCF8574_OUTPUTS_1_8),
    _i2cErrorCount(0)
{
    // Initialize state arrays
    for (int i = 0; i < 16; i++) {
        _outputStates[i] = false;
        _inputStates[i] = false;
    }
    
    for (int i = 0; i < 3; i++) {
        _directInputStates[i] = false;
    }
    
    for (int i = 0; i < 4; i++) {
        _analogValues[i] = 0;
        _analogVoltages[i] = 0.0;
    }
}

void HardwareManager::begin() {
    // Initialize I2C with custom pins
    Wire.begin(SDA_PIN, SCL_PIN);
    Wire.setClock(50000);  // Lower to 50kHz for more reliable communication
    
    // Initialize PCF8574 expanders
    initI2C();
    
    // Initialize direct GPIO inputs
    pinMode(HT1_PIN, INPUT_PULLUP);
    pinMode(HT2_PIN, INPUT_PULLUP);
    pinMode(HT3_PIN, INPUT_PULLUP);
    
    // Initialize output states (All relays OFF)
    writeOutputs();
    
    // Read initial input states
    readInputs();
    
    // Read initial analog values
    readAllAnalogInputs();
    
    Serial.println("Hardware initialized");
}

void HardwareManager::initI2C() {
    // Configure input pins (set as inputs with pull-ups)
    for (int i = 0; i < 8; i++) {
        _inputIC1.pinMode(i, INPUT);
        _inputIC2.pinMode(i, INPUT);
    }
    
    // Configure output pins (set as outputs)
    for (int i = 0; i < 8; i++) {
        _outputIC3.pinMode(i, OUTPUT);
        _outputIC4.pinMode(i, OUTPUT);
    }
    
    // Initialize PCF8574 ICs
    if (!_inputIC1.begin()) {
        Serial.println("Error: Could not initialize Input IC1 (0x22)");
        _i2cErrorCount++;
        _lastErrorMessage = "Failed to initialize Input IC1";
    }
    
    if (!_inputIC2.begin()) {
        Serial.println("Error: Could not initialize Input IC2 (0x21)");
        _i2cErrorCount++;
        _lastErrorMessage = "Failed to initialize Input IC2";
    }
    
    if (!_outputIC3.begin()) {
        Serial.println("Error: Could not initialize Output IC3 (0x25)");
        _i2cErrorCount++;
        _lastErrorMessage = "Failed to initialize Output IC3";
    }
    
    if (!_outputIC4.begin()) {
        Serial.println("Error: Could not initialize Output IC4 (0x24)");
        _i2cErrorCount++;
        _lastErrorMessage = "Failed to initialize Output IC4";
    }
    
    // Initialize all outputs to HIGH (OFF state due to inverted logic)
    for (int i = 0; i < 8; i++) {
        _outputIC3.digitalWrite(i, HIGH);
        _outputIC4.digitalWrite(i, HIGH);
    }
    
    // Initialize input state arrays
    for (int i = 0; i < 16; i++) {
        _inputStates[i] = true;   // Default HIGH (pull-up)
    }
    
    Serial.println("I2C and PCF8574 expanders initialized successfully");
}

bool HardwareManager::readInputs() {
    bool anyChanged = false;
    bool success = true;
    
    // Store previous input states to check for changes
    bool prevInputStates[16];
    bool prevDirectInputStates[3];
    
    // Copy current states to previous states
    for (int i = 0; i < 16; i++) {
        prevInputStates[i] = _inputStates[i];
    }
    
    for (int i = 0; i < 3; i++) {
        prevDirectInputStates[i] = _directInputStates[i];
    }
    
    // Read from PCF8574 input expanders using the PCF8574 library
    // Inputs 1-8 (IC1)
    for (int i = 0; i < 8; i++) {
        bool newState = false;
        try {
            newState = _inputIC1.digitalRead(i);
        }
        catch (const std::exception& e) {
            _i2cErrorCount++;
            _lastErrorMessage = "Error reading from Input IC1";
            success = false;
            Serial.println("Error reading from Input IC1: " + String(e.what()));
            continue;
        }
        
        // Invert because of the pull-up configuration (LOW = active/true)
        newState = !newState;
        
        if (_inputStates[i] != newState) {
            _inputStates[i] = newState;
            anyChanged = true;
            Serial.println("Input " + String(i + 1) + " changed to " + String(newState ? "HIGH" : "LOW"));
        }
    }
    
    // Inputs 9-16 (IC2)
    for (int i = 0; i < 8; i++) {
        bool newState = false;
        try {
            newState = _inputIC2.digitalRead(i);
        }
        catch (const std::exception& e) {
            _i2cErrorCount++;
            _lastErrorMessage = "Error reading from Input IC2";
            success = false;
            Serial.println("Error reading from Input IC2: " + String(e.what()));
            continue;
        }
        
        // Invert because of the pull-up configuration (LOW = active/true)
        newState = !newState;
        
        if (_inputStates[i + 8] != newState) {
            _inputStates[i + 8] = newState;
            anyChanged = true;
            Serial.println("Input " + String(i + 9) + " changed to " + String(newState ? "HIGH" : "LOW"));
        }
    }
    
    // Read direct GPIO inputs with inversion (LOW = active/true)
    bool ht1 = !digitalRead(HT1_PIN);
    bool ht2 = !digitalRead(HT2_PIN);
    bool ht3 = !digitalRead(HT3_PIN);
    
    if (_directInputStates[0] != ht1) {
        _directInputStates[0] = ht1;
        anyChanged = true;
        Serial.println("HT1 changed to " + String(ht1 ? "HIGH" : "LOW"));
    }
    
    if (_directInputStates[1] != ht2) {
        _directInputStates[1] = ht2;
        anyChanged = true;
        Serial.println("HT2 changed to " + String(ht2 ? "HIGH" : "LOW"));
    }
    
    if (_directInputStates[2] != ht3) {
        _directInputStates[2] = ht3;
        anyChanged = true;
        Serial.println("HT3 changed to " + String(ht3 ? "HIGH" : "LOW"));
    }
    
    return anyChanged;
}

bool HardwareManager::writeOutputs() {
    bool success = true;
    
    // Set outputs 1-8 (IC4)
    for (int i = 0; i < 8; i++) {
        try {
            // Write HIGH when output state is false (relays are active LOW)
            _outputIC4.digitalWrite(i, _outputStates[i] ? LOW : HIGH);
        }
        catch (const std::exception& e) {
            _i2cErrorCount++;
            _lastErrorMessage = "Failed to write to Output IC4";
            success = false;
            Serial.println("Error writing to Output IC4: " + String(e.what()));
        }
    }
    
    // Set outputs 9-16 (IC3)
    for (int i = 0; i < 8; i++) {
        try {
            // Write HIGH when output state is false (relays are active LOW)
            _outputIC3.digitalWrite(i, _outputStates[i + 8] ? LOW : HIGH);
        }
        catch (const std::exception& e) {
            _i2cErrorCount++;
            _lastErrorMessage = "Failed to write to Output IC3";
            success = false;
            Serial.println("Error writing to Output IC3: " + String(e.what()));
        }
    }
    
    if (success) {
        Serial.println("Successfully updated all relays");
    }
    else {
        Serial.println("ERROR: Failed to write to some output expanders");
        // Try to recover I2C bus
        Wire.flush();
        delay(50);
    }
    
    return success;
}

int HardwareManager::readAnalogInput(uint8_t index) {
    int pinMapping[] = { ANALOG_PIN_1, ANALOG_PIN_2, ANALOG_PIN_3, ANALOG_PIN_4 };
    
    if (index >= 4) return 0;
    
    // Take multiple readings and average them for better stability
    const int numReadings = 10;  // Increased from 5 to 10 for better accuracy
    int total = 0;
    
    for (int i = 0; i < numReadings; i++) {
        total += analogRead(pinMapping[index]);
        delay(1);  // Short delay between readings
    }
    
    return total / numReadings;
}

bool HardwareManager::readAllAnalogInputs() {
    bool analogChanged = false;
    
    for (int i = 0; i < 4; i++) {
        int newValue = readAnalogInput(i);
        if (abs(newValue - _analogValues[i]) > 10) { // Reduced threshold for more sensitivity
            _analogValues[i] = newValue;
            _analogVoltages[i] = convertAnalogToVoltage(newValue); // Update voltage
            analogChanged = true;
        }
    }
    
    return analogChanged;
}

float HardwareManager::convertAnalogToVoltage(int analogValue) {
    // Calibration data: pairs of [ADC value, Actual Voltage]
    // These should be measured using a calibrated reference
    const int calADC[] = { 0, 820, 1640, 2460, 3270, 4095 };
    const float calVolts[] = { 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f };
    const int numCalPoints = 6;  // Number of calibration points
    
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

int HardwareManager::calculatePercentage(float voltage) {
    // Ensure voltage is in the correct range
    if (voltage > 5.0f) voltage = 5.0f;
    if (voltage < 0.0f) voltage = 0.0f;
    
    // Calculate percentage based on 0-5V range
    return (int)((voltage / 5.0f) * 100.0f);
}

void HardwareManager::printIOStates() {
    Serial.println("--- Current I/O States ---");
    
    // Print input states
    Serial.println("Input States (1=HIGH/OFF, 0=LOW/ON):");
    Serial.print("Inputs 1-8:  ");
    for (int i = 7; i >= 0; i--) {
        Serial.print(_inputStates[i] ? "1" : "0");
    }
    Serial.println();
    
    Serial.print("Inputs 9-16: ");
    for (int i = 15; i >= 8; i--) {
        Serial.print(_inputStates[i] ? "1" : "0");
    }
    Serial.println();
    
    // Print output states
    Serial.println("Output States (1=HIGH/ON, 0=LOW/OFF):");
    Serial.print("Outputs 1-8:  ");
    for (int i = 7; i >= 0; i--) {
        Serial.print(_outputStates[i] ? "1" : "0");
    }
    Serial.println();
    
    Serial.print("Outputs 9-16: ");
    for (int i = 15; i >= 8; i--) {
        Serial.print(_outputStates[i] ? "1" : "0");
    }
    Serial.println();
    
    // Print analog inputs with voltage values
    Serial.println("Analog Inputs (0-5V range):");
    for (int i = 0; i < 4; i++) {
        Serial.print("A");
        Serial.print(i + 1);
        Serial.print(": Raw=");
        Serial.print(_analogValues[i]);
        Serial.print(", Voltage=");
        Serial.print(_analogVoltages[i], 2); // Display with 2 decimal places
        Serial.print("V, ");
        Serial.print(calculatePercentage(_analogVoltages[i]));
        Serial.println("%");
    }
    
    Serial.println("----------------------------");
}

bool HardwareManager::getOutputState(uint8_t index) {
    if (index < 16) {
        return _outputStates[index];
    }
    return false;
}

void HardwareManager::setOutputState(uint8_t index, bool state) {
    if (index < 16) {
        _outputStates[index] = state;
    }
}

void HardwareManager::setAllOutputs(bool state) {
    for (int i = 0; i < 16; i++) {
        _outputStates[i] = state;
    }
}

bool HardwareManager::getInputState(uint8_t index) {
    if (index < 16) {
        return _inputStates[index];
    }
    return false;
}

bool HardwareManager::getDirectInputState(uint8_t index) {
    if (index < 3) {
        return _directInputStates[index];
    }
    return false;
}

int HardwareManager::getAnalogValue(uint8_t index) {
    if (index < 4) {
        return _analogValues[index];
    }
    return 0;
}

float HardwareManager::getAnalogVoltage(uint8_t index) {
    if (index < 4) {
        return _analogVoltages[index];
    }
    return 0.0f;
}