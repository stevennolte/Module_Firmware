#include "ESPudp.h"
#include <Update.h>

ESPudp::ESPudp(ESPconfig* vars) : udp() {
    espConfig = vars;
}

void ESPudp::begin() {
    udp.listen(8888);
    udp.onPacket([this](AsyncUDPPacket packet) {
        // AgOpenGPS messages use the 0x80 0x81 header
        if (packet.length() < 4) return;
        if (packet.data()[0] != 0x80 || packet.data()[1] != 0x81) return;

        switch (packet.data()[3]) {
            case 200: // Hello from AgIO - no action needed
                break;

            case 201: // IP address update
                if (packet.length() >= 10) {
                    espConfig->wifiCfg.ips[0] = packet.data()[7];
                    espConfig->wifiCfg.ips[1] = packet.data()[8];
                    espConfig->wifiCfg.ips[2] = packet.data()[9];
                    espConfig->updateIP();
                    ESP.restart();
                }
                break;

            case 254: // Speed from AgIO
                if (packet.length() >= 7) {
                    espConfig->sectionData.speed =
                        (float((packet.data()[6] << 8) | packet.data()[5]) / 10.0f) * 0.621371f;
                }
                break;

            case 229: { // Section control from AgOpenGPS
                // Byte 4 = data length, bytes 5..6 = section state bits (up to 16 sections)
                if (packet.length() < 7) break;
                espConfig->sectionData.lastSectionMsg = millis();
                uint8_t bitIndex = 0;
                for (size_t i = 5; i <= 6 && bitIndex < NUM_ROWS; i++) {
                    uint8_t _byte = packet.data()[i];
                    for (int bit = 0; bit < 8 && bitIndex < NUM_ROWS; bit++) {
                        espConfig->sectionData.rowStates[bitIndex] = (_byte >> bit) & 0x01;
                        bitIndex++;
                    }
                }
                break;
            }

            default:
                break;
        }
    });
}

void ESPudp::sendUDP() {
    // Reserved for future use: sending status/telemetry back to AgOpenGPS
}
