/**
 * @file ESPdata.h
 * @brief Central data management system for ESP32-AIO agricultural controller
 * 
 * @details This header defines the ESPdata singleton class that manages all system
 *          configuration, runtime data, and persistent storage for the ESP32-AIO
 *          agricultural steering controller. Provides centralized access to:
 *          - System configuration parameters and network settings
 *          - GPS, IMU, and steering sensor data
 *          - Hardware pin assignments and component states
 *          - NVS (Non-Volatile Storage) persistence with power cycle detection
 *          - RTC memory for boot state management across resets
 * 
 * @author Steve Gavel
 * @date 2024
 * @version 4.6.1
 * 
 * @see ESPdata.cpp for implementation details
 * @see main.cpp for usage examples
 */

#ifndef ESPDATA_H
#define ESPDATA_H
#include "Arduino.h"
#include "Preferences.h"
#include "Version.h"

/**
 * @brief RTC memory structure for persistent data across software reboots
 * 
 * @details This structure is stored in RTC memory which persists across
 *          software resets but is cleared on power cycles. Used for:
 *          - Detecting true power cycles vs software resets
 *          - Counting software boots since last power cycle
 *          - Tracking system state and boot mode preferences
 */
struct RTCData {
    uint32_t magic;           ///< @brief Validation number (0xDEADBEEF) to detect power cycles
    uint32_t softwareBoots;   ///< @brief Count of software reboots since power cycle
    uint32_t lastUptime;      ///< @brief Uptime (milliseconds) before last reboot
    uint8_t lastBootMode;     ///< @brief Last requested boot mode (1=normal, 2=recovery)
    uint32_t lastResetReason; ///< @brief Last reset reason from esp_reset_reason()
};

/**
 * @brief Singleton class for centralized system data management
 * 
 * @details Provides thread-safe access to all system configuration and runtime data.
 *          Implements the singleton pattern to ensure consistent data access across
 *          all system components. Manages NVS persistence and RTC memory state.
 */
class ESPdata
{
private:
    static ESPdata* instance; ///< @brief Singleton instance pointer
    Preferences preferences;  ///< @brief NVS preferences object for persistent storage
    
public:
    // Singleton pattern methods
    /**
     * @brief Gets the singleton instance of ESPdata
     * @return Reference to the unique ESPdata instance
     * @note Thread-safe singleton implementation
     */
    static ESPdata& getInstance();
    
    /**
     * @brief Destroys the singleton instance (cleanup)
     * @note Called during system shutdown or reset
     */
    static void destroyInstance();
    
    // Configuration management methods
    /**
     * @brief Loads system configuration from NVS storage
     * @return uint8_t Status code (0=success, 1=partial failure, 2=complete failure)
     * @details Initializes RTC memory and loads all configuration parameters
     */
    uint8_t loadConfig();
    
    /**
     * @brief Saves current configuration to NVS storage  
     * @return uint8_t Status code (0=success, other=failure)
     * @details Persists all configuration changes to non-volatile storage
     */
    uint8_t saveConfig();
    
    /**
     * @brief Updates the IP address configuration
     * @return uint8_t Status code (0=success, other=failure)
     * @details Updates network configuration in memory and storage
     */
    uint8_t updateIP();
    /**
     * @brief Updates server configuration parameters
     * @return uint8_t Status code (0=success, other=failure)
     */
    uint8_t updateServer();
    
    /**
     * @brief Updates steering system configuration
     * @return uint8_t Status code (0=success, other=failure)
     */
    uint8_t updateSteer();
    
    /**
     * @brief Reads GPIO strapping pins for hardware detection
     * @return uint8_t Strapping pin state value
     */
    uint8_t getStrapping();
    
    /**
     * @brief Saves wheel angle sensor zero calibration value
     * @return uint8_t Status code (0=success, other=failure)
     */
    uint8_t saveWASzero();
    
    /**
     * @brief Sets the current program state
     * @param state Program state (1=normal, 2=recovery)
     * @return true if state was set successfully
     */
    bool setState(uint8_t state);
    
    /**
     * @brief Sets the boot mode preference for next restart
     * @param mode Boot mode (1=normal, 2=recovery)
     * @return true if boot mode was set successfully
     */
    bool setBootMode(uint8_t mode);
    
