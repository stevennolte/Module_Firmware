/**
 * @file I2CManager.cpp
 * @brief Implementation of centralized I2C bus management
 * 
 * @details Provides centralized, thread-safe access to all I2C devices
 *          through a single FreeRTOS task with command queuing and
 *          proper bus arbitration.
 * 
 * @author ESP32-AIO Project
 * @date 2024
 */

#include "I2CManager.h"
#include <algorithm>

// Global instance definition
I2CManager i2cManager;

/**
 * @brief Construct a new I2CManager object
 */
I2CManager::I2CManager() : 
    wire(nullptr),
    mcpAddress(0x20),
    adsAddress(0x48),
    taskHandle(nullptr),
    commandQueue(nullptr),
    dataMutex(nullptr),
    taskInterval(10),
    taskRunning(false),
    initialized(false),
    nextCommandId(1) {
    
    // Initialize data structures
    memset(&adsData, 0, sizeof(adsData));
    memset(&mcpData, 0, sizeof(mcpData));
    memset(&adsHealth, 0, sizeof(adsHealth));
    memset(&mcpHealth, 0, sizeof(mcpHealth));
    memset(&stats, 0, sizeof(stats));
    
    adsData.currentGain = GAIN_TWOTHIRDS;
    adsData.currentRate = 128;
}

/**
 * @brief Destroy the I2CManager object
 */
I2CManager::~I2CManager() {
    end();
}

/**
 * @brief Initialize I2C devices and start management task
 */
bool I2CManager::begin(TwoWire* wire, uint8_t mcpAddress, uint8_t adsAddress, uint32_t taskInterval) {
    if (initialized) {
        Serial.println("I2CManager: Already initialized");
        return true;
    }

    this->wire = wire;
    this->mcpAddress = mcpAddress;
    this->adsAddress = adsAddress;
    this->taskInterval = taskInterval;

    // Create synchronization objects
    dataMutex = xSemaphoreCreateMutex();
    if (dataMutex == nullptr) {
        Serial.println("I2CManager: Failed to create data mutex");
        return false;
    }

    commandQueue = xQueueCreate(COMMAND_QUEUE_SIZE, sizeof(I2CCommand));
    if (commandQueue == nullptr) {
        Serial.println("I2CManager: Failed to create command queue");
        vSemaphoreDelete(dataMutex);
        dataMutex = nullptr;
        return false;
    }

    // Initialize ADS1115
    if (!ads.begin(adsAddress, wire)) {
        Serial.printf("I2CManager: Failed to initialize ADS1115 at 0x%02X\n", adsAddress);
        vSemaphoreDelete(dataMutex);
        vQueueDelete(commandQueue);
        dataMutex = nullptr;
        commandQueue = nullptr;
        return false;
    }

    // Configure ADS1115
    ads.setGain(adsData.currentGain);
    ads.setDataRate(RATE_ADS1115_128SPS);

    // Initialize MCP23017
    if (!mcp.begin_I2C(mcpAddress, wire)) {
        Serial.printf("I2CManager: Failed to initialize MCP23017 at 0x%02X\n", mcpAddress);
        vSemaphoreDelete(dataMutex);
        vQueueDelete(commandQueue);
        dataMutex = nullptr;
        commandQueue = nullptr;
        return false;
    }

    // Test initial communication
    int16_t testADS = ads.readADC_SingleEnded(0);
    uint16_t testMCP = mcp.readGPIOAB();
    
    if (testADS == -1) {
        Serial.println("I2CManager: ADS1115 initial test failed");
    } else {
        adsHealth.isOnline = true;
        adsHealth.lastSuccessTime = millis();
    }

    mcpHealth.isOnline = true;
    mcpHealth.lastSuccessTime = millis();

    // Create and start the I2C management task
    BaseType_t taskResult = xTaskCreate(
        taskFunction,           // Task function
        "I2CManager",          // Task name
        TASK_STACK_SIZE,       // Stack size
        this,                  // Parameter (this instance)
        3,                     // Priority (high)
        &taskHandle            // Task handle
    );

    if (taskResult != pdPASS) {
        Serial.println("I2CManager: Failed to create management task");
        vSemaphoreDelete(dataMutex);
        vQueueDelete(commandQueue);
        dataMutex = nullptr;
        commandQueue = nullptr;
        return false;
    }

    initialized = true;
    taskRunning = true;
    
    Serial.printf("I2CManager: Initialized successfully (MCP: 0x%02X, ADS: 0x%02X, interval: %dms)\n", 
                  mcpAddress, adsAddress, taskInterval);
    
    return true;
}

