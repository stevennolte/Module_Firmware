# ESP32-S3 Acconeer A121 Sensor Controller

This project provides an I2C interface for controlling an Acconeer A121 radar sensor using an ESP32-S3 microcontroller.

## Overview

The Acconeer A121 is a pulsed coherent radar (PCR) sensor that can measure distance, detect motion, and provide presence detection. This firmware implements I2C communication to control the A121 sensor and read measurement data.

## Hardware Requirements

- ESP32-S3 Development Board (e.g., ESP32-S3-DevKitC-1)
- Acconeer A121 Sensor Module with I2C interface
- Connection wires for I2C (SDA, SCL) and power

## Pin Configuration

Default I2C pins (can be customized in `main.cpp`):
- **SDA**: GPIO 8
- **SCL**: GPIO 9

## Features

- I2C communication with A121 sensor
- Sensor initialization and configuration
- Distance measurement reading
- Amplitude (signal strength) reading
- Temperature monitoring
- Status monitoring and error detection
- Continuous measurement mode
- **Adjustable measurement frequency** (from 0.1 Hz to 100+ Hz)
- Serial output for debugging and monitoring

## Building and Uploading

This project uses PlatformIO for building and uploading.

### Prerequisites

1. Install [PlatformIO](https://platformio.org/)
2. Install [VS Code](https://code.visualstudio.com/) (recommended) with PlatformIO extension

### Build

```bash
cd ESP32_A121_Sensor
pio run
```

### Upload

```bash
pio run --target upload
```

### Monitor Serial Output

```bash
pio device monitor
```

Or combine upload and monitor:

```bash
pio run --target upload && pio device monitor
```

## Examples

The project includes several examples in the `examples/` directory:

### 1. Basic Example (main.cpp)
The main program continuously reads distance measurements from the A121 sensor at 1-second intervals (1 Hz) and prints the results to the serial monitor.

**Adjusting Measurement Frequency:**
To change how often the sensor reads data, modify the `MEASUREMENT_INTERVAL` constant in `src/main.cpp`:
```cpp
const unsigned long MEASUREMENT_INTERVAL = 1000;  // Default: 1000ms = 1 Hz
```

Examples:
- `100` = 10 Hz (10 readings per second)
- `500` = 2 Hz (2 readings per second)  
- `1000` = 1 Hz (1 reading per second) - default
- `2000` = 0.5 Hz (1 reading every 2 seconds)
- `5000` = 0.2 Hz (1 reading every 5 seconds)

### 2. Advanced Example (examples/advanced_example.cpp)
Demonstrates:
- Object detection and tracking
- Distance filtering using moving average
- Temperature monitoring and warnings
- Automatic error recovery
- State change detection

### 3. I2C Scanner (examples/i2c_scanner.cpp)
A utility to scan the I2C bus and identify connected devices. Useful for debugging connection issues and verifying the A121 sensor address.

To use an example, copy the desired `.cpp` file from the `examples/` directory to `src/main.cpp`.

## Usage

### Basic Usage

```cpp
#include <Arduino.h>
#include "A121Sensor.h"

// Create sensor instance with custom I2C pins
A121Sensor sensor(SDA_PIN, SCL_PIN);

void setup() {
    Serial.begin(115200);
    
    // Initialize sensor
    if (!sensor.begin()) {
        Serial.println("Failed to initialize sensor!");
        while(1);
    }
    
    // Start measurements
    sensor.startMeasurement();
}

void loop() {
    if (sensor.isReady()) {
        uint16_t distance = sensor.getDistance();
        Serial.print("Distance: ");
        Serial.print(distance);
        Serial.println(" mm");
    }
    delay(1000);
}
```

### API Reference

#### Initialization

- `A121Sensor(uint8_t address)` - Constructor with I2C address
- `A121Sensor(int sda, int scl, uint8_t address)` - Constructor with custom pins and address
- `bool begin()` - Initialize sensor with default Wire instance
- `bool begin(TwoWire& wirePort)` - Initialize sensor with custom Wire instance

#### Control Functions

- `bool isConnected()` - Check if sensor is connected via I2C
- `bool reset()` - Reset sensor to default state
- `bool startMeasurement()` - Start continuous measurements
- `bool stopMeasurement()` - Stop measurements
- `bool configure(uint8_t config)` - Configure sensor parameters

#### Status Functions

- `uint8_t getStatus()` - Get current sensor status
- `bool isReady()` - Check if sensor is ready for operation
- `bool isMeasuring()` - Check if sensor is currently measuring
- `bool hasError()` - Check if sensor has an error condition

#### Data Reading Functions

- `uint16_t getDistance()` - Get measured distance in millimeters
- `uint8_t getAmplitude()` - Get signal amplitude/strength
- `int8_t getTemperature()` - Get sensor temperature in Celsius

## Serial Output Format

The program outputs measurement data in the following format:

```
Time(ms) | Distance(mm) | Amplitude | Temperature(C) | Status
---------------------------------------------------------------
1000 | 523 mm | 156 | 25 C | 0x03
2000 | 525 mm | 158 | 25 C | 0x03
```

## Troubleshooting

### Sensor Not Detected

1. Check I2C connections (SDA, SCL)
2. Verify sensor power supply (voltage and current)
3. Confirm I2C address matches (default: 0x52)
4. Use I2C scanner to detect device
5. Check pull-up resistors on I2C lines (typically 4.7kΩ)

### No Measurements

1. Ensure sensor is properly initialized
2. Check if measurements are started
3. Verify sensor is in ready state
4. Check for error status

### Incorrect Readings

1. Verify sensor configuration
2. Check measurement range and environment
3. Ensure stable power supply
4. Check for electromagnetic interference

## I2C Address

Default I2C address: **0x52**

To use a different address, modify the instantiation:

```cpp
A121Sensor sensor(SDA_PIN, SCL_PIN, 0x53);  // Custom address
```

## Notes

**Important:** The register addresses and command definitions in this implementation are placeholders based on typical I2C radar sensor interfaces. For production use with actual A121 hardware, you must:

1. Consult the official Acconeer A121 I2C interface documentation
2. Update the register addresses in `A121Sensor.h` to match your specific A121 module
3. Verify the I2C address (default 0x52) matches your hardware configuration
4. Adjust command values and configuration parameters as needed

Other notes:
- The A121 sensor requires appropriate power supply (check sensor datasheet)
- I2C communication speed is set to 400kHz (Fast Mode)
- This code provides a framework that can be adapted to your specific A121 I2C implementation

## License

This project is part of the Module_Firmware repository.

## References

- [Acconeer A121 Documentation](https://www.acconeer.com/)
- [ESP32-S3 Datasheet](https://www.espressif.com/en/products/socs/esp32-s3)
- [PlatformIO Documentation](https://docs.platformio.org/)
