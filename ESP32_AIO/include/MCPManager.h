#ifndef MCPMANAGER_H
#define MCPMANAGER_H

#include <Arduino.h>
#include <Adafruit_MCP23X17.h>
#include <Wire.h>
#include "ESPdata.h"

class MCPManager {
private:
    static MCPManager* instance;
    Adafruit_MCP23X17 mcp;
    bool initialized;
    ESPdata* espData;  // Pointer to ESPdata for pin access
    
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
    
    // Get direct access to MCP object (for advanced usage or backward compatibility)
    Adafruit_MCP23X17* getMCP();
    
    // Destructor
    ~MCPManager();
};

#endif // MCPMANAGER_H
