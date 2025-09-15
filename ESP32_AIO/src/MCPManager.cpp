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
        mcp.pinMode(espData->mcpPins.inputs.work_switch, INPUT);
        mcp.pinMode(espData->mcpPins.inputs.remote_switch, INPUT);
        mcp.pinMode(espData->mcpPins.inputs.steer_switch, INPUT);
        
        mcp.pinMode(espData->mcpPins.outputs.power_on, OUTPUT);
        mcp.pinMode(espData->mcpPins.outputs.eth_good, OUTPUT);
        mcp.pinMode(espData->mcpPins.outputs.gps_fix, OUTPUT);
        mcp.pinMode(espData->mcpPins.outputs.rtk_fix, OUTPUT);
        mcp.pinMode(espData->mcpPins.outputs.steer_standby, OUTPUT);
        mcp.pinMode(espData->mcpPins.outputs.steer_active, OUTPUT);
        mcp.pinMode(espData->mcpPins.outputs.motor_enb, OUTPUT);
        mcp.pinMode(espData->mcpPins.outputs.motor_ena, OUTPUT);

        mcp.digitalWrite(espData->mcpPins.outputs.power_on, LOW);
        mcp.digitalWrite(espData->mcpPins.outputs.eth_good, LOW);
        mcp.digitalWrite(espData->mcpPins.outputs.gps_fix, LOW);
        mcp.digitalWrite(espData->mcpPins.outputs.rtk_fix, LOW);
        mcp.digitalWrite(espData->mcpPins.outputs.steer_standby, LOW);
        mcp.digitalWrite(espData->mcpPins.outputs.steer_active, LOW);
        mcp.digitalWrite(espData->mcpPins.outputs.motor_enb, LOW);
        mcp.digitalWrite(espData->mcpPins.outputs.motor_ena, LOW);
        
        mcp.digitalWrite(espData->mcpPins.outputs.power_on, HIGH); // Power on the system
        mcp.digitalWrite(espData->mcpPins.outputs.eth_good, HIGH); // Indicate Ethernet good
        mcp.digitalWrite(espData->mcpPins.outputs.gps_fix, HIGH); // Indicate GPS fix
        mcp.digitalWrite(espData->mcpPins.outputs.rtk_fix, HIGH); // Indicate RTK fix
        mcp.digitalWrite(espData->mcpPins.outputs.steer_standby, HIGH); // Indicate steer standby
        mcp.digitalWrite(espData->mcpPins.outputs.steer_active, HIGH); // Indicate steer active
        mcp.digitalWrite(espData->mcpPins.outputs.motor_enb, HIGH); // Enable motor B
        mcp.digitalWrite(espData->mcpPins.outputs.motor_ena, HIGH); // Enable motor A
        delay(1000);
        mcp.digitalWrite(espData->mcpPins.outputs.power_on, LOW); // Clear power on
        mcp.digitalWrite(espData->mcpPins.outputs.eth_good, LOW); //
        mcp.digitalWrite(espData->mcpPins.outputs.gps_fix, LOW); //
        mcp.digitalWrite(espData->mcpPins.outputs.rtk_fix, LOW);
        mcp.digitalWrite(espData->mcpPins.outputs.steer_standby, LOW); //
        mcp.digitalWrite(espData->mcpPins.outputs.steer_active, LOW);
        mcp.digitalWrite(espData->mcpPins.outputs.motor_enb, LOW); //
        mcp.digitalWrite(espData->mcpPins.outputs.motor_ena, LOW);
        return true;
    } else {
        initialized = false;
        Serial.println("MCPManager: Failed to initialize MCP23X17");
        return false;
    }
}

bool MCPManager::setPowerIndicatorOn(bool state) {
    if (!initialized) {
        Serial.println("MCPManager: Cannot set power indicator - MCP not initialized");
        return false;
    }
    mcp.digitalWrite(espData->mcpPins.outputs.power_on, state ? HIGH : LOW);
    return true;
}

bool MCPManager::isInitialized() const {
    return initialized;
}


Adafruit_MCP23X17* MCPManager::getMCP() {
    if (!initialized) {
        Serial.println("MCPManager: Warning - MCP not initialized");
        return nullptr;
    }
    return &mcp;
}

MCPManager::~MCPManager() {
    // Destructor - cleanup if needed
}