    /**
     * @brief Sets the MCP23017 I/O expander state
     * @param state Component state (1=active, 2=failed)
     * @return true if state was set successfully
     */
    bool setMCPstate(uint8_t state);
    
    /**
     * @brief Sets the I2C two-wire interface state
     * @param state Interface state (1=active, 2=failed)
     * @return true if state was set successfully
     */
    bool setTwoWireState(uint8_t state);
    
    /**
     * @brief Sets the ADS1115 ADC state
     * @param state Component state (1=active, 2=failed)
     * @return true if state was set successfully
     */
    bool setADSstate(uint8_t state);
    
    /**
     * @brief Sets the boot counter value
     * @param count New boot count value
     * @return true if count was set successfully
     */
    bool setBootCount(uint32_t count);
    
    // RTC Data methods
    /**
     * @brief Initializes RTC memory data structure
     * @details Sets up power cycle detection and boot tracking
     */
    void initRTCData();
    
    /**
     * @brief Checks if the system has been through a power cycle
     * @return true if this is a power-on reset, false for software reset
     */
    bool isPowerCycle();
    
    /**
     * @brief Updates RTC data before system reboot
     * @param newBootMode Boot mode to request for next startup
     */
    void updateRTCBeforeReboot(uint8_t newBootMode);
    
    /**
     * @brief Gets the count of software boots since last power cycle
     * @return uint32_t Number of software reboots
     */
    uint32_t getSoftwareBootCount();
    
    // Constructor remains public for backward compatibility
    ESPdata();

    /**
     * @brief GPIO pin assignments for hardware interfaces
     * @details Defines all pin connections for the ESP32-S3 board including:
     *          - I2C communication pins (SDA/SCL)
     *          - Serial communication (GPS, IMU)
     *          - Motor driver control pins
     *          - Switch and sensor inputs
     *          - Status indicator outputs
     */
    struct GPIOPins {
        uint8_t LED_PIN = 48;           ///< @brief Built-in LED pin for status indication
        uint8_t SDA_PIN = 41;           ///< @brief I2C data line for sensor communication
        uint8_t SCL_PIN = 42;           ///< @brief I2C clock line for sensor communication
        uint8_t BNO_PIN = 12;           ///< @brief IMU (BNO055) serial communication pin
        uint8_t GPS_TX = 14;            ///< @brief GPS module transmit pin
        uint8_t GPS_RX = 13;            ///< @brief GPS module receive pin
        uint8_t gpsFix = 10;            ///< @brief GPS fix status indicator pin
        uint8_t rtkFix = 11;            ///< @brief RTK fix status indicator pin
        uint8_t mainPowerPin = 39;      ///< @brief Main power control pin
        uint8_t mainPowerDen = 40;      ///< @brief Main power enable pin
        uint8_t mainPowerInd = 8;       ///< @brief Main power indicator pin
        uint8_t MOTOR_A_PIN = 7;        ///< @brief Motor driver direction pin A
        uint8_t MOTOR_B_PIN = 8;        ///< @brief Motor driver direction pin B
        uint8_t MOTOR_PWM_PIN = 9;      ///< @brief Motor driver PWM speed control pin
        uint8_t ENA = 14;               ///< @brief Motor driver enable A pin
        uint8_t ENB = 15;               ///< @brief Motor driver enable B pin
        uint8_t STEER_TEST_PIN = 6;     ///< @brief Steering system test mode pin
        uint8_t STEER_SWITCH_PIN = 5;   ///< @brief Steering engage/disengage switch pin
        uint8_t WORK_SWITCH_PIN = 4;    ///< @brief Work/implement control switch pin
    } pins;
    
    /**
     * @brief ADS1115 ADC configuration for analog sensor inputs
     * @details Configures the 16-bit ADC for high-precision analog measurements
     */
    struct ADSConfig {
        uint8_t mainPowerISpin = 3;     ///< @brief ADS channel for main power current sensing
    } adsConfig;

    /**
     * @brief MCP23017 I/O expander GPIO pin assignments
     * @details Defines input and output pin mappings for the MCP23017 16-bit I/O expander
     *          used for additional digital I/O beyond the ESP32's native pins
     */
    struct MCPgpio {
        /**
         * @brief Input pin assignments on MCP23017
         * @details Digital inputs for switch and sensor monitoring
         */
        struct inputs{
            uint8_t work_switch = 0;    ///< @brief Work/implement engage switch input
            uint8_t remote_switch = 1;  ///< @brief Remote control switch input
            uint8_t steer_switch = 2;   ///< @brief Steering engage switch input
        } inputs;
        
