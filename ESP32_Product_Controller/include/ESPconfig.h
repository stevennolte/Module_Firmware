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
    uint8_t updateRate();

    class GPIO_Definitions{
        public:
            uint8_t LED_PIN = 48;
            uint8_t SDA_PIN = 41;
            uint8_t SCL_PIN = 42;
            uint8_t SDA_H_PIN = 5;
            uint8_t SCL_H_PIN = 6;
            uint8_t CAN_RX = 2;
            uint8_t CAN_TX = 1;
            uint8_t FLOW_PIN = 14;
            uint8_t sectionPins[5] = {13, 12, 11, 10, 9};
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
            const char* ssids[4] = {"FERT", "SSEI"};
            const char* passwords[4] = {"Fert504!", "Nd14il!la"};
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

    class FlowMeterConfig {
        public:
            uint8_t state;
            uint16_t flowCalNumber = 200;
            uint16_t minFlow = 1;
            uint16_t maxFlow = 100;
            float flowRate = 1.0;
            FlowMeterConfig(){}
    };
    FlowMeterConfig flowCfg;

    class RegData {
        public:
            uint8_t state;
            uint16_t currentPosition;
            uint16_t targetPosition;
            uint8_t speed;
            uint32_t lastMsgRecieved;
            uint32_t cmdID = 0x18EFB480;
            
            struct __attribute__ ((packed)) regReport_Struct_t{
                uint64_t position : 16;
                uint8_t valve_status : 8;
                uint8_t valve_alarms_1 : 8;
                uint8_t valve_alarms_2 : 8;
            };
            
            union regReport_u{
                regReport_Struct_t regReport_Struct;
                uint8_t bytes[sizeof(regReport_Struct_t)];
            } regReport;
            
            struct __attribute__ ((packed)) regID_Struct_t{
                uint32_t id : 32;
            };
            
            union regID_u{
                regID_Struct_t regID_Struct;
                uint8_t bytes[sizeof(regID_Struct_t)];
            } regID;

            struct __attribute__ ((packed)) regCommand_Struct_t{
                uint64_t byte_1 : 8;
                uint64_t dic_index_1 : 8;
                uint64_t dic_index_2 : 8;
                uint64_t sub_index : 8;
                uint64_t byte_5 : 8;
                uint64_t movement_speed : 8;
                uint64_t target : 16;
            };
            union regCommand_u{
                regCommand_Struct_t regCommandStruct;
                uint8_t bytes[sizeof(regCommand_Struct_t)];
            } regCommand;
            RegData(){}
    };
    RegData regData;

    // class JoystickData {
    //     public:
    //         uint8_t state;
    //         uint32_t lastMsgRecieved;
    //         uint8_t switchStates[8];
    //         JoystickData(){}
    // };
    // JoystickData joystickData;

    class FoldData {
        public:
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
            
            FoldData(){}
    } foldData;


    class RateData {
        public:
            uint32_t lastSectionMsg;
            float speed;
            uint8_t sectionStates[65];
            uint8_t state;
            uint8_t regState;
            float targetRate;
            float targetFlowRate;
            float targetRowFlowRate;
            float actualFlowRate;
            float targetPressure;
            float targetPressureOffset;
            uint32_t pulseCount;
            uint32_t prevPulseCount;
            uint64_t pulseTime;
            float frequency;
            RateData(){}
    };
    RateData rateData;

    ESPconfig(/* args */);
    
    
};




#endif
