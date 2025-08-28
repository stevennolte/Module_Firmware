# Global Context System Flow - Complete Guide

## Understanding the Flow: From Startup to Runtime

This guide explains how the Global Context system works from ESP32 power-on to runtime operation, so you can understand and apply this pattern elsewhere.

## 1. System Startup Flow

### Phase 1: Hardware Initialization
```
ESP32 Powers On
       ↓
setup() function called
       ↓
Serial.begin(115200)
       ↓
Basic hardware setup
```

### Phase 2: Global Context Creation (CRITICAL STEP)
```cpp
void setup() {
    // Step 1: Get the singleton instance (creates it if needed)
    GlobalContext& ctx = getGlobalContext();
    
    // Step 2: Initialize the entire system
    if (!ctx.initialize()) {
        Serial.println("CRITICAL FAILURE!");
        return; // Stop everything if init fails
    }
    
    // Step 3: Now everything is ready
    Serial.println("System ready!");
}
```

**What happens inside `ctx.initialize()`:**
```
ctx.initialize()
       ↓
configManager.begin() - Initialize NVS storage
       ↓
Check if config exists in NVS
       ↓
If NO config found:
    ↓
    Look for old JSON file
    ↓
    If JSON exists: migrate to NVS
    If NO JSON: load factory defaults
       ↓
If config EXISTS: load from NVS
       ↓
Populate all ESPconfig structures
       ↓
Return true (success) or false (failure)
```

### Phase 3: Component Initialization
```cpp
void setup() {
    // After ctx.initialize() succeeds:
    
    // All components can now safely access config
    espWifi.connect();     // Uses WIFI_CFG
    gps.initialize();      // Uses GPS_CFG
    steer.initialize();    // Uses STEER_CFG
}
```

## 2. Runtime Access Patterns

### Pattern 1: Reading Configuration
```cpp
void someFunction() {
    // Method 1: Direct macro access (recommended)
    float currentGain = STEER_CFG.gainP;
    bool useExternal = GPS_CFG.externalGPS;
    
    // Method 2: Through context reference
    GlobalContext& ctx = getGlobalContext();
    float gain = ctx.getSteerConfig().gainP;
    
    // Method 3: Get reference once, use multiple times
    auto& steerCfg = getGlobalContext().getSteerConfig();
    float gain = steerCfg.gainP;
    uint8_t maxPWM = steerCfg.highPWM;
    uint8_t minPWM = steerCfg.lowPWM;
}
```

### Pattern 2: Modifying Configuration
```cpp
void updateSteeringGain(float newGain) {
    // Step 1: Validate input (optional but recommended)
    if (newGain < 0.1 || newGain > 50.0) {
        Serial.println("Invalid gain value!");
        return;
    }
    
    // Step 2: Update in memory
    STEER_CFG.gainP = newGain;
    
    // Step 3: Save to persistent storage
    if (saveConfig()) {
        Serial.println("Gain updated successfully!");
    } else {
        Serial.println("Failed to save configuration!");
    }
}
```

### Pattern 3: Runtime Data Access
```cpp
void steeringLoop() {
    // Read current data
    float currentAngle = STEER_DATA.actSteerAngle;
    float targetAngle = STEER_DATA.targetSteerAngle;
    
    // Calculate steering correction
    float error = targetAngle - currentAngle;
    float correction = error * STEER_CFG.gainP;  // Use config value
    
    // Update runtime data
    STEER_DATA.lastSteerCmd = correction;
    STEER_DATA.lastSteerTime = millis();
}
```

## 3. Web Interface Integration Flow

### User Interaction Flow:
```
User clicks "Set GPS Source" on webpage
       ↓
JavaScript sends POST to /setGpsSource
       ↓
ESP32 receives request in handleSetGpsSource()
       ↓
Parse JSON: {"source": "external"}
       ↓
Update config: GPS_CFG.externalGPS = true
       ↓
Save to NVS: ctx.saveConfiguration()
       ↓
Send response: "External GPS selected"
       ↓
All other code sees change immediately
```

