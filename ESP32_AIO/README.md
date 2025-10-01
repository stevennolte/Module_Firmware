# ESP32-AIO Agricultural Controller

## Overview

The ESP32-AIO (All-In-One) is a comprehensive precision agriculture steering control system designed for autonomous guidance of agricultural equipment. Built on the ESP32-S3 platform, it provides centimeter-level positioning accuracy and real-time steering control for tractors and other agricultural vehicles.

## Features

### Core Functionality
- **Precision Steering Control**: PID-based closed-loop steering with configurable parameters
- **GPS/GNSS Integration**: Multi-constellation support with RTK correction capability
- **IMU Integration**: BNO055 9-DOF sensor for heading and attitude determination
- **Wheel Angle Sensing**: Support for both wired (analog) and wireless wheel angle sensors

### Hardware Interfaces
- **I2C Expansion**: MCP23017 16-bit I/O expander for additional GPIO
- **Analog Sensing**: ADS1115 16-bit ADC for high-precision sensor readings
- **Motor Control**: H-bridge motor driver with current monitoring
- **WiFi Connectivity**: Dual-band 802.11 b/g/n with AP and station modes
- **Status Indication**: RGB LED with configurable brightness and animation patterns

### Software Architecture
- **Real-time Operation**: FreeRTOS multithreading for responsive control
- **Web Interface**: AsyncWebServer for configuration and monitoring
- **Data Persistence**: NVS storage with power cycle detection
- **Serial Commands**: Runtime control via USB serial interface
- **Recovery Mode**: Emergency configuration access when normal boot fails

## System Architecture

### Core Components

#### ESPdata Singleton
Central data management system providing:
- Configuration persistence with NVS storage
- Power cycle detection using RTC memory
- Thread-safe access to all system parameters
- Boot state management and error recovery

#### Steering Control System
Multi-threaded precision steering implementation:
- Real-time PID control loop (3ms cycle time)
- Wheel angle sensor feedback processing
- Motor driver control with safety interlocks
- Configurable steering parameters and limits

#### Navigation System
Comprehensive positioning and guidance:
- GPS receiver with NMEA sentence parsing
- IMU sensor integration for attitude determination
- NTRIP client for RTK correction data
- Position quality monitoring and validation

#### Communication Systems
Multiple connectivity options:
- WiFi network management with automatic reconnection
- Web-based configuration interface
- Serial command interface for debugging
- Future CAN bus integration capability

## Hardware Configuration

### Pin Assignments
```cpp
// I2C Communication
SDA_PIN = 41
SCL_PIN = 42

// Serial Communication
GPS_TX = 14, GPS_RX = 13
BNO_PIN = 12 (IMU communication)

// Motor Control
MOTOR_A_PIN = 7, MOTOR_B_PIN = 8
MOTOR_PWM_PIN = 9
ENA = 14, ENB = 15

// Switch Inputs
STEER_SWITCH_PIN = 5
WORK_SWITCH_PIN = 4
STEER_TEST_PIN = 6

// Status Indicators
LED_PIN = 48 (RGB NeoPixel)
```

### I2C Devices
- **MCP23017** (0x20): 16-bit I/O expander for switches and LEDs
- **ADS1115** (0x48): 16-bit ADC for wheel angle sensor
- **TLx493D** (0x22): Magnetic wheel angle sensor (optional)

## Software Features

### Power Management
- RTC memory-based power cycle detection
- Boot counter with automatic recovery mode
- Software reboot vs power cycle differentiation
- Configuration state validation and recovery

### Safety Systems
- Steering system watchdog timer
- Motor current monitoring and protection
- Emergency override switches
- Automatic steering disengagement on fault

### Configuration Management
- Web-based parameter adjustment
- Real-time parameter updates without restart
- Configuration backup and restore
- Factory reset capability

### Status Monitoring
- RGB LED status indication with color codes
- Web-based system diagnostics
- Serial debug output with real-time telemetry
- Component health monitoring

## Development Information

### Build System
- PlatformIO development environment
- ESP-IDF framework with Arduino compatibility
- Automatic dependency management
- OTA update capability

### Code Organization
```
src/           - Source implementation files
include/       - Header files and interfaces
lib/           - External libraries
data/          - Web interface files (HTML, CSS, JS)
```

### Key Classes
- **ESPdata**: Central data management singleton
- **ESPsteer**: Steering control system
- **ESPGPS**: GPS and IMU management
- **MCPManager**: I/O expander control
- **MyLED**: Status indication system

## Usage

### Initial Setup
1. Connect hardware according to pin assignments
2. Upload firmware via USB or OTA
3. Configure WiFi credentials via web interface
4. Calibrate wheel angle sensor zero position
5. Adjust PID parameters for vehicle characteristics

### Operation Modes
- **Normal Mode**: Full autonomous steering operation
- **Test Mode**: Manual steering control for calibration
- **Recovery Mode**: Emergency configuration access

### Web Interface
Access the configuration interface at the device IP address:
- System status and diagnostics
- Real-time parameter adjustment
- Network configuration
- Firmware update capability

## Version Information
- **Current Version**: 1.1.005
- **Target Platform**: ESP32-S3
- **Development Framework**: Arduino/ESP-IDF
- **License**: [Specify license here]

## Contributing
[Contribution guidelines and development setup instructions]

## Support
[Support contact information and documentation links]
