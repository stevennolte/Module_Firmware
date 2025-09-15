# MCPManager Usage Guide

The MCPManager provides a singleton interface to control the MCP23017 I/O expander. Here's how to use it:

## Basic Setup

```cpp
#include "MCPManager.h"

// Get the singleton instance
MCPManager& mcpMgr = MCPManager::getInstance();

// Initialize the MCP (done in main.cpp setup())
if (mcpMgr.begin(0x20, &twoWire)) {
    Serial.println("MCP initialized successfully");
}
```

## Motor Control Functions

```cpp
// Setup motor pins (replaces manual pinMode calls)
mcpMgr.setupMotorPins(ENA_PIN, ENB_PIN);

// Enable motor (replaces digitalWrite HIGH calls)
mcpMgr.enableMotor(ENA_PIN, ENB_PIN);

// Disable motor (replaces digitalWrite LOW calls)
mcpMgr.disableMotor(ENA_PIN, ENB_PIN);
```

## Power Control Functions

```cpp
// Setup power pin
mcpMgr.setupPowerPin(POWER_PIN);

// Turn power on/off
mcpMgr.setPowerState(POWER_PIN, true);  // Turn on
mcpMgr.setPowerState(POWER_PIN, false); // Turn off
```

## GPS Indicator Functions

```cpp
// Setup GPS indicator pins
mcpMgr.setupGPSIndicators(GPS_FIX_PIN, RTK_FIX_PIN);

// Test the indicators (brief flash)
mcpMgr.testGPSIndicators(GPS_FIX_PIN, RTK_FIX_PIN);

// Control individual indicators
mcpMgr.setGPSFix(GPS_FIX_PIN, true);   // GPS fix acquired
mcpMgr.setRTKFix(RTK_FIX_PIN, true);   // RTK fix acquired
```

## Direct Pin Control (for custom usage)

```cpp
// Basic pin operations
mcpMgr.pinMode(pin, OUTPUT);
mcpMgr.digitalWrite(pin, HIGH);
uint8_t state = mcpMgr.digitalRead(pin);

// Port-wide operations
mcpMgr.writeGPIOA(0xFF);
mcpMgr.writeGPIOB(0x00);
uint8_t portA = mcpMgr.readGPIOA();
```

## Migration from Direct MCP Usage

### Before (direct MCP usage):
```cpp
mcp->pinMode(enaPin, OUTPUT);
mcp->digitalWrite(enaPin, HIGH);
```

### After (using MCPManager):
```cpp
MCPManager& mcpMgr = MCPManager::getInstance();
mcpMgr.setupMotorPins(enaPin, enbPin);
mcpMgr.enableMotor(enaPin, enbPin);
```

## Benefits

1. **Centralized Control**: Single point of access for all MCP operations
2. **Error Handling**: Built-in checks for MCP initialization
3. **Convenience Functions**: Higher-level functions for common operations
4. **Logging**: Built-in debug messages for operations
5. **Singleton Pattern**: No need to pass MCP pointers around

## Backward Compatibility

The original `mcp` object is still available for existing code. You can gradually migrate to MCPManager or use both approaches simultaneously.

To get direct access to the MCP object through MCPManager:
```cpp
Adafruit_MCP23X17* directMcp = mcpMgr.getMCP();
if (directMcp != nullptr) {
    directMcp->pinMode(pin, OUTPUT);  // Direct access
}
```