**Code flow in the handler:**
```cpp
void handleSetGpsSource(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    // Step 1: Parse incoming JSON
    StaticJsonDocument<128> doc;
    DeserializationError error = deserializeJson(doc, data, len);
    
    // Step 2: Extract and validate
    String source = doc["source"] | "";
    
    // Step 3: Update configuration
    if (source == "external") {
        GPS_CFG.externalGPS = true;        // Update in memory
        ctx.saveConfiguration();           // Persist to NVS
        request->send(200, "text/plain", "External GPS selected");
    }
    
    // Step 4: All other code immediately sees the change!
    // No need to notify anyone - they just read GPS_CFG.externalGPS
}
```

## 4. Data Flow Diagrams

### Configuration Load Flow:
```
ESP32 Startup
      ↓
GlobalContext::initialize()
      ↓
ConfigManager::begin() → Open NVS namespace
      ↓
Check for existing config
      ↓
┌─────────────────────┬─────────────────────┐
│ Config Exists       │ No Config Found     │
│        ↓            │        ↓            │
│ Load from NVS       │ Check for JSON file │
│        ↓            │        ↓            │
│ Populate structs    │ Migrate or defaults │
│        ↓            │        ↓            │
│ Return success      │ Save to NVS         │
└─────────────────────┴─────────────────────┘
      ↓
All components can access config safely
```

### Runtime Access Flow:
```
Component needs config value
      ↓
Use macro: STEER_CFG.gainP
      ↓
Macro expands to: getGlobalContext().getSteerConfig().gainP
      ↓
getGlobalContext() returns singleton instance
      ↓
getSteerConfig() returns reference to config.steerCfg
      ↓
.gainP accesses the float value
      ↓
Value returned to component
```

### Configuration Save Flow:
```
Code updates config: STEER_CFG.gainP = 10.0
      ↓
Value updated in memory immediately
      ↓
Code calls: saveConfig()
      ↓
saveConfig() calls: GlobalContext::saveConfiguration()
      ↓
For each config value, call ConfigManager::putFloat("kp", 10.0)
      ↓
NVS writes value atomically to flash
      ↓
Return success/failure
```

## 5. How to Apply This Pattern Elsewhere

### Step 1: Identify Your Global Data

**Before (scattered globals):**
```cpp
// In different files:
int motorSpeed;
float temperature;
bool systemEnabled;
WiFiClient client;
Preferences prefs;
```

**After (organized in context):**
```cpp
class MyAppContext {
private:
    static MyAppContext* instance;
    
public:
    struct MotorData {
        int speed;
        bool enabled;
        float maxRPM;
    } motorData;
    
    struct SensorData {
        float temperature;
        float humidity;
        uint32_t lastUpdate;
    } sensorData;
    
    struct NetworkData {
        WiFiClient client;
        bool connected;
        String lastError;
    } networkData;
    
    // Singleton access
    static MyAppContext& getInstance();
    
    // Accessors
    MotorData& getMotorData() { return motorData; }
    SensorData& getSensorData() { return sensorData; }
    NetworkData& getNetworkData() { return networkData; }
};
```

### Step 2: Create Access Macros

```cpp
// In globals.h
#define APP_CTX MyAppContext::getInstance()
#define MOTOR_DATA APP_CTX.getMotorData()
#define SENSOR_DATA APP_CTX.getSensorData()
#define NETWORK_DATA APP_CTX.getNetworkData()
```

### Step 3: Initialize in setup()

```cpp
void setup() {
    MyAppContext& ctx = MyAppContext::getInstance();
    if (!ctx.initialize()) {
        Serial.println("Initialization failed!");
        return;
    }
    
    // Now all components can safely access data
    initializeMotor();
    initializeSensors();
    initializeNetwork();
}
```

### Step 4: Use Throughout Your Code

```cpp
void controlMotor() {
    // Read current state
    int currentSpeed = MOTOR_DATA.speed;
    
    // Make decisions
    if (SENSOR_DATA.temperature > 80.0) {
        MOTOR_DATA.speed = 0;  // Emergency stop
        MOTOR_DATA.enabled = false;
    }
    
    // Update state
    MOTOR_DATA.lastUpdate = millis();
}

void networkTask() {
    if (NETWORK_DATA.connected) {
        // Send motor data
        String data = "Speed:" + String(MOTOR_DATA.speed);
        NETWORK_DATA.client.println(data);
    }
}
```

## 6. Common Patterns and Best Practices

