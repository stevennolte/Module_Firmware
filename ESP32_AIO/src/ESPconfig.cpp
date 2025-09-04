#include "ESPconfig.h"

// Static member initialization
ESPconfig* ESPconfig::instance = nullptr;

ESPconfig::ESPconfig() : progCfg(), wifiCfg(), otaCfg() {
    // Initialize Preferences in constructor
    preferences.begin("agopen", false); // "agopen" namespace, read-write mode
}

// Singleton implementation
ESPconfig& ESPconfig::getInstance() {
    if (instance == nullptr) {
        instance = new ESPconfig();
    }
    return *instance;
}

void ESPconfig::destroyInstance() {
    if (instance != nullptr) {
        instance->preferences.end(); // Close preferences before destroying
        delete instance;
        instance = nullptr;
    }
}

uint8_t ESPconfig::getStrapping(){
    pinMode(4, INPUT);
    uint32_t measurement = analogReadMilliVolts(4);
    //TODO: add strapping logic
    return 1;
}

uint8_t ESPconfig::loadConfig(){
    progData.state = 2;
    
    // Set default IP if not found in preferences
    wifiCfg.ips[0] = preferences.getUChar("ip0", 192);
    wifiCfg.ips[1] = preferences.getUChar("ip1", 168);
    wifiCfg.ips[2] = preferences.getUChar("ip2", 5);
    wifiCfg.ips[3] = preferences.getUChar("ip3", 11);
    
    // Load WAS zero angle
    steerData.wasZeroAngle = preferences.getFloat("wasZero", 0.0);
    
    // Load program name and verify
    String configName = preferences.getString("name", "");
    strlcpy(progCfg.name, configName.c_str(), sizeof(progCfg.name));
    
    if (configName != NAME && configName.length() > 0){
        return 5; // Name mismatch
    }
    
    // Parse version from VERSION define
    char version[64];
    strcpy(version, VERSION);
    char *token = strtok(version, ".");
    int i = 0;
    while (token != NULL) {
        int intValue = atoi(token);
        switch (i){
        case 0:
            progCfg.version[0] = intValue;
            break;
        case 1:
            progCfg.version[1] = intValue;
            break;
        case 2:
            progCfg.version[2] = intValue;
            break;
        }
        i++;
        token = strtok(NULL, ".");
    }
    
    // Load PID values
    steerCfg.pidInputFilt = preferences.getFloat("pidInputFilt", 0.1);
    steerCfg.pidOutputFilt = preferences.getFloat("pidOutputFilt", 0.1);
    Serial.print("PID Input Filter: ");
    Serial.println(steerCfg.pidInputFilt);
    Serial.print("PID Output Filter: ");
    Serial.println(steerCfg.pidOutputFilt);
    
    // Load steering configuration
    steerCfg.gainP = preferences.getFloat("Kp", 50.0);
    steerCfg.highPWM = preferences.getUChar("highPWM", 255);
    steerCfg.lowPWM = preferences.getUChar("lowPWM", 10);
    steerCfg.minPWM = preferences.getUChar("minPWM", 5);
    steerCfg.countsPerDeg = preferences.getFloat("countsPerDeg", 10.0);
    steerCfg.steerOffset = preferences.getFloat("wasOffset", 0.0);
    steerCfg.useADS = preferences.getBool("useADS", true);
    
    // Load server configuration
    otaCfg.ipAddr = preferences.getUChar("serverAdr", 192);
    otaCfg.port = preferences.getUShort("serverPort", 8080);
    
    progData.state = 1;
    return 1; // Success
}

uint8_t ESPconfig::saveConfig(){
    // Save all configuration to Preferences
    preferences.putUChar("ip0", wifiCfg.ips[0]);
    preferences.putUChar("ip1", wifiCfg.ips[1]);
    preferences.putUChar("ip2", wifiCfg.ips[2]);
    preferences.putUChar("ip3", wifiCfg.ips[3]);
    
    preferences.putFloat("wasZero", steerData.wasZeroAngle);
    preferences.putString("name", progCfg.name);
    
    // Save PID values
    preferences.putFloat("pidInputFilt", steerCfg.pidInputFilt);
    preferences.putFloat("pidOutputFilt", steerCfg.pidOutputFilt);
    
    // Save steering configuration
    preferences.putFloat("Kp", steerCfg.gainP);
    preferences.putUChar("highPWM", steerCfg.highPWM);
    preferences.putUChar("lowPWM", steerCfg.lowPWM);
    preferences.putUChar("minPWM", steerCfg.minPWM);
    preferences.putFloat("countsPerDeg", steerCfg.countsPerDeg);
    preferences.putFloat("wasOffset", steerCfg.steerOffset);
    preferences.putBool("useADS", steerCfg.useADS);
    
    // Save server configuration
    preferences.putUChar("serverAdr", otaCfg.ipAddr);
    preferences.putUShort("serverPort", otaCfg.port);
    
    return 1; // Success
}

uint8_t ESPconfig::updateIP() {
    preferences.putUChar("ip0", wifiCfg.ips[0]);
    preferences.putUChar("ip1", wifiCfg.ips[1]);
    preferences.putUChar("ip2", wifiCfg.ips[2]);
    preferences.putUChar("ip3", wifiCfg.ips[3]);
    Serial.println(F("Successfully updated IP address in Preferences"));
    return 1;
}

uint8_t ESPconfig::updateServer(){
    preferences.putUChar("serverAdr", otaCfg.ipAddr);
    preferences.putUShort("serverPort", otaCfg.port);
    Serial.println(F("Successfully updated server config in Preferences"));
    return 1;
}

uint8_t ESPconfig::updateSteer(){
    preferences.putFloat("Kp", steerCfg.gainP);
    preferences.putUChar("highPWM", steerCfg.highPWM);
    preferences.putUChar("lowPWM", steerCfg.lowPWM);
    preferences.putUChar("minPWM", steerCfg.minPWM);
    preferences.putFloat("countsPerDeg", steerCfg.countsPerDeg);
    preferences.putFloat("wasOffset", steerCfg.steerOffset);
    preferences.putBool("useADS", steerCfg.useADS);
    preferences.putFloat("pidInputFilt", steerCfg.pidInputFilt);
    preferences.putFloat("pidOutputFilt", steerCfg.pidOutputFilt);
    Serial.println(F("Successfully updated steer config in Preferences"));
    return 1;
}

uint8_t ESPconfig::saveWASzero(){
    preferences.putFloat("wasZero", steerData.wasZeroAngle);
    Serial.println(F("Successfully updated WAS zero in Preferences"));
    return 1;
}
