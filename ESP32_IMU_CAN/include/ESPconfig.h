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
            uint8_t SDA_PIN  = 41;
            uint8_t SCL_PIN  = 42;
            uint8_t CAN_TX   = 1;
            uint8_t CAN_RX   = 2;
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
            float roll  = 0.0f;
            float pitch = 0.0f;
            float yaw   = 0.0f;
            uint8_t accuracy = 0;
            uint32_t lastUpdate = 0;
            uint16_t reportInterval = 10;  // ms between BNO reports
            IMUData() {}
    };
    IMUData imuData;

    class CANConfig {
        public:
            uint32_t txID    = 0x100;   // 11-bit standard CAN ID for IMU data
            bool     extFrame = false;  // false = standard 11-bit frame
            uint32_t txFreq  = 50;      // ms between CAN transmits
            uint32_t txTimestamp = 0;
            CANConfig() {}
    };
    CANConfig canCfg;

    ESPconfig();
};

#endif
