#include "ESPWifi.h"
#include "WiFi.h"
#include <ESPmDNS.h>

ESPWifi::ESPWifi(ESPconfig* vars) {
    espConfig = vars;
}

uint8_t ESPWifi::connect() {
    uint8_t numNetworks = WiFi.scanNetworks();
    
    // Check for stored SSID from preferences first
    for (int i = 0; i < numNetworks; i++) {
        if (WiFi.SSID(i) == espConfig->wifiCfg.ssid) {
            WiFi.setHostname(NAME);
            // First connect with DHCP to get network's IP range
            WiFi.begin(espConfig->wifiCfg.ssid, espConfig->wifiCfg.password);
            
            // Wait for connection (up to 10 seconds)
            int attempts = 0;
            while (WiFi.status() != WL_CONNECTED && attempts < 40) {
                delay(250);
                attempts++;
            }
            
            if (WiFi.status() == WL_CONNECTED) {
                // Get the DHCP-assigned IP to extract the network's first 3 octets
                IPAddress dhcpIP = WiFi.localIP();
                
                // Create static IP using network's first 3 octets + stored 4th octet
                IPAddress local_IP(dhcpIP[0], dhcpIP[1], dhcpIP[2], espConfig->wifiCfg.ips[3]);
                IPAddress gateway(dhcpIP[0], dhcpIP[1], dhcpIP[2], 1);
                IPAddress subnet(255, 255, 255, 0);
                
                // Update stored IP octets with network values
                espConfig->wifiCfg.ips[0] = dhcpIP[0];
                espConfig->wifiCfg.ips[1] = dhcpIP[1];
                espConfig->wifiCfg.ips[2] = dhcpIP[2];
                
                // Reconfigure with static IP
                WiFi.config(local_IP, gateway, subnet);
                
                Serial.print("Network octets detected: ");
                Serial.print(dhcpIP[0]); Serial.print(".");
                Serial.print(dhcpIP[1]); Serial.print(".");
                Serial.print(dhcpIP[2]); Serial.println(".x");
            }
            
            return 1;
        }
    }
    
    // Check for SSEI network
    // for (int i = 0; i < numNetworks; i++) {
    //     if (WiFi.SSID(i) == "SSEI") {
    //         WiFi.setHostname(NAME);
    //         WiFi.begin("SSEI", "Nd14il!la");
    //         WiFi.config(local_IP, gateway, subnet);
    //         return 1;
    //     }
    // }
    
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
    // mDNS is viable in AP mode immediately after softAP is configured
    MDNS.begin(NAME);
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
    bool mdnsStarted = false;
    while (true) {
        switch (espConfig->wifiCfg.state) {
            case 1:
                if (WiFi.isConnected()) {
                    // Start mDNS the first time we confirm a live connection
                    if (!mdnsStarted) {
                        MDNS.begin(NAME);
                        mdnsStarted = true;
                        Serial.print("Connected to WiFi. IP address: ");
                        Serial.println(WiFi.localIP());
                    }
                } else {
                    mdnsStarted = false;
                    connect();
                }
                break;
            default:
                break;
        }
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}
