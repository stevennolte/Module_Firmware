/**
 * @file i2c_scanner.cpp
 * @brief I2C Scanner utility for finding A121 sensor address
 * 
 * This utility scans the I2C bus to detect connected devices
 * and helps identify the A121 sensor's I2C address.
 */

#include <Arduino.h>
#include <Wire.h>

// I2C pins
#define I2C_SDA 8
#define I2C_SCL 9

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("\n=== I2C Scanner for A121 Sensor ===");
    Serial.println("Scanning I2C bus...\n");
    
    // Initialize I2C
    Wire.begin(I2C_SDA, I2C_SCL);
    Wire.setClock(400000);
    
    delay(100);
    
    int deviceCount = 0;
    
    Serial.println("     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F");
    for (int address = 0; address < 128; address++) {
        if (address % 16 == 0) {
            Serial.printf("%02X: ", address);
        }
        
        Wire.beginTransmission(address);
        uint8_t error = Wire.endTransmission();
        
        if (error == 0) {
            Serial.printf("%02X ", address);
            deviceCount++;
        } else {
            Serial.print("-- ");
        }
        
        if ((address + 1) % 16 == 0) {
            Serial.println();
        }
        
        delay(5);
    }
    
    Serial.println("\n================================");
    Serial.print("Devices found: ");
    Serial.println(deviceCount);
    
    if (deviceCount == 0) {
        Serial.println("\nNo I2C devices found!");
        Serial.println("Check:");
        Serial.println("  - Wiring (SDA, SCL)");
        Serial.println("  - Power supply");
        Serial.println("  - Pull-up resistors");
    } else {
        Serial.println("\nNote: A121 default address is 0x52");
    }
    
    Serial.println("\nScan complete.");
}

void loop() {
    // Wait 5 seconds and scan again
    delay(5000);
    
    Serial.println("\n--- Rescanning I2C bus ---");
    int deviceCount = 0;
    
    // Scan all valid I2C addresses (0x00-0x7F, excluding reserved addresses)
    for (int address = 0; address < 128; address++) {
        Wire.beginTransmission(address);
        uint8_t error = Wire.endTransmission();
        
        if (error == 0) {
            Serial.print("Device found at 0x");
            Serial.println(address, HEX);
            deviceCount++;
        }
    }
    
    if (deviceCount == 0) {
        Serial.println("No devices found");
    }
}
