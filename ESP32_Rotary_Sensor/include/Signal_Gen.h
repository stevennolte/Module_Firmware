#ifndef SIGNAL_GEN_H
#define SIGNAL_GEN_H

#include "Arduino.h"
#include "ESPconfig.h"
#include "MCP4725.h"

class Signal_Gen{
    public:
        Signal_Gen(ESPconfig* vars, MCP4725* _sensor);
        void startTask();  // Start the parallel task
    private:
        ESPconfig* espConfig;
        MCP4725* _sensor;
        static void taskHandler(void *param);  // Task handler
        void continuousLoop();  // Function to run in the background task
};

#endif