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
    program.state = 2;
    // Set default IP if not found in preferences
    wifi.ips[0] = preferences.getUChar("ip0", 192);
    wifi.ips[1] = preferences.getUChar("ip1", 168);
    wifi.ips[2] = preferences.getUChar("ip2", 5);
    wifi.ips[3] = preferences.getUChar("ip3", 11);

    
    // Load WAS zero angle
    steer.wasZeroAngle = preferences.getFloat("wasZero", 0.0);

    // Load program name and verify
    String configName = preferences.getString("name", "ESP32_AIO");
    
    
    
    // Load PID values
    steer.pidInputFilt = preferences.getFloat("pidInputFilt", 0.1);
    steer.pidOutputFilt = preferences.getFloat("pidOutputFilt", 0.1);
    Serial.print("PID Input Filter: ");
    Serial.println(steer.pidInputFilt);
    Serial.print("PID Output Filter: ");
    Serial.println(steer.pidOutputFilt);

    // Load steering configuration
    steer.gainP = preferences.getFloat("Kp", 50.0);
    steer.highPWM = preferences.getUChar("highPWM", 255);
    steer.lowPWM = preferences.getUChar("lowPWM", 10);
    steer.minPWM = preferences.getUChar("minPWM", 5);
    steer.countsPerDeg = preferences.getFloat("countsPerDeg", 10.0);
    steer.steerOffset = preferences.getFloat("wasOffset", 0.0);
    steer.useADS = preferences.getBool("useADS", true);

    // Load server configuration
    ota.ipAddr = preferences.getUChar("serverAdr", 192);
    ota.port = preferences.getUShort("serverPort", 8080);

    program.state = 1;
    return 1; // Success
}

uint8_t ESPdata::saveConfig(){
    // Save all configuration to Preferences
    preferences.putUChar("ip0", wifi.ips[0]);
    preferences.putUChar("ip1", wifi.ips[1]);
    preferences.putUChar("ip2", wifi.ips[2]);
    preferences.putUChar("ip3", wifi.ips[3]);

    preferences.putFloat("wasZero", steer.wasZeroAngle);
    preferences.putString("name", program.name);

    // Save PID values
    preferences.putFloat("pidInputFilt", steer.pidInputFilt);
    preferences.putFloat("pidOutputFilt", steer.pidOutputFilt);

    // Save steering configuration
    preferences.putFloat("Kp", steer.gainP);
    preferences.putUChar("highPWM", steer.highPWM);
    preferences.putUChar("lowPWM", steer.lowPWM);
    preferences.putUChar("minPWM", steer.minPWM);
    preferences.putFloat("countsPerDeg", steer.countsPerDeg);
    preferences.putFloat("wasOffset", steer.steerOffset);
    preferences.putBool("useADS", steer.useADS);

    // Save server configuration
    preferences.putUChar("serverAdr", ota.ipAddr);
    preferences.putUShort("serverPort", ota.port);

    return 1; // Success
}

uint8_t ESPdata::updateIP() {
    preferences.putUChar("ip0", wifi.ips[0]);
    preferences.putUChar("ip1", wifi.ips[1]);
    preferences.putUChar("ip2", wifi.ips[2]);
    preferences.putUChar("ip3", wifi.ips[3]);
    Serial.println(F("Successfully updated IP address in Preferences"));
    return 1;
}

uint8_t ESPdata::updateServer(){
    preferences.putUChar("serverAdr", ota.ipAddr);
    preferences.putUShort("serverPort", ota.port);
    Serial.println(F("Successfully updated server config in Preferences"));
    return 1;
}

uint8_t ESPdata::updateSteer(){
    preferences.putFloat("Kp", steer.gainP);
    preferences.putUChar("highPWM", steer.highPWM);
    preferences.putUChar("lowPWM", steer.lowPWM);
    preferences.putUChar("minPWM", steer.minPWM);
    preferences.putFloat("countsPerDeg", steer.countsPerDeg);
    preferences.putFloat("wasOffset", steer.steerOffset);
    preferences.putBool("useADS", steer.useADS);
    preferences.putFloat("pidInputFilt", steer.pidInputFilt);
    preferences.putFloat("pidOutputFilt", steer.pidOutputFilt);
    Serial.println(F("Successfully updated steer config in Preferences"));
    return 1;
}

uint8_t ESPdata::saveWASzero(){
    preferences.putFloat("wasZero", steer.wasZeroAngle);
    Serial.println(F("Successfully updated WAS zero in Preferences"));
    return 1;
}
