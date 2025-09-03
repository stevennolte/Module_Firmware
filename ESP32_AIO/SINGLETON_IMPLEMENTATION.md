# ESPconfig Singleton Pattern Implementation

## Overview

I've added singleton pattern support to your existing ESPconfig class. This provides a cleaner way to access configuration throughout your codebase while maintaining backward compatibility.

## What Was Added

### 1. ESPconfig Header Changes (`include/ESPconfig.h`)

```cpp
class ESPconfig {
private:
    static ESPconfig* instance;  // Singleton instance pointer
    
public:
    // Singleton methods
    static ESPconfig& getInstance();
    static void destroyInstance();
    
    // Existing methods remain unchanged
    ESPconfig();  // Constructor still public for compatibility
    // ... rest of class
};
```

### 2. ESPconfig Implementation Changes (`src/ESPconfig.cpp`)

```cpp
// Static member initialization
ESPconfig* ESPconfig::instance = nullptr;

// Singleton implementation
ESPconfig& ESPconfig::getInstance() {
    if (instance == nullptr) {
        instance = new ESPconfig();
    }
    return *instance;
}

void ESPconfig::destroyInstance() {
    if (instance != nullptr) {
        delete instance;
        instance = nullptr;
    }
}
```

## Usage Examples

### Method 1: Direct Singleton Access

```cpp
// Instead of using global espConfig:
espConfig.steerData.targetSteerAngle = 45.0;

// Use singleton directly:
ESPconfig::getInstance().steerData.targetSteerAngle = 45.0;
```

### Method 2: Using the Convenience Macro

```cpp
// Include the singleton helper
#include "ESPConfigSingleton.h"

// Use the ESP_CONFIG macro:
ESP_CONFIG.steerData.targetSteerAngle = 45.0;
ESP_CONFIG.gpsData.latitude = "1234.5678";

// Or use the section macros:
STEER_DATA.targetSteerAngle = 45.0;
GPS_DATA.latitude = "1234.5678";
```

### Method 3: Get Reference Once, Use Multiple Times

```cpp
void updateSteeringSettings() {
    ESPconfig& config = ESPconfig::getInstance();
    
    config.steerCfg.gainP = 12.5;
    config.steerCfg.highPWM = 200;
    config.steerCfg.lowPWM = 50;
    config.updateSteer();  // Save to file
}
```

## Migration Strategies

### Strategy 1: Gradual Migration (Recommended)

Keep your existing global variable but start using singleton in new code:

```cpp
// main.cpp - Keep both for now
ESPconfig espConfig;                    // OLD: Keep for existing code
ESPconfig& configSingleton = ESPconfig::getInstance();  // NEW: For new code

// Existing components continue to work
GPS gps(&espConfig, &gpsSerial, &bnoSerial, &mcp);  // Uses old global

// New code can use singleton
void newFunction() {
    ESP_CONFIG.steerData.targetSteerAngle = 45.0;  // Uses singleton
}
```

### Strategy 2: Component-by-Component Migration

Update components one at a time to use singleton:

```cpp
// OLD constructor call:
GPS gps(&espConfig, &gpsSerial, &bnoSerial, &mcp);

// NEW constructor call:
GPS gps(&ESP_CONFIG, &gpsSerial, &bnoSerial, &mcp);
```

### Strategy 3: Full Migration

Replace all global espConfig usage with singleton access:

```cpp
// Find and replace throughout codebase:
// espConfig.          -> ESP_CONFIG.
// &espConfig          -> &ESP_CONFIG
// extern ESPconfig    -> (remove - no longer needed)
```

## Benefits of the Singleton Pattern

### 1. **Controlled Access**
```cpp
// Only one instance can exist
ESPconfig& config1 = ESPconfig::getInstance();
ESPconfig& config2 = ESPconfig::getInstance();
// config1 and config2 refer to the same object
```

### 2. **No Global Variable Management**
```cpp
// No need for extern declarations in other files
// OLD WAY in other .cpp files:
extern ESPconfig espConfig;  // Required in every file

// NEW WAY:
#include "ESPconfig.h"
// Just use ESPconfig::getInstance() - no extern needed
```

