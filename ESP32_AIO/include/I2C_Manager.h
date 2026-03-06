// I2CManager.h

#ifndef I2C_MANAGER_H
#define I2C_MANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include <Adafruit_ADS1X15.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h" // For mutex
#include "ESPdata.h"

/**
 * @brief LED animation pattern enumeration for indicator control
 */
enum class LEDPattern {
    OFF,                   // LED is off
    ON,                    // LED is solid on
    SLOW_PULSE,           // Slow pulse (0.5 Hz)
    FAST_PULSE,           // Fast pulse (2 Hz)
    RAPID_PULSE,          // Rapid pulse (5 Hz)
    ERROR_FLASH,          // Error flash pattern (3 quick flashes, pause)
    HEARTBEAT             // Heartbeat pattern (double pulse)
};

/**
 * @brief LED indicator structure for state management
 */
struct LEDIndicator {
    uint8_t pin;
    LEDPattern state;
    unsigned long lastToggle;
    bool currentState;
    uint8_t flashCount;    // For error flash pattern
    
    LEDIndicator() : pin(0), state(LEDPattern::OFF), lastToggle(0), currentState(false), flashCount(0) {}
};

class I2CManager {
public:
    // Singleton pattern - get the single instance
    static I2CManager& getInstance();
    
    // Delete copy constructor and assignment operator to prevent copies
    I2CManager(const I2CManager&) = delete;
    I2CManager& operator=(const I2CManager&) = delete;
    
    ~I2CManager(); // Destructor to clean up the mutex

    bool begin(TwoWire* bus, uint8_t mcp_addr = 0x20, uint8_t ads_addr = 0x48);

    // --- MCP23017 Methods (Thread-Safe) ---
    void mcpPinMode(uint8_t pin, uint8_t mode);
    void mcpDigitalWrite(uint8_t pin, uint8_t value);
    uint8_t mcpDigitalRead(uint8_t pin);
    bool isInitialized() const; // Check if both MCP and ADS are initialized
    // --- ADS1115 Methods (Thread-Safe) ---
    void updateADCReadings(); // Non-blocking state machine method
    void adsSetGain(adsGain_t gain);

    // --- Data Getter (Does not need mutex) ---
    float getVoltage(uint8_t channel) const;
    uint16_t getRawReading(uint8_t channel) const;

    // --- Status Checkers ---
    bool isMcpReady() const;
    bool isAdsReady() const;

    // --- Loop Timing Diagnostics ---
    /**
     * @brief Returns the duration of the last updateADCReadings() call in microseconds.
     *        A non-blocking call should return in well under 1ms.
     */
    uint32_t getADCStateMachineTime() const { return _adcStateMachineTime; }

    /**
     * @brief Returns the maximum observed duration of any updateADCReadings() call in microseconds.
     *        Spikes indicate unexpected blocking on the I2C bus or mutex.
     */
    uint32_t getADCStateMachineMaxTime() const { return _adcStateMachineMaxTime; }

    /**
     * @brief Returns the measured cycle time of the I2C manager background task in milliseconds.
     *        Should be close to the 10ms vTaskDelay configured for the task.
     */
    uint32_t getI2CTaskCycleTime() const { return _i2cTaskCycleTime; }

    // --- LED State Management ---
    void setPowerLED(LEDPattern state);
    void setGPSLED(LEDPattern state);
    void setRTKLED(LEDPattern state);
    void setEthLED(LEDPattern state);
    void setSteerStandbyLED(LEDPattern state);
    void setSteerActiveLED(LEDPattern state);
    void setAllLEDs(LEDPattern state);

    // --- Motor Control ---
    void setMotorEnableA(bool enabled);
    void setMotorEnableB(bool enabled);
    void enableMotor();
    void disableMotor();

private:
    // Private constructor for singleton pattern
    I2CManager();

    // FreeRTOS task for continuous background work
    static void taskRunner(void* pvParameters);
    TaskHandle_t _taskHandle = nullptr;

    // Non-blocking ADC state machine
    void processADCStateMachine();

    // LED state management
    void updateLEDStates();
    void updateLED(LEDIndicator& led);

    // LED Indicators
    LEDIndicator ledPowerOn;
    LEDIndicator ledEthGood;
    LEDIndicator ledGPSFix;
    LEDIndicator ledRTKFix;
    LEDIndicator ledSteerStandby;
    LEDIndicator ledSteerActive;

    TwoWire* _i2c_bus;
    Adafruit_MCP23X17 mcp;
    Adafruit_ADS1115 ads;
    ESPdata& espData;
    SemaphoreHandle_t _i2c_mutex; // Mutex to protect I2C bus access

    bool _mcp_initialized;
    bool _ads_initialized;
    float _voltages[4];
    uint16_t _rawReadings[4];
    bool prevMotorEnableA = false; ///< @brief Previous state of motor enable A
    bool prevMotorEnableB = false; ///< @brief Previous state of motor enable B
    
    // ADS1115 non-blocking state machine variables
    enum class ADCState {
        IDLE,
        WAITING_FOR_CONVERSION
    };
    ADCState _adcState = ADCState::IDLE;
    uint8_t _currentADCChannel = 0;
    unsigned long _conversionStartTime = 0;

    // Loop timing diagnostics
    uint32_t _adcStateMachineTime = 0;    ///< Duration of last processADCStateMachine() call (microseconds)
    uint32_t _adcStateMachineMaxTime = 0; ///< Max observed processADCStateMachine() call duration (microseconds)
    uint32_t _i2cTaskCycleTime = 0;       ///< I2C manager background task cycle time (milliseconds)
    unsigned long _i2cTaskLastCycleMs = 0; ///< Timestamp of previous task cycle start (milliseconds)
};

#endif // I2C_MANAGER_H