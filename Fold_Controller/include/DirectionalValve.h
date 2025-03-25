#ifndef DIRECTIONALVALVE
#define DIRECTIONALVALVE
#include <Arduino.h>

class DirectionalValve{
    public:
        DirectionalValve();
        void begin(uint8_t _pin);
        void loop();
        uint8_t ctrlState; //0 = nothing, 1 = lift, 2 = lower
    private:
        uint8_t pin;
};

#endif