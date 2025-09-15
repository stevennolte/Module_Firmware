#ifndef ESPSTEER_H
#define ESPSTEER_H

#include "ESPdata.h"
#include "MotorDriver.h"
#include "WAS.h"
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include <Adafruit_ADS1X15.h>
#include "ESPudp.h"
#include "AutoTunePID.h"
#include "MCPManager.h"

class ESPudp;

class ESPsteer{
    public:
        void begin(ESPudp* espUdp);
        uint32_t getCurrent();
        void setPIDgains();
        ESPsteer(ESPdata* vars, Adafruit_ADS1115* ads);  // Removed MCP parameter
        WAS was;
    private:
        void steerTestLoop();
        void steerLoop();
        uint8_t _status;
        static void taskHandler(void *param);  // Task handler
        void continuousLoop();  // Function to run in the background task
        uint8_t getTestState();
        ESPdata* espData;
        Adafruit_ADS1115* ads;
        MCPManager& mcpManager;  // Reference to MCPManager singleton
        ESPudp* espUdp;
        MotorDriver motorDriver;
        
        AutoTunePID pid;

};

#endif