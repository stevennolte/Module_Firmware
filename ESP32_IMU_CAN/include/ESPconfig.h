#ifndef ESPCONF_H
#define ESPCONF_H
#include "Arduino.h"
#include "LittleFS.h"
#include "ArduinoJson.h"
#include "Version.h"

class ESPconfig
{
public:
    uint8_t loadConfig();
    uint8_t updateIP();
    uint8_t saveConfig();

    class GPIO_Definitions {
        public:
            uint8_t LED_PIN  = 48;
            uint8_t SDA_PIN  = 1;
            uint8_t SCL_PIN  = 2;
            uint8_t CAN_TX   = 12;
            uint8_t CAN_RX   = 11;
            GPIO_Definitions() {}
    };
    GPIO_Definitions gpioDefs;

    class I2C_Definitions {
        public:
            uint8_t BNO_ADDRESS = 0x4B;   // default SparkFun BNO08x address
            I2C_Definitions() {}
    };
    I2C_Definitions i2cDefs;

    class ProgramConfig {
        public:
            char    name[64];
            uint8_t version[3];
            uint8_t ledBrht   = 254;
            uint8_t confRes   = 0;
            uint16_t debugPrintDelay = 1000;
            uint32_t debugPrintTimestamp = 0;
            ProgramConfig() {}
    };
    ProgramConfig progCfg;

    class ProgramData {
        public:
            uint8_t  state       = 0;
            uint8_t  imuState    = 0;   // 0=uninit, 1=ok, 2=error
            uint8_t  canState    = 0;   // 0=uninit, 1=ok, 2=error
            uint32_t debugTimestamp = 0;
            ProgramData() {}
    };
    ProgramData progData;

    class WifiConfig {
        public:
            const char* ssids[4]     = {"FERT", "SSEI", "NOLTE_FARM", ""};
            const char* passwords[4] = {"Fert504!", "Nd14il!la", "DontLoseMoney89", ""};
            uint8_t ips[4]  = {192, 168, 5, 20};
            uint8_t state   = 0;
            uint8_t apMode  = 0;
            WifiConfig() {}
    };
    WifiConfig wifiCfg;

    class OTAConfig {
        public:
            uint8_t state   = 0;
            uint8_t port    = 0;
            uint8_t ipAddr  = 0;
            OTAConfig() {}
    };
    OTAConfig otaCfg;

    class IMUData {
        public:
            float roll           = 0.0f;
            float pitch          = 0.0f;
            float yaw            = 0.0f;   // magnetic heading (deg, 0-360)
            float headingTrue    = 0.0f;   // true north heading (deg, 0-360)
            float magDeclination = 0.0f;   // magnetic declination (deg, + east / - west)
            float headingOffset  = -260.0f; // heading offset correction (deg)
            bool reverseHeading  = false;  // reverse heading direction
            float magX           = 0.0f;   // raw magnetometer X (uT)
            float magY           = 0.0f;   // raw magnetometer Y (uT)
            float magZ           = 0.0f;   // raw magnetometer Z (uT)
            uint8_t accuracy     = 0;
            uint32_t lastUpdate  = 0;
            uint16_t reportInterval = 10;  // ms between BNO reports
            IMUData() {}
    };
    IMUData imuData;

    class CANConfig {
        public:
            // J1939 address claiming
            uint8_t  j1939SA  = 0x80;   // source address (0x00-0xFD; 0xFE = cannot claim)
            uint32_t txFreq   = 50;     // ms between CAN transmits
            uint32_t txTimestamp = 0;
            bool     addressClaimed = false;
            // J1939 NAME fields (used to build the 8-byte NAME for address claiming)
            //   Manufacturer Code 0x7FF = undefined/proprietary
            //   Function  0x7C = Inclination/Angle Sensor
            //   Industry Group 2 = Agricultural
            //   Arbitrary Address Capable = true
            uint32_t identityNumber   = 0;      // 21 bits; filled from MAC at runtime
            uint16_t manufacturerCode = 0x7FF;  // 11 bits
            uint8_t  ecuInstance      = 0;
            uint8_t  functionInstance = 0;
            uint8_t  function         = 0x7C;   // Inclination Sensor
            uint8_t  vehicleSystem    = 0;
            uint8_t  industryGroup    = 2;      // Agriculture
            CANConfig() {}
    };
    CANConfig canCfg;

    ESPconfig();
};

#endif
