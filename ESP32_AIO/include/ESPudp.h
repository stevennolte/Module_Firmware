#ifndef ESPNETWORK_H
#define ESPNETWORK_H

#include "Arduino.h"
#include "AsyncUDP.h"
#include <AsyncTCP.h>

#include "ESPconfig.h"
#include "GPS.h"

// Forward declaration of the GPS class
class GPS;


class ESPudp{
    public:
        uint8_t aioReply[11];
        void begin(GPS* gps);
        void sendUDP(uint8_t* data, size_t size);
        void sendUDPgps(const char * data);
        AsyncUDP udp;
        AsyncUDP udpGPS;
        uint8_t calcChecksum(uint8_t* data, size_t size);
        ESPudp(ESPconfig* vars);
    private:
        
        
        AsyncUDP udpNtrip;
        
        
        ESPconfig* espConfig;
        GPS* _gps;
    
};

#endif