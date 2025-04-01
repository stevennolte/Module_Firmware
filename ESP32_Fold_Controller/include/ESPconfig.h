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
    uint8_t getStrapping();
    

    class GPIO_Definitions{
        public:
            uint8_t LED_PIN = 48;
            uint8_t SDA_PIN = 41;
            uint8_t SCL_PIN = 42;
            
            uint8_t foldPins1[7] = {12,11,15,16,17,18,9};
            uint8_t foldPins2[7] = {6,7,15,16,17,10,8};
            uint8_t directionalValvePin = 13;
         
            GPIO_Definitions(){}
    };
    GPIO_Definitions gpioDefs;

    class I2C_Definitions{
        public:
            uint8_t ADS_ADDRESS = 0x48;
            I2C_Definitions(){}
    };
    I2C_Definitions i2cDefs;

    class ProgramConfig {
        public:
            char name[64];
            String name2;
            uint8_t version[3];
            uint8_t ledBrht;
            
            ProgramConfig(){}
    };
    ProgramConfig progCfg;
    
    class ProgramData {
        public:
            uint8_t state;
            uint8_t adsState;
            uint8_t confRes;
            uint8_t canState;
            ProgramData(){}
    };
    ProgramData progData;

    class WifiConfig {
        public:
            const char* ssids[4] = {"NOLTE_FARM", "FERT", "SSEI"};
            const char* passwords[4] = {"DontLoseMoney89","Fert504!", "Nd14il!la"};
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

    class JoystickData {
        public:
            uint8_t state;
            uint32_t lastMsgRecieved;
            uint8_t switchStates[8];
            JoystickData(){}
    };
    JoystickData joystickData;

    class FoldData {
        public:
            bool joyStickActive = false;
            uint8_t state;
            uint32_t lastMsgRecieved;
            uint8_t foldStates[7];
            uint8_t leftFlip = 0;
            uint8_t leftLift = 1;
            uint8_t leftFold = 2;
            uint8_t center = 3;
            uint8_t rightFold = 4;
            uint8_t rightLift = 5;
            uint8_t rightFlip = 6;
            bool foldOuterWingsState = false;
            bool foldCenterWingsState = false;
            bool raiseWingsState = false;
            uint8_t lhOuterWingRotate = 0;
            uint8_t lhWingRotate = 1;
            uint8_t lhWingLift = 2;
            uint8_t centerLift = 3;
            uint8_t rhWingLift = 4;
            uint8_t rhWingRotate = 5;
            uint8_t rhOuterWingRotate = 6;
            FoldData(){}
    } foldData;



    ESPconfig(/* args */);
    
    
};




#endif
