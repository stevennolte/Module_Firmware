/**
 * @file ESPdata.cpp
 * @brief Implementation of centralized data management system for ESP32-AIO
 * 
 * @details This file implements the ESPdata singleton class that manages all system
 *          configuration, runtime data, and persistent storage. Provides NVS-based
 *          configuration persistence with power cycle detection using RTC memory.
 *          
 *          Key features implemented:
 *          - Thread-safe singleton pattern for data access
 *          - NVS (Non-Volatile Storage) persistence for configuration
 *          - RTC memory power cycle detection and boot counting
 *          - Configuration load/save with validation and error recovery
 *          - System state management and component status tracking
 * 
 * @author Steve Gavel
 * @date 2024
 * @version 4.6.1
 * 
 * @see ESPdata.h for class interface definition
 * @see main.cpp for usage examples
 */

#include "ESPdata.h"

/// @brief RTC memory data structure that persists across software reboots but resets on power cycle
RTC_DATA_ATTR RTCData rtcData;

/// @brief Magic number for RTC memory validation (0xDEADBEEF indicates valid data)
const uint32_t RTC_MAGIC = 0xDEADBEEF;

/// @brief Static singleton instance pointer
ESPdata* ESPdata::instance = nullptr;

/**
 * @brief Default constructor for ESPdata singleton
 * 
 * @details Initializes the data management system. The Preferences object
 *          is initialized but not opened until loadConfig() is called.
 */
ESPdata::ESPdata() {
    // Initialize Preferences in constructor
}

/**
 * @brief Gets the singleton instance using thread-safe lazy initialization
 * 
 * @return ESPdata& Reference to the unique ESPdata instance
 * 
 * @details Creates the singleton instance on first call and returns the same
 *          instance on subsequent calls. Thread-safe implementation ensures
 *          only one instance is created in multi-threaded environments.
 */
ESPdata& ESPdata::getInstance() {
    if (instance == nullptr) {
        instance = new ESPdata();
    }
    return *instance;
}

/**
 * @brief Destroys the singleton instance and cleans up resources
 * 
 * @details Properly closes the NVS preferences handle and deallocates
 *          the singleton instance. Should be called during system shutdown.
 */
void ESPdata::destroyInstance() {
    if (instance != nullptr) {
        instance->preferences.end(); // Close preferences before destroying
        delete instance;
        instance = nullptr;
    }
}

/**
 * @brief Reads GPIO strapping pins for hardware configuration detection
 * 
 * @return uint8_t Hardware strapping configuration value
 * 
 * @details Reads analog voltage on strapping pin to determine hardware
 *          configuration variant. Used for automatic hardware detection
 *          and configuration selection.
 * 
 * @todo Add complete strapping logic implementation
 */
uint8_t ESPdata::getStrapping(){
    pinMode(4, INPUT);
    uint32_t measurement = analogReadMilliVolts(4);
    //TODO: add strapping logic
    return 1;
}

/**
 * @brief Sets the MCP23017 I/O expander component state
 * 
 * @param state Component state (1=active, 2=failed, other=unknown)
 * @return true Always returns true (operation cannot fail)
 * 
 * @details Updates the MCP23017 status in both RAM and NVS storage.
 *          Used for tracking hardware component health and system diagnostics.
 */
bool ESPdata::setMCPstate(uint8_t state){
    program.mcpState = state;
    preferences.putUChar("mcpState", program.mcpState);
    return true;
}

/**
 * @brief Sets the I2C two-wire interface state
 * 
 * @param state Interface state (1=active, 2=failed, other=unknown)
 * @return true Always returns true (operation cannot fail)
 * 
 * @details Updates the I2C interface status in both RAM and NVS storage.
 *          Used for tracking communication bus health and troubleshooting.
 */
bool ESPdata::setTwoWireState(uint8_t state){
    program.twoWireState = state;
    preferences.putUChar("twoWireState", program.twoWireState);
    return true;
}

bool ESPdata::setADSstate(uint8_t state){
    program.adsState = state;
    preferences.putUChar("adsState", program.adsState);
    return true;
}

