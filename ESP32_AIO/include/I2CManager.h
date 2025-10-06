/**
 * @file I2CManager.h
 * @brief Centralized I2C bus management for ESP32-AIO
 * 
 * @details This class provides centralized management of all I2C devices
 *          to eliminate bus contention and improve system performance.
 *          Manages MCP23017 I/O expander and ADS1115 ADC through a single
 *          FreeRTOS task with proper bus arbitration.
 * 
 * @author ESP32-AIO Project
 * @date 2024
 */

#ifndef I2CMANAGER_H
#define I2CMANAGER_H

#include <Arduino.h>
#include <Wire.h>
#include <vector>
#include <Adafruit_ADS1X15.h>
#include <Adafruit_MCP23X17.h>
#include "ESPdata.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

/**
 * @brief I2C device types managed by I2CManager
 */
enum class I2CDeviceType {
    MCP23017,
    ADS1115
};

/**
 * @brief I2C operation types
 */
enum class I2COperation {
    // ADS1115 operations
    ADS_READ_CHANNEL,
    ADS_SET_GAIN,
    ADS_SET_DATA_RATE,
    
    // MCP23017 operations
    MCP_DIGITAL_WRITE,
    MCP_DIGITAL_READ,
    MCP_PIN_MODE,
    MCP_WRITE_GPIO_AB,
    MCP_READ_GPIO_AB
};

/**
 * @brief I2C command structure for queued operations
 */
struct I2CCommand {
    I2CDeviceType device;
    I2COperation operation;
    uint8_t param1;         // Pin number, channel, etc.
    uint16_t param2;        // Value, mode, etc.
    bool blocking;          // Wait for completion
    uint32_t timestamp;     // When command was queued
    int16_t result;         // Operation result
    bool completed;         // Operation completed flag
    uint32_t commandId;     // Unique command ID
};

/**
 * @class I2CManager
 * @brief Centralized manager for all I2C bus operations
 * 
 * @details Manages MCP23017 and ADS1115 through a single FreeRTOS task
 *          to prevent bus contention. Provides thread-safe access to both
 *          devices with command queuing and result caching.
 */
class I2CManager {
public:
    /**
     * @brief Construct a new I2CManager object
     */
    I2CManager();

    /**
     * @brief Destroy the I2CManager object
     */
    ~I2CManager();

    /**
     * @brief Initialize I2C devices and start management task
     * 
     * @param wire Pointer to TwoWire I2C interface
     * @param mcpAddress MCP23017 I2C address (default: 0x20)
     * @param adsAddress ADS1115 I2C address (default: 0x48)
     * @param taskInterval Task execution interval in ms (default: 10ms)
     * @return true if initialization successful
     * @return false if initialization failed
     */
    bool begin(TwoWire* wire, uint8_t mcpAddress = 0x20, uint8_t adsAddress = 0x48, uint32_t taskInterval = 10);

    /**
     * @brief Stop the I2C management task and cleanup
     */
    void end();

    // ADS1115 Interface
    /**
     * @brief Get the latest raw reading from ADS1115 channel
     * 
     * @param channel ADS channel (0-3)
     * @return int16_t Raw ADC reading, or INT16_MIN if invalid
     */
    int16_t adsGetRawReading(uint8_t channel);

    /**
     * @brief Get the latest voltage reading from ADS1115 channel
     * 
     * @param channel ADS channel (0-3)
     * @return float Voltage in volts, or NAN if invalid
     */
    float adsGetVoltage(uint8_t channel);

    /**
     * @brief Get all ADS1115 channel readings at once
     * 
     * @param rawReadings Array to store 4 raw readings
     * @param voltages Array to store 4 voltage readings
     * @return true if readings are valid and recent
     */
    bool adsGetAllReadings(int16_t rawReadings[4], float voltages[4]);

    /**
     * @brief Set ADS1115 gain
     * 
     * @param gain New gain setting
     */
    void adsSetGain(adsGain_t gain);