/**
 * @brief Stop the I2C management task and cleanup
 */
void I2CManager::end() {
    if (!initialized) {
        return;
    }

    // Stop the task
    if (taskHandle != nullptr) {
        taskRunning = false;
        vTaskDelete(taskHandle);
        taskHandle = nullptr;
    }

    // Cleanup synchronization objects
    if (dataMutex != nullptr) {
        vSemaphoreDelete(dataMutex);
        dataMutex = nullptr;
    }

    if (commandQueue != nullptr) {
        vQueueDelete(commandQueue);
        commandQueue = nullptr;
    }

    completedCommands.clear();
    initialized = false;
    
    Serial.println("I2CManager: Shutdown complete");
}

// ADS1115 Interface Implementation
int16_t I2CManager::adsGetRawReading(uint8_t channel) {
    if (!initialized || channel >= 4) {
        return INT16_MIN;
    }

    int16_t result = INT16_MIN;
    
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (adsData.valid && (millis() - adsData.lastUpdate) < DATA_TIMEOUT_MS) {
            result = adsData.rawChannels[channel];
        }
        xSemaphoreGive(dataMutex);
    }
    
    return result;
}

float I2CManager::adsGetVoltage(uint8_t channel) {
    if (!initialized || channel >= 4) {
        return NAN;
    }

    float result = NAN;
    
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (adsData.valid && (millis() - adsData.lastUpdate) < DATA_TIMEOUT_MS) {
            result = adsData.voltageChannels[channel];
        }
        xSemaphoreGive(dataMutex);
    }
    
    return result;
}

bool I2CManager::adsGetAllReadings(int16_t rawReadings[4], float voltages[4]) {
    if (!initialized || rawReadings == nullptr || voltages == nullptr) {
        return false;
    }

    bool result = false;
    
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (adsData.valid && (millis() - adsData.lastUpdate) < DATA_TIMEOUT_MS) {
            memcpy(rawReadings, adsData.rawChannels, sizeof(adsData.rawChannels));
            memcpy(voltages, adsData.voltageChannels, sizeof(adsData.voltageChannels));
            result = true;
        }
        xSemaphoreGive(dataMutex);
    }
    
    return result;
}

void I2CManager::adsSetGain(adsGain_t gain) {
    I2CCommand cmd = {
        .device = I2CDeviceType::ADS1115,
        .operation = I2COperation::ADS_SET_GAIN,
        .param1 = (uint8_t)gain,
        .param2 = 0,
        .blocking = false,
        .timestamp = millis(),
        .result = 0,
        .completed = false,
        .commandId = 0
    };
    
    queueCommand(cmd);
}

void I2CManager::adsSetDataRate(uint16_t rate) {
    I2CCommand cmd = {
        .device = I2CDeviceType::ADS1115,
        .operation = I2COperation::ADS_SET_DATA_RATE,
        .param1 = 0,
        .param2 = rate,
        .blocking = false,
        .timestamp = millis(),
        .result = 0,
        .completed = false,
        .commandId = 0
    };
    
    queueCommand(cmd);
}

// MCP23017 Interface Implementation
uint32_t I2CManager::mcpPinMode(uint8_t pin, uint8_t mode) {
    I2CCommand cmd = {
        .device = I2CDeviceType::MCP23017,
        .operation = I2COperation::MCP_PIN_MODE,
        .param1 = pin,
        .param2 = mode,
        .blocking = false,
        .timestamp = millis(),
        .result = 0,
        .completed = false,
        .commandId = 0
    };
    
    return queueCommand(cmd);
}

