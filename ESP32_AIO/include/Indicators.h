#ifndef INDICATORS_H
#define INDICATORS_H

#include "Arduino.h"
#include "ESPdata.h"
#include "ESPdata_macros.h"
#include "Adafruit_MCP23X17.h"

class Indicators
{
private:
    ESPdata* espData;
    Adafruit_MCP23X17* mcp;
    
    // Internal state tracking
    bool powerLedState = false;
    bool ethLedState = false;
    bool gpsFixLedState = false;
    bool rtkFixLedState = false;
    bool steerStandbyLedState = false;
    bool steerActiveLedState = false;
    
    uint32_t lastBlinkTime = 0;
    uint32_t blinkInterval = 500; // Default blink rate in ms
    
    // FreeRTOS task handle
    TaskHandle_t indicatorTaskHandle = NULL;
    static Indicators* instance; // For static task function access
    
    // Task function - must be static for FreeRTOS
    static void indicatorTaskFunction(void* parameter);
    
    // Internal loop function
    void taskLoop();
    
public:
    // Constructor
    Indicators(ESPdata* vars, Adafruit_MCP23X17* mcpInstance);
    
    // Initialization
    void init();
    
    // Start the FreeRTOS task
    void startTask(uint16_t stackSize = 4096, uint8_t priority = 1, uint8_t core = 0);
    
    // Stop the FreeRTOS task
    void stopTask();
    
    // Individual indicator control
    void setPowerLed(bool state);
    void setEthLed(bool state);
    void setGpsFixLed(bool state);
    void setRtkFixLed(bool state);
    void setSteerStandbyLed(bool state);
    void setSteerActiveLed(bool state);
    
    // Blinking indicators
    void blinkPowerLed(uint32_t interval = 500);
    void blinkEthLed(uint32_t interval = 500);
    void blinkGpsFixLed(uint32_t interval = 500);
    void blinkRtkFixLed(uint32_t interval = 500);
    
    // System state indicators
    void updatePowerStatus();
    void updateWifiStatus();
    void updateGpsStatus();
    void updateSteerStatus();
    
    // Update all indicators based on system state
    void updateAllIndicators();
    
    // Test sequence
    void testSequence();
    
    // Turn all indicators off
    void allOff();
    
    // Turn all indicators on
    void allOn();
    
    // Legacy loop function (now deprecated - use startTask() instead)
    void loop() __attribute__((deprecated("Use startTask() instead for FreeRTOS task-based operation")));
};

#endif // INDICATORS_H
