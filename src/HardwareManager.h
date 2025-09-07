/**
 * HardwareManager.h - Hardware I/O management for KC868-A16
 * Created by Your Name, Date
 * Released into the public domain.
 */

#ifndef HARDWARE_MANAGER_H
#define HARDWARE_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <PCF8574.h>

// I2C PCF8574 addresses
#define PCF8574_INPUTS_1_8    0x22
#define PCF8574_INPUTS_9_16   0x21
#define PCF8574_OUTPUTS_1_8   0x24
#define PCF8574_OUTPUTS_9_16  0x25

// GPIO definitions
#define SDA_PIN               4
#define SCL_PIN               5
#define HT1_PIN               32
#define HT2_PIN               33
#define HT3_PIN               14
#define RF_RX_PIN             2
#define RF_TX_PIN             15
#define RS485_TX_PIN          13
#define RS485_RX_PIN          16
#define ANALOG_PIN_1          36
#define ANALOG_PIN_2          34
#define ANALOG_PIN_3          35
#define ANALOG_PIN_4          39

// Analog input scaling
#define ADC_MAX_VALUE         4095    // ESP32 ADC is 12-bit (0-4095)
#define ADC_VOLTAGE_MAX       3.3     // ESP32 ADC reference voltage is 3.3V
#define ANALOG_VOLTAGE_MAX    5.0     // Full scale of the analog inputs is 5V

class HardwareManager {
public:
    HardwareManager();
    
    // Initialize hardware
    void begin();
    
    // Read inputs from PCF8574 chips and direct GPIO inputs
    bool readInputs();
    
    // Write outputs to PCF8574 chips
    bool writeOutputs();
    
    // Read a single analog input with improved noise reduction
    int readAnalogInput(uint8_t index);
    
    // Read all analog inputs, return true if any changed significantly
    bool readAllAnalogInputs();
    
    // Convert ADC value to voltage with calibration
    float convertAnalogToVoltage(int analogValue);
    
    // Calculate percentage for 0-5V range
    int calculatePercentage(float voltage);
    
    // Print I/O states for debugging
    void printIOStates();
    
    // Get output states
    bool getOutputState(uint8_t index);
    
    // Set output state
    void setOutputState(uint8_t index, bool state);
    
    // Set all outputs to the same state
    void setAllOutputs(bool state);
    
    // Get input state
    bool getInputState(uint8_t index);
    
    // Get direct input state (HT1-HT3)
    bool getDirectInputState(uint8_t index);
    
    // Get analog value
    int getAnalogValue(uint8_t index);
    
    // Get analog voltage
    float getAnalogVoltage(uint8_t index);
    
    // Get I2C error count
    unsigned long getI2CErrorCount() { return _i2cErrorCount; }
    
    // Get last error message
    String getLastErrorMessage() { return _lastErrorMessage; }
    
private:
    // PCF8574 expanders
    PCF8574 _inputIC1;  // Digital Inputs 1-8
    PCF8574 _inputIC2;  // Digital Inputs 9-16
    PCF8574 _outputIC3; // Digital Outputs 9-16
    PCF8574 _outputIC4; // Digital Outputs 1-8
    
    // State arrays
    bool _outputStates[16];        // Current output states
    bool _inputStates[16];         // Current input states
    bool _directInputStates[3];    // Current HT1-HT3 states
    int _analogValues[4];          // Current analog input values (raw ADC values)
    float _analogVoltages[4];      // Current analog input voltages (0-5V)
    
    // Diagnostics
    unsigned long _i2cErrorCount;
    String _lastErrorMessage;
    
    // Initialize I2C communication with PCF8574 chips
    void initI2C();
};

#endif // HARDWARE_MANAGER_H