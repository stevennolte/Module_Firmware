#include "MCPManager.h"

// Static member initialization
MCPManager* MCPManager::instance = nullptr;

MCPManager::MCPManager() : initialized(false), espData(nullptr) {
    // Constructor initializes with default values
}

MCPManager& MCPManager::getInstance() {
    if (instance == nullptr) {
        instance = new MCPManager();
    }
    return *instance;
}

void MCPManager::destroyInstance() {
    if (instance != nullptr) {
        delete instance;
        instance = nullptr;
    }
}


bool MCPManager::begin(ESPdata* espDataPtr, uint8_t address, TwoWire* wire) {
    espData = espDataPtr;  // Store ESPdata reference
    
    if (mcp.begin_I2C(address, wire)) {
        initialized = true;
        Serial.println("MCPManager: MCP23X17 initialized successfully");
        
        // Setup input pins
        mcp.pinMode(espData->mcpPins.inputs.work_switch, INPUT);
        mcp.pinMode(espData->mcpPins.inputs.remote_switch, INPUT);
        mcp.pinMode(espData->mcpPins.inputs.steer_switch, INPUT);
        
        // Setup output pins
        mcp.pinMode(espData->mcpPins.outputs.power_on, OUTPUT);
        mcp.pinMode(espData->mcpPins.outputs.eth_good, OUTPUT);
        mcp.pinMode(espData->mcpPins.outputs.gps_fix, OUTPUT);
        mcp.pinMode(espData->mcpPins.outputs.rtk_fix, OUTPUT);
        mcp.pinMode(espData->mcpPins.outputs.steer_standby, OUTPUT);
        mcp.pinMode(espData->mcpPins.outputs.steer_active, OUTPUT);
        mcp.pinMode(espData->mcpPins.outputs.motor_enb, OUTPUT);
        mcp.pinMode(espData->mcpPins.outputs.motor_ena, OUTPUT);

        // Initialize all outputs to LOW
        mcp.digitalWrite(espData->mcpPins.outputs.power_on, LOW);
        mcp.digitalWrite(espData->mcpPins.outputs.eth_good, LOW);
        mcp.digitalWrite(espData->mcpPins.outputs.gps_fix, LOW);
        mcp.digitalWrite(espData->mcpPins.outputs.rtk_fix, LOW);
        mcp.digitalWrite(espData->mcpPins.outputs.steer_standby, LOW);
        mcp.digitalWrite(espData->mcpPins.outputs.steer_active, LOW);
        mcp.digitalWrite(espData->mcpPins.outputs.motor_enb, LOW);
        mcp.digitalWrite(espData->mcpPins.outputs.motor_ena, LOW);
        
        // Initialize LED indicators
        ledPowerOn.pin = espData->mcpPins.outputs.power_on;
        ledPowerOn.state = MCPLEDState::OFF;
        ledPowerOn.lastToggle = 0;
        ledPowerOn.currentState = false;
        ledPowerOn.flashCount = 0;
        
        ledGPSFix.pin = espData->mcpPins.outputs.gps_fix;
        ledGPSFix.state = MCPLEDState::OFF;
        ledGPSFix.lastToggle = 0;
        ledGPSFix.currentState = false;
        ledGPSFix.flashCount = 0;
        
        ledRTKFix.pin = espData->mcpPins.outputs.rtk_fix;
        ledRTKFix.state = MCPLEDState::OFF;
        ledRTKFix.lastToggle = 0;
        ledRTKFix.currentState = false;
        ledRTKFix.flashCount = 0;
        
        ledEthGood.pin = espData->mcpPins.outputs.eth_good;
        ledEthGood.state = MCPLEDState::OFF;
        ledEthGood.lastToggle = 0;
        ledEthGood.currentState = false;
        ledEthGood.flashCount = 0;
        
        ledSteerStandby.pin = espData->mcpPins.outputs.steer_standby;
        ledSteerStandby.state = MCPLEDState::OFF;
        ledSteerStandby.lastToggle = 0;
        ledSteerStandby.currentState = false;
        ledSteerStandby.flashCount = 0;
        
        ledSteerActive.pin = espData->mcpPins.outputs.steer_active;
        ledSteerActive.state = MCPLEDState::OFF;
        ledSteerActive.lastToggle = 0;
        ledSteerActive.currentState = false;
        ledSteerActive.flashCount = 0;
        
        // Start LED update task
        xTaskCreate(
            ledUpdateTask,      // Task function
            "LED_Update",       // Task name
            2048,              // Stack size
            this,              // Task parameter (this instance)
            1,                 // Priority
            &ledTaskHandle     // Task handle
        );
        
        return true;
    } else {
        initialized = false;
        Serial.println("MCPManager: Failed to initialize MCP23X17");
        return false;
    }
}


