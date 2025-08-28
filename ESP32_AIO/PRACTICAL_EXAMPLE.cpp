// SIMPLE EXAMPLE: Applying Global Context to a Temperature Controller
// This shows the pattern applied to a basic project

#include <Arduino.h>
#include <Preferences.h>

// ========================================
// 1. DEFINE YOUR DATA STRUCTURES
// ========================================

class TemperatureControllerContext {
private:
    static TemperatureControllerContext* instance;
    TemperatureControllerContext() {} // Private constructor
    
public:
    // ========================================
    // 2. ORGANIZE YOUR DATA INTO LOGICAL GROUPS
    // ========================================
    
    struct SensorData {
        float temperature;
        float humidity;
        uint32_t lastReading;
        bool sensorOK;
    } sensorData;
    
    struct HeaterConfig {
        float targetTemp;
        float hysteresis;
        uint8_t heaterPin;
        bool enabled;
    } heaterConfig;
    
    struct HeaterData {
        bool isOn;
        uint32_t lastToggle;
        uint32_t onTime;     // Total time on
        uint32_t cycles;     // Number of on/off cycles
    } heaterData;
    
    struct NetworkConfig {
        String ssid;
        String password;
        String deviceName;
        bool webServerEnabled;
    } networkConfig;
    
    // ========================================
    // 3. SINGLETON PATTERN IMPLEMENTATION
    // ========================================
    
    static TemperatureControllerContext& getInstance() {
        if (instance == nullptr) {
            instance = new TemperatureControllerContext();
        }
        return *instance;
    }
    
    // ========================================
    // 4. INITIALIZATION AND CONFIGURATION
    // ========================================
    
    bool initialize() {
        Serial.println("Initializing Temperature Controller...");
        
        // Initialize preferences
        if (!prefs.begin("temp_ctrl", false)) {
            Serial.println("Failed to initialize preferences!");
            return false;
        }
        
        // Load configuration
        loadConfiguration();
        
        // Initialize hardware
        pinMode(heaterConfig.heaterPin, OUTPUT);
        digitalWrite(heaterConfig.heaterPin, LOW);
        
        Serial.println("Temperature Controller ready!");
        return true;
    }
    
    void loadConfiguration() {
        // Load with defaults
        heaterConfig.targetTemp = prefs.getFloat("target_temp", 25.0);
        heaterConfig.hysteresis = prefs.getFloat("hysteresis", 1.0);
        heaterConfig.heaterPin = prefs.getUChar("heater_pin", 2);
        heaterConfig.enabled = prefs.getBool("enabled", true);
        
        networkConfig.ssid = prefs.getString("ssid", "MyWiFi");
        networkConfig.password = prefs.getString("password", "");
        networkConfig.deviceName = prefs.getString("device_name", "TempController");
        networkConfig.webServerEnabled = prefs.getBool("web_enabled", true);
        
        Serial.printf("Config loaded: Target=%.1f°C, Hysteresis=%.1f°C\n", 
                      heaterConfig.targetTemp, heaterConfig.hysteresis);
    }
    
    bool saveConfiguration() {
        prefs.putFloat("target_temp", heaterConfig.targetTemp);
        prefs.putFloat("hysteresis", heaterConfig.hysteresis);
        prefs.putUChar("heater_pin", heaterConfig.heaterPin);
        prefs.putBool("enabled", heaterConfig.enabled);
        
        prefs.putString("ssid", networkConfig.ssid);
        prefs.putString("password", networkConfig.password);
        prefs.putString("device_name", networkConfig.deviceName);
        prefs.putBool("web_enabled", networkConfig.webServerEnabled);
        
        Serial.println("Configuration saved to flash");
        return true;
    }
    
    // ========================================
    // 5. TYPE-SAFE ACCESSORS
    // ========================================
    
    SensorData& getSensorData() { return sensorData; }
    HeaterConfig& getHeaterConfig() { return heaterConfig; }
    HeaterData& getHeaterData() { return heaterData; }
    NetworkConfig& getNetworkConfig() { return networkConfig; }
    
    // ========================================
    // 6. BUSINESS LOGIC METHODS
    // ========================================
    
    void updateTemperature(float temp, float humid) {
        sensorData.temperature = temp;
        sensorData.humidity = humid;
        sensorData.lastReading = millis();
        sensorData.sensorOK = true;
    }
    
    void controlHeater() {
        if (!heaterConfig.enabled) {
            setHeater(false);
            return;
        }
        
        float temp = sensorData.temperature;
        float target = heaterConfig.targetTemp;
        float hyst = heaterConfig.hysteresis;
        
        // Simple hysteresis control
        if (temp < target - hyst && !heaterData.isOn) {
            setHeater(true);
        } else if (temp > target + hyst && heaterData.isOn) {
            setHeater(false);
        }
    }
    
    void printStatus() {
        Serial.println("=== Temperature Controller Status ===");
        Serial.printf("Temperature: %.1f°C (target: %.1f°C)\n", 
                      sensorData.temperature, heaterConfig.targetTemp);
        Serial.printf("Heater: %s (Pin %d)\n", 
                      heaterData.isOn ? "ON" : "OFF", heaterConfig.heaterPin);
        Serial.printf("Cycles: %d, On-time: %d seconds\n", 
                      heaterData.cycles, heaterData.onTime / 1000);
        Serial.printf("Network: %s\n", networkConfig.ssid.c_str());
    }
    
private:
    Preferences prefs;
    
