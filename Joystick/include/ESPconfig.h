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
            uint8_t BNO_PIN = 12;
            uint8_t inputPins[10] = {1, 2, 3,4,5,6,7,8,9,43};
            // lhbtn = 10
            // lhlft = 5
            // lhlwr = 6
            // rhlft = 4
            // rhlwr = 7
            // rhbtn = 8
            // cntrlift = 3
            // cntrlowr = 2
            GPIO_Definitions(){}
    };
    GPIO_Definitions gpioDefs;

    class I2C_Definitions{
        public:
            uint8_t TLE_ADDRESS = 0x22;
            uint8_t MCP_ADDRESS = 0x20;
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
            ProgramConfig(){}
    };
    ProgramConfig progCfg;

    class ProgramData {
        public:
            uint8_t state;
            uint8_t mcpState;
            uint8_t adsState;
            uint8_t steerDriverState;
            uint32_t lastDebugRequest;
            ProgramData(){}
    };
    ProgramData progData;
    
    class WifiConfig {
        public:
            const char* ssids[4] = {"NOLTE_FARM", "FERT","SSEI","ss"};
            const char* passwords[4] = {"DontLoseMoney89","Fert504!","Nd14il!la","ss"};
            uint8_t ips[4];
            IPAddress moduleIP;
            // IPAddress AIO_IP;
            uint8_t state;
            uint8_t apMode;
            uint16_t aioPort = 9999;
            uint16_t ntripPort = 2233;
            uint16_t modPort = 8888;
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

    class JoyStick{
        public:
            uint8_t aogByte1 = 0x80;
            uint8_t aogByte2 = 0x81;
            uint8_t sourceAddress = 61;
            uint8_t PGN = 162;
            uint8_t length = 10;
            uint8_t switch1 = 0;
            uint8_t switch2 = 0;
            uint8_t switch3 = 0;
            uint8_t switch4 = 0;
            uint8_t switch5 = 0;
            uint8_t switch6 = 0;
            uint8_t switch7 = 0;
            uint8_t switch8 = 0;

            //lh btn = 10
            bool changeInCmd = false;
            uint8_t leftLift = 4;
            uint8_t leftLower = 5;
            uint8_t rightLift = 3;
            uint8_t rightLower = 6;
            uint8_t centerLift = 2;
            uint8_t centerLower = 1;
            uint8_t autoSteer = 9;
            uint8_t sectionControl = 7;
            JoyStick(){}
    };
    JoyStick joyCmds;
    ESPconfig(/* args */);
    
    
};




#endif
