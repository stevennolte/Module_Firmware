#include "Indicators.h"

// Static instance pointer for FreeRTOS task access
Indicators* Indicators::instance = nullptr;

Indicators::Indicators(ESPdata* vars, Adafruit_MCP23X17* mcpInstance)
{
    espData = vars;
    mcp = mcpInstance;
    instance = this; // Set static instance for task access
}

void Indicators::init()
{
    // Initialize all indicator pins as outputs
    if (mcp != nullptr) {
        mcp->pinMode(espData->indicators.powerOn, OUTPUT);
        mcp->pinMode(espData->indicators.ethGood, OUTPUT);
        mcp->pinMode(espData->pins.gpsFix, OUTPUT);
        mcp->pinMode(espData->pins.rtkFix, OUTPUT);
        mcp->pinMode(espData->indicators.steerStandby, OUTPUT);
        mcp->pinMode(espData->indicators.steerActive, OUTPUT);
        
        // Start with all indicators off
        allOff();
        
        Serial.println("Indicators: Initialized with MCP23X17");
    } else {
        Serial.println("Indicators: Warning - MCP23X17 not available");
    }
}

void Indicators::setPowerLed(bool state)
{
    if (mcp != nullptr) {
        mcp->digitalWrite(espData->indicators.powerOn, state ? HIGH : LOW);
        powerLedState = state;
    }
}

void Indicators::setEthLed(bool state)
{
    if (mcp != nullptr) {
        mcp->digitalWrite(espData->indicators.ethGood, state ? HIGH : LOW);
        ethLedState = state;
    }
}

void Indicators::setGpsFixLed(bool state)
{
    if (mcp != nullptr) {
        mcp->digitalWrite(espData->pins.gpsFix, state ? HIGH : LOW);
        gpsFixLedState = state;
    }
}

void Indicators::setRtkFixLed(bool state)
{
    if (mcp != nullptr) {
        mcp->digitalWrite(espData->pins.rtkFix, state ? HIGH : LOW);
        rtkFixLedState = state;
    }
}

void Indicators::setSteerStandbyLed(bool state)
{
    if (mcp != nullptr) {
        mcp->digitalWrite(espData->indicators.steerStandby, state ? HIGH : LOW);
        steerStandbyLedState = state;
    }
}

void Indicators::setSteerActiveLed(bool state)
{
    if (mcp != nullptr) {
        mcp->digitalWrite(espData->indicators.steerActive, state ? HIGH : LOW);
        steerActiveLedState = state;
    }
}

void Indicators::blinkPowerLed(uint32_t interval)
{
    if (millis() - lastBlinkTime >= interval) {
        setPowerLed(!powerLedState);
        lastBlinkTime = millis();
    }
}

void Indicators::blinkEthLed(uint32_t interval)
{
    if (millis() - lastBlinkTime >= interval) {
        setEthLed(!ethLedState);
        lastBlinkTime = millis();
    }
}

void Indicators::blinkGpsFixLed(uint32_t interval)
{
    if (millis() - lastBlinkTime >= interval) {
        setGpsFixLed(!gpsFixLedState);
        lastBlinkTime = millis();
    }
}

void Indicators::blinkRtkFixLed(uint32_t interval)
{
    if (millis() - lastBlinkTime >= interval) {
        setRtkFixLed(!rtkFixLedState);
        lastBlinkTime = millis();
    }
}

void Indicators::updatePowerStatus()
{
    // Power LED shows system power status
    setPowerLed(espData->program.state == 1);
}

void Indicators::updateWifiStatus()
{
    // Ethernet LED shows WiFi connection status
    if (espData->wifi.state == 1) {
        setEthLed(true);  // Solid on when connected
    } else if (espData->wifi.state == 2) {
        blinkEthLed(1000);  // Slow blink when connecting
    } else {
        setEthLed(false);  // Off when disconnected
    }
}

