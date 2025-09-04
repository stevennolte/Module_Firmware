#include "ESPdata.h"

// Static member initialization
ESPdata* ESPdata::instance = nullptr;

ESPdata::ESPdata() {
    // Initialize Preferences in constructor
    preferences.begin("agopen", false); // "agopen" namespace, read-write mode
}

// Singleton implementation
ESPdata& ESPdata::getInstance() {
    if (instance == nullptr) {
        instance = new ESPdata();
    }
    return *instance;
}

void ESPdata::destroyInstance() {
    if (instance != nullptr) {
        instance->preferences.end(); // Close preferences before destroying
        delete instance;
        instance = nullptr;
    }
}

uint8_t ESPdata::getStrapping(){
    pinMode(4, INPUT);
    uint32_t measurement = analogReadMilliVolts(4);
    //TODO: add strapping logic
    return 1;
}

uint8_t ESPdata::loadConfig(){
    data.prog.state = 2;
    // Set default IP if not found in preferences
    config.wifi.ips[0] = preferences.getUChar("ip0", 192);
    config.wifi.ips[1] = preferences.getUChar("ip1", 168);
    config.wifi.ips[2] = preferences.getUChar("ip2", 5);
    config.wifi.ips[3] = preferences.getUChar("ip3", 11);

    
    // Load WAS zero angle
    config.steer.wasZeroAngle = preferences.getFloat("wasZero", 0.0);

    // Load program name and verify
    String configName = preferences.getString("name", "ESP32_AIO");
    
    
    
    // Load PID values
    config.steer.pidInputFilt = preferences.getFloat("pidInputFilt", 0.1);
    config.steer.pidOutputFilt = preferences.getFloat("pidOutputFilt", 0.1);
    Serial.print("PID Input Filter: ");
    Serial.println(config.steer.pidInputFilt);
    Serial.print("PID Output Filter: ");
    Serial.println(config.steer.pidOutputFilt);

    // Load steering configuration
    config.steer.gainP = preferences.getFloat("Kp", 50.0);
    config.steer.highPWM = preferences.getUChar("highPWM", 255);
    config.steer.lowPWM = preferences.getUChar("lowPWM", 10);
    config.steer.minPWM = preferences.getUChar("minPWM", 5);
    config.steer.countsPerDeg = preferences.getFloat("countsPerDeg", 10.0);
    config.steer.steerOffset = preferences.getFloat("wasOffset", 0.0);
    config.steer.useADS = preferences.getBool("useADS", true);

    // Load server configuration
    config.ota.ipAddr = preferences.getUChar("serverAdr", 192);
    config.ota.port = preferences.getUShort("serverPort", 8080);

    data.prog.state = 1;
    return 1; // Success
}

uint8_t ESPdata::saveConfig(){
    // Save all configuration to Preferences
    preferences.putUChar("ip0", config.wifi.ips[0]);
    preferences.putUChar("ip1", config.wifi.ips[1]);
    preferences.putUChar("ip2", config.wifi.ips[2]);
    preferences.putUChar("ip3", config.wifi.ips[3]);

    preferences.putFloat("wasZero", config.steer.wasZeroAngle);
    preferences.putString("name", config.prog.name);

    // Save PID values
    preferences.putFloat("pidInputFilt", config.steer.pidInputFilt);
    preferences.putFloat("pidOutputFilt", config.steer.pidOutputFilt);

    // Save steering configuration
    preferences.putFloat("Kp", config.steer.gainP);
    preferences.putUChar("highPWM", config.steer.highPWM);
    preferences.putUChar("lowPWM", config.steer.lowPWM);
    preferences.putUChar("minPWM", config.steer.minPWM);
    preferences.putFloat("countsPerDeg", config.steer.countsPerDeg);
    preferences.putFloat("wasOffset", config.steer.steerOffset);
    preferences.putBool("useADS", config.steer.useADS);

    // Save server configuration
    preferences.putUChar("serverAdr", config.ota.ipAddr);
    preferences.putUShort("serverPort", config.ota.port);

    return 1; // Success
}

uint8_t ESPdata::updateIP() {
    preferences.putUChar("ip0", config.wifi.ips[0]);
    preferences.putUChar("ip1", config.wifi.ips[1]);
    preferences.putUChar("ip2", config.wifi.ips[2]);
    preferences.putUChar("ip3", config.wifi.ips[3]);
    Serial.println(F("Successfully updated IP address in Preferences"));
    return 1;
}

uint8_t ESPdata::updateServer(){
    preferences.putUChar("serverAdr", config.ota.ipAddr);
    preferences.putUShort("serverPort", config.ota.port);
    Serial.println(F("Successfully updated server config in Preferences"));
    return 1;
}

uint8_t ESPdata::updateSteer(){
    preferences.putFloat("Kp", config.steer.gainP);
    preferences.putUChar("highPWM", config.steer.highPWM);
    preferences.putUChar("lowPWM", config.steer.lowPWM);
    preferences.putUChar("minPWM", config.steer.minPWM);
    preferences.putFloat("countsPerDeg", config.steer.countsPerDeg);
    preferences.putFloat("wasOffset", config.steer.steerOffset);
    preferences.putBool("useADS", config.steer.useADS);
    preferences.putFloat("pidInputFilt", config.steer.pidInputFilt);
    preferences.putFloat("pidOutputFilt", config.steer.pidOutputFilt);
    Serial.println(F("Successfully updated steer config in Preferences"));
    return 1;
}

uint8_t ESPdata::saveWASzero(){
    preferences.putFloat("wasZero", config.steer.wasZeroAngle);
    Serial.println(F("Successfully updated WAS zero in Preferences"));
    return 1;
}
