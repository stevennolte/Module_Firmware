#ifndef ESPWIFI_H
#define ESPWIFI_H

#include <Arduino.h>
#include "ESPdata.h"
#include "ESPmDNS.h"

class ESPWifi
 {
    public:
        uint8_t connect();
        uint8_t makeAP();
        void startMonitor();
        void scanNetworks();
        ESPWifi(ESPdata* vars);
    

    private:
        static void taskHandler(void *param);  // Task handler
        void continuousLoop();  // Function to run in the background task
        ESPdata* espData;
};


#endif