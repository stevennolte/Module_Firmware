#ifndef JOYSTICK_H
#define JOYSTICK_H
#include <Arduino.h>
#include "ESPconfig.h"

class Joystick {
public:
    Joystick(ESPconfig* vars);
       

    void initialize();
    

private:
    uint8_t prevsection;
    uint8_t prevautosteer;
    static void taskHandler(void *param);  // Task handler
    void continuousLoop();  // Function to run in the background task
    ESPconfig* espConfig;
};

#endif // JOYSTICK_H