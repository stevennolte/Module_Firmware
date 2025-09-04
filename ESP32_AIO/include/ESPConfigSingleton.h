#ifndef ESP_CONFIG_SINGLETON_H
#define ESP_CONFIG_SINGLETON_H

#include "ESPdata.h"

// Convenience macro for accessing the ESPconfig singleton
#define ESP_CONFIG ESPdata::getInstance()

// Quick access macros for frequently used sections
#define PROG_DATA ESP_CONFIG.progData
#define PROG_CFG ESP_CONFIG.progCfg
#define WIFI_CFG ESP_CONFIG.wifiCfg
#define STEER_CFG ESP_CONFIG.steerCfg
#define STEER_DATA ESP_CONFIG.steerData
#define GPS_CFG ESP_CONFIG.gpsCfg
#define GPS_DATA ESP_CONFIG.gpsData
#define JOYSTICK_DATA ESP_CONFIG.joystickData
#define GPIO_DEFS ESP_CONFIG.gpioDefs
#define I2C_DEFS ESP_CONFIG.i2cDefs

// Helper functions for common operations
inline void saveSteerConfig() {
    ESP_CONFIG.updateSteer();
}

inline void saveWASZero() {
    ESP_CONFIG.saveWASzero();
}

inline void saveIPConfig() {
    ESP_CONFIG.updateIP();
}

// Usage examples:
/*
// Instead of accessing global espConfig:
espConfig.steerData.targetSteerAngle = 45.0;
espConfig.gpsData.latitude = "1234.5678";

// Use the singleton pattern:
STEER_DATA.targetSteerAngle = 45.0;
GPS_DATA.latitude = "1234.5678";

// Or explicit singleton access:
ESPdata::getInstance().steerData.targetSteerAngle = 45.0;
ESP_CONFIG.gpsData.latitude = "1234.5678";

// Save configuration:
saveSteerConfig();      // Helper function
ESP_CONFIG.updateSteer(); // Direct method call
*/

#endif // ESP_CONFIG_SINGLETON_H