void Indicators::updateGpsStatus()
{
    // GPS Fix LED based on GPS state and fix quality
    if (espData->gps.state == 1) {
        if (espData->gps.fixQualityInt >= 4) {
            setGpsFixLed(true);  // Solid on for good fix (RTK)
            setRtkFixLed(true);  // RTK LED on for high quality
        } else if (espData->gps.fixQualityInt >= 1) {
            setGpsFixLed(true);   // GPS fix available
            setRtkFixLed(false);  // But no RTK
        } else {
            blinkGpsFixLed(2000);  // Slow blink for no fix
            setRtkFixLed(false);
        }
    } else {
        setGpsFixLed(false);  // GPS not working
        setRtkFixLed(false);
    }
}

void Indicators::updateSteerStatus()
{
    // Steering status based on steer data
    if (espData->steer.switchState == 1) {
        if (abs(espData->steer.pwmCmd) > 50) {
            setSteerActiveLed(true);   // Active steering
            setSteerStandbyLed(false);
        } else {
            setSteerActiveLed(false);  // Standby
            setSteerStandbyLed(true);
        }
    } else {
        setSteerActiveLed(false);  // Steering off
        setSteerStandbyLed(false);
    }
}

void Indicators::updateAllIndicators()
{
    updatePowerStatus();
    updateWifiStatus();
    updateGpsStatus();
    updateSteerStatus();
}

void Indicators::testSequence()
{
    Serial.println("Indicators: Running test sequence");
    
    // Turn all on
    allOn();
    delay(1000);
    
    // Turn all off
    allOff();
    delay(500);
    
    // Test each indicator individually
    setPowerLed(true);
    delay(300);
    setPowerLed(false);
    
    setEthLed(true);
    delay(300);
    setEthLed(false);
    
    setGpsFixLed(true);
    delay(300);
    setGpsFixLed(false);
    
    setRtkFixLed(true);
    delay(300);
    setRtkFixLed(false);
    
    setSteerStandbyLed(true);
    delay(300);
    setSteerStandbyLed(false);
    
    setSteerActiveLed(true);
    delay(300);
    setSteerActiveLed(false);
    
    Serial.println("Indicators: Test sequence complete");
}

void Indicators::allOff()
{
    setPowerLed(false);
    setEthLed(false);
    setGpsFixLed(false);
    setRtkFixLed(false);
    setSteerStandbyLed(false);
    setSteerActiveLed(false);
}

void Indicators::allOn()
{
    setPowerLed(true);
    setEthLed(true);
    setGpsFixLed(true);
    setRtkFixLed(true);
    setSteerStandbyLed(true);
    setSteerActiveLed(true);
}

void Indicators::loop()
{
    // This should be called in the main loop for time-based operations
    // Currently used for blinking operations
    updateAllIndicators();
}

// FreeRTOS task implementation
void Indicators::startTask(uint16_t stackSize, uint8_t priority, uint8_t core)
{
    if (indicatorTaskHandle == NULL) {
        xTaskCreatePinnedToCore(
            indicatorTaskFunction,    // Task function
            "IndicatorTask",          // Task name
            stackSize,                // Stack size (words)
            this,                     // Task parameter
            priority,                 // Priority
            &indicatorTaskHandle,     // Task handle
            core                      // Core (0 or 1)
        );
        
        if (indicatorTaskHandle != NULL) {
            Serial.println("Indicators: FreeRTOS task started successfully");
        } else {
            Serial.println("Indicators: Failed to create FreeRTOS task");
        }
    } else {
        Serial.println("Indicators: Task already running");
    }
}

void Indicators::stopTask()
{
    if (indicatorTaskHandle != NULL) {
        vTaskDelete(indicatorTaskHandle);
        indicatorTaskHandle = NULL;
        Serial.println("Indicators: FreeRTOS task stopped");
    }
}

void Indicators::indicatorTaskFunction(void* parameter)
{
    Indicators* indicators = (Indicators*)parameter;
    indicators->taskLoop();
}

void Indicators::taskLoop()
{
    Serial.println("Indicators: Task loop started");
    
    while (true) {
        // Update all indicators based on system state
        updateAllIndicators();
        
        // Small delay to prevent excessive CPU usage
        // Adjust this value based on your needs (100ms = 10Hz update rate)
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
