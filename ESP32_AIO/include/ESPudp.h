#ifndef ESPNETWORK_H
#define ESPNETWORK_H

#include "Arduino.h"
#include "AsyncUDP.h"
#include <AsyncTCP.h>

#include "ESPdata.h"
#include "GPS.h"

// Forward declaration of the GPS class
class ESPGPS;


class ESPudp{
    public:
        uint8_t aioReply[11];
        void begin(ESPGPS* gps);
        void sendUDP(uint8_t* data, size_t size);
        void sendUDPgps(const char * data);
        AsyncUDP udp;
        AsyncUDP udpGPS;
        AsyncUDP udpWAS;
        uint8_t calcChecksum(uint8_t* data, size_t size);
        ESPudp(ESPdata* vars);
    private:
        
        
        AsyncUDP udpNtrip;
        AsyncUDP udpJoystick;
        
        ESPdata* espData;
        ESPGPS* _gps;
    
};

#endif