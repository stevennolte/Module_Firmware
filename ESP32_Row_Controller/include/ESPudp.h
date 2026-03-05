#ifndef ESPUDP_H
#define ESPUDP_H

#include "Arduino.h"
#include "AsyncUDP.h"
#include <AsyncTCP.h>
#include "ESPconfig.h"

class ESPudp {
    public:
        void begin();
        void sendUDP();
        ESPudp(ESPconfig* vars);
    private:
        AsyncUDP udp;
        ESPconfig* espConfig;
};

#endif
