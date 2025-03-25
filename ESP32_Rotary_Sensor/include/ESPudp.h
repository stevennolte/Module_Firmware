#ifndef ESPNETWORK_H
#define ESPNETWORK_H

#include "Arduino.h"
#include "AsyncUDP.h"
#include <AsyncTCP.h>

#include "ESPconfig.h"


// Forward declaration of the GPS class



class ESPudp{
    public:
        void begin();
        void sendUDP();
        ESPudp(ESPconfig* vars);
        void sendSteerData();
    private:
        
        AsyncUDP udp;
        ESPconfig* espConfig;
       
    
};

#endif