bool MCPManager::isInitialized() const {
    return initialized;
}

// Basic pin control functions
void MCPManager::pinMode(uint8_t pin, uint8_t mode) {
    if (!initialized) {
        Serial.println("MCPManager: Warning - MCP not initialized");
        return;
    }
    mcp.pinMode(pin, mode);
}

void MCPManager::digitalWrite(uint8_t pin, uint8_t value) {
    if (!initialized) {
        Serial.println("MCPManager: Warning - MCP not initialized");
        return;
    }
    mcp.digitalWrite(pin, value);
}

uint8_t MCPManager::digitalRead(uint8_t pin) {
    if (!initialized) {
        Serial.println("MCPManager: Warning - MCP not initialized");
        return LOW;
    }
    return mcp.digitalRead(pin);
}

void MCPManager::pullUp(uint8_t pin, uint8_t value) {
    if (!initialized) {
        Serial.println("MCPManager: Warning - MCP not initialized");
        return;
    }
    mcp.pinMode(pin, value == HIGH ? INPUT_PULLUP : INPUT);
}

void MCPManager::writeGPIOA(uint8_t value) {
    if (!initialized) {
        Serial.println("MCPManager: Warning - MCP not initialized");
        return;
    }
    mcp.writeGPIOA(value);
}

void MCPManager::writeGPIOB(uint8_t value) {
    if (!initialized) {
        Serial.println("MCPManager: Warning - MCP not initialized");
        return;
    }
    mcp.writeGPIOB(value);
}

uint8_t MCPManager::readGPIOA() {
    if (!initialized) {
        Serial.println("MCPManager: Warning - MCP not initialized");
        return 0;
    }
    return mcp.readGPIOA();
}

uint8_t MCPManager::readGPIOB() {
    if (!initialized) {
        Serial.println("MCPManager: Warning - MCP not initialized");
        return 0;
    }
    return mcp.readGPIOB();
}

// Motor driver specific functions
void MCPManager::setupMotorPins(uint8_t enaPin, uint8_t enbPin) {
    if (!initialized) {
        Serial.println("MCPManager: Warning - MCP not initialized");
        return;
    }
    mcp.pinMode(enaPin, OUTPUT);
    mcp.pinMode(enbPin, OUTPUT);
    mcp.digitalWrite(enaPin, LOW);
    mcp.digitalWrite(enbPin, LOW);
}

void MCPManager::enableMotor(uint8_t enaPin, uint8_t enbPin) {
    if (!initialized) {
        Serial.println("MCPManager: Warning - MCP not initialized");
        return;
    }
    mcp.digitalWrite(enaPin, HIGH);
    mcp.digitalWrite(enbPin, HIGH);
}

void MCPManager::disableMotor(uint8_t enaPin, uint8_t enbPin) {
    if (!initialized) {
        Serial.println("MCPManager: Warning - MCP not initialized");
        return;
    }
    mcp.digitalWrite(enaPin, LOW);
    mcp.digitalWrite(enbPin, LOW);
}

// Power control specific functions
void MCPManager::setupPowerPin(uint8_t powerPin) {
    if (!initialized) {
        Serial.println("MCPManager: Warning - MCP not initialized");
        return;
    }
    mcp.pinMode(powerPin, OUTPUT);
    mcp.digitalWrite(powerPin, LOW);
}

void MCPManager::setPowerState(uint8_t powerPin, bool state) {
    if (!initialized) {
        Serial.println("MCPManager: Warning - MCP not initialized");
        return;
    }
    mcp.digitalWrite(powerPin, state ? HIGH : LOW);
}

// GPS indicator specific functions
void MCPManager::setupGPSIndicators(uint8_t gpsFixPin, uint8_t rtkFixPin) {
    if (!initialized) {
        Serial.println("MCPManager: Warning - MCP not initialized");
        return;
    }
    mcp.pinMode(gpsFixPin, OUTPUT);
    mcp.pinMode(rtkFixPin, OUTPUT);
    mcp.digitalWrite(gpsFixPin, LOW);
    mcp.digitalWrite(rtkFixPin, LOW);
}

