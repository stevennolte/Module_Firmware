#include "ESPWifi.h"
#include "WiFi.h"


ESPWifi::ESPWifi(ESPdata* vars){
    espData = vars;
}

uint8_t ESPWifi::connect(){
    // WiFi.disconnect();
    
    uint8_t numNetworks = WiFi.scanNetworks();
    for (int i = 0; i < numNetworks; i++){
        for (int j = 0; j < 4; j++){
            // Serial.println(espData->wifiCfg.ssids[j]);
            if (WiFi.SSID(i) == espData->wifi.ssids[j]){
                Serial.println("Found network");
                Serial.println(espData->wifi.ssids[j]);
                Serial.println(espData->wifi.passwords[j]);
                WiFi.begin(espData->wifi.ssids[j],espData->wifi.passwords[j]);
                while(WiFi.status() != WL_CONNECTED){
                    Serial.print(".");
                    delay(100);
                }
                Serial.println();
                Serial.println(WiFi.localIP().toString());
                IPAddress ip = WiFi.localIP();
                IPAddress local_IP(ip[0],ip[1],ip[2],espData->wifi.ips[3]);
                IPAddress gateway(ip[0],ip[1],ip[2],1);
                IPAddress subnet(255,255,255,0);
                WiFi.config(local_IP,gateway,subnet);
                MDNS.begin(NAME);
                Serial.println(WiFi.localIP().toString());
                startMonitor();
                return 1;
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
    return 3;
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
