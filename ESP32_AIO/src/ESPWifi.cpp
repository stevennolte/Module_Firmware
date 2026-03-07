#include "ESPWifi.h"
#include "WiFi.h"
#include "Version.h"


ESPWifi::ESPWifi(ESPdata* vars){
    espData = vars;
}

uint8_t ESPWifi::connect(){
    // Ensure WiFi is in station mode and disconnected before attempting connection
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(NAME);
    WiFi.disconnect();
    delay(100);
    
    Serial.println("Scanning for networks...");
    uint8_t numNetworks = WiFi.scanNetworks();
    Serial.print("Found ");
    Serial.print(numNetworks);
    Serial.println(" networks");
    
    // Print all found networks for debugging
    for (int i = 0; i < numNetworks; i++){
        Serial.print("  ");
        Serial.print(i);
        Serial.print(": ");
        Serial.print(WiFi.SSID(i));
        Serial.print(" (");
        Serial.print(WiFi.RSSI(i));
        Serial.print(" dBm) ");
        Serial.println(WiFi.encryptionType(i) == WIFI_AUTH_OPEN ? "Open" : "Encrypted");
    }
    
    for (int i = 0; i < numNetworks; i++){
        for (int j = 0; j < 4; j++){
            // Serial.println(espData->wifiCfg.ssids[j]);
            if (WiFi.SSID(i) == espData->wifi.ssids[j]){
                Serial.print("Found configured network: ");
                Serial.println(espData->wifi.ssids[j]);
                Serial.print("Attempting connection with password: ");
                Serial.println(espData->wifi.passwords[j]);
                
                WiFi.begin(espData->wifi.ssids[j], espData->wifi.passwords[j]);
                int attempts = 0;
                while(WiFi.status() != WL_CONNECTED && attempts < 100){
                    Serial.print(".");
                    delay(100);
                    attempts++;
                }
                Serial.println();
                
                // Check if actually connected
                wl_status_t status = WiFi.status();
                Serial.print("WiFi status: ");
                Serial.print(status);
                Serial.print(" - ");
                switch(status) {
                    case WL_CONNECTED: Serial.println("Connected"); break;
                    case WL_NO_SSID_AVAIL: Serial.println("SSID not available"); break;
                    case WL_CONNECT_FAILED: Serial.println("Connection Failed (wrong password or encryption)"); break;
                    case WL_DISCONNECTED: Serial.println("Disconnected"); break;
                    default: Serial.println("Unknown status"); break;
                }
                
                if (status == WL_CONNECTED) {
                    Serial.print("Connected! IP: ");
                    Serial.println(WiFi.localIP().toString());
                    
                    // Save the network information for future use
                    IPAddress ip = WiFi.localIP();
                    espData->wifi.ips[0] = ip[0];
                    espData->wifi.ips[1] = ip[1];
                    espData->wifi.ips[2] = ip[2];
                    espData->wifi.ips[3] = ip[3];
                    espData->saveConfig();
                    
                    MDNS.begin(NAME);
                    startMonitor();
                    return 1;
                } else {
                    Serial.println("Connection failed");
                    Serial.print("Signal strength: ");
                    Serial.print(WiFi.RSSI());
                    Serial.println(" dBm");
                    WiFi.disconnect();
                    delay(1000);  // Wait before trying next network
                    // Continue to try next network
                }
            }
        }
    }
    return 2;
}

uint8_t ESPWifi::makeAP(){
    IPAddress local_IP(espData->wifi.ips[0],espData->wifi.ips[1],espData->wifi.ips[2],espData->wifi.ips[3]);
    IPAddress gateway(espData->wifi.ips[0],espData->wifi.ips[1],espData->wifi.ips[2],1);
    IPAddress subnet(255,255,255,0);
    WiFi.setHostname(NAME);
    WiFi.mode(WIFI_AP_STA);   
    WiFi.softAP(NAME, "1234567890");
    delay(100);
    WiFi.softAPConfig(local_IP, local_IP, subnet);
    MDNS.begin(NAME);
    // startMonitor();
    return 1;
}

void ESPWifi::startMonitor(){
    xTaskCreate(
        taskHandler,   // Task function
        "TaskB",       // Name of the task
        4096,          // Stack size (in words)
        this,          // Pass the current instance as the task parameter
        1,             // Priority of the task
        NULL           // Task handle (not needed)
    );
}

void ESPWifi::taskHandler(void *param){
    ESPWifi* instance = (ESPWifi*)param;
    instance->continuousLoop();
}

void ESPWifi::continuousLoop(){
    while (true){
        espData->wifi.moduleIP = WiFi.localIP();
        switch(espData->wifi.state){
            case 0:
                break;
            case 1:
            if (WiFi.status() != WL_CONNECTED) {
                // Serial.print(millis());
                Serial.println("Reconnecting to WiFi...");
                WiFi.disconnect();
                WiFi.reconnect();
              }
                break;
            case 2:
                break;
            case 3:
                if(espData->wifi.apMode==0){
                    // scanNetworks();
                }    
            // scanNetworks();
                break;
            default:
                break;
        }
        
        vTaskDelay(5000/portTICK_PERIOD_MS);
    }
    
}

void ESPWifi::scanNetworks(){
    int numNetworks = WiFi.scanNetworks();
    for (int i = 0; i < numNetworks; i++){
        Serial.println(WiFi.SSID(i));
    }
}