        /**
         * @brief Output pin assignments on MCP23017
         * @details Digital outputs for status indication and control signals
         */
        struct outputs{
            uint8_t power_on = 8;       ///< @brief System power-on indicator LED
            uint8_t eth_good = 9;       ///< @brief Ethernet connection status LED
            uint8_t gps_fix = 10;       ///< @brief GPS fix status indicator LED
            uint8_t rtk_fix = 11;       ///< @brief RTK correction status indicator LED
            uint8_t steer_standby = 12; ///< @brief Steering system standby indicator LED
            uint8_t steer_active = 13;  ///< @brief Steering system active indicator LED
            uint8_t motor_enb = 14;     ///< @brief Motor driver enable B output
            uint8_t motor_ena = 15;     ///< @brief Motor driver enable A output
        } outputs;
    } mcpPins;

    /**
     * @brief I2C device addresses for hardware components
     * @details Defines the I2C slave addresses for connected sensors and peripherals
     */
    struct I2CAddresses {
        uint8_t TLE_ADDRESS = 0x22;     ///< @brief TLx493D magnetic wheel angle sensor address
        uint8_t MCP_ADDRESS = 0x20;     ///< @brief MCP23017 I/O expander address
    } i2c;

    /**
     * @brief WiFi network configuration and connection management
     * @details Manages wireless network connectivity with multiple SSID support
     *          and network service port configurations
     */
    struct Wifi {
        const char* ssids[4] = {"NOLTE_FARM", "FERT","SSEI","ss"};        ///< @brief Available WiFi network SSIDs
        const char* passwords[4] = {"DontLoseMoney89","Fert504!","Nd14il!la","ss"}; ///< @brief WiFi network passwords
        uint8_t ips[4];             ///< @brief IP address octets for static assignment
        IPAddress moduleIP;         ///< @brief Module's assigned IP address
        uint8_t state;              ///< @brief WiFi connection state (0=disconnected, 1=connected, 2=failed)
        uint8_t apMode;             ///< @brief Access Point mode flag (0=station, 1=AP)
        uint16_t aioPort = 9999;    ///< @brief AgIO communication service port
        uint16_t ntripPort = 2233;  ///< @brief NTRIP correction data service port
        uint16_t modPort = 8888;    ///< @brief Module communication service port
    } wifi;

    /**
     * @brief Power management and monitoring data
     * @details Tracks main system power consumption and status
     */
    struct Power {
        float mainCurrent;          ///< @brief Main power rail current consumption (Amps)
        uint16_t mainCurrentRaw;    ///< @brief Raw ADC reading for current measurement
    } power;

    /**
     * @brief GPS receiver and IMU sensor data structure
     * @details Contains all navigation, positioning, and orientation data from
     *          GPS receiver and inertial measurement unit
     */
    struct GPS {
        uint8_t state;              ///< @brief GPS receiver state (0=off, 1=searching, 2=fixed)
        uint8_t imuState;           ///< @brief IMU state (0=off, 1=calibrating, 2=ready)
        uint8_t positionType;       ///< @brief Position solution type (1=GPS, 2=DGPS, 4=RTK_FIXED, 5=RTK_FLOAT)
        uint32_t posAge;            ///< @brief Age of position solution in milliseconds
    
        // GGA message data
        char fixTime[12];           ///< @brief UTC time of GPS fix (HHMMSS.SSS format)
        char latitude[15];          ///< @brief Latitude in degrees and decimal minutes
        char latNS[3];              ///< @brief Latitude hemisphere (N/S)
        char longitude[15];         ///< @brief Longitude in degrees and decimal minutes
        char lonEW[3];              ///< @brief Longitude hemisphere (E/W)
        char fixQuality[2];         ///< @brief GPS fix quality indicator string
        uint8_t fixQualityInt;      ///< @brief GPS fix quality as integer (0=invalid, 1=GPS, 2=DGPS, etc.)
        char numSats[4];            ///< @brief Number of satellites in use
        char HDOP[5];               ///< @brief Horizontal dilution of precision
        char altitude[12];          ///< @brief Altitude above mean sea level (meters)
        char ageDGPS[10];           ///< @brief Age of differential GPS corrections (seconds)
        
