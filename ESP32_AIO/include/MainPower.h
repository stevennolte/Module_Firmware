#ifndef MAINPOWER_H
#define MAINPOWER_H
#include "Arduino.h"
#include "ESPdata.h"
#include <Adafruit_MCP23X17.h>
#include "Adafruit_ADS1X15.h"

class MainPower
{
    public:
        MainPower(ESPdata* config, Adafruit_MCP23X17* mcp, Adafruit_ADS1115* ads);
        void startTask();
    private:
        uint8_t _powerOn;
        uint8_t _mainPowerPin;
        void getCurrent();
        static void taskHandler(void *param);  // Task handler
        void continuousLoop();  // Function to run in the background task
        ESPdata* _data;
        Adafruit_MCP23X17* _mcp;
        Adafruit_ADS1115* _ads;

};



#endif