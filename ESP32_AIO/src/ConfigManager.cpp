#include "ConfigManager.h"
#include "ArduinoJson.h"
#include "LittleFS.h"

ConfigManager::ConfigManager() {
}

ConfigManager::~ConfigManager() {
    prefs.end();
}

bool ConfigManager::begin() {
    return prefs.begin(NAMESPACE, false); // false = read/write mode
}

String ConfigManager::getString(const char* key, const String& defaultValue) {
    return prefs.getString(key, defaultValue);
}

bool ConfigManager::putString(const char* key, const String& value) {
    return prefs.putString(key, value) > 0;
}

int ConfigManager::getInt(const char* key, int defaultValue) {
    return prefs.getInt(key, defaultValue);
}

bool ConfigManager::putInt(const char* key, int value) {
    return prefs.putInt(key, value) > 0;
}

float ConfigManager::getFloat(const char* key, float defaultValue) {
    return prefs.getFloat(key, defaultValue);
}

bool ConfigManager::putFloat(const char* key, float value) {
    return prefs.putFloat(key, value) > 0;
}

bool ConfigManager::getBool(const char* key, bool defaultValue) {
    return prefs.getBool(key, defaultValue);
}

bool ConfigManager::putBool(const char* key, bool value) {
    return prefs.putBool(key, value) > 0;
}

uint8_t ConfigManager::getUChar(const char* key, uint8_t defaultValue) {
    return prefs.getUChar(key, defaultValue);
}

bool ConfigManager::putUChar(const char* key, uint8_t value) {
    return prefs.putUChar(key, value) > 0;
}

size_t ConfigManager::getBytes(const char* key, void* buf, size_t maxLen) {
    return prefs.getBytes(key, buf, maxLen);
}

bool ConfigManager::putBytes(const char* key, const void* value, size_t len) {
    return prefs.putBytes(key, value, len) > 0;
}

void ConfigManager::clear() {
    prefs.clear();
}

bool ConfigManager::remove(const char* key) {
    return prefs.remove(key);
}

bool ConfigManager::isKey(const char* key) {
    return prefs.isKey(key);
}

bool ConfigManager::migrateFromJSON(const String& jsonString) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    if (error) {
        Serial.println("Failed to parse JSON for migration");
        return false;
    }
    
    // Migrate basic settings
    if (doc.containsKey("Name")) {
        putString("name", doc["Name"].as<String>());
    }
    
    // Migrate IP address array
    if (doc.containsKey("ipAddress")) {
        for (int i = 0; i < 4; i++) {
            String key = "ip_" + String(i);
            putUChar(key.c_str(), doc["ipAddress"][i].as<uint8_t>());
        }
    }
    
    // Migrate WiFi settings
    if (doc.containsKey("ssids")) {
        for (int i = 0; i < 4; i++) {
            String ssidKey = "ssid_" + String(i);
            String passKey = "pass_" + String(i);
            if (i < doc["ssids"].size()) {
                putString(ssidKey.c_str(), doc["ssids"][i].as<String>());
            }
            if (i < doc["passwords"].size()) {
                putString(passKey.c_str(), doc["passwords"][i].as<String>());
            }
        }
    }
    
    // Migrate steering settings
    if (doc.containsKey("Kp")) putFloat("kp", doc["Kp"]);
    if (doc.containsKey("lowPWM")) putUChar("low_pwm", doc["lowPWM"]);
    if (doc.containsKey("highPWM")) putUChar("high_pwm", doc["highPWM"]);
    if (doc.containsKey("minPWM")) putUChar("min_pwm", doc["minPWM"]);
    if (doc.containsKey("wasOffset")) putInt("was_offset", doc["wasOffset"]);
    if (doc.containsKey("countsPerDeg")) putFloat("counts_per_deg", doc["countsPerDeg"]);
    if (doc.containsKey("useADS")) putBool("use_ads", doc["useADS"]);
    if (doc.containsKey("wasZero")) putFloat("was_zero", doc["wasZero"]);
    
    // Migrate PID filter settings
    if (doc.containsKey("pidInputFilt")) putFloat("pid_input_filt", doc["pidInputFilt"]);
    if (doc.containsKey("pidOutputFilt")) putFloat("pid_output_filt", doc["pidOutputFilt"]);
    
    Serial.println("Successfully migrated JSON config to Preferences");
    return true;
}

void ConfigManager::loadDefaults() {
    Serial.println("Loading default configuration...");
    
    // Default program settings
    putString("name", "ESP32_AIO");
    
    // Default network settings
    putUChar("ip_0", 192);
    putUChar("ip_1", 168);
    putUChar("ip_2", 5);
    putUChar("ip_3", 126);
    
    // Default WiFi networks
    putString("ssid_0", "NOLTE_FARM");
    putString("pass_0", "DontLoseMoney89");
    putString("ssid_1", "FERT");
    putString("pass_1", "Fert504!");
    putString("ssid_2", "SSEI");
    putString("pass_2", "Nd14il!la");
    putString("ssid_3", "");
    putString("pass_3", "");
    
    // Default steering settings
    putFloat("kp", 9.0);
    putUChar("low_pwm", 52);
    putUChar("high_pwm", 156);
    putUChar("min_pwm", 3);
    putInt("was_offset", 768);
    putFloat("counts_per_deg", 110.0);
    putBool("use_ads", true);
    putFloat("was_zero", 0.0);
    
    // Default PID filter settings
    putFloat("pid_input_filt", 0.9);
    putFloat("pid_output_filt", 0.9);
    
    // Default GPS settings
    putBool("external_gps", false);
    
    Serial.println("Default configuration loaded");
}
