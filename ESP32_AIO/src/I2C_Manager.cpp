// I2CManager.cpp

#include "I2C_Manager.h"

// Singleton implementation
I2CManager& I2CManager::getInstance() {
    static I2CManager instance;
    return instance;
}

I2CManager::I2CManager()
    : _i2c_bus(nullptr),
      _mcp_initialized(false),
      _ads_initialized(false),
      _voltages{0.0f, 0.0f, 0.0f, 0.0f},
      espData(ESPdata::getInstance()),
      _taskHandle(nullptr)
{
    // Create the mutex to protect the I2C bus
    _i2c_mutex = xSemaphoreCreateMutex();
    if (_i2c_mutex == NULL) {
        Serial.println("Error: Failed to create I2C mutex!");
    }
}

// Destructor to clean up FreeRTOS resources
I2CManager::~I2CManager() {
    if (_taskHandle != nullptr) {
        vTaskDelete(_taskHandle);
        _taskHandle = nullptr;
    }
    if (_i2c_mutex != NULL) {
        vSemaphoreDelete(_i2c_mutex);
    }
}

bool I2CManager::begin(TwoWire* bus, uint8_t mcp_addr, uint8_t ads_addr) {
    if (bus == nullptr) return false;

    _i2c_bus = bus;

    if (_i2c_mutex == NULL) return false;

    // Take the mutex before accessing the bus
    if (xSemaphoreTake(_i2c_mutex, portMAX_DELAY) == pdTRUE) {
        if (!mcp.begin_I2C(mcp_addr, _i2c_bus)) {
            Serial.println("Error: Failed to find MCP23017 chip.");
            _mcp_initialized = false;
        } else {
            Serial.println("MCP23017 found.");
            
            // Initialize LED indicators
            ledPowerOn.pin = espData.mcpPins.indOutputs.power_on;
            ledEthGood.pin = espData.mcpPins.indOutputs.eth_good;
            ledGPSFix.pin = espData.mcpPins.indOutputs.gps_fix;
            ledRTKFix.pin = espData.mcpPins.indOutputs.rtk_fix;
            ledSteerStandby.pin = espData.mcpPins.indOutputs.steer_standby;
            ledSteerActive.pin = espData.mcpPins.indOutputs.steer_active;
            
            // Set all LED pins as outputs and initialize to OFF
            for(int i=0; i < sizeof(espData.mcpPins.indOutputs.pins); i++){
                mcp.pinMode(espData.mcpPins.indOutputs.pins[i], OUTPUT);
                mcp.digitalWrite(espData.mcpPins.indOutputs.pins[i], LOW);
            }
            
            // Test sequence - quick flash all LEDs
            for(int i=0; i < sizeof(espData.mcpPins.indOutputs.pins); i++){
                mcp.digitalWrite(espData.mcpPins.indOutputs.pins[i], HIGH);
                delay(100);
                mcp.digitalWrite(espData.mcpPins.indOutputs.pins[i], LOW);
            }
            mcp.pinMode(espData.mcpPins.motorOutputs.motor_ena, OUTPUT);
            mcp.pinMode(espData.mcpPins.motorOutputs.motor_enb, OUTPUT);
            mcp.digitalWrite(espData.mcpPins.motorOutputs.motor_ena, LOW);
            mcp.digitalWrite(espData.mcpPins.motorOutputs.motor_enb, LOW);
            _mcp_initialized = true;
        }

        if (!ads.begin(ads_addr, _i2c_bus)) {
            Serial.println("Error: Failed to find ADS1115 chip.");
            _ads_initialized = false;
        } else {
            Serial.println("ADS1115 found.");
            ads.setGain(GAIN_TWOTHIRDS);
            _ads_initialized = true;
        }

        // Give the mutex back
        xSemaphoreGive(_i2c_mutex);
    }

    // Start the FreeRTOS task if not already running
    if (_taskHandle == nullptr) {
        xTaskCreate(
            I2CManager::taskRunner,
            "I2CManagerTask",
            4096, // Stack size
            this,
            4, // Lower priority to avoid conflicts
            &_taskHandle
        );
        if (_taskHandle != nullptr) {
            Serial.println("I2CManager: FreeRTOS task started");
        } else {
            Serial.println("I2CManager: Failed to start FreeRTOS task");
        }
    }

    return _mcp_initialized && _ads_initialized;
}
// FreeRTOS task function
void I2CManager::taskRunner(void* pvParameters) {
    I2CManager* self = static_cast<I2CManager*>(pvParameters);
    const TickType_t delayTicks = pdMS_TO_TICKS(10); // 10ms loop for responsive non-blocking reads
    while (true) {
        // Measure task cycle time
        unsigned long now = millis();
        if (self->_i2cTaskLastCycleMs != 0) {
            self->_i2cTaskCycleTime = (uint32_t)(now - self->_i2cTaskLastCycleMs);
        }
        self->_i2cTaskLastCycleMs = now;

        if (self->_ads_initialized) {
            // Measure how long the non-blocking ADC state machine call takes
            unsigned long adcStart = micros();
            self->updateADCReadings(); // Non-blocking state machine
            uint32_t adcDuration = (uint32_t)(micros() - adcStart);
            self->_adcStateMachineTime = adcDuration;
            if (adcDuration > self->_adcStateMachineMaxTime) {
                self->_adcStateMachineMaxTime = adcDuration;
            }
        }
        if (self->_mcp_initialized) {
            self->updateLEDStates();
        }
        vTaskDelay(delayTicks);
    }
}