uint32_t I2CManager::mcpDigitalWrite(uint8_t pin, uint8_t value) {
    I2CCommand cmd = {
        .device = I2CDeviceType::MCP23017,
        .operation = I2COperation::MCP_DIGITAL_WRITE,
        .param1 = pin,
        .param2 = value,
        .blocking = false,
        .timestamp = millis(),
        .result = 0,
        .completed = false,
        .commandId = 0
    };
    
    return queueCommand(cmd);
}

uint8_t I2CManager::mcpDigitalRead(uint8_t pin) {
    I2CCommand cmd = {
        .device = I2CDeviceType::MCP23017,
        .operation = I2COperation::MCP_DIGITAL_READ,
        .param1 = pin,
        .param2 = 0,
        .blocking = true,
        .timestamp = millis(),
        .result = 0,
        .completed = false,
        .commandId = 0
    };
    
    uint32_t cmdId = queueCommand(cmd);
    
    // Wait for completion with timeout
    uint32_t startTime = millis();
    while (!isCommandComplete(cmdId) && (millis() - startTime) < COMMAND_TIMEOUT_MS) {
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    
    int16_t result = getCommandResult(cmdId);
    return (result >= 0) ? (uint8_t)result : 0xFF;
}

uint32_t I2CManager::mcpWriteGPIOAB(uint16_t value) {
    I2CCommand cmd = {
        .device = I2CDeviceType::MCP23017,
        .operation = I2COperation::MCP_WRITE_GPIO_AB,
        .param1 = 0,
        .param2 = value,
        .blocking = false,
        .timestamp = millis(),
        .result = 0,
        .completed = false,
        .commandId = 0
    };
    
    return queueCommand(cmd);
}

uint16_t I2CManager::mcpReadGPIOAB() {
    if (!initialized) {
        return 0xFFFF;
    }

    uint16_t result = 0xFFFF;
    
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        if (mcpData.valid && (millis() - mcpData.lastUpdate) < DATA_TIMEOUT_MS) {
            result = mcpData.gpioState;
        }
        xSemaphoreGive(dataMutex);
    }
    
    return result;
}

// Status and Health Monitoring
bool I2CManager::isHealthy() {
    return isDeviceHealthy(I2CDeviceType::ADS1115) && 
           isDeviceHealthy(I2CDeviceType::MCP23017);
}

bool I2CManager::isDeviceHealthy(I2CDeviceType device) {
    if (!initialized) {
        return false;
    }

    DeviceHealth* health = nullptr;
    if (device == I2CDeviceType::ADS1115) {
        health = &adsHealth;
    } else if (device == I2CDeviceType::MCP23017) {
        health = &mcpHealth;
    }

    if (health == nullptr) {
        return false;
    }

    return health->isOnline && 
           (millis() - health->lastSuccessTime) < DATA_TIMEOUT_MS;
}

uint32_t I2CManager::getLastOperationAge(I2CDeviceType device) {
    if (!initialized) {
        return UINT32_MAX;
    }

    DeviceHealth* health = nullptr;
    if (device == I2CDeviceType::ADS1115) {
        health = &adsHealth;
    } else if (device == I2CDeviceType::MCP23017) {
        health = &mcpHealth;
    }

    if (health == nullptr) {
        return UINT32_MAX;
    }

    return millis() - health->lastSuccessTime;
}

void I2CManager::getStatistics(uint32_t &totalCommands, uint32_t &errorCount, 
                              float &avgProcessTime, uint32_t &queueDepth) {
    totalCommands = stats.totalCommands;
    errorCount = stats.errorCount;
    
    if (stats.totalCommands > 0) {
        avgProcessTime = (float)stats.totalProcessTime / stats.totalCommands;
    } else {
        avgProcessTime = 0.0f;
    }
    
    queueDepth = (commandQueue != nullptr) ? uxQueueMessagesWaiting(commandQueue) : 0;
}

