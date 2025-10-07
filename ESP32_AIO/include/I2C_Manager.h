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
 * @brief LED state enumeration for indicator control
 */
enum class LEDState {
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
    LEDState state;
    unsigned long lastToggle;
    bool currentState;
    uint8_t flashCount;    // For error flash pattern
    
    LEDIndicator() : pin(0), state(LEDState::OFF), lastToggle(0), currentState(false), flashCount(0) {}
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
    void readAllVoltages();
    void adsSetGain(adsGain_t gain);

    // --- Data Getter (Does not need mutex) ---
    float getVoltage(uint8_t channel) const;
    uint16_t getRawReading(uint8_t channel) const;

    // --- Status Checkers ---
    bool isMcpReady() const;
    bool isAdsReady() const;

    // --- LED State Management ---
    void setPowerLED(LEDState state);
    void setGPSLED(LEDState state);
    void setRTKLED(LEDState state);
    void setEthLED(LEDState state);
    void setSteerStandbyLED(LEDState state);
    void setSteerActiveLED(LEDState state);
    void setAllLEDs(LEDState state);

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

    // Helper function for raw ADC reads. Called internally from a mutex-protected block.
    int16_t adsReadSingleEnded(uint8_t channel);

    // LED state management
    void updateLEDStates();
    void updateLED(LEDIndicator& led);

    // LED flash timing and state tracking
    unsigned long _lastFlashTime = 0;
    bool _flashState = false;
    unsigned long _fastFlashTime = 0;
    bool _fastFlashState = false;

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
};

#endif // I2C_MANAGER_H