#ifndef GPS_H
#define GPS_H

#include "Arduino.h"
#include "ESPdata.h"
// #include <TinyGPSPlus.h>
#include "ESPudp.h"
#include "ESPdata_macros.h"


#include <zNMEAParser.h>
#include <SparkFun_Unicore_GNSS_Arduino_Library.h>
#include "Adafruit_BNO08x_RVC.h"
#include "Adafruit_MCP23X17.h"
#include "MCPManager.h"

class ESPudp;

class ESPGPS{
    public:
        
        // New constructor using MCPManager singleton (no MCP pointer needed)
        ESPGPS(ESPdata* vars, HardwareSerial* gpsSerial, HardwareSerial* bnoSerial);

        void init(ESPudp* espUdp);
        
        // Alternative method using MCPManager singleton
        void initWithSingleton(ESPudp* espUdp);
        
        void continuousLoop();
        static void taskHandler(void *param);
        void buildNmea();
        void calculateChecksum();
        void test();
        void sendNTRIP(uint8_t* data, uint8_t len);
        // static void errorHandler();
        void GGA_Handler();
        void displayInfo();
        
        // MCPManager helper methods
        void updateGPSIndicators();
        void setGPSIndicators(bool hasGPSFix, bool hasRTKFix);
    private:
        static ESPGPS* instance;
        static void staticGGA_Handler();
        char fixTime[12];
        uint32_t imuWatchdog;
        uint32_t gpsWatchdog;
        uint8_t _gpsFixIndPin;
        uint8_t _rtkFixIndPin;
        ESPdata* espData;
        ESPudp* espUdp;
        MCPManager& mcpManager;  // Reference to MCPManager singleton
        NMEAParser<2> parser;
        UM980 myGNSS;
        HardwareSerial* gpsSerial;
        HardwareSerial* bnoSerial;

        Adafruit_MCP23X17 mcp;
        Adafruit_BNO08x_RVC rvc;
};

#endif