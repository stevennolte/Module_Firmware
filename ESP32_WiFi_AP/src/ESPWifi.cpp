#include "ESPWifi.h"
#include "WiFi.h"
#include <ESPmDNS.h>
#include "esp_wifi.h"

ESPWifi::ESPWifi(ESPconfig* vars) {
    espConfig = vars;
}

void ESPWifi::startAP() {
    IPAddress apIP(espConfig->apCfg.ips[0], espConfig->apCfg.ips[1],
                   espConfig->apCfg.ips[2], espConfig->apCfg.ips[3]);
    IPAddress subnet(255, 255, 255, 0);

    // Set WiFi mode based on wifiMode setting:
    // 0 = AP only, 1 = AP+STA, 2 = STA only
    if (espConfig->wifiMode == 2) {
        WiFi.mode(WIFI_STA);
    } else if (espConfig->wifiMode == 1) {
        WiFi.mode(WIFI_AP_STA);
    } else {
        WiFi.mode(WIFI_AP);
    }

    // Disable power saving for lowest possible latency
    esp_wifi_set_ps(WIFI_PS_NONE);

    // Maximum transmit power for best range and reliability
    WiFi.setTxPower(WIFI_POWER_19_5dBm);

    if (espConfig->wifiMode != 2) {
        // Start AP unless in STA-only mode
        WiFi.softAP(espConfig->apCfg.ssid,
                    espConfig->apCfg.password,
                    espConfig->apCfg.channel,
                    0,  // ssid_hidden = 0 (visible)
                    espConfig->apCfg.maxClients);
        delay(100);
        WiFi.softAPConfig(apIP, apIP, subnet);

        Serial.printf("AP started: SSID=%s  IP=%s  ch=%d  maxClients=%d\n",
                      espConfig->apCfg.ssid,
                      apIP.toString().c_str(),
                      espConfig->apCfg.channel,
                      espConfig->apCfg.maxClients);
    }

    MDNS.begin(NAME);
    Serial.printf("mDNS: %s.local\n", NAME);
}

void ESPWifi::connectSTA() {
    if (espConfig->wifiMode == 0 || espConfig->staCfg.count == 0) {
        return;
    }

    Serial.printf("STA: scanning for %d configured network(s)...\n", espConfig->staCfg.count);

    // Scan available networks and find the first configured one
    int found = WiFi.scanNetworks();
    int bestIdx = -1;
    for (int si = 0; si < found && bestIdx < 0; si++) {
        String scannedSSID = WiFi.SSID(si);
        for (int ci = 0; ci < espConfig->staCfg.count; ci++) {
            if (scannedSSID == espConfig->staCfg.ssids[ci]) {
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

    Serial.printf("STA: connecting to %s ...\n", espConfig->staCfg.ssids[bestIdx]);
    espConfig->staCfg.activeIdx = bestIdx;

    // Static IP if configured, otherwise DHCP
    bool useStatic = (espConfig->staCfg.ips[0] != 0);
    if (useStatic) {
        IPAddress ip(espConfig->staCfg.ips[0], espConfig->staCfg.ips[1],
                     espConfig->staCfg.ips[2], espConfig->staCfg.ips[3]);
        IPAddress apIP(espConfig->apCfg.ips[0], espConfig->apCfg.ips[1],
                       espConfig->apCfg.ips[2], espConfig->apCfg.ips[3]);
        IPAddress gw(espConfig->staCfg.ips[0], espConfig->staCfg.ips[1],
                     espConfig->staCfg.ips[2], 1);
        IPAddress subnet(255, 255, 255, 0);
        WiFi.config(ip, gw, subnet);
    }

    WiFi.setHostname(NAME);
    WiFi.begin(espConfig->staCfg.ssids[bestIdx], espConfig->staCfg.passwords[bestIdx]);
}

void ESPWifi::startMonitor() {
    xTaskCreate(taskHandler, "WifiMonitor", 4096, this, 1, NULL);
}

void ESPWifi::taskHandler(void* param) {
    ((ESPWifi*)param)->continuousLoop();
}

void ESPWifi::continuousLoop() {
    bool mdnsStarted = false;
    while (true) {
        if (espConfig->wifiMode != 0 && espConfig->staCfg.count > 0) {
            if (WiFi.status() == WL_CONNECTED) {
                if (!mdnsStarted) {
                    mdnsStarted = true;
                    espConfig->staCfg.state = 1;
                    Serial.printf("STA connected. IP: %s\n",
                                  WiFi.localIP().toString().c_str());
                }
            } else {
                if (mdnsStarted) {
                    mdnsStarted = false;
                    espConfig->staCfg.state = 0;
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
