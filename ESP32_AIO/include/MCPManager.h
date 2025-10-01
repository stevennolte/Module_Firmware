/**
 * @file MCPManager.h
 * @brief MCP23017 I/O expander management with LED state control
 * 
 * @details This header defines the MCPManager singleton class that provides
 *          comprehensive management of the MCP23017 16-bit I/O expander including:
 *          - GPIO pin configuration and control
 *          - Advanced LED state management with multiple animation patterns
 *          - Input monitoring for switches and sensors
 *          - Status indication for system health and operational states
 *          - Thread-safe singleton pattern for system-wide access
 * 
 * @author Steve Gavel
 * @date 2024
 * @version 4.6.1
 * 
 * @see MCPManager.cpp for implementation details
 * @see ESPdata.h for pin assignments and configuration
 */

#ifndef MCPMANAGER_H
#define MCPMANAGER_H

#include <Arduino.h>
#include <Adafruit_MCP23X17.h>
#include <Wire.h>
#include "ESPdata.h"

/**
 * @brief LED animation state enumeration for status indication
 * 
 * @details Defines various LED patterns for system status communication:
 *          - Static states (off/on) for basic indication
 *          - Pulse patterns at different frequencies for activity indication
 *          - Error patterns for fault indication
 *          - Heartbeat pattern for system health monitoring
 */
enum class MCPLEDState {
    OFF,                    ///< @brief LED completely off
    ON,                     ///< @brief LED solid on (constant illumination)
    SLOW_PULSE,            ///< @brief Slow pulse at 1 Hz for low-priority status
    FAST_PULSE,            ///< @brief Fast pulse at 2 Hz for active status
    RAPID_PULSE,           ///< @brief Rapid pulse at 5 Hz for urgent status
    ERROR_FLASH,           ///< @brief Error flash pattern (3 quick flashes, pause)
    HEARTBEAT              ///< @brief Heartbeat pattern (double pulse) for system health
};

/**
 * @brief LED indicator structure for individual LED management
 * 
 * @details Contains all state information for a single LED including:
 *          - Hardware pin assignment
 *          - Current animation state and timing
 *          - Flash counting for complex patterns
 *          - State tracking for pattern generation
 */
struct LEDIndicator {
    uint8_t pin;            ///< @brief MCP23017 pin number for this LED
    MCPLEDState state;      ///< @brief Current LED animation state
    unsigned long lastToggle; ///< @brief Timestamp of last state change
    bool currentState;      ///< @brief Current physical LED state (on/off)
    uint8_t flashCount;     ///< @brief Flash counter for error flash pattern
    
    /// @brief Default constructor initializes LED to off state
    LEDIndicator() : pin(0), state(MCPLEDState::OFF), lastToggle(0), currentState(false), flashCount(0) {}
    
    /**
     * @brief Constructor with pin assignment
     * @param p MCP23017 pin number for this LED
     */
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
