# I2C Manager Migration Guide
## Complete I2C Bus Centralization for ESP32-AIO

### Overview
The I2CManager provides complete centralized management of both MCP23017 and ADS1115 devices through a single FreeRTOS task with command queuing and proper bus arbitration.

### Architecture Benefits

**Previous Architecture Issues:**
- Multiple tasks accessing I2C bus simultaneously
- Bus contention between WAS, MainPower, ESPsteer, MCPManager
- No centralized error handling or recovery
- Difficult to diagnose I2C issues

**New I2CManager Architecture:**
- **Single I2C task** handles all bus operations
- **Command queuing** with priority and timeout handling
- **Centralized health monitoring** for all devices
- **Thread-safe cached data** for fast access
- **Comprehensive statistics** and error tracking

### Files Created/Modified

✅ **New Files:**
- `include/I2CManager.h` - Complete API for both MCP23017 and ADS1115
- `src/I2CManager.cpp` - Implementation with FreeRTOS task and command queue
- `main.cpp` - Updated to use I2CManager instead of individual managers

⚠️ **Files Requiring Migration:**
- `src/WAS.cpp` - Replace ADS calls with `i2cManager.adsGetRawReading()`
- `src/MainPower.cpp` - Replace ADS calls with I2CManager methods
- `src/ESPsteer.cpp` - Replace ADS calls with I2CManager methods
- `src/MCPManager.cpp` - Replace MCP calls with I2CManager methods (or deprecate)

### API Comparison

#### ADS1115 Operations
**Before (Direct ADS):**
```cpp
extern Adafruit_ADS1115 ads;
int16_t raw = ads.readADC_SingleEnded(0);
float voltage = ads.computeVolts(raw);
```

**After (I2CManager):**
```cpp
extern I2CManager i2cManager;
int16_t raw = i2cManager.adsGetRawReading(0);
float voltage = i2cManager.adsGetVoltage(0);
```

#### MCP23017 Operations
**Before (Direct MCP):**
```cpp
extern Adafruit_MCP23X17 mcp;
mcp.digitalWrite(pin, HIGH);
uint8_t state = mcp.digitalRead(pin);
```

**After (I2CManager):**
```cpp
extern I2CManager i2cManager;
uint32_t cmdId = i2cManager.mcpDigitalWrite(pin, HIGH);  // Non-blocking
uint8_t state = i2cManager.mcpDigitalRead(pin);          // Blocking
```

#### Batch Operations (Recommended)
```cpp
// Get all ADS readings at once (most efficient)
int16_t rawReadings[4];
float voltages[4];
if (i2cManager.adsGetAllReadings(rawReadings, voltages)) {
    // Use readings...
}

// Read all MCP GPIO at once
uint16_t gpioState = i2cManager.mcpReadGPIOAB();
```

### Migration Steps

#### 1. Update WAS.cpp
**Current Code:**
```cpp
if (ads != nullptr) {
    espData->steer.rawADS = ads->readADC_SingleEnded(0);
}
```

**Migrated Code:**
```cpp
int16_t rawReading = i2cManager.adsGetRawReading(0);
if (rawReading != INT16_MIN) {
    espData->steer.rawADS = rawReading;
    espData->steer.actSteerAngle = i2cManager.adsGetVoltage(0);
} else {
    Serial.println("WAS: Invalid ADS reading from I2C Manager");
}
```

#### 2. Update MainPower.cpp
**Current Code:**
```cpp
if (_ads != nullptr) {
    _data->power.mainCurrentRaw = _ads->readADC_SingleEnded(_data->adsConfig.mainPowerISpin);
}
```

**Migrated Code:**
```cpp
int16_t currentReading = i2cManager.adsGetRawReading(_data->adsConfig.mainPowerISpin);
if (currentReading != INT16_MIN) {
    _data->power.mainCurrentRaw = currentReading;
    _data->power.mainCurrent = i2cManager.adsGetVoltage(_data->adsConfig.mainPowerISpin) * 3.0; // Apply conversion factor
} else {
    _data->power.mainCurrentRaw = 0;
    Serial.println("MainPower: Invalid ADS reading from I2C Manager");
}
```

#### 3. Update ESPsteer.cpp
**Current Code:**
```cpp
if (ads != nullptr) {
    return ads->readADC_SingleEnded(2);
}
```

**Migrated Code:**
```cpp
int16_t currentReading = i2cManager.adsGetRawReading(2);
return (currentReading != INT16_MIN) ? currentReading : 0;
```

#### 4. Update MCPManager.cpp (Optional - Can be deprecated)
**Option A: Migrate MCPManager to use I2CManager**
```cpp
// In MCPManager methods, replace direct MCP calls:
// mcp.digitalWrite(pin, value);
uint32_t cmdId = i2cManager.mcpDigitalWrite(pin, value);

// mcp.digitalRead(pin);
return i2cManager.mcpDigitalRead(pin);
```

