#include "ESPWifi.h"
#include "WiFi.h"

ESPWifi::ESPWifi(ESPconfig* vars) {
    espConfig = vars;
}

uint8_t ESPWifi::connect() {
    uint8_t numNetworks = WiFi.scanNetworks();
    for (int i = 0; i < numNetworks; i++) {
        for (int j = 0; j < 4; j++) {
            if (WiFi.SSID(i) == espConfig->wifiCfg.ssids[j]) {
                Serial.println("Found network: " + String(espConfig->wifiCfg.ssids[j]));
                WiFi.begin(espConfig->wifiCfg.ssids[j], espConfig->wifiCfg.passwords[j]);
                while (WiFi.status() != WL_CONNECTED) {
                    Serial.print(".");
                    delay(100);
                }
                Serial.println();
                Serial.println(WiFi.localIP().toString());
                IPAddress ip = WiFi.localIP();
                IPAddress local_IP(ip[0], ip[1], ip[2], espConfig->wifiCfg.ips[3]);
                IPAddress gateway(ip[0], ip[1], ip[2], 1);
                IPAddress subnet(255, 255, 255, 0);
                WiFi.config(local_IP, gateway, subnet);
                MDNS.begin(NAME);
                Serial.println(WiFi.localIP().toString());
                startMonitor();
                return 1;
            }
        }
    }
    return 2;
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
    MDNS.begin(NAME);
    return 3;
}

void ESPWifi::startMonitor() {
    xTaskCreate(taskHandler, "WifiMonitor", 4096, this, 1, NULL);
}

void ESPWifi::taskHandler(void *param) {
    ESPWifi* instance = (ESPWifi*)param;
    instance->continuousLoop();
}

void ESPWifi::continuousLoop() {
    while (true) {
        if (espConfig->wifiCfg.state == 1) {
            if (WiFi.status() != WL_CONNECTED) {
                Serial.println("Reconnecting to WiFi...");
                WiFi.disconnect();
                WiFi.reconnect();
            }
        }
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
