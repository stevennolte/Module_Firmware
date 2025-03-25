#ifndef MyWifi_h
#define MyWifi_h

#include "Arduino.h"


class MyWifi
{
    private:
        
        
        const char* ssids[4] = {"NOLTE_FARM", "FERT"};
        const char* passwords[4] = {"DontLoseMoney89","Fert504!"};
    public:
        MyWifi();
        const char* ssid = "";
        const char* password = "";
        uint8_t connect(uint8_t * ipAddr, char * sketchConfig);
        uint8_t makeAP(uint8_t * ipAddr , char * sketchConfig);
        IPAddress gateway();
        IPAddress subnet();
        IPAddress local_IP();
        // uint8_t ipAddress[4];
        // uint8_t address_1;
        // uint8_t address_2;
        // uint8_t address_3;
        // uint8_t address_4; 
       
};
#endif