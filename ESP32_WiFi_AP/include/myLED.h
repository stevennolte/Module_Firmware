#ifndef MYLED_H
#define MYLED_H
#include "ESPconfig.h"
#include <Adafruit_NeoPixel.h>

class MyLED {
public:
    explicit MyLED(ESPconfig* vars);
    void startTask();

private:
    ESPconfig* espConfig;
    Adafruit_NeoPixel pixel;
    static void taskHandler(void* param);
    void continuousLoop();
    uint32_t rainbowStep = 0;
};

#endif