    /**
     * @brief Set ADS1115 data rate
     * 
     * @param rate Data rate in SPS
     */
    void adsSetDataRate(uint16_t rate);

    // MCP23017 Interface
    /**
     * @brief Set MCP23017 pin mode (non-blocking)
     * 
     * @param pin Pin number (0-15)
     * @param mode Pin mode (INPUT, OUTPUT, INPUT_PULLUP)
     * @return uint32_t Command ID for tracking
     */
    uint32_t mcpPinMode(uint8_t pin, uint8_t mode);

    /**
     * @brief Write to MCP23017 digital pin (non-blocking)
     * 
     * @param pin Pin number (0-15)
     * @param value Digital value (HIGH/LOW)
     * @return uint32_t Command ID for tracking
     */
    uint32_t mcpDigitalWrite(uint8_t pin, uint8_t value);

    /**
     * @brief Read from MCP23017 digital pin (blocking)
     * 
     * @param pin Pin number (0-15)
     * @return uint8_t Pin state (HIGH/LOW) or 0xFF if error
     */
    uint8_t mcpDigitalRead(uint8_t pin);

    /**
     * @brief Write to both MCP23017 GPIO registers at once
     * 
     * @param value 16-bit value (GPIOA = low byte, GPIOB = high byte)
     * @return uint32_t Command ID for tracking
     */
    uint32_t mcpWriteGPIOAB(uint16_t value);

    /**
     * @brief Read both MCP23017 GPIO registers
     * 
     * @return uint16_t 16-bit GPIO state or 0xFFFF if error
     */
    uint16_t mcpReadGPIOAB();

    // Status and Health Monitoring
    /**
     * @brief Check if I2C manager is healthy and devices responding
     * 
     * @return true if all devices healthy
     * @return false if any device has issues
     */
    bool isHealthy();

    /**
     * @brief Get device-specific health status
     * 
     * @param device Device to check
     * @return true if device is healthy
     * @return false if device has issues
     */
    bool isDeviceHealthy(I2CDeviceType device);

    /**
     * @brief Get the age of last successful operation in milliseconds
     * 
     * @param device Device to check
     * @return uint32_t Age in milliseconds
     */
    uint32_t getLastOperationAge(I2CDeviceType device);

    /**
     * @brief Get I2C bus statistics
     * 
     * @param totalCommands Total commands processed
     * @param errorCount Number of failed commands
     * @param avgProcessTime Average command processing time in ms
     * @param queueDepth Current command queue depth
     */
    void getStatistics(uint32_t &totalCommands, uint32_t &errorCount, 
                      float &avgProcessTime, uint32_t &queueDepth);

    /**
     * @brief Check if a command has completed
     * 
     * @param commandId Command ID to check
     * @return true if command completed
     * @return false if command still pending or invalid ID
     */
    bool isCommandComplete(uint32_t commandId);

    /**
     * @brief Get result of a completed command
     * 
     * @param commandId Command ID
     * @return int16_t Command result or -1 if not found/completed
     */
    int16_t getCommandResult(uint32_t commandId);

private:
    /**
     * @brief ADS1115 data structure
     */
    struct ADSData {
        int16_t rawChannels[4];     ///< Raw readings for channels 0-3
        float voltageChannels[4];   ///< Voltage readings for channels 0-3
        uint32_t lastUpdate;       ///< Timestamp of last update
        bool valid;                 ///< Data validity flag
        adsGain_t currentGain;      ///< Current gain setting
        uint16_t currentRate;       ///< Current data rate
    };

    /**
     * @brief MCP23017 data structure
     */
    struct MCPData {
        uint16_t gpioState;         ///< Current GPIO state (GPIOA + GPIOB)
        uint16_t pinModes;          ///< Pin mode configuration
        uint32_t lastUpdate;       ///< Timestamp of last update
        bool valid;                 ///< Data validity flag
    };

