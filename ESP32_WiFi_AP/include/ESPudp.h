#ifndef ESPUDP_H
#define ESPUDP_H
#include "ESPconfig.h"
#include <AsyncUDP.h>

// AgOpenGPS UDP ports to relay
#define UDP_PORT_MAIN   8888
#define UDP_PORT_SECTION 9999
#define UDP_PORT_SPEED  8887
#define UDP_PORT_WAS    8889

class ESPudp {
public:
    explicit ESPudp(ESPconfig* vars);
    void begin();

private:
    ESPconfig* espConfig;
    AsyncUDP udpMain;
    AsyncUDP udpSection;
    AsyncUDP udpSpeed;
    AsyncUDP udpWAS;
    AsyncUDP udpRelay;  // Persistent sender – reused for all outgoing relay packets

    void relayPacket(AsyncUDPPacket& packet, uint16_t port);
};

#endif
