#include "GlobalContext.h"
#include "Version.h"
#include "LittleFS.h"
#include "ArduinoJson.h"

// Static member initialization
GlobalContext* GlobalContext::instance = nullptr;

GlobalContext::GlobalContext() {
    // Private constructor
}

GlobalContext& GlobalContext::getInstance() {
    if (instance == nullptr) {
        instance = new GlobalContext();
    }
    return *instance;
}

void GlobalContext::destroy() {
    if (instance != nullptr) {
        delete instance;
        instance = nullptr;
    }
}

bool GlobalContext::initialize() {
    Serial.println("Initializing Global Context...");
    
    // Initialize the config manager
    if (!configManager.begin()) {
        Serial.println("Failed to initialize ConfigManager");
        return false;
    }
    
    // Load configuration
    if (!loadConfiguration()) {
        Serial.println("Failed to load configuration");
        return false;
    }
    
    Serial.println("Global Context initialized successfully");
    return true;
}

bool GlobalContext::loadConfiguration() {
    Serial.println("Loading configuration...");
    
    // Check if we need to migrate from JSON
    bool needsMigration = false;
    if (!configManager.isKey("name")) {
        Serial.println("No existing Preferences config found, checking for JSON config...");
        needsMigration = true;
    }
    
    if (needsMigration) {
        // Try to migrate from existing JSON config
        if (LittleFS.begin(true)) {
            File file = LittleFS.open("/config.json", "r");
            if (file) {
                String jsonString;
                while (file.available()) {
                    jsonString += char(file.read());
                }
                file.close();
                
                Serial.println("Found JSON config, migrating to Preferences...");
                if (configManager.migrateFromJSON(jsonString)) {
                    Serial.println("Migration successful");
                } else {
                    Serial.println("Migration failed, using defaults");
                    configManager.loadDefaults();
                }
            } else {
                Serial.println("No JSON config found, loading defaults");
                configManager.loadDefaults();
            }
        } else {
            Serial.println("LittleFS not available, loading defaults");
            configManager.loadDefaults();
        }
    }
    
    // Load configuration into ESPconfig structure
    
    // Program configuration
    String name = configManager.getString("name", "ESP32_AIO");
    strlcpy(config.progCfg.name, name.c_str(), sizeof(config.progCfg.name));
    
    // Parse version from VERSION define
    char version[64];
    strcpy(version, VERSION);
    char *token = strtok(version, ".");
    int i = 0;
    while (token != NULL && i < 3) {
        config.progCfg.version[i] = atoi(token);
        i++;
        token = strtok(NULL, ".");
    }
    
    // Network configuration
    for (int i = 0; i < 4; i++) {
        String key = "ip_" + String(i);
        config.wifiCfg.ips[i] = configManager.getUChar(key.c_str(), 192);
    }
    
    // WiFi networks (keeping existing arrays for compatibility)
    // Note: This maintains compatibility with the existing const char* arrays
    // while allowing updates through the config system
    
    // Steering configuration
    config.steerCfg.gainP = configManager.getFloat("kp", 9.0);
    config.steerCfg.lowPWM = configManager.getUChar("low_pwm", 52);
    config.steerCfg.highPWM = configManager.getUChar("high_pwm", 156);
    config.steerCfg.minPWM = configManager.getUChar("min_pwm", 3);
    config.steerCfg.steerOffset = configManager.getInt("was_offset", 768);
    config.steerCfg.countsPerDeg = configManager.getFloat("counts_per_deg", 110.0);
    config.steerCfg.useADS = configManager.getBool("use_ads", true);
    config.steerData.wasZeroAngle = configManager.getFloat("was_zero", 0.0);
    
    // PID filter settings
    config.steerCfg.pidInputFilt = configManager.getFloat("pid_input_filt", 0.9);
    config.steerCfg.pidOutputFilt = configManager.getFloat("pid_output_filt", 0.9);
    
    // GPS configuration
    config.gpsCfg.externalGPS = configManager.getBool("external_gps", false);
    
    // Set initial state
    config.progData.state = 1; // Loaded successfully
    
    Serial.println("Configuration loaded successfully");
    return true;
}

bool GlobalContext::saveConfiguration() {
    Serial.println("Saving configuration...");
    
    // Save current config values to preferences
    configManager.putString("name", String(config.progCfg.name));
    
    // Network settings
    for (int i = 0; i < 4; i++) {
        String key = "ip_" + String(i);
        configManager.putUChar(key.c_str(), config.wifiCfg.ips[i]);
    }
    
    // Steering settings
    configManager.putFloat("kp", config.steerCfg.gainP);
    configManager.putUChar("low_pwm", config.steerCfg.lowPWM);
    configManager.putUChar("high_pwm", config.steerCfg.highPWM);
    configManager.putUChar("min_pwm", config.steerCfg.minPWM);
    configManager.putInt("was_offset", config.steerCfg.steerOffset);
    configManager.putFloat("counts_per_deg", config.steerCfg.countsPerDeg);
    configManager.putBool("use_ads", config.steerCfg.useADS);
    configManager.putFloat("was_zero", config.steerData.wasZeroAngle);
    
    // PID filter settings
    configManager.putFloat("pid_input_filt", config.steerCfg.pidInputFilt);
    configManager.putFloat("pid_output_filt", config.steerCfg.pidOutputFilt);
    
    // GPS settings
    configManager.putBool("external_gps", config.gpsCfg.externalGPS);
    
    Serial.println("Configuration saved successfully");
    return true;
}

bool GlobalContext::resetToDefaults() {
    Serial.println("Resetting configuration to defaults...");
    
    configManager.clear();
    configManager.loadDefaults();
    
    return loadConfiguration();
}

void GlobalContext::printConfigStatus() {
    Serial.println("=== Configuration Status ===");
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
    Serial.printf("External GPS: %s\n", config.gpsCfg.externalGPS ? "Yes" : "No");
    Serial.printf("Steer Gain: %.2f\n", config.steerCfg.gainP);
    Serial.printf("WAS Zero: %.2f\n", config.steerData.wasZeroAngle);
    Serial.println("============================");
}

// Global accessor function
GlobalContext& getGlobalContext() {
    return GlobalContext::getInstance();
}
