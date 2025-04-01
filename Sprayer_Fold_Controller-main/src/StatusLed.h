#ifndef StatusLed
#define StatusLed

#include "Arduino.h"


class StatusLED
{
    private:
        
    public:
        StatusLED();
        void setup();
        void connectionsGood();
        void connectedToWifi();
};
#endif