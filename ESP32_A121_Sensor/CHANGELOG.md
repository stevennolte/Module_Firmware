# Changelog

All notable changes to the ESP32-S3 A121 Sensor Controller project will be documented in this file.

## [1.0.0] - 2025-12-31

### Added
- Initial release of ESP32-S3 A121 Sensor I2C controller
- A121Sensor class for I2C communication
- Support for distance measurement
- Support for amplitude (signal strength) reading
- Temperature monitoring capability
- Sensor status monitoring and error detection
- Configuration support with multiple presets:
  - Short range (0-50cm)
  - Medium range (0-150cm)
  - Long range (0-500cm)
  - Low power mode
  - High accuracy mode
- Basic example with continuous distance measurement
- Advanced example with object detection and tracking
- I2C scanner utility for debugging
- Comprehensive README documentation
- PlatformIO configuration for ESP32-S3
- VS Code integration files

### Features
- Configurable I2C pins (default: SDA=GPIO8, SCL=GPIO9)
- 400kHz I2C communication speed
- Automatic sensor initialization and reset
- Error handling and recovery
- Moving average filter for distance measurements
- Serial output for monitoring and debugging

### Hardware Support
- ESP32-S3 DevKitC-1
- Acconeer A121 radar sensor via I2C

### Dependencies
- Arduino framework for ESP32
- Wire library (I2C communication)
- PlatformIO build system
