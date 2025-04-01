#ifndef MyWifi_h
#define MyWifi_h

#include "Arduino.h"


class MyWifi
{
    private:
        const char* ssid = "";
        const char* password = "";
        uint8_t addressPins[4] = {39,40,41,42};
        
        uint8_t getAddress();
        const char* ssids[4] = {"NOLTE_FARM","FERT"};
        const char* passwords[4] = {"DontLoseMoney89","Fert504!"};
    public:
        MyWifi();
        bool connect();
        IPAddress gateway();
        IPAddress subnet();
        IPAddress local_IP();
        uint8_t address_1;
        uint8_t address_2;
        uint8_t address_3;
        uint8_t address_4; 
        uint8_t addressPin;
};
#endif