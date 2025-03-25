#ifndef FOLDVALVE
#define FOLDVALVE

#include "Arduino.h"

class FoldValve{
    public:
        FoldValve();
        void begin(uint8_t _pin1, uint8_t _pin2);
        void loop();
        uint8_t ctrlState; //0 = nothing, 1 = lift, 2 = lower
        // uint8_t dirState; //used as checker to make sure we are in the right directional state
    private:
        uint8_t pin1;
        uint8_t pin2;
};

#endif