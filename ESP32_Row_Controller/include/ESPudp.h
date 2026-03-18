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
        AsyncUDP udpSend;  // Separate UDP instance for sending
        ESPconfig* espConfig;
        void sendPGN234();  // Send section control data
        uint8_t calcCRC(uint8_t* data, uint8_t length);
};

#endif
