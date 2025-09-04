#ifndef WAS_H
#define WAS_H

// #define WAS_DEBUG

#include "Arduino.h"
#include "ESPdata.h"
#include <Wire.h>
#include "Adafruit_ADS1X15.h"


class WAS{
    public:
        WAS(ESPdata* vars, Adafruit_ADS1115* ads);
        void init();
        void loop();
        void zeroSteerAngle();

    private:
        #ifdef WAS_DEBUG
        float rampValue;
        float rampIncrement; // 40 units over 10 seconds, with 10ms delay
        float rampDirection ; // 1 for up, -1 for down
        uint32_t lastUpdateTime;
        #endif
        void updateRampValue();
        ESPdata* espData;
        Adafruit_ADS1115* ads;
       
        
};

#endif