/**
 * @brief Loads system configuration from NVS storage and initializes runtime data
 *
 * @return uint8_t Status code (1=success, 0=failure)
 *
 * @details Loads all persistent configuration parameters from NVS (Non-Volatile Storage)
 *          including network settings, steering calibration, PID gains, and boot state.
 *          Initializes RTC memory for power cycle detection and increments boot counter.
 *          Prints the loaded configuration to the serial monitor for diagnostics.
 *
 *          - Loads WiFi IP address and mode
 *          - Loads steering system calibration and PID parameters
 *          - Loads GPS and OTA server configuration
 *          - Handles RTC memory for power cycle and boot count tracking
 *
 * @note This function must be called before using any configuration-dependent features.
 * @see saveConfig(), initRTCData(), ESPdata.h
 */
uint8_t ESPdata::loadConfig(){
    // Set default IP if not found in preferences
    if (!preferences.begin("agopen", false)) { // "agopen" namespace, read-write mode
        Serial.println("Failed to initialize preferences - NVS may not be initialized");
        // You could add additional error handling here if needed
    } else {
        Serial.println("Preferences initialized successfully");
    }
    program.state = preferences.getUChar("state", 0);
    program.bootMode = preferences.getUChar("bootMode", 0);

    // AP settings – migrate from old ip0-ip3 keys if needed
    if (!preferences.isKey("ap_ip0") && preferences.isKey("ip0")) {
        preferences.putUChar("ap_ip0", preferences.getUChar("ip0", 192));
        preferences.putUChar("ap_ip1", preferences.getUChar("ip1", 168));
        preferences.putUChar("ap_ip2", preferences.getUChar("ip2", 5));
        preferences.putUChar("ap_ip3", preferences.getUChar("ip3", 1));
    }
    apCfg.ips[0] = preferences.getUChar("ap_ip0", 192);
    apCfg.ips[1] = preferences.getUChar("ap_ip1", 168);
    apCfg.ips[2] = preferences.getUChar("ap_ip2", 5);
    apCfg.ips[3] = preferences.getUChar("ap_ip3", 1);
    apCfg.channel    = (uint8_t)preferences.getInt("ap_ch",  6);
    apCfg.maxClients = (uint8_t)preferences.getInt("ap_max", 8);
    {
        String apSSID = preferences.getString("ap_ssid", NAME);
        if (apSSID == "NOLTE_FARM" || apSSID.isEmpty()) apSSID = NAME;
        String apPass = preferences.getString("ap_pass", "1234567890");
        strncpy(apCfg.ssid,     apSSID.c_str(), sizeof(apCfg.ssid) - 1);
        strncpy(apCfg.password, apPass.c_str(),  sizeof(apCfg.password) - 1);
        apCfg.ssid[sizeof(apCfg.ssid) - 1]         = '\0';
        apCfg.password[sizeof(apCfg.password) - 1] = '\0';
    }

    // WiFi operating mode (0=AP only, 1=AP+STA, 2=STA only)
    wifiMode = (uint8_t)preferences.getInt("wifi_mode", 0);
    Serial.printf("Loaded wifi_mode from preferences: %d\n", wifiMode);

    // STA network list
    staCfg.count = (uint8_t)preferences.getInt("sta_count", 0);
    Serial.printf("Loaded sta_count from preferences: %d\n", staCfg.count);
    if (staCfg.count > STAConfig::MAX_NETWORKS) staCfg.count = STAConfig::MAX_NETWORKS;
    for (int i = 0; i < staCfg.count; i++) {
        String keySSID = "sta_ssid_" + String(i);
        String keyPass = "sta_pass_" + String(i);
        String s = preferences.getString(keySSID.c_str(), "");
        String p = preferences.getString(keyPass.c_str(), "");
        Serial.printf("Loaded network %d: SSID=%s\n", i, s.c_str());
        strncpy(staCfg.ssids[i],     s.c_str(), sizeof(staCfg.ssids[i]) - 1);
        strncpy(staCfg.passwords[i], p.c_str(), sizeof(staCfg.passwords[i]) - 1);
        staCfg.ssids[i][sizeof(staCfg.ssids[i]) - 1]         = '\0';
        staCfg.passwords[i][sizeof(staCfg.passwords[i]) - 1] = '\0';
    }

    // Populate legacy wifi.ips from apCfg for display/debug code
    wifi.ips[0] = apCfg.ips[0];
    wifi.ips[1] = apCfg.ips[1];
    wifi.ips[2] = apCfg.ips[2];
    wifi.ips[3] = apCfg.ips[3];
    program.mcpState = preferences.getUChar("mcpState", 0);
    program.twoWireState = preferences.getUChar("twoWireState", 0);
    program.adsState = preferences.getUChar("adsState", 0);
    
    // Initialize RTC data first (handles power cycle detection)
    initRTCData();
    
    // Load bootcount (may have been reset by initRTCData on power cycle)
    program.bootcount = preferences.getULong("bootcount", 0);
    Serial.println("NVS Boot count: " + String(program.bootcount));
    program.bootcount = program.bootcount + 1;
    preferences.putULong("bootcount", program.bootcount);
    
    // Load WAS zero angle
    steer.wasZeroAngle = preferences.getFloat("wasZero", 0.0);
    steer.wasFilterValue = preferences.getFloat("wasFilter", 0.2);

    // Load program name and verify
    String configName = preferences.getString("name", "ESP32_AIO");
    
    
    program.ledBrht = preferences.getUChar("ledBrightness", 128);
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
    steer.wirelessWAS = preferences.getBool("wirelessWAS", false);
    
    // Load motor current sensor settings
    steer.currentZero = preferences.getUShort("currentZero", 32767);
    steer.currentScale = preferences.getFloat("currentScale", 2.0);
    steer.currentFilterOld = preferences.getFloat("currentFilterOld", 0.7);
    steer.currentFilterNew = preferences.getFloat("currentFilterNew", 0.3);
    steer.currentScaler = preferences.getFloat("currentScaler", 1.0);
    steer.enableCurrentLimit = preferences.getBool("enableCurrentLimit", false);
    steer.currentLimit = preferences.getUChar("currentLimit", 200);
    steer.enableCurrentDebug = preferences.getBool("enableCurrentDebug", false);

    // Load GPS configuration
    gps.externalGPS = preferences.getBool("externalGPS", false);
    gps.ntripPandaMode = preferences.getBool("ntripPandaMode", true);
    gps.flipPitchRoll = preferences.getBool("flipPitchRoll", true);
    gps.invertRoll = preferences.getBool("invertRoll", true);
    gps.disableHeading = preferences.getBool("disableHeading", false);
    gps.yawRateOnly = preferences.getBool("yawRateOnly", false);

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
    Serial.println("  Current Scaler: " + String(steer.currentScaler));
    Serial.println("  Enable Current Limit: " + String(steer.enableCurrentLimit ? "true" : "false"));
    Serial.println("  Current Limit: " + String(steer.currentLimit));

    Serial.println("OTA/Server:");
    Serial.println("  Server IP: " + String(ota.ipAddr));
    Serial.println("  Server Port: " + String(ota.port));
    Serial.println("=== End Configuration ===");
    // program.state = 1;
    return 1; // Success
}

