/**
 * @file ADSManager.cpp
 * @brief Implementation of centralized ADS1115 management
 * 
 * @details Provides centralized, thread-safe access to ADS1115 readings
 *          through a single FreeRTOS task to minimize I2C bus contention.
 * 
 * @author ESP32-AIO Project
 * @date 2024
 */

#include "ADSManager.h"
#include <Arduino.h>

// Global instance definition
ADSManager adsManager;

/**
 * @brief Construct a new ADSManager object
 */
ADSManager::ADSManager() : 
    readingInterval(100),
    i2cAddress(0x48),
    initialized(false),
    taskRunning(false),
    taskHandle(nullptr),
    mutex(nullptr),
    currentGain(GAIN_TWOTHIRDS),
    currentRate(128) {
    
    // Initialize readings structure
    memset(&readings, 0, sizeof(readings));
    readings.valid = false;
    
    // Initialize statistics
    memset(&stats, 0, sizeof(stats));
}

/**
 * @brief Destroy the ADSManager object
 */
ADSManager::~ADSManager() {
    end();
}

/**
 * @brief Initialize the ADS1115 and start the reading task
 * 
 * @param i2cAddress I2C address of the ADS1115
 * @param intervalMs Reading interval in milliseconds
 * @return true if initialization successful
 * @return false if initialization failed
 */
bool ADSManager::begin(uint8_t i2cAddress, uint32_t intervalMs) {
    if (initialized) {
        Serial.println("ADSManager: Already initialized");
        return true;
    }

    this->i2cAddress = i2cAddress;
    this->readingInterval = max(intervalMs, MIN_INTERVAL_MS);

    // Create mutex for thread-safe access
    mutex = xSemaphoreCreateMutex();
    if (mutex == nullptr) {
        Serial.println("ADSManager: Failed to create mutex");
        return false;
    }

    // Initialize ADS1115
    if (!ads.begin(i2cAddress)) {
        Serial.printf("ADSManager: Failed to initialize ADS1115 at address 0x%02X\n", i2cAddress);
        vSemaphoreDelete(mutex);
        mutex = nullptr;
        return false;
    }

    // Configure ADS1115 settings
    ads.setGain(currentGain);
    ads.setDataRate(RATE_ADS1115_128SPS);

    // Test initial reading to verify communication
    int16_t testReading = ads.readADC_SingleEnded(0);
    if (testReading == -1) {
        Serial.println("ADSManager: Failed initial test reading");
        vSemaphoreDelete(mutex);
        mutex = nullptr;
        return false;
    }

    // Create and start the reading task
    BaseType_t taskResult = xTaskCreate(
        taskFunction,           // Task function
        "ADSManager",          // Task name
        TASK_STACK_SIZE,       // Stack size
        this,                  // Parameter (this instance)
        2,                     // Priority (medium)
        &taskHandle            // Task handle
    );

    if (taskResult != pdPASS) {
        Serial.println("ADSManager: Failed to create reading task");
        vSemaphoreDelete(mutex);
        mutex = nullptr;
        return false;
    }

    initialized = true;
    taskRunning = true;
    
    Serial.printf("ADSManager: Initialized successfully (address: 0x%02X, interval: %dms)\n", 
                  i2cAddress, readingInterval);
    
    return true;
}

/**
 * @brief Stop the reading task and cleanup
 */
void ADSManager::end() {
    if (!initialized) {
        return;
    }

    // Stop the task
    if (taskHandle != nullptr) {
        taskRunning = false;
        vTaskDelete(taskHandle);
        taskHandle = nullptr;
    }

    // Cleanup mutex
    if (mutex != nullptr) {
        vSemaphoreDelete(mutex);
        mutex = nullptr;
    }

    initialized = false;
    
    Serial.println("ADSManager: Shutdown complete");
}

/**
 * @brief Get the latest raw reading for a specific channel
 * 
 * @param channel ADS1115 channel (0-3)
 * @return int16_t Raw ADC reading, or INT16_MIN if invalid
 */
int16_t ADSManager::getRawReading(uint8_t channel) {
    if (!isValidChannel(channel) || !initialized) {
        return INT16_MIN;
    }

    int16_t result = INT16_MIN;
    
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (readings.valid && (millis() - readings.timestamp) < READING_TIMEOUT_MS) {
            result = readings.rawChannels[channel];
        }
        xSemaphoreGive(mutex);
    }
    
    return result;
}

