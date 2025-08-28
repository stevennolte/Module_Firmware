#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Preferences.h>
#include "Arduino.h"

class ConfigManager {
private:
    Preferences prefs;
    const char* NAMESPACE = "esp32_config";
    
public:
    ConfigManager();
    ~ConfigManager();
    
    // Initialize the config manager
    bool begin();
    
    // Generic get/set methods for different data types
    String getString(const char* key, const String& defaultValue = "");
    bool putString(const char* key, const String& value);
    
    int getInt(const char* key, int defaultValue = 0);
    bool putInt(const char* key, int value);
    
    float getFloat(const char* key, float defaultValue = 0.0);
    bool putFloat(const char* key, float value);
    
    bool getBool(const char* key, bool defaultValue = false);
    bool putBool(const char* key, bool value);
    
    uint8_t getUChar(const char* key, uint8_t defaultValue = 0);
    bool putUChar(const char* key, uint8_t value);
    
    // Array helpers
    size_t getBytes(const char* key, void* buf, size_t maxLen);
    bool putBytes(const char* key, const void* value, size_t len);
    
    // Utility methods
    void clear();
    bool remove(const char* key);
    bool isKey(const char* key);
    
    // Migration helper - migrate from JSON config
    bool migrateFromJSON(const String& jsonString);
    
    // Load defaults if no config exists
    void loadDefaults();
};

#endif // CONFIG_MANAGER_H
