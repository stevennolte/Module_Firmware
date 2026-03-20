#include "ESPudp.h"

ESPudp::ESPudp(ESPconfig* vars) {
    espConfig = vars;
}

// Re-broadcast a received packet on all AP-side clients.
// The destination is the AP subnet broadcast address so every
// device connected to the AP (including the sender) receives it.
// Uses a persistent AsyncUDP sender to avoid per-packet allocation overhead.
void ESPudp::relayPacket(AsyncUDPPacket& packet, uint16_t port) {
    if (packet.length() == 0) return;

    // Build subnet broadcast from the AP IP (e.g. 192.168.1.255)
    IPAddress dest(espConfig->apCfg.ips[0],
                   espConfig->apCfg.ips[1],
                   espConfig->apCfg.ips[2],
                   255);

    AsyncUDPMessage msg;
    msg.write(packet.data(), packet.length());
    udpRelay.sendTo(msg, dest, port);

    espConfig->udpStats.packetsRelayed++;
    espConfig->udpStats.bytesRelayed  += packet.length();
    espConfig->udpStats.lastPacketMs   = millis();
}

void ESPudp::begin() {
    // Listen on the main AgOpenGPS port
    if (udpMain.listen(UDP_PORT_MAIN)) {
        udpMain.onPacket([this](AsyncUDPPacket packet) {
            relayPacket(packet, UDP_PORT_MAIN);
        });
        Serial.printf("UDP relay active on port %d\n", UDP_PORT_MAIN);
    }

    // Listen on the section/NMEA broadcast port
    if (udpSection.listen(UDP_PORT_SECTION)) {
        udpSection.onPacket([this](AsyncUDPPacket packet) {
            relayPacket(packet, UDP_PORT_SECTION);
        });
        Serial.printf("UDP relay active on port %d\n", UDP_PORT_SECTION);
    }

    // Listen on the joystick/speed port
    if (udpSpeed.listen(UDP_PORT_SPEED)) {
        udpSpeed.onPacket([this](AsyncUDPPacket packet) {
            relayPacket(packet, UDP_PORT_SPEED);
        });
        Serial.printf("UDP relay active on port %d\n", UDP_PORT_SPEED);
    }

    // Listen on the WAS (wheel angle sensor) port
    if (udpWAS.listen(UDP_PORT_WAS)) {
        udpWAS.onPacket([this](AsyncUDPPacket packet) {
            relayPacket(packet, UDP_PORT_WAS);
        });
        Serial.printf("UDP relay active on port %d\n", UDP_PORT_WAS);
    }
}