        // VTG message data
        char vtgHeading[12] = { };  ///< @brief True heading from VTG message (degrees)
        char speedKnots[10] = { };  ///< @brief Ground speed in knots
        
        // IMU sensor data
        char imuHeading[6];         ///< @brief IMU compass heading (degrees)
        char imuRoll[6];            ///< @brief IMU roll angle (degrees)
        char imuPitch[6];           ///< @brief IMU pitch angle (degrees)
        char imuYawRate[6];         ///< @brief IMU yaw rate (degrees/second)
        
        String lastNtripData;       ///< @brief Last received NTRIP correction data
        uint8_t lastNtripDataLen;   ///< @brief Length of last NTRIP data packet
        char nmea[100];             ///< @brief Raw NMEA sentence buffer
        const char* asciiHex = "0123456789ABCDEF"; ///< @brief Hex encoding lookup table
        bool externalGPS = false;   ///< @brief GPS source flag (false=onboard, true=wireless)
        uint16_t gpsBaud;           ///< @brief GPS communication baud rate
        uint8_t gpsTxPin;           ///< @brief GPS module transmit pin assignment
        uint8_t gpsRxPin;           ///< @brief GPS module receive pin assignment
        uint8_t bnoPin;             ///< @brief BNO055 IMU communication pin
        uint16_t bnoBaud;           ///< @brief BNO055 IMU communication baud rate
        const bool invertRoll = true; ///< @brief IMU roll inversion flag for dual antenna setup
    } gps;

    /**
     * @brief System program state and configuration data
     * @details Contains program metadata, boot information, and component states
     */
    struct Program {
        char name[64];              ///< @brief Program name string buffer
        String name2;               ///< @brief Alternative program name (String object)
        uint8_t version[3];         ///< @brief Program version array [major, minor, patch]
        uint8_t ledBrht;            ///< @brief LED brightness level (0-255)
        uint8_t confRes;            ///< @brief Configuration load result (0=success, 1=partial, 2=failed)
        uint8_t state;              ///< @brief Program operational state (1=normal, 2=recovery)
        uint8_t mcpState;           ///< @brief MCP23017 component state (1=active, 2=failed)
        uint8_t adsState;           ///< @brief ADS1115 component state (1=active, 2=failed)
        uint8_t steerDriverState;   ///< @brief Steering motor driver state
            uint32_t lastDebugRequest;
            uint32_t bootcount;
            uint8_t bootMode;
            uint8_t twoWireState;
        } program;

        struct OTA {
            uint8_t state;
            uint8_t port;
            uint8_t ipAddr;
            char basePath[64];
        } ota;

        struct Indicators {
            uint8_t powerOn = 8;
            uint8_t ethGood = 9;
            uint8_t steerStandby = 12;
            uint8_t steerActive = 13;
            // uint8_t indicatorPins[6] = {powerOn, ethGood, gpsFix, rtkFix, steerStandby, steerActive};
        } indicators;