// --- Thread-Safe Methods ---
bool I2CManager::isInitialized() const {
    return _mcp_initialized && _ads_initialized;
}

void I2CManager::mcpPinMode(uint8_t pin, uint8_t mode) {
    if (_mcp_initialized && xSemaphoreTake(_i2c_mutex, portMAX_DELAY) == pdTRUE) {
        mcp.pinMode(pin, mode);
        xSemaphoreGive(_i2c_mutex);
    }
}

void I2CManager::mcpDigitalWrite(uint8_t pin, uint8_t value) {
    if (_mcp_initialized && xSemaphoreTake(_i2c_mutex, portMAX_DELAY) == pdTRUE) {
        mcp.digitalWrite(pin, value);
        xSemaphoreGive(_i2c_mutex);
    }
}

uint8_t I2CManager::mcpDigitalRead(uint8_t pin) {
    uint8_t value = 0;
    if (_mcp_initialized && xSemaphoreTake(_i2c_mutex, portMAX_DELAY) == pdTRUE) {
        value = mcp.digitalRead(pin);
        xSemaphoreGive(_i2c_mutex);
    }
    return value;
}

// Non-blocking ADC update method - called from task loop
void I2CManager::updateADCReadings() {
    processADCStateMachine();
}

// Non-blocking state machine for ADC readings
void I2CManager::processADCStateMachine() {
    if (!_ads_initialized) return;
    
    // Try to take mutex with minimal blocking (10ms timeout)
    if (xSemaphoreTake(_i2c_mutex, pdMS_TO_TICKS(10)) != pdTRUE) {
        return; // Skip this cycle if mutex is busy
    }
    
    switch (_adcState) {
        case ADCState::IDLE: {
            // Start conversion on current channel.
            // The MUX constants are spaced 0x1000 apart (0x4000, 0x5000, 0x6000, 0x7000),
            // so use a lookup table instead of adding the channel index directly.
            static const uint16_t muxChannels[4] = {
                ADS1X15_REG_CONFIG_MUX_SINGLE_0,
                ADS1X15_REG_CONFIG_MUX_SINGLE_1,
                ADS1X15_REG_CONFIG_MUX_SINGLE_2,
                ADS1X15_REG_CONFIG_MUX_SINGLE_3
            };
            ads.startADCReading(muxChannels[_currentADCChannel], false);
            _conversionStartTime = millis();
            _adcState = ADCState::WAITING_FOR_CONVERSION;
            break;
        }
            
        case ADCState::WAITING_FOR_CONVERSION:
            // Check if enough time has passed for conversion (typical 8ms at default 128 SPS)
            if (millis() - _conversionStartTime >= 8) {
                // Read the conversion result
                _rawReadings[_currentADCChannel] = ads.getLastConversionResults();
                _voltages[_currentADCChannel] = ads.computeVolts(_rawReadings[_currentADCChannel]);
                espData.adsConfig.readings[_currentADCChannel] = _rawReadings[_currentADCChannel];
                
                // Move to next channel
                _currentADCChannel++;
                if (_currentADCChannel >= 4) {
                    _currentADCChannel = 0;
                }
                
                // Return to IDLE to start next conversion
                _adcState = ADCState::IDLE;
            }
            break;
    }
    
    xSemaphoreGive(_i2c_mutex);
}

void I2CManager::adsSetGain(adsGain_t gain) {
    if (_ads_initialized && xSemaphoreTake(_i2c_mutex, portMAX_DELAY) == pdTRUE) {
        ads.setGain(gain);
        xSemaphoreGive(_i2c_mutex);
    }
}

uint16_t I2CManager::getRawReading(uint8_t channel) const {
    if (channel < 4) {
        return _rawReadings[channel];
    }
    return 0; 
}

// --- Getter (No mutex needed as float read is atomic on ESP32) ---
float I2CManager::getVoltage(uint8_t channel) const {
    if (channel < 4) {
        return _voltages[channel];
    }
    return 0.0f; 
}


// --- Status Checkers ---
bool I2CManager::isMcpReady() const { return _mcp_initialized; }
bool I2CManager::isAdsReady() const { return _ads_initialized; }