    void setHeater(bool state) {
        if (heaterData.isOn != state) {
            heaterData.isOn = state;
            digitalWrite(heaterConfig.heaterPin, state);
            
            uint32_t now = millis();
            if (state) {
                // Turning on
                heaterData.lastToggle = now;
            } else {
                // Turning off
                heaterData.onTime += (now - heaterData.lastToggle);
                heaterData.cycles++;
            }
            
            Serial.printf("Heater %s\n", state ? "ON" : "OFF");
        }
    }
};

// Static member definition
TemperatureControllerContext* TemperatureControllerContext::instance = nullptr;

// ========================================
// 7. CONVENIENCE MACROS
// ========================================

#define TEMP_CTRL TemperatureControllerContext::getInstance()
#define SENSOR_DATA TEMP_CTRL.getSensorData()
#define HEATER_CFG TEMP_CTRL.getHeaterConfig()
#define HEATER_DATA TEMP_CTRL.getHeaterData()
#define NETWORK_CFG TEMP_CTRL.getNetworkConfig()

// Helper functions
inline void saveConfig() { TEMP_CTRL.saveConfiguration(); }
inline void printStatus() { TEMP_CTRL.printStatus(); }

// ========================================
// 8. MAIN APPLICATION USING THE PATTERN
// ========================================

void setup() {
    Serial.begin(115200);
    delay(2000);
    
    // Initialize the global context
    if (!TEMP_CTRL.initialize()) {
        Serial.println("CRITICAL: Initialization failed!");
        while(1) delay(1000); // Stop here
    }
    
    // Show initial status
    printStatus();
}

void loop() {
    static uint32_t lastSensorRead = 0;
    static uint32_t lastControl = 0;
    static uint32_t lastStatus = 0;
    
    uint32_t now = millis();
    
    // Read sensor every 2 seconds
    if (now - lastSensorRead > 2000) {
        // Simulate reading DHT22 or similar
        float temp = 20.0 + random(-50, 150) / 10.0; // 15-35°C
        float humid = 40.0 + random(0, 400) / 10.0;  // 40-80%
        
        TEMP_CTRL.updateTemperature(temp, humid);
        lastSensorRead = now;
    }
    
    // Control heater every 5 seconds
    if (now - lastControl > 5000) {
        TEMP_CTRL.controlHeater();
        lastControl = now;
    }
    
    // Print status every 30 seconds
    if (now - lastStatus > 30000) {
        printStatus();
        lastStatus = now;
    }
    
    // Example: Web server would handle configuration changes
    // handleWebRequests(); // Implementation not shown
}

// ========================================
// 9. EXAMPLE WEB HANDLERS (if using AsyncWebServer)
// ========================================

void setupWebHandlers() {
    // Set target temperature
    server.on("/setTarget", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("temp")) {
            float newTarget = request->getParam("temp")->value().toFloat();
            
            if (newTarget >= 10.0 && newTarget <= 50.0) {
                HEATER_CFG.targetTemp = newTarget;
                saveConfig();
                request->send(200, "text/plain", "Target temperature set to " + String(newTarget));
            } else {
                request->send(400, "text/plain", "Invalid temperature range (10-50°C)");
            }
        }
    });
    
    // Get current status
    server.on("/status", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = "{";
        json += "\"temperature\":" + String(SENSOR_DATA.temperature) + ",";
        json += "\"target\":" + String(HEATER_CFG.targetTemp) + ",";
        json += "\"heater\":" + String(HEATER_DATA.isOn ? "true" : "false") + ",";
        json += "\"cycles\":" + String(HEATER_DATA.cycles);
        json += "}";
        
        request->send(200, "application/json", json);
    });
    
    // Enable/disable heater
    server.on("/enable", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("state")) {
            bool enable = request->getParam("state")->value() == "true";
            HEATER_CFG.enabled = enable;
            saveConfig();
            request->send(200, "text/plain", enable ? "Heater enabled" : "Heater disabled");
        }
    });
}

// ========================================
// 10. BENEFITS DEMONSTRATED
// ========================================

/*
BENEFITS OF THIS PATTERN:

1. ORGANIZED DATA:
   - Instead of scattered globals, everything is organized
   - Related data is grouped together
   - Clear separation between config and runtime data

2. TYPE SAFETY:
   - HEATER_CFG.targetTemp is guaranteed to be a float
   - Can't accidentally assign wrong type
   - Compiler catches errors

3. EASY ACCESS:
   - SENSOR_DATA.temperature anywhere in code
   - No need for extern declarations
   - Consistent access pattern

4. RELIABLE PERSISTENCE:
   - saveConfig() handles all the complexity
   - Power-fail safe storage
   - Automatic error handling

5. CENTRALIZED INITIALIZATION:
   - One place to initialize everything
   - Clear success/failure indication
   - Proper dependency management

6. EASY DEBUGGING:
   - printStatus() shows everything
   - Single place to add debug info
   - Clear data flow

7. SCALABLE:
   - Easy to add new data structures
   - Easy to add new configuration items
   - Pattern scales to large projects

8. WEB INTEGRATION:
   - Clean handlers that update config
   - Immediate effect throughout code
   - Simple JSON responses

USAGE EXAMPLES:

// Reading values anywhere in code:
if (SENSOR_DATA.temperature > HEATER_CFG.targetTemp) {
    // Do something
}

// Updating configuration:
HEATER_CFG.targetTemp = 23.5;
saveConfig();  // Persists to flash

// Checking runtime state:
if (HEATER_DATA.isOn && HEATER_DATA.cycles > 1000) {
    // Maybe heater is cycling too much
}

This pattern transforms chaotic global variables into organized,
type-safe, reliable data management!
*/
