/**
 * @file ADSManager.h
 * @brief Centralized ADS1115 management to reduce I2C bus contention
 * 
 * @details This class provides centralized management of ADS1115 readings
 *          to reduce I2C bus contention by having a single task read all
 *          channels and store them in a shared structure accessible by
 *          other components (WAS, MainPower, ESPsteer).
 * 
 * @author ESP32-AIO Project
 * @date 2024
 */

#ifndef ADSMANAGER_H
#define ADSMANAGER_H

#include <Arduino.h>
#include <Adafruit_ADS1X15.h>
#include "ESPdata.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

/**
 * @class ADSManager
 * @brief Centralized manager for ADS1115 ADC readings
 * 
 * @details Manages all ADS1115 operations through a single FreeRTOS task
 *          to prevent I2C bus contention. Stores readings in a thread-safe
 *          structure accessible by WAS, MainPower, and ESPsteer components.
 */
class ADSManager {
public:
    /**
     * @brief Construct a new ADSManager object
     */
    ADSManager();

    /**
     * @brief Destroy the ADSManager object
     */
    ~ADSManager();

    /**
     * @brief Initialize the ADS1115 and start the reading task
     * 
     * @param i2cAddress I2C address of the ADS1115 (default: 0x48)
     * @param intervalMs Reading interval in milliseconds (default: 100ms)
     * @return true if initialization successful
     * @return false if initialization failed
     */
    bool begin(uint8_t i2cAddress = 0x48, uint32_t intervalMs = 100);

    /**
     * @brief Stop the reading task and cleanup
     */
    void end();

    /**
     * @brief Get the latest raw reading for a specific channel
     * 
     * @param channel ADS1115 channel (0-3)
     * @return int16_t Raw ADC reading, or INT16_MIN if invalid
     */
    int16_t getRawReading(uint8_t channel);

    /**
     * @brief Get the latest voltage reading for a specific channel
     * 
     * @param channel ADS1115 channel (0-3)
     * @return float Voltage reading in volts, or NAN if invalid
     */
    float getVoltage(uint8_t channel);

    /**
     * @brief Get all channel readings at once
     * 
     * @param rawReadings Array to store 4 raw readings (channels 0-3)
     * @param voltages Array to store 4 voltage readings (channels 0-3)
     * @return true if readings are valid and recent
     * @return false if readings are stale or invalid
     */
    bool getAllReadings(int16_t rawReadings[4], float voltages[4]);

    /**
     * @brief Get the age of the last reading in milliseconds
     * 
     * @return uint32_t Age of last reading in ms
     */
    uint32_t getReadingAge();

    /**
     * @brief Set the reading interval
     * 
     * @param intervalMs Interval between readings in milliseconds (min: 50ms)
     */
    void setReadingInterval(uint32_t intervalMs);

    /**
     * @brief Check if ADS1115 is responding and readings are current
     * 
     * @return true if ADS is responding and readings are fresh
     * @return false if ADS is not responding or readings are stale
     */
    bool isHealthy();

    /**
     * @brief Get the current gain setting
     * 
     * @return adsGain_t Current gain setting
     */
    adsGain_t getGain();

    /**
     * @brief Set the gain for all channels
     * 
     * @param gain New gain setting
     */
    void setGain(adsGain_t gain);

    /**
     * @brief Get the current data rate
     * 
     * @return uint16_t Current data rate in SPS
     */
    uint16_t getDataRate();

    /**
     * @brief Set the data rate
     * 
     * @param rate New data rate
     */
    void setDataRate(uint16_t rate);

    /**
     * @brief Get reading statistics
     * 
     * @param totalReadings Total number of readings taken
     * @param errorCount Number of failed readings
     * @param avgReadTime Average time per reading cycle in ms
     */
    void getStatistics(uint32_t &totalReadings, uint32_t &errorCount, float &avgReadTime);

private:
    /**
     * @brief Structure to hold all channel readings with metadata
     */
    struct ADSReadings {
        int16_t rawChannels[4];     ///< Raw readings for channels 0-3
        float voltageChannels[4];   ///< Voltage readings for channels 0-3
        uint32_t timestamp;         ///< Timestamp of last update (millis())
        bool valid;                 ///< Whether readings are valid
        uint32_t sequenceNumber;    ///< Incremental sequence number
    };

    /**
     * @brief Statistics structure
     */
    struct Statistics {
        uint32_t totalReadings;     ///< Total successful readings
        uint32_t errorCount;        ///< Number of failed readings
        uint32_t totalReadTime;     ///< Cumulative read time in ms
        uint32_t lastCycleTime;     ///< Time of last reading cycle
    };

    Adafruit_ADS1115 ads;           ///< ADS1115 instance
    ADSReadings readings;           ///< Current readings (protected by mutex)
    Statistics stats;               ///< Reading statistics
    uint32_t readingInterval;       ///< Interval between readings (ms)
    uint8_t i2cAddress;            ///< I2C address of ADS1115
    bool initialized;               ///< Initialization status
    bool taskRunning;              ///< Task running status
    TaskHandle_t taskHandle;        ///< FreeRTOS task handle
    SemaphoreHandle_t mutex;        ///< Mutex for thread-safe access to readings
    adsGain_t currentGain;         ///< Current gain setting
    uint16_t currentRate;          ///< Current data rate setting

    static constexpr uint32_t TASK_STACK_SIZE = 4096;  ///< Task stack size
    static constexpr uint32_t READING_TIMEOUT_MS = 5000; ///< Max age for valid readings
    static constexpr uint32_t MIN_INTERVAL_MS = 50;     ///< Minimum reading interval

    /**
     * @brief FreeRTOS task function for reading ADS channels
     * 
     * @param param Pointer to ADSManager instance
     */
    static void taskFunction(void* param);

    /**
     * @brief Main task loop - reads all ADS channels sequentially
     */
    void taskLoop();

    /**
     * @brief Read all ADS channels and update internal structure
     * 
     * @return true if all readings successful
     * @return false if any reading failed
     */
    bool updateReadings();

    /**
     * @brief Validate channel number
     * 
     * @param channel Channel to validate (0-3)
     * @return true if channel is valid
     * @return false if channel is invalid
     */
    bool isValidChannel(uint8_t channel);

    /**
     * @brief Convert raw ADC value to voltage
     * 
     * @param raw Raw ADC reading
     * @return float Voltage in volts
     */
    float rawToVoltage(int16_t raw);
};

// Global instance declaration
extern ADSManager adsManager;

#endif // ADSMANAGER_H
