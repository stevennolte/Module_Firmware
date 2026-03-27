#ifndef ESPCONF_H
#define ESPCONF_H
#include "Arduino.h"
#include "LittleFS.h"
#include "ArduinoJson.h"
#include "Version.h"
#include "Preferences.h"

class ESPconfig
{
public:
    uint8_t loadConfig();
    uint8_t updateIP();
    Preferences preferences;

    class GPIO_Definitions {
    public:
        uint8_t LED_PIN = 48;
        GPIO_Definitions() {}
    };
    GPIO_Definitions gpioDefs;

    class ProgramConfig {
    public:
        char name[64];
        uint8_t version[3];
        uint8_t ledBrht;
        ProgramConfig() {}
    };
    ProgramConfig progCfg;

    class ProgramData {
    public:
        uint8_t state;
        uint8_t confRes;
        ProgramData() {}
    };
    ProgramData progData;

    // AP (access point) settings
    class APConfig {
    public:
        char ssid[64];
        char password[64];
        uint8_t ips[4];    // AP IP address
        uint8_t channel;   // WiFi channel (1–13)
        uint8_t maxClients;
        APConfig() {
            strncpy(ssid, "AgOpenGPS", sizeof(ssid));
            strncpy(password, "1234567890", sizeof(password));
            ips[0] = 192; ips[1] = 168; ips[2] = 1; ips[3] = 1;
            channel    = 6;
            maxClients = 8;
        }
    };
    APConfig apCfg;

    // STA (station / upstream) settings – supports up to MAX_NETWORKS networks
    class STAConfig {
    public:
        static const int MAX_NETWORKS = 4;
        char ssids[MAX_NETWORKS][64];
        char passwords[MAX_NETWORKS][64];
        uint8_t ips[4];    // STA IP (0.0.0.0 = DHCP)
        uint8_t count;     // number of configured networks
        uint8_t state;     // 0 = not connected, 1 = connected
        int8_t  activeIdx; // index of currently connected network (-1 = none)
        STAConfig() {
            for (int i = 0; i < MAX_NETWORKS; i++) {
                memset(ssids[i], 0, sizeof(ssids[i]));
                memset(passwords[i], 0, sizeof(passwords[i]));
            }
            ips[0] = 0; ips[1] = 0; ips[2] = 0; ips[3] = 0;
            count     = 0;
            state     = 0;
            activeIdx = -1;
        }
    };
    STAConfig staCfg;

    // WiFi operating mode: 0 = AP only, 1 = AP+STA (bridge), 2 = STA only
    uint8_t wifiMode;

    // UDP relay statistics (updated at runtime)
    class UDPStats {
    public:
        uint32_t packetsRelayed;
        uint32_t bytesRelayed;
        uint32_t lastPacketMs;
        UDPStats() : packetsRelayed(0), bytesRelayed(0), lastPacketMs(0) {}
    };
    UDPStats udpStats;

    ESPconfig();
};

#endif
