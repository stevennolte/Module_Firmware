#ifndef GLOBALS_H
#define GLOBALS_H

#include "GlobalContext.h"

// Convenient macros for accessing global context
#define GLOBAL_CTX getGlobalContext()
#define CONFIG GLOBAL_CTX.getConfig()
#define CONFIG_MGR GLOBAL_CTX.getConfigManager()

// Quick access to config sections
#define PROG_DATA GLOBAL_CTX.getProgramData()
#define PROG_CFG GLOBAL_CTX.getProgramConfig()
#define WIFI_CFG GLOBAL_CTX.getWifiConfig()
#define STEER_CFG GLOBAL_CTX.getSteerConfig()
#define STEER_DATA GLOBAL_CTX.getSteerData()
#define GPS_CFG GLOBAL_CTX.getGPSConfig()
#define GPS_DATA GLOBAL_CTX.getGPSData()
#define JOYSTICK_DATA GLOBAL_CTX.getJoystickData()

// Helper functions for common operations
inline void saveConfig() {
    GLOBAL_CTX.saveConfiguration();
}

inline void printConfigStatus() {
    GLOBAL_CTX.printConfigStatus();
}

// Helper function for updating and saving config values
inline bool updateAndSaveFloat(const char* key, float value) {
    bool success = CONFIG_MGR.putFloat(key, value);
    if (success) {
        GLOBAL_CTX.loadConfiguration(); // Reload to update struct
    }
    return success;
}

inline bool updateAndSaveInt(const char* key, int value) {
    bool success = CONFIG_MGR.putInt(key, value);
    if (success) {
        GLOBAL_CTX.loadConfiguration(); // Reload to update struct
    }
    return success;
}

inline bool updateAndSaveBool(const char* key, bool value) {
    bool success = CONFIG_MGR.putBool(key, value);
    if (success) {
        GLOBAL_CTX.loadConfiguration(); // Reload to update struct
    }
    return success;
}

#endif // GLOBALS_H
