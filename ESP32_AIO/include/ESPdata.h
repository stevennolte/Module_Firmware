#ifndef ESPDATA_H
#define ESPDATA_H
#include "Arduino.h"
#include "Preferences.h"
#include "Version.h"

class ESPdata
{
private:
    static ESPdata* instance;
    Preferences preferences;  // Add Preferences object
    
public:
    // Singleton pattern methods
    static ESPdata& getInstance();
    static void destroyInstance();
    
    // Updated methods for Preferences
    uint8_t loadConfig();
    uint8_t saveConfig();
    uint8_t updateIP();
    uint8_t updateServer();
    uint8_t updateSteer();
    uint8_t getStrapping();
    uint8_t saveWASzero();
    
    // Constructor remains public for backward compatibility
    ESPdata();

    
        struct GPIOPins {
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
            uint8_t STEER_SWITCH_PIN = 5;
            uint8_t WORK_SWITCH_PIN = 4;
        } pins;
        
        struct I2CAddresses {
            uint8_t TLE_ADDRESS = 0x22;
            uint8_t MCP_ADDRESS = 0x20;
        } i2c;

        struct Wifi {
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
        } wifi;

        struct GPS {
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
            String lastNtripData;
            uint8_t lastNtripDataLen;
            char nmea[100];
            const char* asciiHex = "0123456789ABCDEF";
            bool externalGPS = false;
            uint16_t gpsBaud;
            uint8_t gpsTxPin;
            uint8_t gpsRxPin;
            uint8_t bnoPin;
            uint16_t bnoBaud;
            const bool invertRoll = true;  //Used for IMU with dual antenna
        } gps;

        struct Program {
            char name[64];
            String name2;
            uint8_t version[3];
            uint8_t ledBrht;
            uint8_t confRes;
            uint8_t state;
            uint8_t mcpState;
            uint8_t adsState;
            uint8_t steerDriverState;
            uint32_t lastDebugRequest;
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

        struct Steer {
            bool steerSwitch;
            uint32_t steerSwitchLastTime;
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
            uint16_t minCmd;
            uint16_t maxCmd;
            float minScalar;
            float maxScalar;
            uint32_t lastWAStime;
            uint32_t watchdog;
            float pidCmd;
            uint8_t byte1;
            uint8_t byte2;
            uint8_t byte3;
            uint8_t byte4;
            float absAngle;
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
            uint8_t useADS;
            bool wirelessWAS;
            float wasZeroAngle;
        } steer;

        struct CAN {
            uint8_t txPin;
            uint8_t rxPin;
            uint16_t baudRate;
        } can;
   
       
        struct Joystick {
            uint8_t state;
            bool joyStickActive = false;
            uint32_t lastMsgRecieved;
            uint8_t switchStates[8];
        } joystick;
        
        struct Switch {
            bool steerSwitch;
            bool workSwitch;
            uint32_t workSwitchLastTime;

        } switches;


};

#endif
