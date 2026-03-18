#include "ESPudp.h"
#include <Update.h>

ESPudp::ESPudp(ESPconfig* vars) : udp(), udpSend() {
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
    sendPGN234();
}

// Calculate CRC checksum for AgOpenGPS messages
uint8_t ESPudp::calcCRC(uint8_t* data, uint8_t length) {
    uint8_t crc = 0;
    for (uint8_t i = 2; i < length; i++) {
        crc += data[i];
    }
    return crc;
}

// Send PGN 234 - Section control data (matches official AgOpenGPS implementation)
void ESPudp::sendPGN234() {
    uint8_t message[14];
    message[0] = 0x80;  // Header
    message[1] = 0x81;  // Header
    message[2] = 0x7B;  // From section control (123)
    message[3] = 0xEA;  // PGN 234 - Section control data
    message[4] = 8;     // Data length (8 bytes)
    
    // Byte 5: Main control byte
    // Bit 0: Auto mode on (sections controlled by AOG)
    // Bit 1: Main switch OFF (all sections off)
    message[5] = 0;
    
    bool autoMode = !espConfig->sectionData.toolbarUp;  // Auto when toolbar is down
    
    // Bytes 6-8: Reserved/unused in this implementation
    message[6] = 0;
    message[7] = 0;
    message[8] = 0;
    
    // Bytes 9 & 11: Section relay states (only sent in MANUAL mode to override AOG)
    // In AUTO mode, send 0 to let AgOpenGPS control the sections
    message[9] = 0;
    message[11] = 0;
    
    // Bytes 10 & 12: Sections forced OFF
    message[10] = 0;
    message[12] = 0;
    
    if (autoMode) {
        // AUTO mode: toolbar is down, let AgOpenGPS control sections
        message[5] |= 0x01;  // Set auto bit
        // Leave bytes 9, 11 as 0 (AgOpenGPS controls sections)
        // Leave bytes 10, 12 as 0 (no sections forced off)
    } else {
        // MANUAL/OFF mode: toolbar is up, force all sections OFF
        message[5] |= 0x02;  // Set main OFF bit
        // Force all sections OFF by setting bits in forced-off bytes
        for (int i = 0; i < 8 && i < NUM_ROWS; i++) {
            message[10] |= (1 << i);
        }
        for (int i = 8; i < 16 && i < NUM_ROWS; i++) {
            message[12] |= (1 << (i - 8));
        }
    }
    
    // Byte 13: CRC checksum
    message[13] = calcCRC(message, 13);
    
    // Send using beginPacket/write/endPacket pattern (official AgOpenGPS method)
    IPAddress destIP(espConfig->wifiCfg.ips[0], 
                     espConfig->wifiCfg.ips[1], 
                     espConfig->wifiCfg.ips[2], 
                     255);
    
    AsyncUDPMessage udpMsg;
    udpMsg.write(message, sizeof(message));
    udpSend.sendTo(udpMsg, destIP, 9999);
    
    // Debug output
    static uint32_t lastDebug = 0;
    if (millis() - lastDebug >= 2000) {
        lastDebug = millis();
        Serial.printf("PGN234: Mode=%s, Main=%02X, Relay=%02X %02X, ForceOff=%02X %02X, CRC=%02X\n",
                      autoMode ? "AUTO" : "OFF",
                      message[5], message[9], message[11], message[10], message[12], message[13]);
    }
}