void MCPManager::testGPSIndicators(uint8_t gpsFixPin, uint8_t rtkFixPin) {
    if (!initialized) {
        Serial.println("MCPManager: Warning - MCP not initialized");
        return;
    }
    // Test sequence
    mcp.digitalWrite(gpsFixPin, HIGH);
    mcp.digitalWrite(rtkFixPin, HIGH);
    delay(500);
    mcp.digitalWrite(gpsFixPin, LOW);
    mcp.digitalWrite(rtkFixPin, LOW);
}

void MCPManager::setGPSFix(uint8_t gpsFixPin, bool state) {
    if (!initialized) {
        Serial.println("MCPManager: Warning - MCP not initialized");
        return;
    }
    mcp.digitalWrite(gpsFixPin, state ? HIGH : LOW);
}

void MCPManager::setRTKFix(uint8_t rtkFixPin, bool state) {
    if (!initialized) {
        Serial.println("MCPManager: Warning - MCP not initialized");
        return;
    }
    mcp.digitalWrite(rtkFixPin, state ? HIGH : LOW);
}

// Convenience methods using ESPdata pin definitions
void MCPManager::setupMotorPins() {
    if (!espData) {
        Serial.println("MCPManager: Warning - ESPdata not available");
        return;
    }
    setupMotorPins(espData->mcpPins.outputs.motor_ena, espData->mcpPins.outputs.motor_enb);
}

void MCPManager::enableMotor() {
    if (!espData) {
        Serial.println("MCPManager: Warning - ESPdata not available");
        return;
    }
    enableMotor(espData->mcpPins.outputs.motor_ena, espData->mcpPins.outputs.motor_enb);
}

void MCPManager::disableMotor() {
    if (!espData) {
        Serial.println("MCPManager: Warning - ESPdata not available");
        return;
    }
    disableMotor(espData->mcpPins.outputs.motor_ena, espData->mcpPins.outputs.motor_enb);
}

void MCPManager::setupPowerPin() {
    if (!espData) {
        Serial.println("MCPManager: Warning - ESPdata not available");
        return;
    }
    setupPowerPin(espData->mcpPins.outputs.power_on);
}

void MCPManager::setPowerState(bool state) {
    if (!espData) {
        Serial.println("MCPManager: Warning - ESPdata not available");
        return;
    }
    setPowerState(espData->mcpPins.outputs.power_on, state);
}

void MCPManager::setupGPSIndicators() {
    if (!espData) {
        Serial.println("MCPManager: Warning - ESPdata not available");
        return;
    }
    setupGPSIndicators(espData->mcpPins.outputs.gps_fix, espData->mcpPins.outputs.rtk_fix);
}

void MCPManager::testGPSIndicators() {
    if (!espData) {
        Serial.println("MCPManager: Warning - ESPdata not available");
        return;
    }
    testGPSIndicators(espData->mcpPins.outputs.gps_fix, espData->mcpPins.outputs.rtk_fix);
}

void MCPManager::setGPSFix(bool state) {
    if (!espData) {
        Serial.println("MCPManager: Warning - ESPdata not available");
        return;
    }
    setGPSFix(espData->mcpPins.outputs.gps_fix, state);
}

void MCPManager::setRTKFix(bool state) {
    if (!espData) {
        Serial.println("MCPManager: Warning - ESPdata not available");
        return;
    }
    setRTKFix(espData->mcpPins.outputs.rtk_fix, state);
}

Adafruit_MCP23X17* MCPManager::getMCP() {
    if (!initialized) {
        Serial.println("MCPManager: Warning - MCP not initialized");
        return nullptr;
    }
    return &mcp;
}

MCPManager::~MCPManager() {
    // Stop LED task if running
    if (ledTaskHandle != nullptr) {
        vTaskDelete(ledTaskHandle);
        ledTaskHandle = nullptr;
    }
}

// LED Task Implementation
void MCPManager::ledUpdateTask(void* parameter) {
    MCPManager* mcpManager = static_cast<MCPManager*>(parameter);
    
    while (true) {
        if (mcpManager->initialized) {
            mcpManager->updateLEDs();
        }
        vTaskDelay(pdMS_TO_TICKS(50)); // Update every 50ms
    }
}

