#include "ESPWifi.h"
#include "WiFi.h"
#include "Version.h"
#include "esp_wifi.h"

ESPWifi::ESPWifi(ESPdata* vars) {
    espData = vars;
}

void ESPWifi::startAP() {
    IPAddress apIP(espData->apCfg.ips[0], espData->apCfg.ips[1],
                   espData->apCfg.ips[2], espData->apCfg.ips[3]);
    IPAddress subnet(255, 255, 255, 0);

    // Set WiFi mode based on wifiMode setting:
    // 0 = AP only, 1 = AP+STA, 2 = STA only
    if (espData->wifiMode == 2) {
        WiFi.mode(WIFI_STA);
    } else if (espData->wifiMode == 1) {
        WiFi.mode(WIFI_AP_STA);
    } else {
        WiFi.mode(WIFI_AP);
    }

    // Disable power saving for lowest possible latency
    esp_wifi_set_ps(WIFI_PS_NONE);

    if (espData->wifiMode != 2) {
        // Start AP unless in STA-only mode
        WiFi.softAP(espData->apCfg.ssid,
                    espData->apCfg.password,
                    espData->apCfg.channel,
                    0,  // ssid_hidden = 0 (visible)
                    espData->apCfg.maxClients);
        delay(100);
        WiFi.softAPConfig(apIP, apIP, subnet);

        // Update legacy wifi.ips with AP IP for display code
        espData->wifi.ips[0] = espData->apCfg.ips[0];
        espData->wifi.ips[1] = espData->apCfg.ips[1];
        espData->wifi.ips[2] = espData->apCfg.ips[2];
        espData->wifi.ips[3] = espData->apCfg.ips[3];

        Serial.printf("AP started: SSID=%s  IP=%s  ch=%d  maxClients=%d\n",
                      espData->apCfg.ssid,
                      apIP.toString().c_str(),
                      espData->apCfg.channel,
                      espData->apCfg.maxClients);
    }

    MDNS.begin(NAME);
    Serial.printf("mDNS: %s.local\n", NAME);
    espData->wifi.state = 3;  // AP mode active
}

void ESPWifi::connectSTA() {
    if (espData->wifiMode == 0 || espData->staCfg.count == 0) {
        return;
    }

    Serial.printf("STA: scanning for %d configured network(s)...\n", espData->staCfg.count);

    // Scan available networks and find the first configured one
    int found = WiFi.scanNetworks();
    int bestIdx = -1;
    for (int si = 0; si < found && bestIdx < 0; si++) {
        String scannedSSID = WiFi.SSID(si);
        for (int ci = 0; ci < espData->staCfg.count; ci++) {
            if (scannedSSID == espData->staCfg.ssids[ci]) {
                bestIdx = ci;
                break;
            }
        }
    }
    WiFi.scanDelete();

    if (bestIdx < 0) {
        // No matching network visible – fall back to the first configured entry
        bestIdx = 0;
    }

    Serial.printf("STA: connecting to %s ...\n", espData->staCfg.ssids[bestIdx]);
    espData->staCfg.activeIdx = bestIdx;

    WiFi.setHostname(NAME);
    WiFi.begin(espData->staCfg.ssids[bestIdx], espData->staCfg.passwords[bestIdx]);
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
    bool mdnsStarted = false;
    while (true) {
        if (espData->wifiMode != 0 && espData->staCfg.count > 0) {
            if (WiFi.status() == WL_CONNECTED) {
                if (!mdnsStarted) {
                    mdnsStarted = true;
                    espData->staCfg.state = 1;
                    espData->wifi.state = 1;
                    // Update legacy wifi.ips with STA IP
                    IPAddress ip = WiFi.localIP();
                    espData->wifi.moduleIP = ip;
                    espData->wifi.ips[0] = ip[0];
                    espData->wifi.ips[1] = ip[1];
                    espData->wifi.ips[2] = ip[2];
                    espData->wifi.ips[3] = ip[3];
                    Serial.printf("STA connected. IP: %s\n", ip.toString().c_str());
                }
            } else {
                if (mdnsStarted) {
                    mdnsStarted = false;
                    espData->staCfg.state = 0;
                    espData->wifi.state = 3;  // back to AP only
                    // Restore AP IP in wifi.ips for display
                    espData->wifi.ips[0] = espData->apCfg.ips[0];
                    espData->wifi.ips[1] = espData->apCfg.ips[1];
                    espData->wifi.ips[2] = espData->apCfg.ips[2];
                    espData->wifi.ips[3] = espData->apCfg.ips[3];
                    Serial.println("STA disconnected – reconnecting...");
                }
                connectSTA();
            }
        }
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

int ESPWifi::getConnectedClients() const {
    return (int)WiFi.softAPgetStationNum();
}
