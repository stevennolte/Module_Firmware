#include "ESPconfig.h"

ESPconfig::ESPconfig() : progCfg(), progData(), apCfg(), staCfg(), udpStats() {}

uint8_t ESPconfig::loadConfig() {
    preferences.begin("config", false);

    // AP settings
    apCfg.ips[0] = preferences.getInt("ap_ip0", 192);
    apCfg.ips[1] = preferences.getInt("ap_ip1", 168);
    apCfg.ips[2] = preferences.getInt("ap_ip2", 1);
    apCfg.ips[3] = preferences.getInt("ap_ip3", 1);
    apCfg.channel    = (uint8_t)preferences.getInt("ap_ch",  6);
    apCfg.maxClients = (uint8_t)preferences.getInt("ap_max", 8);

    String apSSID = preferences.getString("ap_ssid", "AgOpenGPS");
    String apPass = preferences.getString("ap_pass", "1234567890");
    strncpy(apCfg.ssid,     apSSID.c_str(), sizeof(apCfg.ssid) - 1);
    strncpy(apCfg.password, apPass.c_str(),  sizeof(apCfg.password) - 1);
    apCfg.ssid[sizeof(apCfg.ssid) - 1]         = '\0';
    apCfg.password[sizeof(apCfg.password) - 1] = '\0';

    // STA settings
    staCfg.enabled = preferences.getBool("sta_en", false);
    String staSSID = preferences.getString("sta_ssid", "");
    String staPass = preferences.getString("sta_pass", "");
    strncpy(staCfg.ssid,     staSSID.c_str(), sizeof(staCfg.ssid) - 1);
    strncpy(staCfg.password, staPass.c_str(),  sizeof(staCfg.password) - 1);
    staCfg.ssid[sizeof(staCfg.ssid) - 1]         = '\0';
    staCfg.password[sizeof(staCfg.password) - 1] = '\0';
    staCfg.ips[0] = preferences.getInt("sta_ip0", 0);
    staCfg.ips[1] = preferences.getInt("sta_ip1", 0);
    staCfg.ips[2] = preferences.getInt("sta_ip2", 0);
    staCfg.ips[3] = preferences.getInt("sta_ip3", 0);

    // Parse version string
    char versionBuf[64];
    strncpy(versionBuf, VERSION, sizeof(versionBuf) - 1);
    versionBuf[sizeof(versionBuf) - 1] = '\0';
    char* token = strtok(versionBuf, ".");
    int i = 0;
    while (token != NULL) {
        int val = atoi(token);
        switch (i) {
            case 0: progCfg.version[0] = val; break;
            case 1: progCfg.version[1] = val; break;
            case 2: progCfg.version[2] = val; break;
        }
        i++;
        token = strtok(NULL, ".");
    }

    return 1;
}

uint8_t ESPconfig::updateIP() {
    preferences.putInt("ap_ip0", apCfg.ips[0]);
    preferences.putInt("ap_ip1", apCfg.ips[1]);
    preferences.putInt("ap_ip2", apCfg.ips[2]);
    preferences.putInt("ap_ip3", apCfg.ips[3]);
    return 1;
}
