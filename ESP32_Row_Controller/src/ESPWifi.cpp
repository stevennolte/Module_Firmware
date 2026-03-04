#include "ESPWifi.h"
#include "WiFi.h"

ESPWifi::ESPWifi(ESPconfig* vars) {
    espConfig = vars;
}

uint8_t ESPWifi::connect() {
    IPAddress local_IP(espConfig->wifiCfg.ips[0], espConfig->wifiCfg.ips[1],
                       espConfig->wifiCfg.ips[2], espConfig->wifiCfg.ips[3]);
    IPAddress gateway(espConfig->wifiCfg.ips[0], espConfig->wifiCfg.ips[1],
                      espConfig->wifiCfg.ips[2], 1);
    IPAddress subnet(255, 255, 255, 0);
    uint8_t numNetworks = WiFi.scanNetworks();
    for (int i = 0; i < numNetworks; i++) {
        if (WiFi.SSID(i) == espConfig->wifiCfg.ssid) {
            WiFi.begin(espConfig->wifiCfg.ssid, espConfig->wifiCfg.password);
            WiFi.config(local_IP, gateway, subnet);
            return 1;
        }
    }
    return 0;
}

uint8_t ESPWifi::makeAP() {
    IPAddress local_IP(espConfig->wifiCfg.ips[0], espConfig->wifiCfg.ips[1],
                       espConfig->wifiCfg.ips[2], espConfig->wifiCfg.ips[3]);
    IPAddress gateway(espConfig->wifiCfg.ips[0], espConfig->wifiCfg.ips[1],
                      espConfig->wifiCfg.ips[2], 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.setHostname(NAME);
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(NAME, "1234567890");
    delay(100);
    WiFi.softAPConfig(local_IP, local_IP, subnet);
    startMonitor();
    return 3;
}

void ESPWifi::startMonitor() {
    xTaskCreate(
        taskHandler,
        "WifiMonitor",
        4096,
        this,
        1,
        NULL
    );
}

void ESPWifi::taskHandler(void *param) {
    ESPWifi* instance = (ESPWifi*)param;
    instance->continuousLoop();
}

void ESPWifi::continuousLoop() {
    while (true) {
        switch (espConfig->wifiCfg.state) {
            case 1:
                if (!WiFi.isConnected()) {
                    connect();
                }
                break;
            default:
                break;
        }
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
