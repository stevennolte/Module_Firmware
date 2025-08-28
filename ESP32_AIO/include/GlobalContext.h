#ifndef GLOBAL_CONTEXT_H
#define GLOBAL_CONTEXT_H

#include "ESPconfig.h"
#include "ConfigManager.h"

class GlobalContext {
private:
    static GlobalContext* instance;
    GlobalContext(); // Private constructor for singleton
    
public:
    // Singleton access
    static GlobalContext& getInstance();
    static void destroy();
    
    // Core components
    ESPconfig config;
    ConfigManager configManager;
    
    // Initialization
    bool initialize();
    
    // Config management with Preferences
    bool loadConfiguration();
    bool saveConfiguration();
    bool resetToDefaults();
    
    // Convenient accessors for commonly used data
    inline ESPconfig& getConfig() { return config; }
    inline ConfigManager& getConfigManager() { return configManager; }
    
    // Quick access to frequently used config sections
    inline auto& getProgramData() { return config.progData; }
    inline auto& getProgramConfig() { return config.progCfg; }
    inline auto& getWifiConfig() { return config.wifiCfg; }
    inline auto& getSteerConfig() { return config.steerCfg; }
    inline auto& getSteerData() { return config.steerData; }
    inline auto& getGPSConfig() { return config.gpsCfg; }
    inline auto& getGPSData() { return config.gpsData; }
    inline auto& getJoystickData() { return config.joystickData; }
    
    // Debug and status
    void printConfigStatus();
};

// Global accessor function for convenience
GlobalContext& getGlobalContext();

#endif // GLOBAL_CONTEXT_H
