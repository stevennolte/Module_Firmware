#ifndef PRODUCT_CTRL_H
#define PRODUCT_CTRL_H

#include "Arduino.h"
#include "ESPconfig.h"
#include <PulseFlowMeter.h>
#include "CANBUS.h"


class Product_Ctrl{
    public:
        Product_Ctrl(ESPconfig* vars, CANBUS* canbus);
        void begin();
        
    private:
        float lpmConversion = 0.26417287472922;    
        static void taskHandler(void *param);  // Task handler
        void continuousLoop();  // Function to run in the background task

        ESPconfig* espConfig;
        PulseFlowMeter meter;
};

#endif