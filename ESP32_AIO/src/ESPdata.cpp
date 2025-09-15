#include "ESPdata.h"

// Static member initialization
ESPdata* ESPdata::instance = nullptr;

ESPdata::ESPdata() {
    // Initialize Preferences in constructor
    
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
    if (!preferences.begin("agopen", false)) { // "agopen" namespace, read-write mode
        Serial.println("Failed to initialize preferences - NVS may not be initialized");
        // You could add additional error handling here if needed
    } else {
        Serial.println("Preferences initialized successfully");
    }
    program.bootMode = preferences.getUChar("bootMode", 0);
    wifi.ips[0] = preferences.getUChar("ip0", 192);
    wifi.ips[1] = preferences.getUChar("ip1", 168);
    wifi.ips[2] = preferences.getUChar("ip2", 5);
    wifi.ips[3] = preferences.getUChar("ip3", 11);

    program.bootcount = preferences.getULong("bootcount", 0);
    Serial.println("Boot count: " + String(program.bootcount));
    program.bootcount = program.bootcount + 1;
    preferences.putULong("bootcount", program.bootcount);
    
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
    // preferences.putFloat("Kp", 50.0); // Ensure Kp has a default value if not set   
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
    // Print entire configuration to serial monitor
    Serial.println("=== ESP32 Configuration ===");
    Serial.println("Program:");
    Serial.println("  Name: " + String(NAME));
    Serial.println("  Boot Mode: " + String(program.bootMode));
    Serial.println("  Boot Count: " + String(program.bootcount));
    Serial.println("  State: " + String(program.state));

    Serial.println("WiFi:");
    Serial.println("  IP: " + String(wifi.ips[0]) + "." + String(wifi.ips[1]) + "." + String(wifi.ips[2]) + "." + String(wifi.ips[3]));

    Serial.println("Steering:");
    Serial.println("  WAS Zero Angle: " + String(steer.wasZeroAngle));
    Serial.println("  WAS Offset: " + String(steer.steerOffset));
    Serial.println("  Gain P (Kp): " + String(steer.gainP));
    Serial.println("  High PWM: " + String(steer.highPWM));
    Serial.println("  Low PWM: " + String(steer.lowPWM));
    Serial.println("  Min PWM: " + String(steer.minPWM));
    Serial.println("  Counts Per Degree: " + String(steer.countsPerDeg));
    Serial.println("  Use ADS: " + String(steer.useADS ? "true" : "false"));
    Serial.println("  PID Input Filter: " + String(steer.pidInputFilt));
    Serial.println("  PID Output Filter: " + String(steer.pidOutputFilt));

    Serial.println("OTA/Server:");
    Serial.println("  Server IP: " + String(ota.ipAddr));
    Serial.println("  Server Port: " + String(ota.port));
    Serial.println("=== End Configuration ===");
    program.state = 1;
    return 1; // Success
}

bool ESPdata::setBootMode(uint8_t mode){
    program.bootMode = mode;
    preferences.putUChar("bootMode", program.bootMode);
    return true;
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