### Pattern 1: Initialization with Error Handling
```cpp
bool MyAppContext::initialize() {
    Serial.println("Initializing application context...");
    
    // Initialize components in order
    if (!initializeStorage()) {
        Serial.println("Storage init failed!");
        return false;
    }
    
    if (!loadConfiguration()) {
        Serial.println("Config load failed!");
        return false;
    }
    
    if (!initializeHardware()) {
        Serial.println("Hardware init failed!");
        return false;
    }
    
    Serial.println("Application context ready!");
    return true;
}
```

### Pattern 2: Safe Configuration Updates
```cpp
bool updateMotorMaxRPM(float newMaxRPM) {
    // Validate
    if (newMaxRPM < 100 || newMaxRPM > 10000) {
        Serial.println("Invalid RPM value!");
        return false;
    }
    
    // Update
    MOTOR_DATA.maxRPM = newMaxRPM;
    
    // Persist
    return saveConfiguration();
}
```

### Pattern 3: Status Reporting
```cpp
void printSystemStatus() {
    Serial.println("=== System Status ===");
    Serial.printf("Motor: %s, Speed: %d RPM\n", 
                  MOTOR_DATA.enabled ? "ON" : "OFF",
                  MOTOR_DATA.speed);
    Serial.printf("Temperature: %.1f°C\n", SENSOR_DATA.temperature);
    Serial.printf("Network: %s\n", 
                  NETWORK_DATA.connected ? "Connected" : "Disconnected");
}
```

## 7. Debugging and Troubleshooting

### Debug Pattern 1: Context State Inspection
```cpp
void debugContext() {
    MyAppContext& ctx = MyAppContext::getInstance();
    
    Serial.println("=== Context Debug ===");
    Serial.printf("Instance address: %p\n", &ctx);
    Serial.printf("Motor data address: %p\n", &ctx.getMotorData());
    Serial.printf("Sensor data address: %p\n", &ctx.getSensorData());
    
    // Verify singleton behavior
    MyAppContext& ctx2 = MyAppContext::getInstance();
    Serial.printf("Same instance? %s\n", (&ctx == &ctx2) ? "YES" : "NO");
}
```

### Debug Pattern 2: Configuration Validation
```cpp
bool validateConfiguration() {
    bool valid = true;
    
    if (MOTOR_DATA.maxRPM < 100 || MOTOR_DATA.maxRPM > 10000) {
        Serial.println("ERROR: Invalid motor max RPM!");
        valid = false;
    }
    
    if (SENSOR_DATA.lastUpdate == 0) {
        Serial.println("WARNING: Sensors never updated!");
        valid = false;
    }
    
    return valid;
}
```

## 8. Migration Strategy for Existing Code

### Step 1: Identify Global Variables
```bash
# Find all global variables in your project
grep -r "^[a-zA-Z].*;" *.cpp *.h | grep -v "function\|class"
```

### Step 2: Group Related Data
```cpp
// Before: Scattered globals
int motorSpeed;
bool motorEnabled;
float motorTemp;
int sensorValue1;
int sensorValue2;
bool systemRunning;

// After: Grouped in structures
struct MotorData {
    int speed;
    bool enabled;
    float temperature;
};

struct SensorData {
    int value1;
    int value2;
    uint32_t lastReading;
};

struct SystemData {
    bool running;
    uint32_t uptime;
};
```

### Step 3: Gradual Migration
```cpp
// Phase 1: Create context but keep old variables
MyAppContext& ctx = MyAppContext::getInstance();
extern int motorSpeed;  // Keep old global for now

void updateMotorSpeed(int speed) {
    motorSpeed = speed;           // Update old way
    MOTOR_DATA.speed = speed;     // Also update new way
}

// Phase 2: Update consumers one by one
void motorControl() {
    // Old: int speed = motorSpeed;
    int speed = MOTOR_DATA.speed;  // New way
}

// Phase 3: Remove old globals
// Remove: extern int motorSpeed;
```

This flow shows you exactly how the Global Context system works from power-on to runtime, and how you can apply this pattern to organize any embedded project with multiple global variables and shared state.

The key insight is that instead of scattering global variables throughout your code, you organize them into a single, controlled access point that manages initialization, provides type safety, and makes debugging much easier.
