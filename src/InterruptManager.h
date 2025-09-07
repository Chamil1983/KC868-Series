/**
 * InterruptManager.h - Interrupt handling for KC868-A16
 * Created by Your Name, Date
 * Released into the public domain.
 */

#ifndef INTERRUPT_MANAGER_H
#define INTERRUPT_MANAGER_H

#include <Arduino.h>
#include <PCF8574.h>
#include "HardwareManager.h"
#include "ScheduleManager.h"

// Forward declarations
class HardwareManager;
class ScheduleManager;

// Constants for interrupt management
#define MAX_INTERRUPT_HANDLERS 16
#define INPUT_PRIORITY_HIGH 1
#define INPUT_PRIORITY_MEDIUM 2
#define INPUT_PRIORITY_LOW 3
#define INPUT_PRIORITY_NONE 0

// Constants for interrupt trigger types
#define INTERRUPT_TRIGGER_RISING     0
#define INTERRUPT_TRIGGER_FALLING    1
#define INTERRUPT_TRIGGER_CHANGE     2
#define INTERRUPT_TRIGGER_HIGH_LEVEL 3
#define INTERRUPT_TRIGGER_LOW_LEVEL  4

// Structure for interrupt configuration
struct InterruptConfig {
    bool enabled;
    uint8_t priority;     // 0=disabled, 1=high, 2=medium, 3=low
    uint8_t inputIndex;   // 0-15 for 16 digital inputs
    uint8_t triggerType;  // 0=rising, 1=falling, 2=change, 3=high level, 4=low level
    char name[32];        // Name for this interrupt
};

class InterruptManager {
public:
    InterruptManager(HardwareManager& hardwareManager, ScheduleManager& scheduleManager);
    
    // Initialize interrupt configurations
    void begin();
    
    // Save interrupt configurations to EEPROM
    void saveInterruptConfigs();
    
    // Load interrupt configurations from EEPROM
    void loadInterruptConfigs();
    
    // Set up input interrupts
    void setupInputInterrupts();
    
    // Disable input interrupts
    void disableInputInterrupts();
    
    // Process input interrupts
    void processInputInterrupts();
    
    // Process input change
    void processInputChange(int inputIndex, bool newState);
    
    // Poll non-interrupt inputs
    void pollNonInterruptInputs();
    
    // Get interrupt configuration
    InterruptConfig* getInterruptConfig(int index);
    
    // Update interrupt configuration
    bool updateInterruptConfig(int index, InterruptConfig& config);
    
    // Enable/disable interrupts
    bool enableInterrupt(int index, bool enable);
    
    // Enable/disable all interrupts
    void enableAllInterrupts(bool enable);
    
    // Check if interrupts are enabled
    bool areInterruptsEnabled() { return _interruptsEnabled; }
    
private:
    // Reference to hardware manager
    HardwareManager& _hardwareManager;
    
    // Reference to schedule manager
    ScheduleManager& _scheduleManager;
    
    // Interrupt configurations
    InterruptConfig _interruptConfigs[16];
    
    // Interrupt state variables
    volatile bool _inputStateChanged[16];
    bool _interruptsEnabled;
    
    // Timing for polling non-interrupt inputs
    unsigned long _lastInputReadTime;
    const unsigned long INPUT_READ_INTERVAL = 20; // ms for polling
    
    // EEPROM address for interrupt configuration
    const int EEPROM_INTERRUPT_CONFIG_ADDR = 3584;
    
    // Initialize default interrupt configurations
    void initInterruptConfigs();
};

#endif // INTERRUPT_MANAGER_H