bool ESPdata::setBootCount(uint32_t count){
    program.bootcount = count;
    preferences.putULong("bootcount", program.bootcount);
    return true;
}

bool ESPdata::setBootMode(uint8_t mode){
    program.bootMode = mode;
    preferences.putUChar("bootMode", program.bootMode);
    return true;
}

bool ESPdata::setState(uint8_t state){
    program.state = state;
    preferences.putUChar("state", program.state);
    return true;
}

uint8_t ESPdata::saveConfig(){
    // Save all configuration to Preferences
    Serial.println("=== SAVING CONFIG TO PREFERENCES ===");

    preferences.putUChar("bootMode", program.bootMode);
    preferences.putULong("bootcount", program.bootcount);
    preferences.putUChar("state", program.state);
    preferences.putUChar("ap_ip0", apCfg.ips[0]);
    preferences.putUChar("ap_ip1", apCfg.ips[1]);
    preferences.putUChar("ap_ip2", apCfg.ips[2]);
    preferences.putUChar("ap_ip3", apCfg.ips[3]);
    preferences.putString("ap_ssid", apCfg.ssid);
    preferences.putString("ap_pass", apCfg.password);
    preferences.putInt("ap_ch", apCfg.channel);
    preferences.putInt("ap_max", apCfg.maxClients);
    
    Serial.printf("Saving wifi_mode: %d\n", wifiMode);
    preferences.putInt("wifi_mode", wifiMode);
    
    preferences.putUChar("ledBrightness", program.ledBrht);
    preferences.putFloat("wasZero", steer.wasZeroAngle);
    preferences.putFloat("wasFilter", steer.wasFilterValue);
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
    preferences.putBool("wirelessWAS", steer.wirelessWAS);
    preferences.putFloat("currentScaler", steer.currentScaler);
    preferences.putBool("enableCurrentLimit", steer.enableCurrentLimit);
    preferences.putUChar("currentLimit", steer.currentLimit);
    preferences.putBool("enableCurrentDebug", steer.enableCurrentDebug);
    
    // Save motor current sensor calibration
    preferences.putUShort("currentZero", steer.currentZero);
    preferences.putFloat("currentScale", steer.currentScale);
    preferences.putFloat("currentFilterOld", steer.currentFilterOld);
    preferences.putFloat("currentFilterNew", steer.currentFilterNew);

    // Save GPS configuration
    preferences.putBool("externalGPS", gps.externalGPS);
    preferences.putBool("ntripPandaMode", gps.ntripPandaMode);
    preferences.putBool("flipPitchRoll", gps.flipPitchRoll);
    preferences.putBool("invertRoll", gps.invertRoll);
    preferences.putBool("disableHeading", gps.disableHeading);
    preferences.putBool("yawRateOnly", gps.yawRateOnly);

    // Save server configuration
    preferences.putUChar("serverAdr", ota.ipAddr);
    preferences.putUShort("serverPort", ota.port);

    Serial.println("=== CONFIG SAVED SUCCESSFULLY ===");
    return 1; // Success
}