bool I2CManager::isCommandComplete(uint32_t commandId) {
    for (const auto& cmd : completedCommands) {
        if (cmd.commandId == commandId) {
            return cmd.completed;
        }
    }
    return false;
}

int16_t I2CManager::getCommandResult(uint32_t commandId) {
    for (const auto& cmd : completedCommands) {
        if (cmd.commandId == commandId && cmd.completed) {
            return cmd.result;
        }
    }
    return -1;
}

// Private Implementation
void I2CManager::taskFunction(void* param) {
    I2CManager* manager = static_cast<I2CManager*>(param);
    if (manager != nullptr) {
        manager->taskLoop();
    }
    vTaskDelete(nullptr);
}

void I2CManager::taskLoop() {
    TickType_t lastWakeTime = xTaskGetTickCount();
    
    Serial.println("I2CManager: Management task started");
    
    while (taskRunning) {
        uint32_t cycleStart = millis();
        
        // Process queued commands
        I2CCommand cmd;
        while (xQueueReceive(commandQueue, &cmd, 0) == pdTRUE) {
            uint32_t cmdStart = millis();
            bool success = processCommand(cmd);
            uint32_t cmdTime = millis() - cmdStart;
            
            stats.totalCommands++;
            stats.totalProcessTime += cmdTime;
            
            if (!success) {
                stats.errorCount++;
            }
            
            // Store completed command
            cmd.completed = true;
            cmd.result = success ? cmd.result : -1;
            completedCommands.push_back(cmd);
            
            // Clean up old commands
            if (completedCommands.size() > COMPLETED_CMD_HISTORY) {
                cleanupCompletedCommands();
            }
        }
        
        // Update device readings periodically
        static uint32_t lastADSUpdate = 0;
        static uint32_t lastMCPUpdate = 0;
        
        uint32_t now = millis();
        
        if (now - lastADSUpdate >= 100) {  // Update ADS every 100ms
            updateADSReadings();
            lastADSUpdate = now;
        }
        
        if (now - lastMCPUpdate >= 50) {   // Update MCP every 50ms
            updateMCPState();
            lastMCPUpdate = now;
        }
        
        // Wait for next cycle
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(taskInterval));
    }
    
    Serial.println("I2CManager: Management task stopped");
}

bool I2CManager::processCommand(I2CCommand& cmd) {
    bool success = false;
    
    switch (cmd.device) {
        case I2CDeviceType::ADS1115:
            switch (cmd.operation) {
                case I2COperation::ADS_SET_GAIN:
                    ads.setGain((adsGain_t)cmd.param1);
                    adsData.currentGain = (adsGain_t)cmd.param1;
                    success = true;
                    break;
                    
                case I2COperation::ADS_SET_DATA_RATE:
                    // Convert rate to ADS enum
                    if (cmd.param2 <= 8) {
                        ads.setDataRate(RATE_ADS1115_8SPS);
                    } else if (cmd.param2 <= 16) {
                        ads.setDataRate(RATE_ADS1115_16SPS);
                    } else if (cmd.param2 <= 32) {
                        ads.setDataRate(RATE_ADS1115_32SPS);
                    } else if (cmd.param2 <= 64) {
                        ads.setDataRate(RATE_ADS1115_64SPS);
                    } else if (cmd.param2 <= 128) {
                        ads.setDataRate(RATE_ADS1115_128SPS);
                    } else if (cmd.param2 <= 250) {
                        ads.setDataRate(RATE_ADS1115_250SPS);
                    } else if (cmd.param2 <= 475) {
                        ads.setDataRate(RATE_ADS1115_475SPS);
                    } else {
                        ads.setDataRate(RATE_ADS1115_860SPS);
                    }
                    adsData.currentRate = cmd.param2;
                    success = true;
                    break;
                    
                default:
                    break;
            }
            updateDeviceHealth(I2CDeviceType::ADS1115, success);
            break;
            
        case I2CDeviceType::MCP23017:
            switch (cmd.operation) {
                case I2COperation::MCP_PIN_MODE:
                    mcp.pinMode(cmd.param1, cmd.param2);
                    success = true;
                    break;
                    
                case I2COperation::MCP_DIGITAL_WRITE:
                    mcp.digitalWrite(cmd.param1, cmd.param2);
                    success = true;
                    break;
                    
                case I2COperation::MCP_DIGITAL_READ:
                    cmd.result = mcp.digitalRead(cmd.param1);
                    success = (cmd.result >= 0);
                    break;
                    
                case I2COperation::MCP_WRITE_GPIO_AB:
                    mcp.writeGPIOAB(cmd.param2);
                    success = true;
                    break;
                    
                case I2COperation::MCP_READ_GPIO_AB:
                    cmd.result = mcp.readGPIOAB();
                    success = (cmd.result >= 0);
                    break;
                    
                default:
                    break;
            }
            updateDeviceHealth(I2CDeviceType::MCP23017, success);
            break;
    }
    
    return success;
}