### 3. **Lazy Initialization**
```cpp
// Instance is created only when first accessed
// Guaranteed initialization before use
ESPconfig& config = ESPconfig::getInstance();  // Creates if needed
```

### 4. **Easy Testing and Mocking**
```cpp
void testFunction() {
    // Can control when instance is created/destroyed
    ESPconfig::destroyInstance();  // Clean slate for testing
    ESPconfig& config = ESPconfig::getInstance();  // Fresh instance
    // Run tests...
    ESPconfig::destroyInstance();  // Clean up
}
```

## Web Handler Examples

### Using Singleton in Web Handlers

```cpp
void handleSetGpsSource(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    StaticJsonDocument<128> doc;
    deserializeJson(doc, data, len);
    
    String source = doc["source"] | "";
    
    if (source == "external") {
        // Use singleton to update config
        ESP_CONFIG.gpsCfg.externalGPS = true;
        ESP_CONFIG.updateServer();  // Save to file
        request->send(200, "text/plain", "External GPS selected");
    }
}

void handleWASzero(AsyncWebServerRequest *request) {
    // Update using singleton
    ESP_CONFIG.steerData.wasZeroAngle = ESP_CONFIG.steerData.absAngle;
    
    if (ESP_CONFIG.saveWASzero()) {
        request->send(200, "text/plain", "WAS zeroed and saved");
    } else {
        request->send(500, "text/plain", "Failed to save");
    }
}
```

## Debug and Status Functions

```cpp
void printSystemStatus() {
    ESPconfig& config = ESPconfig::getInstance();
    
    Serial.println("=== System Status ===");
    Serial.printf("Program: %s v%d.%d.%d\n", 
                  config.progCfg.name,
                  config.progCfg.version[0],
                  config.progCfg.version[1],
                  config.progCfg.version[2]);
    Serial.printf("IP: %d.%d.%d.%d\n", 
                  config.wifiCfg.ips[0],
                  config.wifiCfg.ips[1],
                  config.wifiCfg.ips[2],
                  config.wifiCfg.ips[3]);
    Serial.printf("Steer Gain: %.2f\n", config.steerCfg.gainP);
    Serial.printf("GPS External: %s\n", config.gpsCfg.externalGPS ? "Yes" : "No");
}
```

## Best Practices

### 1. **Include the Helper Header**
```cpp
#include "ESPConfigSingleton.h"  // For convenience macros
```

### 2. **Use Descriptive Macros**
```cpp
// Instead of long chains:
ESPconfig::getInstance().steerData.targetSteerAngle = 45.0;

// Use clear macros:
STEER_DATA.targetSteerAngle = 45.0;
```

### 3. **Cache References for Multiple Access**
```cpp
void complexSteeringOperation() {
    auto& steerData = ESP_CONFIG.steerData;
    auto& steerCfg = ESP_CONFIG.steerCfg;
    
    // Multiple operations using cached references
    steerData.targetSteerAngle = calculateTarget();
    steerData.lastSteerTime = millis();
    float error = steerData.targetSteerAngle - steerData.actSteerAngle;
    float correction = error * steerCfg.gainP;
}
```

### 4. **Consistent Error Handling**
```cpp
bool updateAndSaveSteerConfig(float newGain) {
    if (newGain < 0.1 || newGain > 50.0) {
        Serial.println("Invalid steering gain!");
        return false;
    }
    
    ESP_CONFIG.steerCfg.gainP = newGain;
    return ESP_CONFIG.updateSteer() == 1;  // Returns 1 on success
}
```

## Backward Compatibility

The singleton implementation maintains full backward compatibility:

- Existing global `espConfig` variable still works
- All existing function calls remain unchanged
- Components that take `ESPconfig*` parameters still work
- No breaking changes to your existing codebase

You can migrate gradually, testing each change as you go.

## Summary

The singleton pattern provides:
- **Cleaner code**: No global variable management
- **Better organization**: Clear access patterns
- **Easier testing**: Controlled instance lifecycle
- **Thread safety**: Can be added if needed later
- **Backward compatibility**: Existing code continues to work

This gives you a modern, maintainable approach while preserving all your existing functionality.