    /**
     * @brief Steering system control and sensor data
     * @details Comprehensive steering system management including PID control,
     *          wheel angle sensing, motor control, and safety monitoring
     */
    struct Steer {
        bool steerSwitch = false;           ///< @brief Steering engage switch state (true=engaged)
        uint32_t steerSwitchLastTime = 0; ///< @brief Last steering switch activation timestamp
        uint16_t speed = 0;             ///< @brief Vehicle ground speed for speed-dependent control
        uint8_t status = 0;             ///< @brief Steering system status code
        float targetSteerAngle = 0.0;     ///< @brief Target steering angle from guidance system (degrees)
        uint8_t xte = 0;                ///< @brief Cross-track error magnitude
        float actSteerAngle = 0.0;        ///< @brief Actual measured steering angle (degrees)
        uint8_t switchState = 0;        ///< @brief Physical switch state reading
        uint8_t pwmDisplay = 0;         ///< @brief PWM value for display purposes
        uint16_t pwmCmd = 0;            ///< @brief Current PWM command to motor driver
        uint8_t testState = 0;          ///< @brief Steering test mode state
        uint32_t lastSteerOutMsgTime = 0; ///< @brief Timestamp of last steering output message
        uint32_t steerCurrent = 0;      ///< @brief Motor current consumption measurement
        float pidOutput = 0.0;            ///< @brief PID controller output value
        float pidInput = 0.0;             ///< @brief PID controller input value (angle error)
        uint16_t minCmd = 0;            ///< @brief Minimum motor command value
        uint16_t maxCmd = 0;            ///< @brief Maximum motor command value
        float minScalar = 0.0;            ///< @brief Minimum scaling factor
        float maxScalar = 0.0;            ///< @brief Maximum scaling factor
        uint32_t lastWAStime = 0;       ///< @brief Timestamp of last wheel angle sensor reading
        uint32_t watchdog = 0;          ///< @brief Steering system watchdog timer
        float pidCmd = 0.0;               ///< @brief PID controller command value
        uint8_t byte1 = 0;              ///< @brief Steering data byte 1 (protocol specific)
        uint8_t byte2 = 0;              ///< @brief Steering data byte 2 (protocol specific)
        uint8_t byte3 = 0;              ///< @brief Steering data byte 3 (protocol specific)
        uint8_t byte4 = 0;              ///< @brief Steering data byte 4 (protocol specific)
        float absAngle = 0.0;             ///< @brief Absolute steering angle (degrees)
        uint8_t settingsUpdated = 0;    ///< @brief Flag indicating steering settings have been updated
        uint8_t gainP = 1;          ///< @brief PID proportional gain value
        uint8_t highPWM = 255;      ///< @brief High-speed PWM limit for motor control
        uint8_t lowPWM = 0;         ///< @brief Low-speed PWM limit for motor control
        uint8_t minPWM = 0;             ///< @brief Minimum PWM threshold for motor activation
        uint8_t countsPerDeg = 0;       ///< @brief Sensor counts per degree of steering angle
        uint16_t steerOffset = 0;       ///< @brief Steering angle offset calibration value
        uint8_t ackermanFix = 0;        ///< @brief Ackerman steering geometry correction factor
        uint8_t set0 = 0;               ///< @brief Configuration setting 0 (multipurpose)
        uint8_t pulseCount = 0;         ///< @brief Pulse counter for encoder-based sensors
        uint8_t minSpeed = 0;           ///< @brief Minimum vehicle speed for steering activation
        uint8_t set1 = 0;               ///< @brief Configuration setting 1 (multipurpose)
        uint16_t steerMsgRate = 100; ///< @brief Steering message update rate (milliseconds)
        float pidInputFilt = 0.0;         ///< @brief PID input filter coefficient (0.0-1.0)
        float pidOutputFilt = 0.0;        ///< @brief PID output filter coefficient (0.0-1.0)
        uint8_t useADS = 0;     
        uint16_t rawADS = 0;        ///< @brief Raw ADS1115 ADC value for analog sensors
        bool wirelessWAS = false;           ///< @brief Wheel angle sensor source (false=wired, true=wireless)
        float wasZeroAngle = 0.0;     ///< @brief Wheel angle sensor zero calibration value
        uint32_t looptime = 0;   
        uint32_t looptimestamp = 0; 
    } steer;

    /**
     * @brief CAN bus communication configuration
     * @details Future expansion for CAN bus integration with agricultural implements
     */
    struct CAN {
        uint8_t txPin;              ///< @brief CAN transmit pin assignment
        uint8_t rxPin;              ///< @brief CAN receive pin assignment
        uint16_t baudRate;          ///< @brief CAN bus communication baud rate
    } can;
   
    /**
     * @brief Joystick control interface data structure
     * @details Future expansion for joystick/gamepad control integration
     */
    struct Joystick {
        uint8_t state;              ///< @brief Joystick connection state
        bool joyStickActive = false; ///< @brief Joystick active flag
        uint32_t lastMsgRecieved;   ///< @brief Timestamp of last joystick message (note: keeping original spelling for compatibility)
        uint8_t switchStates[8];    ///< @brief Array of joystick switch states
    } joystick;
    
    /**
     * @brief Physical switch input monitoring
     * @details Tracks state of physical switches for manual control override
     */
    struct Switch {
        bool steerSwitch;           ///< @brief Steering engage/disengage switch state
        bool workSwitch;            ///< @brief Work/implement control switch state
        uint32_t workSwitchLastTime; ///< @brief Last work switch activation timestamp (for debouncing)
    } switches;
};

#endif
