#include "ESPconfig.h"

ESPconfig::ESPconfig() : progCfg(), wifiCfg(), otaCfg() {}

uint8_t ESPconfig::getStrapping(){
    pinMode(4, INPUT);
    uint32_t measurement = analogReadMilliVolts(4);
    //TODO: add strapping logic
    return 1;
}

uint8_t ESPconfig::loadConfig(){
    // This method is now deprecated - configuration loading is handled by GlobalContext
    // Kept for compatibility, but recommend using GlobalContext::loadConfiguration()
    Serial.println("Warning: ESPconfig::loadConfig() is deprecated. Use GlobalContext instead.");
    progData.state = 1; // Mark as loaded
    return 1;
}

uint8_t ESPconfig::updateIP() {
    // This method is now deprecated - use GlobalContext::saveConfiguration()
    Serial.println("Warning: ESPconfig::updateIP() is deprecated. Use GlobalContext::saveConfiguration() instead.");
    return 1;
}

uint8_t ESPconfig::updateServer(){
    // This method is now deprecated - use GlobalContext::saveConfiguration()
    Serial.println("Warning: ESPconfig::updateServer() is deprecated. Use GlobalContext::saveConfiguration() instead.");
    return 1;
}

uint8_t ESPconfig::updateSteer(){
    // This method is now deprecated - use GlobalContext::saveConfiguration()
    Serial.println("Warning: ESPconfig::updateSteer() is deprecated. Use GlobalContext::saveConfiguration() instead.");
    return 1;
}

uint8_t ESPconfig::saveWASzero() {
    // This method is now deprecated - use GlobalContext::saveConfiguration()
    Serial.println("Warning: ESPconfig::saveWASzero() is deprecated. Use GlobalContext::saveConfiguration() instead.");
    return 1;
}
