#include <Arduino.h>
#include "A121Sensor.h"
#include "A121Config.h"

// ESP32-S3 I2C pins (can be customized)
#define I2C_SDA 8  // GPIO8
#define I2C_SCL 9  // GPIO9

// Create A121 sensor instance
A121Sensor sensor(I2C_SDA, I2C_SCL);

// Measurement interval in milliseconds (adjust this to change reading frequency)
// Examples: 100 = 10 Hz, 500 = 2 Hz, 1000 = 1 Hz, 2000 = 0.5 Hz
const unsigned long MEASUREMENT_INTERVAL = A121_DEFAULT_MEASUREMENT_INTERVAL_MS;
unsigned long lastMeasurementTime = 0;

void setup() {
    // Initialize serial communication
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=== Acconeer A121 I2C Sensor Example ===");
    Serial.println("Initializing sensor...");
    
    // Initialize the A121 sensor
    if (!sensor.begin()) {
        Serial.println("ERROR: Failed to initialize A121 sensor!");
        Serial.println("Please check:");
        Serial.println("  - I2C connections (SDA, SCL)");
        Serial.println("  - Sensor power supply");
        Serial.println("  - I2C address (default: 0x52)");
        while (1) {
            delay(1000);
        }
    }
    
    Serial.println("A121 sensor initialized successfully!");
    
    // Configure sensor (example: set default configuration)
    if (sensor.configure(0x00)) {
        Serial.println("Sensor configured");
    }
    
    // Start continuous measurements
    if (sensor.startMeasurement()) {
        Serial.println("Measurements started");
    } else {
        Serial.println("ERROR: Failed to start measurements");
    }
    
    Serial.println("\n--- Starting continuous measurements ---");
    Serial.println("Format: Time(ms) | Distance(mm) | Amplitude | Temperature(C) | Status");
    Serial.println("---------------------------------------------------------------");
}

void loop() {
    unsigned long currentTime = millis();
    
    // Perform measurement at specified interval
    if (currentTime - lastMeasurementTime >= MEASUREMENT_INTERVAL) {
        lastMeasurementTime = currentTime;
        
        // Check if sensor is ready
        if (sensor.isReady()) {
            // Read sensor data
            uint16_t distance = sensor.getDistance();
            uint8_t amplitude = sensor.getAmplitude();
            int8_t temperature = sensor.getTemperature();
            uint8_t status = sensor.getStatus();
            
            // Print results
            Serial.print(currentTime);
            Serial.print(" | ");
            Serial.print(distance);
            Serial.print(" mm | ");
            Serial.print(amplitude);
            Serial.print(" | ");
            Serial.print(temperature);
            Serial.print(" C | 0x");
            Serial.println(status, HEX);
            
            // Check for errors
            if (sensor.hasError()) {
                Serial.println("WARNING: Sensor error detected!");
            }
        } else {
            Serial.println("Sensor not ready");
        }
    }
    
    // Small delay to prevent excessive CPU usage
    delay(10);
}
