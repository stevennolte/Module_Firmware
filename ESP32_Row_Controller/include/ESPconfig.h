#ifndef ESPCONF_H
#define ESPCONF_H
#include "Arduino.h"
#include "LittleFS.h"
#include "ArduinoJson.h"
#include "Version.h"
#include "Preferences.h"

#define NUM_ROWS 12

class ESPconfig
{
public:
    uint8_t loadConfig();
    uint8_t updateIP();
    uint8_t updateRowPins();
    uint8_t updateToolbarPins();
    Preferences preferences;

    class GPIO_Definitions {
        public:
            uint8_t LED_PIN = 48;
            // Main power relay control (turns on after boot)
            uint8_t POWER_RELAY_PIN = 12;
            // 12 row unit MOSFET output pins
            uint8_t rowPins[NUM_ROWS] = {11, 10, 9, 8, 18, 17, 16, 15, 7, 6, 5, 4};
            // Digital inputs: toolbar lifted sensors (HIGH = toolbar up, LOW = toolbar down)
            uint8_t toolbarPins[2] = {1, 2};
            // Row pin active state: true = active HIGH, false = active LOW
            bool rowActiveHigh = true;
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
                strncpy(ssid, NAME, sizeof(ssid) - 1);
                ssid[sizeof(ssid) - 1] = '\0';
                strncpy(password, "1234567890", sizeof(password) - 1);
                password[sizeof(password) - 1] = '\0';
                ips[0] = 192; ips[1] = 168; ips[2] = 1; ips[3] = 5;
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
            uint8_t count;     // number of configured networks
            uint8_t state;     // 0 = not connected, 1 = connected
            int8_t  activeIdx; // index of currently connected network (-1 = none)
            STAConfig() {
                for (int i = 0; i < MAX_NETWORKS; i++) {
                    memset(ssids[i], 0, sizeof(ssids[i]));
                    memset(passwords[i], 0, sizeof(passwords[i]));
                }
                count     = 0;
                state     = 0;
                activeIdx = -1;
            }
    };
    STAConfig staCfg;

    // WiFi operating mode: 0 = AP only, 1 = AP+STA, 2 = STA only
    uint8_t wifiMode;

    // WiFi/Network configuration (destination IP for UDP communication)
    class WifiConfig {
        public:
            uint8_t ips[4];    // Destination IP address (e.g., AgIO computer)
            WifiConfig() {
                ips[0] = 192; ips[1] = 168; ips[2] = 1; ips[3] = 255;
            }
    };
    WifiConfig wifiCfg;

    class OTAConfig {
        public:
            uint8_t state;
            OTAConfig() {}
    };
    OTAConfig otaCfg;

    class SectionData {
        public:
            // One state per row unit: 1 = active/planting, 0 = off
            uint8_t rowStates[NUM_ROWS];
            uint32_t lastSectionMsg;
            float speed;
            // Toolbar state: true = toolbar is UP (do not plant), false = toolbar is DOWN (allow planting)
            bool toolbarUp;
            // Manual toolbar override
            bool toolbarOverrideEnabled;
            bool toolbarOverrideValue;  // true = force UP, false = force DOWN
            SectionData() {
                memset(rowStates, 0, sizeof(rowStates));
                lastSectionMsg = 0;
                speed = 0.0f;
                toolbarUp = true;
                toolbarOverrideEnabled = false;
                toolbarOverrideValue = false;
            }
    };
    SectionData sectionData;

    ESPconfig() : progCfg(), progData(), apCfg(), staCfg(), wifiMode(0), wifiCfg(), otaCfg() {}
};

#endif
