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
    Preferences preferences;

    class GPIO_Definitions {
        public:
            uint8_t LED_PIN = 48;
            // 12 row unit MOSFET output pins
            uint8_t rowPins[NUM_ROWS] = {4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
            // Digital input: toolbar lifted sensor (HIGH = toolbar up, LOW = toolbar down)
            uint8_t TOOLBAR_PIN = 16;
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

    class WifiConfig {
        public:
            char ssid[64];
            char password[64];
            uint8_t ips[4];
            uint8_t state;
            uint8_t apMode;
            WifiConfig() {
                strncpy(ssid, "NOLTE_FARM", sizeof(ssid));
                strncpy(password, "DontLoseMoney89", sizeof(password));
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
            SectionData() {
                memset(rowStates, 0, sizeof(rowStates));
                lastSectionMsg = 0;
                speed = 0.0f;
                toolbarUp = true;
            }
    };
    SectionData sectionData;

    ESPconfig();
};

#endif
