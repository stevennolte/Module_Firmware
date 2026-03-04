#ifndef myLED_h
#define myLED_h

#include "ESPconfig.h"
#include <Adafruit_NeoPixel.h>

class MyLED {
    public:
        MyLED(ESPconfig* vars);
        void showColor(uint32_t color);
        void startTask();

    private:
        ESPconfig* espConfig;
        Adafruit_NeoPixel pixel;

        static void taskHandler(void *param);
        void continuousLoop();
};

#endif
