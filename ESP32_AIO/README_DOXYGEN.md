# ESP32-AIO Agricultural Controller Documentation

## Project Overview

The ESP32-AIO (All-In-One) Agricultural Controller is a precision steering control system designed for agricultural vehicles and implements. This system provides centimeter-level accuracy for auto-guidance applications including:

- **Precision Steering Control**: PID-based closed-loop steering with real-time feedback
- **GPS/RTK Integration**: Multi-constellation GNSS with RTK correction support
- **Wireless Connectivity**: WiFi network management with automatic failover
- **Sensor Integration**: Wheel angle sensors, IMU, current monitoring
- **Web Interface**: Complete configuration and monitoring via web browser
- **Safety Systems**: Multiple redundancy and emergency override capabilities

## Key Features

### Hardware Integration
- **ESP32-S3**: Main controller with dual-core processing
- **MCP23017**: 16-bit I/O expander for additional GPIO
- **ADS1115**: 16-bit ADC for precision analog measurements
- **BNO055**: 9-DOF IMU for heading and attitude
- **GPS Module**: Multi-constellation GNSS receiver

### Control Systems
- **PID Controller**: Auto-tuning capable with configurable parameters
- **Motor Driver**: Bidirectional control with current monitoring
- **Wheel Angle Sensor**: Both wired (analog) and wireless support
- **Safety Interlocks**: Emergency stop and manual override

### Connectivity
- **WiFi Client/AP**: Automatic network connection with fallback AP mode
- **NTRIP Client**: Real-time correction data for RTK positioning
- **Web Server**: Full-featured configuration and monitoring interface
- **Serial Interface**: Runtime command and debugging support

### Software Architecture
- **Singleton Pattern**: Thread-safe data management across all components
- **FreeRTOS Tasks**: Multi-threaded operation for real-time performance
- **NVS Storage**: Non-volatile configuration with power cycle detection
- **RTC Memory**: Boot state persistence across software resets

## System Architecture

### Core Components
1. **ESPdata**: Central data management singleton
2. **ESPGPS**: GPS receiver and IMU integration
3. **ESPsteer**: Steering control system with PID
4. **MCPManager**: I/O expander management
5. **ESPWifi**: Network connectivity management

### Data Flow
```
GPS/IMU → ESPdata ← Web Interface
    ↓        ↓         ↑
Navigation → Steering Control → Motor Driver
    ↓        ↑
Guidance ← Wheel Angle Sensor
```

## Getting Started

### Prerequisites
- PlatformIO IDE or Arduino IDE with ESP32 support
- ESP32-S3 development board
- Required hardware components (see Hardware section)

### Building
1. Clone the repository
2. Open in PlatformIO
3. Build and upload to ESP32-S3

### Configuration
1. Power on the device
2. Connect to ESP32_AIO_AP network (password: recovery123)
3. Navigate to 192.168.4.1 for initial setup
4. Configure WiFi, GPS, and steering parameters

## Documentation Structure

- **Classes**: Core system classes and their interfaces
- **Files**: Source and header file documentation
- **Data Structures**: Configuration and runtime data organization
- **Functions**: Detailed function documentation with parameters

## Version Information
- **Version**: 4.6.1
- **Author**: Steve Gavel
- **Date**: 2024
- **License**: Proprietary

## Support
For technical support and documentation updates, please refer to the project repository or contact the development team.
