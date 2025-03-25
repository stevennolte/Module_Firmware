#ifndef myLED_h
#define myLED_h

// #include "Arduino.h"
#include "ESPconfig.h"
#include <Adafruit_NeoPixel.h>

class MyLED{
    public:
        MyLED(ESPconfig* vars);
        void showColor(uint32_t color);
        void startTask();  // Start the parallel task
        
    private:
        ESPconfig* espConfig;
        Adafruit_NeoPixel pixel;

        static void taskHandler(void *param);  // Task handler
        void continuousLoop();  // Function to run in the background task
};



#endif