**Option B: Deprecate MCPManager and use I2CManager directly**
Replace all MCPManager usage with direct I2CManager calls.

### Advanced Features

#### Health Monitoring
```cpp
// Check overall I2C health
if (!i2cManager.isHealthy()) {
    Serial.println("I2C bus issues detected");
}

// Check specific device health
if (!i2cManager.isDeviceHealthy(I2CDeviceType::ADS1115)) {
    Serial.println("ADS1115 not responding");
}

if (!i2cManager.isDeviceHealthy(I2CDeviceType::MCP23017)) {
    Serial.println("MCP23017 not responding");
}
```

#### Performance Statistics
```cpp
uint32_t totalCmds, errorCount, queueDepth;
float avgTime;
i2cManager.getStatistics(totalCmds, errorCount, avgTime, queueDepth);

Serial.printf("I2C Stats: %d commands, %d errors, %.1fms avg, queue: %d\n", 
              totalCmds, errorCount, avgTime, queueDepth);
```

#### Non-blocking Operations
```cpp
// Queue multiple operations without blocking
uint32_t cmd1 = i2cManager.mcpDigitalWrite(0, HIGH);
uint32_t cmd2 = i2cManager.mcpDigitalWrite(1, LOW);
uint32_t cmd3 = i2cManager.mcpDigitalWrite(2, HIGH);

// Later, check if operations completed
if (i2cManager.isCommandComplete(cmd1)) {
    Serial.println("Command 1 completed");
}
```

### Configuration Options

#### Task Priority and Timing
```cpp
// Initialize with custom timing
i2cManager.begin(&twoWire, 0x20, 0x48, 5);  // 5ms task interval for higher frequency

// ADS configuration
i2cManager.adsSetDataRate(475);  // Up to 860 SPS
i2cManager.adsSetGain(GAIN_ONE); // Adjust for your voltage range
```

#### Channel Assignment
```cpp
#define WAS_CHANNEL 0           // Wheel Angle Sensor
#define MAIN_POWER_CHANNEL 1    // Main Power monitoring
#define STEER_CURRENT_CHANNEL 2 // Steering current
#define SPARE_CHANNEL 3         // Available for expansion
```

### Expected Performance Improvements

**I2C Traffic Reduction:**
- **Before**: ~40 transactions/second (multiple uncoordinated tasks)
- **After**: ~15-20 transactions/second (single coordinated task)
- **Improvement**: 50-60% reduction in bus traffic

**System Responsiveness:**
- Eliminated I2C bus contention
- Predictable timing and latency
- Better error recovery and handling
- Comprehensive health monitoring

**Debugging and Maintenance:**
- Centralized I2C error logging
- Performance statistics collection
- Device health status monitoring
- Command queue depth tracking

### Troubleshooting

#### Common Issues and Solutions

**1. Invalid Readings (INT16_MIN or NAN)**
```cpp
// Check device health first
if (!i2cManager.isDeviceHealthy(I2CDeviceType::ADS1115)) {
    Serial.println("ADS1115 offline - check connections");
}

// Check data age
uint32_t age = i2cManager.getLastOperationAge(I2CDeviceType::ADS1115);
if (age > 5000) {
    Serial.printf("ADS data is %d ms old\n", age);
}
```

**2. High Queue Depth**
```cpp
uint32_t totalCmds, errorCount, queueDepth;
float avgTime;
i2cManager.getStatistics(totalCmds, errorCount, avgTime, queueDepth);

if (queueDepth > 10) {
    Serial.println("Warning: High I2C command queue depth");
    // Consider reducing command frequency or increasing task priority
}
```

**3. High Error Rate**
```cpp
if (errorCount > totalCmds * 0.1) {  // More than 10% errors
    Serial.println("Warning: High I2C error rate - check bus integrity");
}
```

### Debug Variables Added to Web Interface

The following debug information is now available in the web interface:
- I2C Manager overall health status
- Individual device health (ADS1115, MCP23017)
- Command statistics (total, errors, queue depth)
- Last operation age for each device

This provides comprehensive visibility into I2C bus performance and health.

### Next Steps

1. **Test Current Build**: Verify I2CManager initializes and operates correctly
2. **Monitor Web Interface**: Check debug variables for I2C health status
3. **Migrate Components**: Update WAS, MainPower, ESPsteer one at a time
4. **Performance Testing**: Verify improved system responsiveness
5. **Optional**: Deprecate old ADSManager and individual MCP usage

The I2CManager provides a robust, scalable solution for all I2C operations with comprehensive monitoring and error handling.
