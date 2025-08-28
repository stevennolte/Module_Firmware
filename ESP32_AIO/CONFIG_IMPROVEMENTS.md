# ESP32 Configuration and Variable Sharing Improvements

This document outlines the improvements made to the ESP32_AIO firmware for better configuration management and variable sharing.

## Overview of Changes

### 1. Configuration System Improvements

#### **Replaced JSON file configuration with ESP32 Preferences library**

**Before:**
- Configuration stored in `/config.json` file on LittleFS
- Manual JSON parsing and serialization
- Error-prone file operations
- Limited type safety

**After:**
- Configuration stored in ESP32 NVS (Non-Volatile Storage) using Preferences library
- Type-safe get/set operations
- Automatic migration from existing JSON config
- Better error handling and reliability

#### **New Files Added:**
- `include/ConfigManager.h` - Modern configuration management interface
- `src/ConfigManager.cpp` - Implementation with type-safe operations
- `include/GlobalContext.h` - Singleton pattern for global state management
- `src/GlobalContext.cpp` - Implementation of global context
- `include/Globals.h` - Helper macros for convenient access

### 2. Variable Sharing Improvements

#### **Before:**
```cpp
// Global espConfig object accessed directly across files
extern ESPconfig espConfig;

// Direct access throughout codebase
espConfig.steerData.actSteerAngle = value;
```

#### **After:**
```cpp
// Centralized access through GlobalContext singleton
GlobalContext& ctx = getGlobalContext();

// Type-safe accessors
auto& steerData = ctx.getSteerData();
steerData.actSteerAngle = value;

// Or using convenience macros
STEER_DATA.actSteerAngle = value;
```

### 3. Configuration Migration

The system automatically detects and migrates existing JSON configurations to the new Preferences system:

1. **First Boot**: Checks for existing Preferences
2. **Migration**: If no Preferences found, looks for `/config.json`
3. **Import**: Converts JSON settings to Preferences format
4. **Fallback**: Uses built-in defaults if no config exists

### 4. Web Interface Enhancements

#### **New Configuration Endpoints:**
- `POST /setGpsSource` - Set GPS source (UM982 or External)
- `GET /saveConfig` - Save current configuration
- `GET /resetConfig` - Reset to factory defaults

#### **Updated HTML Interface:**
- GPS source dropdown with real-time updates
- Configuration save/reset buttons
- Improved error handling with timeouts

### 5. Debug Variable Improvements

NTRIP data is now included in debug variables:
- `..Last Ntrip Data` - Hex representation of received NTRIP data
- `..Last Ntrip Data Length` - Length of received NTRIP packet

## Usage Examples

### Configuration Management

```cpp
// Initialize the global context
GlobalContext& ctx = getGlobalContext();
if (!ctx.initialize()) {
    Serial.println("Failed to initialize!");
    return;
}

// Save a configuration value
ctx.getConfigManager().putFloat("kp", 9.5);
ctx.saveConfiguration();

// Access config data
float kp = ctx.getSteerConfig().gainP;

// Use convenience macros
STEER_CFG.gainP = 10.0;
saveConfig(); // Helper function
```

### Variable Access Patterns

```cpp
// Old way (still works for compatibility)
espConfig.steerData.targetSteerAngle = 15.0;

// New recommended way
STEER_DATA.targetSteerAngle = 15.0;

// Or explicit
getGlobalContext().getSteerData().targetSteerAngle = 15.0;
```

### Web Configuration

The web interface now allows users to:
1. Select GPS source (UM982 vs External)
2. Save current configuration to NVS
3. Reset configuration to factory defaults
4. View NTRIP data in debug variables

## Benefits

### **Reliability:**
- NVS is more reliable than file system operations
- Automatic wear leveling and error correction
- No risk of file corruption

### **Performance:**
- Faster read/write operations
- No JSON parsing overhead during runtime
- Reduced memory fragmentation

### **Maintainability:**
- Centralized configuration management
- Type-safe operations reduce bugs
- Clear separation of concerns
- Easier testing and debugging

### **Features:**
- Automatic migration from old configs
- Factory reset capability
- Better error handling
- Web-based configuration management

## Migration Notes

### **Backward Compatibility:**
- Existing code continues to work with `espConfig` reference
- Old JSON files are automatically migrated
- Deprecated methods show warnings but still function

### **Recommended Updates:**
1. Replace direct `espConfig` access with `GlobalContext` accessors
2. Use new web endpoints for configuration changes
3. Remove manual JSON file operations
4. Use `saveConfig()` helper instead of individual save methods

## File Structure

```
include/
├── ConfigManager.h          # Modern config management
├── GlobalContext.h         # Centralized state management  
├── Globals.h              # Convenience macros
└── ESPconfig.h           # Original config (maintained)

src/
├── ConfigManager.cpp      # Config implementation
├── GlobalContext.cpp     # Global state implementation
├── ESPconfig.cpp        # Updated (deprecated methods)
└── main.cpp            # Updated to use new system

data/
└── index.html          # Enhanced web interface
```

This modernization provides a robust foundation for future development while maintaining backward compatibility.