void MCPManager::updateLEDs() {
    updateLED(ledPowerOn);
    updateLED(ledGPSFix);
    updateLED(ledRTKFix);
    updateLED(ledEthGood);
    updateLED(ledSteerStandby);
    updateLED(ledSteerActive);
}

void MCPManager::updateLED(LEDIndicator& led) {
    unsigned long currentTime = millis();
    bool shouldBeOn = false;
    
    switch (led.state) {
        case MCPLEDState::OFF:
            shouldBeOn = false;
            break;
            
        case MCPLEDState::ON:
            shouldBeOn = true;
            break;
            
        case MCPLEDState::SLOW_PULSE:  // 0.5 Hz (1 second on, 1 second off)
            if (currentTime - led.lastToggle >= 1000) {
                led.currentState = !led.currentState;
                led.lastToggle = currentTime;
            }
            shouldBeOn = led.currentState;
            break;
            
        case MCPLEDState::FAST_PULSE:  // 2 Hz (250ms on, 250ms off)
            if (currentTime - led.lastToggle >= 250) {
                led.currentState = !led.currentState;
                led.lastToggle = currentTime;
            }
            shouldBeOn = led.currentState;
            break;
            
        case MCPLEDState::RAPID_PULSE:  // 5 Hz (100ms on, 100ms off)
            if (currentTime - led.lastToggle >= 100) {
                led.currentState = !led.currentState;
                led.lastToggle = currentTime;
            }
            shouldBeOn = led.currentState;
            break;
            
        case MCPLEDState::ERROR_FLASH:  // 3 quick flashes, then pause
            if (led.flashCount < 6) { // 3 on/off cycles = 6 state changes
                if (currentTime - led.lastToggle >= 100) {
                    led.currentState = !led.currentState;
                    led.lastToggle = currentTime;
                    led.flashCount++;
                }
                shouldBeOn = led.currentState;
            } else {
                // Pause between flash sequences
                if (currentTime - led.lastToggle >= 1000) {
                    led.flashCount = 0;
                    led.currentState = false;
                    led.lastToggle = currentTime;
                }
                shouldBeOn = false;
            }
            break;
            
        case MCPLEDState::HEARTBEAT:  // Double pulse pattern
            {
                unsigned long cycleTime = currentTime % 2000; // 2 second cycle
                if ((cycleTime < 100) || (cycleTime >= 200 && cycleTime < 300)) {
                    shouldBeOn = true;
                } else {
                    shouldBeOn = false;
                }
            }
            break;
    }
    
    // Update the physical LED only if state changed
    if (shouldBeOn != (mcp.digitalRead(led.pin) == HIGH)) {
        mcp.digitalWrite(led.pin, shouldBeOn ? HIGH : LOW);
    }
}

// LED State Management Methods
void MCPManager::setPowerLED(MCPLEDState state) {
    ledPowerOn.state = state;
    ledPowerOn.lastToggle = millis();
    ledPowerOn.flashCount = 0;
    ledPowerOn.currentState = false;
}

void MCPManager::setGPSLED(MCPLEDState state) {
    ledGPSFix.state = state;
    ledGPSFix.lastToggle = millis();
    ledGPSFix.flashCount = 0;
    ledGPSFix.currentState = false;
}

void MCPManager::setRTKLED(MCPLEDState state) {
    ledRTKFix.state = state;
    ledRTKFix.lastToggle = millis();
    ledRTKFix.flashCount = 0;
    ledRTKFix.currentState = false;
}

void MCPManager::setEthLED(MCPLEDState state) {
    ledEthGood.state = state;
    ledEthGood.lastToggle = millis();
    ledEthGood.flashCount = 0;
    ledEthGood.currentState = false;
}

void MCPManager::setSteerStandbyLED(MCPLEDState state) {
    ledSteerStandby.state = state;
    ledSteerStandby.lastToggle = millis();
    ledSteerStandby.flashCount = 0;
    ledSteerStandby.currentState = false;
}

void MCPManager::setSteerActiveLED(MCPLEDState state) {
    ledSteerActive.state = state;
    ledSteerActive.lastToggle = millis();
    ledSteerActive.flashCount = 0;
    ledSteerActive.currentState = false;
}

void MCPManager::setAllLEDs(MCPLEDState state) {
    setPowerLED(state);
    setGPSLED(state);
    setRTKLED(state);
    setEthLED(state);
    setSteerStandbyLED(state);
    setSteerActiveLED(state);
}

