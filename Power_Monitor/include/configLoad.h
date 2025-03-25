#ifndef configLoad_h
#define configLoad_h
#include "Arduino.h"
#include "messageStructs.h"
#include "LittleFS.h"
#include <ArduinoJSON.h>

class ConfigLoad{
    public:
        ConfigLoad(ProgramData_t &data);
        bool begin();
        bool updateConfig();
        
    private:
        ProgramData_t &data;
        const size_t capacity = JSON_OBJECT_SIZE(10) + 200;

        
};



#endif