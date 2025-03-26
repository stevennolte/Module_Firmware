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
            uint8_t GPS_TX = 14;
            uint8_t GPS_RX = 13;
            uint8_t gpsFix = 10;
            uint8_t rtkFix = 11;
            uint8_t mainPowerPin = 39;
            uint8_t mainPowerDen = 40;
            uint8_t mainPowerInd = 8;
            uint8_t MOTOR_A_PIN = 7;
            uint8_t MOTOR_B_PIN = 8;
            uint8_t MOTOR_PWM_PIN = 9;
            uint8_t ENA = 14;
            uint8_t ENB = 15;
            uint8_t STEER_TEST_PIN = 6;
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

    class MagConfig {
        public:
            uint8_t sensitivity;
            MagConfig(){}
    };

    class IndicatorsConfig {
        public:
            
            uint8_t powerOn = 8;
            uint8_t ethGood = 9;
            
            uint8_t steerStandby = 12;
            uint8_t steerActive = 13;
            // uint8_t indicatorPins[6] = {powerOn, ethGood, gpsFix, rtkFix, steerStandby, steerActive};
            IndicatorsConfig(){}
        private:
            
    };
    IndicatorsConfig indicatorsCfg;

    class GPSConfig {
        public:
            
            bool externalGPS = false;
            uint16_t gpsBaud;
            uint8_t gpsTxPin;
            uint8_t gpsRxPin;
            uint8_t bnoPin;
            uint16_t bnoBaud;
            const bool invertRoll= true;  //Used for IMU with dual antenna

            GPSConfig(){}
    };
    GPSConfig gpsCfg;

    class GPSdata {
        public:
            uint8_t state;
            uint8_t imuState;
            uint8_t positionType;
            uint32_t posAge;
           
            // GGA
            char fixTime[12];
            char latitude[15];
            char latNS[3];
            char longitude[15];
            char lonEW[3];
            char fixQuality[2];
            uint8_t fixQualityInt;
            char numSats[4];
            char HDOP[5];
            char altitude[12];
            char ageDGPS[10];
            // VTG
            char vtgHeading[12] = { };
            char speedKnots[10] = { };
            // IMU
            char imuHeading[6];
            char imuRoll[6];
            char imuPitch[6];
            char imuYawRate[6];
            
            char nmea[100];
            const char* asciiHex = "0123456789ABCDEF";
            GPSdata(){}
    };
    GPSdata gpsData;

    class SteerConfig{
        public:
            uint8_t settingsUpdated;
            uint8_t gainP = 1;
            uint8_t highPWM;
            uint8_t lowPWM;
            uint8_t minPWM;
            uint8_t countsPerDeg;
            uint16_t steerOffset;
            uint8_t ackermanFix;
            uint8_t set0;
            uint8_t pulseCount;
            uint8_t minSpeed;
            uint8_t set1;
            uint16_t steerMsgRate = 100;
            float pidInputFilt;
            float pidOutputFilt;
            
            bool wirelessWAS;
            
            SteerConfig(){}
    };
    SteerConfig steerCfg;

    class SteerData{
        public:
            uint16_t speed;
            uint8_t status;
            float targetSteerAngle;
            uint8_t xte;
            float actSteerAngle;
            uint8_t switchState;
            uint8_t pwmDisplay;
            uint16_t pwmCmd;
            uint8_t testState;
            uint32_t lastSteerOutMsgTime;
            uint32_t steerCurrent;
            float pidOutput;
            float pidInput;
            uint32_t lastWAStime;
            uint32_t watchdog;
            float pidCmd;
            uint8_t byte1;
            uint8_t byte2;
            uint8_t byte3;
            uint8_t byte4;
            SteerData(){}
    };
    SteerData steerData;

    class CANConfig{
        public:
            uint8_t txPin;
            uint8_t rxPin;
            uint16_t baudRate;
            CANConfig(){}
    };
    CANConfig canCfg;
    ESPconfig(/* args */);
    
    
};




#endif
