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
    uint8_t updateServer();
    uint8_t updateSteer();
    uint8_t getStrapping();

    class GPIO_Definitions{
        public:
            uint8_t LED_PIN = 48;
            uint8_t SDA_PIN = 41;
            uint8_t SCL_PIN = 42;
            uint8_t SDA_H_PIN = 5;
            uint8_t SCL_H_PIN = 6;
            
            
            GPIO_Definitions(){}
    };
    GPIO_Definitions gpioDefs;

    class I2C_Definitions{
        public:
            uint8_t TMAG_ADDRESS = 0x22;
            uint8_t MCP_ADDRESS = 0x60;
            I2C_Definitions(){}
    };
    I2C_Definitions i2cDefs;

    class ProgramConfig {
        public:
            char name[64];
            String name2;
            uint8_t version[3];
            uint8_t ledBrht;
            uint8_t confRes;
            uint8_t magState;
            uint8_t dacState;
            
            ProgramConfig(){}
    };
    ProgramConfig progCfg;
    
    class ProgramData {
        public:
            uint8_t state;
            uint8_t tmagState;
            uint8_t dacState;
            uint8_t steerDriverState;
            uint32_t debugTimestamp;
            ProgramData(){}
    };
    ProgramData progData;

    class WifiConfig {
        public:
            const char* ssids[4] = {"FERT", "SSEI","NOLTE_FARM"};
            const char* passwords[4] = {"Fert504!", "Nd14il!la","DontLoseMoney89"};
            uint8_t ips[4];
            uint8_t state;
            uint8_t apMode;
            WifiConfig(){}
    };
    WifiConfig wifiCfg;


    class OTAConfig {
        public:
            uint8_t state;
            uint8_t port;
            uint8_t ipAddr;
            char basePath[64];
            OTAConfig(){}
    };
    OTAConfig otaCfg;

    class WasData {
        public:
            uint16_t udpFreq = 100;
            uint32_t udpTimestamp;
            uint8_t state;
            uint8_t mode;
            uint16_t setPoint;
            uint16_t angle;   // Angle in degrees x 100
            uint16_t sigReading;
            float sensorAngle;
            float x;
            float y;
            float z;
            float t;
            WasData(){}
    };
    WasData wasData;

    ESPconfig(/* args */);
    
    
};




#endif