    /**
     * @brief Device health tracking
     */
    struct DeviceHealth {
        uint32_t lastSuccessTime;  ///< Last successful operation
        uint32_t errorCount;       ///< Cumulative error count
        uint32_t totalOperations;  ///< Total operations attempted
        bool isOnline;             ///< Device online status
    };

    /**
     * @brief I2C bus statistics
     */
    struct I2CStats {
        uint32_t totalCommands;    ///< Total commands processed
        uint32_t errorCount;       ///< Total errors
        uint32_t totalProcessTime; ///< Cumulative processing time
        uint32_t maxQueueDepth;    ///< Maximum queue depth reached
    };

    // Hardware interfaces
    TwoWire* wire;                  ///< I2C interface
    Adafruit_ADS1115 ads;          ///< ADS1115 instance
    Adafruit_MCP23X17 mcp;         ///< MCP23017 instance

    // Device addresses
    uint8_t mcpAddress;            ///< MCP23017 I2C address
    uint8_t adsAddress;            ///< ADS1115 I2C address

    // Task management
    TaskHandle_t taskHandle;        ///< FreeRTOS task handle
    QueueHandle_t commandQueue;     ///< Command queue
    SemaphoreHandle_t dataMutex;    ///< Data access mutex
    uint32_t taskInterval;          ///< Task execution interval
    bool taskRunning;              ///< Task running flag
    bool initialized;              ///< Initialization status

    // Device data
    ADSData adsData;               ///< ADS1115 data cache
    MCPData mcpData;               ///< MCP23017 data cache
    DeviceHealth adsHealth;        ///< ADS1115 health tracking
    DeviceHealth mcpHealth;        ///< MCP23017 health tracking
    I2CStats stats;                ///< Bus statistics

    // Command tracking
    uint32_t nextCommandId;        ///< Next command ID
    std::vector<I2CCommand> completedCommands; ///< Completed commands history

    static constexpr uint32_t TASK_STACK_SIZE = 8192;      ///< Task stack size
    static constexpr uint32_t COMMAND_QUEUE_SIZE = 32;     ///< Command queue depth
    static constexpr uint32_t DATA_TIMEOUT_MS = 5000;      ///< Data validity timeout
    static constexpr uint32_t COMMAND_TIMEOUT_MS = 1000;   ///< Command timeout
    static constexpr uint32_t COMPLETED_CMD_HISTORY = 16;  ///< Completed command history size

    /**
     * @brief FreeRTOS task function
     * 
     * @param param Pointer to I2CManager instance
     */
    static void taskFunction(void* param);

    /**
     * @brief Main task loop
     */
    void taskLoop();

    /**
     * @brief Process a single I2C command
     * 
     * @param cmd Command to process
     * @return true if command processed successfully
     * @return false if command failed
     */
    bool processCommand(I2CCommand& cmd);

    /**
     * @brief Update ADS1115 readings
     * 
     * @return true if update successful
     * @return false if update failed
     */
    bool updateADSReadings();

    /**
     * @brief Update MCP23017 state
     * 
     * @return true if update successful
     * @return false if update failed
     */
    bool updateMCPState();

    /**
     * @brief Convert raw ADC value to voltage
     * 
     * @param raw Raw ADC reading
     * @param gain Current gain setting
     * @return float Voltage in volts
     */
    float rawToVoltage(int16_t raw, adsGain_t gain);

    /**
     * @brief Queue a command for execution
     * 
     * @param cmd Command to queue
     * @return uint32_t Command ID
     */
    uint32_t queueCommand(const I2CCommand& cmd);

    /**
     * @brief Clean up old completed commands
     */
    void cleanupCompletedCommands();

    /**
     * @brief Update device health status
     * 
     * @param device Device type
     * @param success Operation success status
     */
    void updateDeviceHealth(I2CDeviceType device, bool success);
};

// Global instance declaration
extern I2CManager i2cManager;

#endif // I2CMANAGER_H