/**
 * @brief Get the latest voltage reading for a specific channel
 * 
 * @param channel ADS1115 channel (0-3)
 * @return float Voltage reading in volts, or NAN if invalid
 */
float ADSManager::getVoltage(uint8_t channel) {
    if (!isValidChannel(channel) || !initialized) {
        return NAN;
    }

    float result = NAN;
    
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (readings.valid && (millis() - readings.timestamp) < READING_TIMEOUT_MS) {
            result = readings.voltageChannels[channel];
        }
        xSemaphoreGive(mutex);
    }
    
    return result;
}

/**
 * @brief Get all channel readings at once
 * 
 * @param rawReadings Array to store 4 raw readings
 * @param voltages Array to store 4 voltage readings
 * @return true if readings are valid and recent
 */
bool ADSManager::getAllReadings(int16_t rawReadings[4], float voltages[4]) {
    if (!initialized || rawReadings == nullptr || voltages == nullptr) {
        return false;
    }

    bool result = false;
    
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (readings.valid && (millis() - readings.timestamp) < READING_TIMEOUT_MS) {
            memcpy(rawReadings, readings.rawChannels, sizeof(readings.rawChannels));
            memcpy(voltages, readings.voltageChannels, sizeof(readings.voltageChannels));
            result = true;
        }
        xSemaphoreGive(mutex);
    }
    
    return result;
}

/**
 * @brief Get the age of the last reading in milliseconds
 * 
 * @return uint32_t Age of last reading in ms
 */
uint32_t ADSManager::getReadingAge() {
    if (!initialized) {
        return UINT32_MAX;
    }

    uint32_t age = UINT32_MAX;
    
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (readings.valid) {
            age = millis() - readings.timestamp;
        }
        xSemaphoreGive(mutex);
    }
    
    return age;
}

/**
 * @brief Set the reading interval
 * 
 * @param intervalMs Interval between readings in milliseconds
 */
void ADSManager::setReadingInterval(uint32_t intervalMs) {
    readingInterval = max(intervalMs, MIN_INTERVAL_MS);
    Serial.printf("ADSManager: Reading interval set to %dms\n", readingInterval);
}

/**
 * @brief Check if ADS1115 is responding and readings are current
 * 
 * @return true if healthy
 * @return false if unhealthy
 */
bool ADSManager::isHealthy() {
    if (!initialized || !taskRunning) {
        return false;
    }
    
    return getReadingAge() < READING_TIMEOUT_MS;
}

/**
 * @brief Get the current gain setting
 * 
 * @return adsGain_t Current gain setting
 */
adsGain_t ADSManager::getGain() {
    return currentGain;
}

/**
 * @brief Set the gain for all channels
 * 
 * @param gain New gain setting
 */
void ADSManager::setGain(adsGain_t gain) {
    currentGain = gain;
    if (initialized) {
        ads.setGain(gain);
        Serial.printf("ADSManager: Gain set to %d\n", (int)gain);
    }
}

/**
 * @brief Get the current data rate
 * 
 * @return uint16_t Current data rate in SPS
 */
uint16_t ADSManager::getDataRate() {
    return currentRate;
}

/**
 * @brief Set the data rate
 * 
 * @param rate New data rate in SPS
 */
void ADSManager::setDataRate(uint16_t rate) {
    currentRate = rate;
    if (initialized) {
        // Convert rate to ADS enum and set
        if (rate <= 8) {
            ads.setDataRate(RATE_ADS1115_8SPS);
        } else if (rate <= 16) {
            ads.setDataRate(RATE_ADS1115_16SPS);
        } else if (rate <= 32) {
            ads.setDataRate(RATE_ADS1115_32SPS);
        } else if (rate <= 64) {
            ads.setDataRate(RATE_ADS1115_64SPS);
        } else if (rate <= 128) {
            ads.setDataRate(RATE_ADS1115_128SPS);
        } else if (rate <= 250) {
            ads.setDataRate(RATE_ADS1115_250SPS);
        } else if (rate <= 475) {
            ads.setDataRate(RATE_ADS1115_475SPS);
        } else {
            ads.setDataRate(RATE_ADS1115_860SPS);
        }
        Serial.printf("ADSManager: Data rate set to %d SPS\n", rate);
    }
}

