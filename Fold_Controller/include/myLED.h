#ifndef myLED_h
#define myLED_h

// #include "Arduino.h"
#include "messageStructs.h"
#include <Adafruit_NeoPixel.h>

class MyLED{
    public:
        MyLED(ProgramData_t &data, uint8_t pin, uint8_t intensity);
        void showColor(uint32_t color);
        void startTask();  // Start the parallel task
        
    private:
        ProgramData_t &data;
        Adafruit_NeoPixel pixel;

        static void taskHandler(void *param);  // Task handler
        void continuousLoop();  // Function to run in the background task
};



#endif