bool I2CManager::updateADSReadings() {
    bool allSuccess = true;
    
    // Read all 4 channels
    for (uint8_t channel = 0; channel < 4; channel++) {
        int16_t rawValue = ads.readADC_SingleEnded(channel);
        
        if (rawValue == -1) {
            allSuccess = false;
            adsData.rawChannels[channel] = INT16_MIN;
            adsData.voltageChannels[channel] = NAN;
        } else {
            adsData.rawChannels[channel] = rawValue;
            adsData.voltageChannels[channel] = rawToVoltage(rawValue, adsData.currentGain);
        }
    }
    
    // Update shared data with mutex protection
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
        adsData.lastUpdate = millis();
        adsData.valid = allSuccess;
        xSemaphoreGive(dataMutex);
    }
    
    updateDeviceHealth(I2CDeviceType::ADS1115, allSuccess);
    return allSuccess;
}

bool I2CManager::updateMCPState() {
    uint16_t gpioState = mcp.readGPIOAB();
    bool success = (gpioState != 0xFFFF);  // Assuming 0xFFFF indicates error
    
    if (success) {
        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            mcpData.gpioState = gpioState;
            mcpData.lastUpdate = millis();
            mcpData.valid = true;
            xSemaphoreGive(dataMutex);
        }
    }
    
    updateDeviceHealth(I2CDeviceType::MCP23017, success);
    return success;
}

float I2CManager::rawToVoltage(int16_t raw, adsGain_t gain) {
    float voltsPerBit;
    
    switch (gain) {
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

uint32_t I2CManager::queueCommand(const I2CCommand& cmd) {
    if (!initialized || commandQueue == nullptr) {
        return 0;
    }
    
    I2CCommand queuedCmd = cmd;
    queuedCmd.commandId = nextCommandId++;
    
    if (xQueueSend(commandQueue, &queuedCmd, pdMS_TO_TICKS(10)) == pdTRUE) {
        // Update queue depth statistics
        uint32_t currentDepth = uxQueueMessagesWaiting(commandQueue);
        if (currentDepth > stats.maxQueueDepth) {
            stats.maxQueueDepth = currentDepth;
        }
        return queuedCmd.commandId;
    }
    
    return 0;  // Failed to queue
}

void I2CManager::cleanupCompletedCommands() {
    if (completedCommands.size() > COMPLETED_CMD_HISTORY) {
        completedCommands.erase(completedCommands.begin(), 
                               completedCommands.begin() + (completedCommands.size() - COMPLETED_CMD_HISTORY));
    }
}

void I2CManager::updateDeviceHealth(I2CDeviceType device, bool success) {
    DeviceHealth* health = nullptr;
    
    if (device == I2CDeviceType::ADS1115) {
        health = &adsHealth;
    } else if (device == I2CDeviceType::MCP23017) {
        health = &mcpHealth;
    }
    
    if (health == nullptr) {
        return;
    }
    
    health->totalOperations++;
    
    if (success) {
        health->lastSuccessTime = millis();
        health->isOnline = true;
    } else {
        health->errorCount++;
        // Mark offline if too many consecutive errors
        if (health->errorCount > 10) {
            health->isOnline = false;
        }
    }
}
