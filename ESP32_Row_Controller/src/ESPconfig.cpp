#include "ESPconfig.h"

ESPconfig::ESPconfig() : progCfg(), wifiCfg(), otaCfg() {}

uint8_t ESPconfig::loadConfig() {
    preferences.begin("config", false);

    wifiCfg.ips[0] = preferences.getInt("ip0", 192);
    wifiCfg.ips[1] = preferences.getInt("ip1", 168);
    wifiCfg.ips[2] = preferences.getInt("ip2", 1);
    wifiCfg.ips[3] = preferences.getInt("ip3", 5);

    String ssid = preferences.getString("ssid", "NOLTE_FARM");
    String password = preferences.getString("password", "DontLoseMoney89");
    strncpy(wifiCfg.ssid, ssid.c_str(), sizeof(wifiCfg.ssid) - 1);
    wifiCfg.ssid[sizeof(wifiCfg.ssid) - 1] = '\0';
    strncpy(wifiCfg.password, password.c_str(), sizeof(wifiCfg.password) - 1);
    wifiCfg.password[sizeof(wifiCfg.password) - 1] = '\0';

    char version[64];
    strcpy(version, VERSION);
    char *token = strtok(version, ".");
    int i = 0;
    while (token != NULL) {
        int intValue = atoi(token);
        switch (i) {
            case 0: progCfg.version[0] = intValue; break;
            case 1: progCfg.version[1] = intValue; break;
            case 2: progCfg.version[2] = intValue; break;
        }
        i++;
        token = strtok(NULL, ".");
    }

    return 1;
}

uint8_t ESPconfig::updateIP() {
    preferences.putInt("ip0", wifiCfg.ips[0]);
    preferences.putInt("ip1", wifiCfg.ips[1]);
    preferences.putInt("ip2", wifiCfg.ips[2]);
    preferences.putInt("ip3", wifiCfg.ips[3]);
    return 1;
}