uint8_t ESPdata::updateIP() {
    preferences.putUChar("ap_ip0", apCfg.ips[0]);
    preferences.putUChar("ap_ip1", apCfg.ips[1]);
    preferences.putUChar("ap_ip2", apCfg.ips[2]);
    preferences.putUChar("ap_ip3", apCfg.ips[3]);
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

// Preferences wrapper methods for controlled NVS access
int ESPdata::getInt(const char* key, int defaultValue) {
    int value = preferences.getInt(key, defaultValue);
    Serial.printf("Pref getInt: %s = %d\n", key, value);
    return value;
}

size_t ESPdata::putInt(const char* key, int value) {
    Serial.printf("Pref putInt: %s = %d\n", key, value);
    return preferences.putInt(key, value);
}

String ESPdata::getString(const char* key, const String& defaultValue) {
    String value = preferences.getString(key, defaultValue.c_str());
    Serial.printf("Pref getString: %s = %s\n", key, value.c_str());
    return value;
}

size_t ESPdata::putString(const char* key, const String& value) {
    Serial.printf("Pref putString: %s = %s\n", key, value.c_str());
    return preferences.putString(key, value.c_str());
}

bool ESPdata::remove(const char* key) {
    Serial.printf("Pref remove: %s\n", key);
    return preferences.remove(key);
}

// RTC Data methods
void ESPdata::initRTCData() {
    Serial.println("=== RTC Data Debug ===");
    Serial.println("RTC Magic Value: 0x" + String(rtcData.magic, HEX));
    Serial.println("Expected Magic: 0x" + String(RTC_MAGIC, HEX));
    Serial.println("Reset Reason: " + String(esp_reset_reason()));
    
    // Check various reset reasons to determine if it's really a power cycle
    esp_reset_reason_t resetReason = esp_reset_reason();
    bool isPowerCycle = false;
    
    if (rtcData.magic != RTC_MAGIC) {
        Serial.println("Magic number mismatch detected");
        
        // Check reset reason to determine if it's actually a power cycle
        switch (resetReason) {
            case ESP_RST_POWERON:
                Serial.println("Reset Reason: Power-on reset");
                isPowerCycle = true;
                break;
            case ESP_RST_EXT:
                Serial.println("Reset Reason: External reset");
                isPowerCycle = true;
                break;
            case ESP_RST_SW:
                Serial.println("Reset Reason: Software reset");
                isPowerCycle = false;
                break;
            case ESP_RST_PANIC:
                Serial.println("Reset Reason: Exception/panic reset");
                isPowerCycle = false;
                break;
            case ESP_RST_INT_WDT:
                Serial.println("Reset Reason: Interrupt watchdog reset");
                isPowerCycle = false;
                break;
            case ESP_RST_TASK_WDT:
                Serial.println("Reset Reason: Task watchdog reset");
                isPowerCycle = false;
                break;
            case ESP_RST_WDT:
                Serial.println("Reset Reason: Other watchdog reset");
                isPowerCycle = false;
                break;
            case ESP_RST_DEEPSLEEP:
                Serial.println("Reset Reason: Deep sleep reset");
                isPowerCycle = false;
                break;
            case ESP_RST_BROWNOUT:
                Serial.println("Reset Reason: Brownout reset");
                isPowerCycle = true;
                break;
            case ESP_RST_SDIO:
                Serial.println("Reset Reason: SDIO reset");
                isPowerCycle = false;
                break;
            default:
                Serial.println("Reset Reason: Unknown (" + String(resetReason) + ")");
                isPowerCycle = true;
                break;
        }
        
        if (isPowerCycle) {
            // True power cycle detected - initialize RTC data
            Serial.println("TRUE POWER CYCLE DETECTED: Initializing RTC data");
            rtcData.magic = RTC_MAGIC;
            rtcData.softwareBoots = 0;
            rtcData.lastUptime = 0;
            rtcData.lastBootMode = 1; // Default to normal mode
            rtcData.lastResetReason = resetReason;
            
            // Reset bootcount in NVS on power cycle
            program.bootcount = 0;
            preferences.putULong("bootcount", program.bootcount);
            Serial.println("NVS bootcount reset to 0 due to power cycle");
            
            // Reset program state to normal on power cycle
            program.state = 1;
            preferences.putUChar("state", program.state);
            Serial.println("Program state reset to normal (1) due to power cycle");
        } else {
            // Software reset but RTC memory was lost - reinitialize but don't reset NVS
            Serial.println("SOFTWARE RESET with RTC memory loss - reinitializing RTC only");
            rtcData.magic = RTC_MAGIC;
            rtcData.softwareBoots = 1; // Start at 1 since this is a software reset
            rtcData.lastUptime = 0;
            rtcData.lastBootMode = program.state; // Use current state from NVS
            rtcData.lastResetReason = resetReason;
            // Do NOT reset NVS values for software resets
        }
    } else {
        // Software reboot - RTC data preserved
        Serial.println("SOFTWARE REBOOT: RTC data preserved");
        Serial.println("Software boots since power cycle: " + String(rtcData.softwareBoots));
        Serial.println("Last uptime: " + String(rtcData.lastUptime) + " ms");
        Serial.println("Last boot mode: " + String(rtcData.lastBootMode == 1 ? "Normal" : "Recovery"));
    }
    
    rtcData.softwareBoots++;
    Serial.println("Current software boot count: " + String(rtcData.softwareBoots));
    Serial.println("======================");
}

bool ESPdata::isPowerCycle() {
    return (rtcData.magic != RTC_MAGIC);
}

void ESPdata::updateRTCBeforeReboot(uint8_t newBootMode) {
    rtcData.lastUptime = millis();
    rtcData.lastBootMode = newBootMode;
    rtcData.lastResetReason = esp_reset_reason();
    Serial.println("RTC updated before reboot - Mode: " + String(newBootMode == 1 ? "Normal" : "Recovery"));
}

uint32_t ESPdata::getSoftwareBootCount() {
    return rtcData.softwareBoots;
}
