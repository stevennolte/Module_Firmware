#ifndef MCPMANAGER_H
#define MCPMANAGER_H

#include <Arduino.h>
#include <Adafruit_MCP23X17.h>
#include <Wire.h>
#include "ESPdata.h"

// LED State Management
enum class MCPLEDState {
    OFF,                    // LED off
    ON,                     // LED solid on
    SLOW_PULSE,            // Slow pulse (1 Hz)
    FAST_PULSE,            // Fast pulse (2 Hz)
    RAPID_PULSE,           // Rapid pulse (5 Hz)
    ERROR_FLASH,           // Error flash pattern (3 quick flashes, pause)
    HEARTBEAT              // Heartbeat pattern (double pulse)
};

struct LEDIndicator {
    uint8_t pin;
    MCPLEDState state;
    unsigned long lastToggle;
    bool currentState;
    uint8_t flashCount;    // For error flash pattern
    
    LEDIndicator() : pin(0), state(MCPLEDState::OFF), lastToggle(0), currentState(false), flashCount(0) {}
    LEDIndicator(uint8_t p) : pin(p), state(MCPLEDState::OFF), lastToggle(0), currentState(false), flashCount(0) {}
};

class MCPManager {
private:
    static MCPManager* instance;
    Adafruit_MCP23X17 mcp;
    bool initialized;
    ESPdata* espData;  // Pointer to ESPdata for pin access
    
    // LED Indicators
    LEDIndicator ledGPSFix;
    LEDIndicator ledRTKFix;
    LEDIndicator ledPowerOn;
    LEDIndicator ledEthGood;
    LEDIndicator ledSteerStandby;
    LEDIndicator ledSteerActive;
    
    TaskHandle_t ledTaskHandle;
    
    // Private methods
    static void ledUpdateTask(void* parameter);
    void updateLEDs();
    void updateLED(LEDIndicator& led);
    
    // Private constructor for singleton
    MCPManager();
    
public:
    // Get singleton instance
    static MCPManager& getInstance();
    
    // Destroy singleton instance
    static void destroyInstance();
    
    // Initialize the MCP23X17 with ESPdata reference
    bool begin(ESPdata* espDataPtr, uint8_t address = 0x20, TwoWire* wire = &Wire);
    
    // Check if MCP is initialized
    bool isInitialized() const;
    
    // Basic pin control functions
    void pinMode(uint8_t pin, uint8_t mode);
    void digitalWrite(uint8_t pin, uint8_t value);
    uint8_t digitalRead(uint8_t pin);
    
    // Pull-up control
    void pullUp(uint8_t pin, uint8_t value);
    
    // Port-wide operations
    void writeGPIOA(uint8_t value);
    void writeGPIOB(uint8_t value);
    uint8_t readGPIOA();
    uint8_t readGPIOB();
    
    // Motor driver specific functions
    void setupMotorPins(uint8_t enaPin, uint8_t enbPin);
    void enableMotor(uint8_t enaPin, uint8_t enbPin);
    void disableMotor(uint8_t enaPin, uint8_t enbPin);
    
    // Power control specific functions
    void setupPowerPin(uint8_t powerPin);
    void setPowerState(uint8_t powerPin, bool state);
    
    // GPS indicator specific functions
    void setupGPSIndicators(uint8_t gpsFixPin, uint8_t rtkFixPin);
    void testGPSIndicators(uint8_t gpsFixPin, uint8_t rtkFixPin);
    void setGPSFix(uint8_t gpsFixPin, bool state);
    void setRTKFix(uint8_t rtkFixPin, bool state);
    
    // Convenience methods using ESPdata pin definitions
    void setupMotorPins();  // Uses ESPdata pin definitions
    void enableMotor();     // Uses ESPdata pin definitions  
    void disableMotor();    // Uses ESPdata pin definitions
    void setupPowerPin();   // Uses ESPdata pin definitions
    void setPowerState(bool state);  // Uses ESPdata pin definitions
    void setupGPSIndicators();       // Uses ESPdata pin definitions
    void testGPSIndicators();        // Uses ESPdata pin definitions
    void setGPSFix(bool state);      // Uses ESPdata pin definitions
    void setRTKFix(bool state);      // Uses ESPdata pin definitions
    
    // LED State Management methods
    void setPowerLED(MCPLEDState state);
    void setGPSLED(MCPLEDState state);
    void setRTKLED(MCPLEDState state);
    void setEthLED(MCPLEDState state);
    void setSteerStandbyLED(MCPLEDState state);
    void setSteerActiveLED(MCPLEDState state);
    void setAllLEDs(MCPLEDState state);
    
    // Get direct access to MCP object (for advanced usage or backward compatibility)
    Adafruit_MCP23X17* getMCP();
    
    // Destructor
    ~MCPManager();
};

#endif // MCPMANAGER_H
