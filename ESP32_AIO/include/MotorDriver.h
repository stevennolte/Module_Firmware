#ifndef MOTORDRIVER_H
#define MOTORDRIVER_H

#include "Arduino.h"
#include "ESPdata.h"
#include <Adafruit_MCP23X17.h>
#include "MCPManager.h"



class MotorDriver{
    public:
        uint8_t state;
        void init();
        
        void setOutput(float value);
        void enable();
        void disable();
        void setCW();
        void setCCW();
        MotorDriver(ESPdata* vars);  // Removed MCP parameter
        
    private:
        
        uint16_t maxPWM=8000;
        uint8_t inaPin;
        uint8_t inbPin;
        uint8_t pwmPin;
        uint8_t enaPin;
        uint8_t enbPin;
        uint16_t cmdValue;
        
        uint8_t dirCmd;
        float pidInput;
        float pidOutput;

        ESPdata* espData;
        MCPManager& mcpManager;  // Reference to MCPManager singleton
       

};

#endif