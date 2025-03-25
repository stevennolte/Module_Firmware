#ifndef ESPSTEER_H
#define ESPSTEER_H

#include "ESPconfig.h"
#include "MotorDriver.h"
#include "WAS.h"
#include <Wire.h>
#include <Adafruit_MCP23X17.h>
#include <Adafruit_ADS1X15.h>
#include "ESPudp.h"
#include "AutoTunePID.h"

class ESPudp;

class ESPsteer{
    public:
        void begin(ESPudp* espUdp);
        uint32_t getCurrent();
        void setPIDgains();
        ESPsteer(ESPconfig* vars, Adafruit_ADS1115* ads, Adafruit_MCP23X17* mcp);
    private:
        void steerTestLoop();
        void steerLoop();
        uint8_t _status;
        static void taskHandler(void *param);  // Task handler
        void continuousLoop();  // Function to run in the background task
        uint8_t getTestState();
        ESPconfig* espConfig;
        Adafruit_ADS1115* ads;
        Adafruit_MCP23X17* mcp;
        ESPudp* espUdp;
        MotorDriver motorDriver;
        WAS was;
        AutoTunePID pid;

};

#endif