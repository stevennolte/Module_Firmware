#ifndef MCP_MANAGER_H
#define MCP_MANAGER_H

#include <Adafruit_MCP23X17.h>
#include <Wire.h>
#include "ESPdata.h"

/**
 * @brief Singleton manager for Adafruit_MCP23X17 I/O expander
 * 
 * Provides centralized management of the MCP23X17 chip with singleton pattern,
 * ensuring only one instance exists throughout the application.
 */
class MCPManager {
private:
    static MCPManager* instance;
    Adafruit_MCP23X17 mcp;
    bool initialized;
    ESPdata& _data;  // Store reference to avoid repeated getInstance() calls
    
    // Private constructor to prevent direct instantiation
    MCPManager() : initialized(false), _data(ESPdata::getInstance()) {}

    // Prevent copy construction and assignment
    MCPManager(const MCPManager&) = delete;
    MCPManager& operator=(const MCPManager&) = delete;

public:
    /**
     * @brief Get the singleton instance
     * @return Reference to the MCPManager singleton instance
     */
    static MCPManager& getInstance() {
        if (instance == nullptr) {
            instance = new MCPManager();
        }
        return *instance;
    }
    
    /**
     * @brief Destroy the singleton instance
     * Call this during cleanup if needed
     */
    static void destroyInstance() {
        delete instance;
        instance = nullptr;
    }
    
    /**
     * @brief Initialize the MCP23X17
     * @param address I2C address (default 0x20)
     * @param wire TwoWire interface to use
     * @return true if initialization successful
     */
    bool begin(uint8_t address = 0x20, TwoWire* wire = &Wire) {
        if (!initialized) {
            initialized = mcp.begin_I2C(address, wire);
            if (initialized) {
                Serial.println("MCPManager: MCP23X17 initialized successfully");
            } else {
                Serial.println("MCPManager: Failed to initialize MCP23X17");
            }
        }
        return initialized;
    }
    
    /**
     * @brief Get reference to the underlying MCP23X17 object
     * @return Reference to Adafruit_MCP23X17
     */
    Adafruit_MCP23X17& getMCP() {
        return mcp;
    }
    
    /**
     * @brief Check if MCP is initialized
     * @return true if initialized
     */
    bool isInitialized() const {
        return initialized;
    }
    
    // Convenience methods that delegate to the underlying MCP
    /**
     * @brief Set pin mode
     * @param pin Pin number (0-15)
     * @param mode INPUT, OUTPUT, etc.
     */
    void pinMode(uint8_t pin, uint8_t mode) {
        if (initialized) {
            mcp.pinMode(pin, mode);
        }
    }
    
    /**
     * @brief Write digital value to pin
     * @param pin Pin number (0-15)
     * @param value HIGH or LOW
     */
    void digitalWrite(uint8_t pin, uint8_t value) {
        if (initialized) {
            mcp.digitalWrite(pin, value);
        }
    }
    
    /**
     * @brief Read digital value from pin
     * @param pin Pin number (0-15)
     * @return HIGH or LOW
     */
    uint8_t digitalRead(uint8_t pin) {
        if (initialized) {
            return mcp.digitalRead(pin);
        }
        return LOW;
    }

    void setGPSactive(){
        mcp.pinMode(_data.pins.gpsFix, OUTPUT);
        mcp.digitalWrite(_data.pins.gpsFix, HIGH);
        return;
    }
};

// Convenience macro for accessing the MCP singleton
#define MCP_MANAGER MCPManager::getInstance()

#endif // MCP_MANAGER_H
