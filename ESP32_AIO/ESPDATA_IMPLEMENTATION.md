# ESPdata Implementation Guide

## Overview
ESPdata is a singleton class that provides persistent storage for all program runtime data using ESP32 Preferences (NVS). This complements ESPconfig by storing dynamic program state rather than configuration settings.

## Files Created
- `include/ESPdata.h` - Header file with class definition
- `src/ESPdata.cpp` - Implementation file with Preferences storage

## Integration in main.cpp

### Includes
```cpp
#include "ESPdata.h"
```

### Singleton Instance
```cpp
// Using singleton pattern - single access point for program data
ESPdata& espData = ESPdata::getInstance();
```

### Initialization in setup()
```cpp
void setup(){
    // ...existing code...
    espConfig.progCfg.confRes = espConfig.loadConfig();
    
    // Load program data from Preferences
    espData.loadData();
    Serial.println("Program data loaded from Preferences");
    
    // ...rest of setup...
}
```

### Usage in loop()
```cpp
void loop(){
    // Update runtime data
    espData.setUptime(millis());
    
    // Periodic saving (every 30 seconds in this example)
    static uint8_t saveCounter = 0;
    saveCounter++;
    if (saveCounter >= 6) {
        espData.saveData();
        saveCounter = 0;
        Serial.println("Program data saved to Preferences");
    }
    
    delay(5000);
}
```

## Current Data Fields

### Basic Example Fields
- **state** (uint8_t) - Current program state
- **lastSteerAngle** (float) - Last recorded steering angle
- **uptime** (uint32_t) - System uptime in milliseconds

## Usage Examples

### Reading Data
```cpp
ESPdata& data = ESPdata::getInstance();
data.loadData();

uint8_t currentState = data.getState();
float angle = data.getLastSteerAngle();
uint32_t runtime = data.getUptime();
```

### Writing Data
```cpp
ESPdata& data = ESPdata::getInstance();

data.setState(2);
data.setLastSteerAngle(123.45);
data.setUptime(millis());

// Save immediately
data.saveData();
```

### Adding New Data Fields

To add new data fields, modify both the header and implementation:

#### 1. Add to ESPdata.h
```cpp
class ESPdata {
private:
    // Add new member variable
    int16_t newField;
    
public:
    // Add getter/setter
    int16_t getNewField() const { return newField; }
    void setNewField(int16_t value) { newField = value; }
};
```

#### 2. Update ESPdata.cpp
```cpp
// Constructor - initialize new field
ESPdata::ESPdata() : state(0), lastSteerAngle(0.0), uptime(0), newField(0) {
    preferences.begin("espdata", false);
}

// loadData() - add loading
void ESPdata::loadData() {
    state = preferences.getUChar("state", 0);
    lastSteerAngle = preferences.getFloat("lastSteerAngle", 0.0);
    uptime = preferences.getUInt("uptime", 0);
    newField = preferences.getShort("newField", 0);  // Add this line
}

// saveData() - add saving
void ESPdata::saveData() {
    preferences.putUChar("state", state);
    preferences.putFloat("lastSteerAngle", lastSteerAngle);
    preferences.putUInt("uptime", uptime);
    preferences.putShort("newField", newField);  // Add this line
}
```

## Preferences Data Types

| C++ Type | Preferences Method | Key Size Limit |
|----------|-------------------|----------------|
| bool | getBool/putBool | 15 chars |
| int8_t | getChar/putChar | 15 chars |
| uint8_t | getUChar/putUChar | 15 chars |
| int16_t | getShort/putShort | 15 chars |
| uint16_t | getUShort/putUShort | 15 chars |
| int32_t | getInt/putInt | 15 chars |
| uint32_t | getUInt/putUInt | 15 chars |
| float | getFloat/putFloat | 15 chars |
| double | getDouble/putDouble | 15 chars |
| String | getString/putString | 15 chars |
| bytes | getBytes/putBytes | 15 chars |

## Best Practices

### 1. Periodic Saving
- Don't save on every change (wear leveling)
- Save periodically or on significant events
- Example: Every 30 seconds or on state changes

### 2. Default Values
- Always provide sensible defaults in loadData()
- Handle first-run scenarios gracefully

### 3. Data Organization
- Use ESPconfig for settings/configuration
- Use ESPdata for runtime state/data
- Keep related data together

### 4. Key Naming
- Use descriptive but short key names (15 char limit)
- Be consistent with naming conventions
- Avoid special characters

## Error Handling

The Preferences library is generally robust, but you can add error checking:

```cpp
void ESPdata::saveData() {
    if (preferences.putUChar("state", state) == 0) {
        Serial.println("Failed to save state");
    }
    // Continue with other saves...
}
```

## Memory Usage

- NVS partitions are typically 4KB-16KB
- Each key uses ~32 bytes + data size
- Monitor usage if storing large amounts of data
- Use `preferences.freeEntries()` to check available space

## Integration Complete

ESPdata is now fully integrated into your main.cpp file and ready for use. The singleton pattern ensures consistent access throughout your application, and the Preferences storage provides reliable persistence across power cycles.