// --- LED State Management ---
void I2CManager::updateLEDStates() {
    if (!_mcp_initialized) return;
    
    if (xSemaphoreTake(_i2c_mutex, portMAX_DELAY) == pdTRUE) {
        updateLED(ledPowerOn);
        updateLED(ledEthGood);
        updateLED(ledGPSFix);
        updateLED(ledRTKFix);
        updateLED(ledSteerStandby);
        updateLED(ledSteerActive);
        xSemaphoreGive(_i2c_mutex);
    }
}

void I2CManager::updateLED(LEDIndicator& led) {
    unsigned long currentTime = millis();
    bool shouldToggle = false;
    bool newState = led.currentState;
    
    switch (led.state) {
        case LEDPattern::OFF:
            newState = false;
            break;
            
        case LEDPattern::ON:
            newState = true;
            break;
            
        case LEDPattern::SLOW_PULSE:
            if (currentTime - led.lastToggle >= 1000) { // 1 second
                shouldToggle = true;
            }
            break;
            
        case LEDPattern::FAST_PULSE:
            if (currentTime - led.lastToggle >= 250) { // 250ms
                shouldToggle = true;
            }
            break;
            
        case LEDPattern::RAPID_PULSE:
            if (currentTime - led.lastToggle >= 100) { // 100ms
                shouldToggle = true;
            }
            break;
            
        case LEDPattern::ERROR_FLASH:
            if (led.flashCount < 6) { // 3 flashes = 6 state changes
                if (currentTime - led.lastToggle >= 100) { // 100ms on/off
                    shouldToggle = true;
                    led.flashCount++;
                }
            } else {
                if (currentTime - led.lastToggle >= 1000) { // 1 second pause
                    led.flashCount = 0;
                    shouldToggle = true;
                }
            }
            break;
            
        case LEDPattern::HEARTBEAT:
            // Double pulse pattern: on-off-on-off-pause
            if (led.flashCount < 4) {
                if (currentTime - led.lastToggle >= 150) { // 150ms for heartbeat pulses
                    shouldToggle = true;
                    led.flashCount++;
                }
            } else {
                if (currentTime - led.lastToggle >= 1000) { // 1 second pause
                    led.flashCount = 0;
                    shouldToggle = true;
                }
            }
            break;
    }
    
    if (shouldToggle) {
        newState = !led.currentState;
        led.lastToggle = currentTime;
    }
    
    if (newState != led.currentState) {
        led.currentState = newState;
        mcp.digitalWrite(led.pin, newState ? HIGH : LOW);
    }
}

// LED State Control Methods
void I2CManager::setPowerLED(LEDPattern state) {
    ledPowerOn.state = state;
    ledPowerOn.flashCount = 0;
}

void I2CManager::setGPSLED(LEDPattern state) {
    ledGPSFix.state = state;
    ledGPSFix.flashCount = 0;
}

void I2CManager::setRTKLED(LEDPattern state) {
    ledRTKFix.state = state;
    ledRTKFix.flashCount = 0;
}

void I2CManager::setEthLED(LEDPattern state) {
    ledEthGood.state = state;
    ledEthGood.flashCount = 0;
}

void I2CManager::setSteerStandbyLED(LEDPattern state) {
    ledSteerStandby.state = state;
    ledSteerStandby.flashCount = 0;
}

void I2CManager::setSteerActiveLED(LEDPattern state) {
    ledSteerActive.state = state;
    ledSteerActive.flashCount = 0;
}

void I2CManager::setAllLEDs(LEDPattern state) {
    setPowerLED(state);
    setGPSLED(state);
    setRTKLED(state);
    setEthLED(state);
    setSteerStandbyLED(state);
    setSteerActiveLED(state);
}

void I2CManager::setMotorEnableA(bool enabled) {
    if (_mcp_initialized && xSemaphoreTake(_i2c_mutex, portMAX_DELAY) == pdTRUE) {
        if (enabled != prevMotorEnableA) {
            mcp.digitalWrite(espData.mcpPins.motorOutputs.motor_ena, enabled ? HIGH : LOW);
            Serial.printf("Motor Enable A set to: %s\n", enabled ? "ENABLED" : "DISABLED");
            prevMotorEnableA = enabled;
        }
        xSemaphoreGive(_i2c_mutex);
    }
}

void I2CManager::setMotorEnableB(bool enabled) {
    if (_mcp_initialized && xSemaphoreTake(_i2c_mutex, portMAX_DELAY) == pdTRUE) {
        if (enabled != prevMotorEnableB) {
            mcp.digitalWrite(espData.mcpPins.motorOutputs.motor_enb, enabled ? HIGH : LOW);
            Serial.printf("Motor Enable B set to: %s\n", enabled ? "ENABLED" : "DISABLED");
            prevMotorEnableB = enabled;
        }
        xSemaphoreGive(_i2c_mutex);
    }
}

void I2CManager::enableMotor(){
    setMotorEnableA(true);
    setMotorEnableB(true);
    return;
}

void I2CManager::disableMotor(){
    setMotorEnableA(false);
    setMotorEnableB(false);
    return;
}