/**
 * @brief Get reading statistics
 * 
 * @param totalReadings Total number of readings taken
 * @param errorCount Number of failed readings
 * @param avgReadTime Average time per reading cycle in ms
 */
void ADSManager::getStatistics(uint32_t &totalReadings, uint32_t &errorCount, float &avgReadTime) {
    totalReadings = stats.totalReadings;
    errorCount = stats.errorCount;
    
    if (stats.totalReadings > 0) {
        avgReadTime = (float)stats.totalReadTime / stats.totalReadings;
    } else {
        avgReadTime = 0.0f;
    }
}

/**
 * @brief FreeRTOS task function for reading ADS channels
 * 
 * @param param Pointer to ADSManager instance
 */
void ADSManager::taskFunction(void* param) {
    ADSManager* manager = static_cast<ADSManager*>(param);
    if (manager != nullptr) {
        manager->taskLoop();
    }
    vTaskDelete(nullptr);  // Should never reach here
}

/**
 * @brief Main task loop - reads all ADS channels sequentially
 */
void ADSManager::taskLoop() {
    TickType_t lastWakeTime = xTaskGetTickCount();
    
    Serial.println("ADSManager: Reading task started");
    
    while (taskRunning) {
        uint32_t cycleStart = millis();
        
        // Update all readings
        bool success = updateReadings();
        
        uint32_t cycleTime = millis() - cycleStart;
        stats.lastCycleTime = cycleTime;
        stats.totalReadTime += cycleTime;
        
        if (success) {
            stats.totalReadings++;
        } else {
            stats.errorCount++;
        }
        
        // Wait for next cycle
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(readingInterval));
    }
    
    Serial.println("ADSManager: Reading task stopped");
}

/**
 * @brief Read all ADS channels and update internal structure
 * 
 * @return true if all readings successful
 * @return false if any reading failed
 */
bool ADSManager::updateReadings() {
    ADSReadings newReadings;
    bool allSuccess = true;
    
    // Read all 4 channels
    for (uint8_t channel = 0; channel < 4; channel++) {
        int16_t rawValue = ads.readADC_SingleEnded(channel);
        
        if (rawValue == -1) {
            // Reading failed
            allSuccess = false;
            newReadings.rawChannels[channel] = INT16_MIN;
            newReadings.voltageChannels[channel] = NAN;
        } else {
            newReadings.rawChannels[channel] = rawValue;
            newReadings.voltageChannels[channel] = rawToVoltage(rawValue);
        }
    }
    
    // Update shared structure with mutex protection
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        newReadings.timestamp = millis();
        newReadings.valid = allSuccess;
        newReadings.sequenceNumber = readings.sequenceNumber + 1;
        
        readings = newReadings;
        
        xSemaphoreGive(mutex);
    } else {
        Serial.println("ADSManager: Failed to acquire mutex for reading update");
        allSuccess = false;
    }
    
    return allSuccess;
}

/**
 * @brief Validate channel number
 * 
 * @param channel Channel to validate
 * @return true if valid (0-3)
 * @return false if invalid
 */
bool ADSManager::isValidChannel(uint8_t channel) {
    return channel < 4;
}

/**
 * @brief Convert raw ADC value to voltage
 * 
 * @param raw Raw ADC reading
 * @return float Voltage in volts
 */
float ADSManager::rawToVoltage(int16_t raw) {
    // Calculate voltage based on current gain setting
    float voltsPerBit;
    
    switch (currentGain) {
        case GAIN_TWOTHIRDS:  voltsPerBit = 0.1875e-3; break;  // +/- 6.144V
        case GAIN_ONE:        voltsPerBit = 0.125e-3;  break;  // +/- 4.096V
        case GAIN_TWO:        voltsPerBit = 0.0625e-3; break;  // +/- 2.048V
        case GAIN_FOUR:       voltsPerBit = 0.03125e-3; break; // +/- 1.024V
        case GAIN_EIGHT:      voltsPerBit = 0.015625e-3; break;// +/- 0.512V
        case GAIN_SIXTEEN:    voltsPerBit = 0.0078125e-3; break;// +/- 0.256V
        default:              voltsPerBit = 0.125e-3;  break;  // Default to GAIN_ONE
    }
    
    return raw * voltsPerBit;
}
