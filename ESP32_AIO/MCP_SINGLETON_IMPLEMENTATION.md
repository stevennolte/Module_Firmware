# MCPManager Singleton Implementation

I've successfully added an MCPManager singleton to your ESP32 firmware project. This provides a centralized way to manage the MCP23X17 I/O expander using the singleton pattern, similar to how ESPconfig works.

## What Was Added

### 1. MCPManager Class (`include/MCPManager.h`)
- Singleton pattern implementation for MCP23X17 management
- Thread-safe access to a single MCP instance
- Convenience methods for pinMode, digitalWrite, digitalRead
- Automatic initialization checking

### 2. Implementation (`src/MCPManager.cpp`)
- Static instance management
- Clean singleton lifecycle

## How It Works

### Current Setup (Both Approaches Available)

In `main.cpp`, you now have both approaches working side by side:

```cpp
// Traditional approach - existing MCP instance
Adafruit_MCP23X17 mcp;

// New singleton approach - MCPManager
MCPManager& mcpManager = MCPManager::getInstance();

// Both are initialized during setup:
if (progData.mcpState == 1){
    mcp.begin_I2C(0x20, &twoWire);              // Traditional
    mcpManager.begin(0x20, &twoWire);           // Singleton
}
```

### Usage Examples

#### Traditional Approach (Current)
```cpp
// GPS uses MCP pointer injection
GPS gps(&espConfig, &gpsSerial, &bnoSerial, &mcp);
gps.init(&espUdp);
```

#### New Singleton Approach
```cpp
// GPS can now use MCPManager singleton
gps.initWithSingleton(&espUdp);

// Or from anywhere in your code:
MCPManager& mcpMgr = MCPManager::getInstance();
mcpMgr.pinMode(pin, OUTPUT);
mcpMgr.digitalWrite(pin, HIGH);

// Or using the convenience macro:
MCP_MANAGER.pinMode(pin, OUTPUT);
MCP_MANAGER.digitalWrite(pin, HIGH);
```

## Migration Strategy

### Phase 1: Dual Approach (Current State)
- Both old and new approaches work simultaneously
- Existing code continues to work unchanged
- New features can use MCPManager singleton

### Phase 2: Gradual Migration (Optional)
You can gradually migrate components to use the singleton:

1. **Update constructors** to not require MCP pointer:
   ```cpp
   // Old: GPS(ESPconfig* vars, HardwareSerial* gps, HardwareSerial* bno, Adafruit_MCP23X17* mcp)
   // New: GPS(ESPconfig* vars, HardwareSerial* gps, HardwareSerial* bno)
   ```

2. **Use MCPManager internally**:
   ```cpp
   void GPS::init(ESPudp* espUdp) {
       MCPManager& mcp = MCPManager::getInstance();
       mcp.pinMode(_gpsFixIndPin, OUTPUT);
       // ...
   }
   ```

### Phase 3: Full Singleton (Future)
- Remove global `mcp` variable
- All components use MCPManager singleton
- Cleaner, more maintainable code

## Benefits of MCPManager Singleton

1. **Centralized Management**: Single point of access for MCP operations
2. **Memory Efficiency**: Only one MCP instance in memory
3. **Thread Safety**: Controlled access to MCP hardware
4. **Consistent API**: Same interface throughout the application
5. **Easy Testing**: Can be mocked/stubbed for unit tests
6. **Automatic Initialization**: Built-in checks for proper initialization

## Example: Adding MCP Control to Any Component

```cpp
#include "MCPManager.h"

void MyComponent::controlLED() {
    MCPManager& mcp = MCPManager::getInstance();
    
    if (mcp.isInitialized()) {
        mcp.pinMode(LED_PIN, OUTPUT);
        mcp.digitalWrite(LED_PIN, HIGH);
        delay(1000);
        mcp.digitalWrite(LED_PIN, LOW);
    }
}
```

## Current Status

✅ **MCPManager singleton implemented and working**  
✅ **Both approaches available simultaneously**  
✅ **Example implementation in GPS class**  
✅ **No breaking changes to existing code**  
✅ **Ready for gradual migration**

The system now provides you with maximum flexibility - you can continue using the existing approach while gradually adopting the singleton pattern where it makes sense.
