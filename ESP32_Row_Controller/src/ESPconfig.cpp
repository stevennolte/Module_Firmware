#include "ESPconfig.h"

uint8_t ESPconfig::loadConfig() {
    preferences.begin("config", false);

    // AP settings
    apCfg.ips[0] = preferences.getInt("ap_ip0", 192);
    apCfg.ips[1] = preferences.getInt("ap_ip1", 168);
    apCfg.ips[2] = preferences.getInt("ap_ip2", 1);
    apCfg.ips[3] = preferences.getInt("ap_ip3", 5);
    apCfg.channel    = (uint8_t)preferences.getInt("ap_ch",  6);
    apCfg.maxClients = (uint8_t)preferences.getInt("ap_max", 8);

    String apSSID = preferences.getString("ap_ssid", NAME);
    String apPass = preferences.getString("ap_pass", "1234567890");
    strncpy(apCfg.ssid,     apSSID.c_str(), sizeof(apCfg.ssid) - 1);
    strncpy(apCfg.password, apPass.c_str(),  sizeof(apCfg.password) - 1);
    apCfg.ssid[sizeof(apCfg.ssid) - 1]         = '\0';
    apCfg.password[sizeof(apCfg.password) - 1] = '\0';

    // WiFi operating mode (0=AP only, 1=AP+STA, 2=STA only)
    wifiMode = (uint8_t)preferences.getInt("wifi_mode", 0);

    // STA network list
    // Migrate from old single-network format if needed
    if (!preferences.isKey("sta_count") && preferences.isKey("ssid")) {
        String legSSID = preferences.getString("ssid", "");
        String legPass = preferences.getString("password", "");
        if (legSSID.length() > 0) {
            preferences.putInt("sta_count", 1);
            preferences.putString("sta_ssid_0", legSSID);
            preferences.putString("sta_pass_0", legPass);
            if (wifiMode == 0) {
                wifiMode = 1;  // AP+STA
                preferences.putInt("wifi_mode", wifiMode);
            }
        }
    }

    staCfg.count = (uint8_t)preferences.getInt("sta_count", 0);
    if (staCfg.count > STAConfig::MAX_NETWORKS) staCfg.count = STAConfig::MAX_NETWORKS;
    for (int i = 0; i < staCfg.count; i++) {
        String keySSID = "sta_ssid_" + String(i);
        String keyPass = "sta_pass_" + String(i);
        String s = preferences.getString(keySSID.c_str(), "");
        String p = preferences.getString(keyPass.c_str(), "");
        strncpy(staCfg.ssids[i],     s.c_str(), sizeof(staCfg.ssids[i]) - 1);
        strncpy(staCfg.passwords[i], p.c_str(), sizeof(staCfg.passwords[i]) - 1);
        staCfg.ssids[i][sizeof(staCfg.ssids[i]) - 1]         = '\0';
        staCfg.passwords[i][sizeof(staCfg.passwords[i]) - 1] = '\0';
    }

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
