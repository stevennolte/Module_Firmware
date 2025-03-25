#ifndef WAS_H
#define WAS_H

#define WAS_DEBUG

#include "Arduino.h"
#include "ESPconfig.h"
#include <Wire.h>
#include "Adafruit_ADS1X15.h"


class WAS{
    public:
        WAS(ESPconfig* vars, Adafruit_ADS1115* ads);
        void init();
        void loop();
        

    private:
        #ifdef WAS_DEBUG
        float rampValue;
        float rampIncrement; // 40 units over 10 seconds, with 10ms delay
        float rampDirection ; // 1 for up, -1 for down
        uint32_t lastUpdateTime;
        #endif
        void updateRampValue();
        ESPconfig* espConfig;
        Adafruit_ADS1115* ads;
       
        
};

#endif