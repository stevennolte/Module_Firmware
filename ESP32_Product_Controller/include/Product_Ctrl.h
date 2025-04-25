#ifndef PRODUCT_CTRL_H
#define PRODUCT_CTRL_H

#include "Arduino.h"
#include "ESPconfig.h"

#include "CANBUS.h"
#include "Adafruit_ADS1X15.h"


class Product_Ctrl{
    public:
        Product_Ctrl(ESPconfig* vars, CANBUS* canbus, Adafruit_ADS1015* ads);
        void begin();
        
    private:
        
        float lpmConversion = 0.26417287472922;    
        static void taskHandler(void *param);  // Task handler
        void continuousLoop();  // Function to run in the background task

        ESPconfig* espConfig;
        Adafruit_ADS1015* ads;
        uint8_t* sectionStates; // Pointer to sectionStates array
        uint8_t* sectionPins;
        uint8_t* pressState; // Pointer to pressState
        uint32_t* lastSectionMsg;
        float* targetRowFlowRate;
        float* targetPressure;
        uint8_t* sectionsActive;
};

#endif