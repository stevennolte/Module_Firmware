#ifndef GPS_H
#define GPS_H

#include "Arduino.h"
#include "ESPconfig.h"
// #include <TinyGPSPlus.h>
#include "ESPudp.h"
#include "Adafruit_MCP23X17.h"
#include <zNMEAParser.h>
#include <SparkFun_Unicore_GNSS_Arduino_Library.h>
#include "Adafruit_BNO08x_RVC.h"
class ESPudp;

class GPS{
    public:
        GPS(ESPconfig* vars, HardwareSerial* gpsSerial, HardwareSerial* bnoSerial, Adafruit_MCP23X17* mcp);
        void init(ESPudp* espUdp);
        void continuousLoop();
        static void taskHandler(void *param);
        void buildNmea();
        void calculateChecksum();
        void test();
        void sendNTRIP(uint8_t* data, uint8_t len);
        // static void errorHandler();
        void GGA_Handler();
        void displayInfo();
    private:
        static GPS* instance;
        static void staticGGA_Handler();
        char fixTime[12];
        uint32_t imuWatchdog;
        uint32_t gpsWatchdog;
        uint8_t _gpsFixIndPin;
        uint8_t _rtkFixIndPin;
        ESPconfig* espConfig;
        ESPudp* espUdp;
        NMEAParser<2> parser;
        UM980 myGNSS;
        HardwareSerial* gpsSerial;
        HardwareSerial* bnoSerial;
        Adafruit_MCP23X17* mcp;
        
        Adafruit_BNO08x_RVC rvc;
};

#endif