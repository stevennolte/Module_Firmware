/**
 * @file advanced_example.cpp
 * @brief Advanced example showing multiple A121 sensor features
 * 
 * This example demonstrates:
 * - Distance measurement with filtering
 * - Object detection and tracking
 * - Temperature monitoring
 * - Error handling
 * - Different configuration modes
 */

#include <Arduino.h>
#include "A121Sensor.h"
#include "A121Config.h"

// I2C Configuration
#define I2C_SDA A121_DEFAULT_SDA_PIN
#define I2C_SCL A121_DEFAULT_SCL_PIN

// Detection thresholds
#define DISTANCE_THRESHOLD_MM 500    // Object detection threshold
#define AMPLITUDE_THRESHOLD 50       // Signal strength threshold
#define TEMP_WARNING_C 60            // Temperature warning threshold

// Create sensor instance
A121Sensor sensor(I2C_SDA, I2C_SCL);

// State variables
bool objectDetected = false;
uint16_t lastDistance = 0;
unsigned long detectionTime = 0;

// Simple moving average filter
class MovingAverageFilter {
private:
    static const int WINDOW_SIZE = 5;
    uint16_t readings[WINDOW_SIZE];
    int index;
    int count;
    
public:
    MovingAverageFilter() : index(0), count(0) {
        for (int i = 0; i < WINDOW_SIZE; i++) {
            readings[i] = 0;
        }
    }
    
    uint16_t addReading(uint16_t reading) {
        readings[index] = reading;
        index = (index + 1) % WINDOW_SIZE;
        if (count < WINDOW_SIZE) count++;
        
        uint32_t sum = 0;
        for (int i = 0; i < count; i++) {
            sum += readings[i];
        }
        return sum / count;
    }
    
    void reset() {
        index = 0;
        count = 0;
    }
};

MovingAverageFilter distanceFilter;

void printSensorInfo() {
    Serial.println("\n=== A121 Sensor Information ===");
    Serial.println("Configuration: Medium Range");
    Serial.print("Detection Threshold: ");
    Serial.print(DISTANCE_THRESHOLD_MM);
    Serial.println(" mm");
    Serial.print("Amplitude Threshold: ");
    Serial.println(AMPLITUDE_THRESHOLD);
    Serial.print("Temperature Warning: ");
    Serial.print(TEMP_WARNING_C);
    Serial.println(" C");
    Serial.println("===============================\n");
}

void checkSensorHealth() {
    // Check temperature
    int8_t temp = sensor.getTemperature();
    if (temp > TEMP_WARNING_C) {
        Serial.print("WARNING: High temperature: ");
        Serial.print(temp);
        Serial.println(" C");
    }
    
    // Check for errors
    if (sensor.hasError()) {
        Serial.println("ERROR: Sensor error detected!");
        uint8_t status = sensor.getStatus();
        Serial.print("Status code: 0x");
        Serial.println(status, HEX);
        
        // Attempt recovery
        Serial.println("Attempting sensor reset...");
        if (sensor.reset()) {
            delay(100);
            sensor.startMeasurement();
            Serial.println("Sensor reset successful");
        } else {
            Serial.println("Sensor reset failed!");
        }
    }
}

void detectObject() {
    if (!sensor.isReady()) {
        return;
    }
    
    // Read raw measurements
    uint16_t rawDistance = sensor.getDistance();
    uint8_t amplitude = sensor.getAmplitude();
    
    // Apply filtering
    uint16_t filteredDistance = distanceFilter.addReading(rawDistance);
    
    // Check if object is detected
    bool currentlyDetected = false;
    if (filteredDistance > 0 && 
        filteredDistance < DISTANCE_THRESHOLD_MM && 
        amplitude > AMPLITUDE_THRESHOLD) {
        currentlyDetected = true;
    }
    
    // Detect state changes
    if (currentlyDetected && !objectDetected) {
        // New object detected
        objectDetected = true;
        detectionTime = millis();
        Serial.println("\n>>> OBJECT DETECTED <<<");
        Serial.print("Distance: ");
        Serial.print(filteredDistance);
        Serial.println(" mm");
    } else if (!currentlyDetected && objectDetected) {
        // Object no longer detected
        objectDetected = false;
        unsigned long duration = millis() - detectionTime;
        Serial.println("\n>>> OBJECT LEFT <<<");
        Serial.print("Detection duration: ");
        Serial.print(duration);
        Serial.println(" ms");
        distanceFilter.reset();
    } else if (objectDetected) {
        // Track moving object
        int distanceDiff = (int)filteredDistance - (int)lastDistance;
        if (distanceDiff > 10 || distanceDiff < -10) {
            Serial.print("Object moving - Distance: ");
            Serial.print(filteredDistance);
            Serial.print(" mm (Δ: ");
            Serial.print(distanceDiff);
            Serial.println(" mm)");
        }
    }
    
    lastDistance = filteredDistance;
    
    // Print detailed status periodically
    static unsigned long lastStatusPrint = 0;
    if (millis() - lastStatusPrint > 5000) {
        lastStatusPrint = millis();
        
        Serial.println("\n--- Status Update ---");
        Serial.print("Raw: ");
        Serial.print(rawDistance);
        Serial.print(" mm | Filtered: ");
        Serial.print(filteredDistance);
        Serial.print(" mm | Amplitude: ");
        Serial.print(amplitude);
        Serial.print(" | Temp: ");
        Serial.print(sensor.getTemperature());
        Serial.print(" C | Detected: ");
        Serial.println(objectDetected ? "YES" : "NO");
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=== A121 Advanced Example ===");
    Serial.println("Initializing sensor...");
    
    // Initialize sensor
    if (!sensor.begin()) {
        Serial.println("ERROR: Failed to initialize A121 sensor!");
        Serial.println("Check connections and power supply.");
        while (1) {
            delay(1000);
        }
    }
    
    Serial.println("Sensor initialized successfully!");
    
    // Configure for medium range detection
    if (sensor.configure(A121_CONFIG_MEDIUM_RANGE)) {
        Serial.println("Configuration: Medium Range Mode");
    } else {
        Serial.println("WARNING: Configuration failed, using defaults");
    }
    
    // Start measurements
    if (sensor.startMeasurement()) {
        Serial.println("Measurements started");
    } else {
        Serial.println("ERROR: Failed to start measurements");
        while (1) {
            delay(1000);
        }
    }
    
    printSensorInfo();
    Serial.println("Starting object detection...\n");
}

void loop() {
    // Main detection loop
    detectObject();
    
    // Periodic health check
    static unsigned long lastHealthCheck = 0;
    if (millis() - lastHealthCheck > 30000) {  // Every 30 seconds
        lastHealthCheck = millis();
        checkSensorHealth();
    }
    
    // Small delay
    delay(100);
}
