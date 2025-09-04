# ESPconfig Migration from JSON to Preferences

## Overview
Successfully migrated the ESPconfig class from JSON file storage to ESP32 Preferences (NVS) for more reliable and efficient configuration management.

## Changes Made

### Header File Updates (`include/ESPconfig.h`)
```cpp
// BEFORE: JSON-based storage
#include "LittleFS.h"
#include "ArduinoJson.h"

// AFTER: Preferences-based storage
#include "Preferences.h"
```

### Class Structure Updates
```cpp
class ESPconfig {
private:
    static ESPconfig* instance;
    Preferences preferences;  // NEW: Added Preferences object
    
public:
    // NEW METHOD: Added saveConfig() for comprehensive saving
    uint8_t saveConfig();
    
    // Existing methods updated internally to use Preferences
    uint8_t loadConfig();
    uint8_t updateIP();
    uint8_t updateServer();
    uint8_t updateSteer();
    uint8_t saveWASzero();
};
```

## Implementation Changes

### 1. Constructor Enhancement
```cpp
ESPconfig::ESPconfig() : progCfg(), wifiCfg(), otaCfg() {
    preferences.begin("agopen", false); // Initialize NVS namespace
}
```

### 2. Destructor Enhancement
```cpp
void ESPconfig::destroyInstance() {
    if (instance != nullptr) {
        instance->preferences.end(); // Properly close NVS
        delete instance;
        instance = nullptr;
    }
}
```

### 3. Load Configuration (Preferences-based)
```cpp
uint8_t ESPconfig::loadConfig(){
    // Load with defaults if not found
    wifiCfg.ips[0] = preferences.getUChar("ip0", 192);
    wifiCfg.ips[1] = preferences.getUChar("ip1", 168);
    wifiCfg.ips[2] = preferences.getUChar("ip2", 5);
    wifiCfg.ips[3] = preferences.getUChar("ip3", 11);
    
    steerData.wasZeroAngle = preferences.getFloat("wasZero", 0.0);
    
    // PID configuration
    steerCfg.pidInputFilt = preferences.getFloat("pidInputFilt", 0.1);
    steerCfg.pidOutputFilt = preferences.getFloat("pidOutputFilt", 0.1);
    
    // Steering parameters
    steerCfg.gainP = preferences.getFloat("Kp", 50.0);
    steerCfg.highPWM = preferences.getUChar("highPWM", 255);
    steerCfg.lowPWM = preferences.getUChar("lowPWM", 10);
    steerCfg.minPWM = preferences.getUChar("minPWM", 5);
    steerCfg.countsPerDeg = preferences.getFloat("countsPerDeg", 10.0);
    steerCfg.steerOffset = preferences.getFloat("wasOffset", 0.0);
    steerCfg.useADS = preferences.getBool("useADS", true);
    
    // Server configuration
    otaCfg.ipAddr = preferences.getUChar("serverAdr", 192);
    otaCfg.port = preferences.getUShort("serverPort", 8080);
    
    return 1; // Success
}
```

### 4. Save Methods (Individual Updates)
```cpp
uint8_t ESPconfig::updateIP() {
    preferences.putUChar("ip0", wifiCfg.ips[0]);
    preferences.putUChar("ip1", wifiCfg.ips[1]);
    preferences.putUChar("ip2", wifiCfg.ips[2]);
    preferences.putUChar("ip3", wifiCfg.ips[3]);
    return 1;
}

uint8_t ESPconfig::updateSteer(){
    preferences.putFloat("Kp", steerCfg.gainP);
    preferences.putUChar("highPWM", steerCfg.highPWM);
    preferences.putUChar("lowPWM", steerCfg.lowPWM);
    preferences.putUChar("minPWM", steerCfg.minPWM);
    preferences.putFloat("countsPerDeg", steerCfg.countsPerDeg);
    preferences.putFloat("wasOffset", steerCfg.steerOffset);
    preferences.putBool("useADS", steerCfg.useADS);
    preferences.putFloat("pidInputFilt", steerCfg.pidInputFilt);
    preferences.putFloat("pidOutputFilt", steerCfg.pidOutputFilt);
    return 1;
}
```

## Benefits of Preferences over JSON

### 1. **Reliability**
- ✅ **No file system dependencies** - No LittleFS mounting required
- ✅ **Atomic writes** - Preferences are written atomically
- ✅ **Wear leveling** - ESP32 NVS handles flash wear automatically
- ✅ **Power failure safe** - Data integrity maintained during power loss

### 2. **Performance**
- ✅ **Faster access** - Direct NVS access vs file I/O
- ✅ **Lower memory usage** - No JSON parsing overhead
- ✅ **Instant read/write** - No file opening/closing overhead

### 3. **Simplicity**
- ✅ **Type safety** - putFloat(), putUChar(), putBool() with correct types
- ✅ **Default values** - Built-in fallback values if keys don't exist
- ✅ **No parsing errors** - No JSON deserialization failures

### 4. **Storage Efficiency**
- ✅ **Compact storage** - Binary data vs text JSON
- ✅ **Only changed values** - Individual key updates vs full file rewrites
- ✅ **Namespace isolation** - "agopen" namespace keeps config separate

## Data Types Used

| Configuration Item | Preferences Type | Key Name | Default Value |
|-------------------|------------------|----------|---------------|
| IP Address | `putUChar()` | "ip0"-"ip3" | 192.168.5.11 |
| WAS Zero Angle | `putFloat()` | "wasZero" | 0.0 |
| PID Input Filter | `putFloat()` | "pidInputFilt" | 0.1 |
| PID Output Filter | `putFloat()` | "pidOutputFilt" | 0.1 |
| Proportional Gain | `putFloat()` | "Kp" | 50.0 |
| PWM Values | `putUChar()` | "highPWM", "lowPWM", "minPWM" | 255, 10, 5 |
| Counts Per Degree | `putFloat()` | "countsPerDeg" | 10.0 |
| WAS Offset | `putFloat()` | "wasOffset" | 0.0 |
| Use ADS | `putBool()` | "useADS" | true |
| Server Address | `putUChar()` | "serverAdr" | 192 |
| Server Port | `putUShort()` | "serverPort" | 8080 |

## Usage Examples

### Load Configuration
```cpp
ESPconfig& config = ESPconfig::getInstance();
uint8_t result = config.loadConfig();
if (result == 1) {
    Serial.println("Configuration loaded successfully");
}
```

### Update Individual Settings
```cpp
// Update IP address
config.wifiCfg.ips[0] = 10;
config.updateIP();

// Update steering parameters
config.steerCfg.gainP = 75.5;
config.updateSteer();

// Save WAS zero point
config.steerData.wasZeroAngle = 1456.7;
config.saveWASzero();
```

### Save All Configuration
```cpp
// Save everything at once
config.saveConfig();
```

## Migration Benefits Summary

1. **Eliminated Dependencies**: No more LittleFS or ArduinoJson requirements
2. **Improved Reliability**: Power-safe, atomic operations
3. **Better Performance**: Faster, lower memory footprint
4. **Simpler Code**: Type-safe API with default values
5. **Future-Proof**: ESP32 NVS is the recommended storage method

The ESPconfig class now provides a more robust, efficient, and maintainable configuration management system using ESP32's built-in Preferences (NVS) storage.
