#include <WiFi.h>
#include "Arduino.h"
#include "myWifi.h"
#include <ESPmDNS.h>


MyWifi::MyWifi()
{
    
};

uint8_t MyWifi::connect(uint8_t * ipAddr, char * sketchConfig){
    // IPAddress local_IP(192, 168, 0, getAddress());
    // IPAddress local_IP(ipAddr[0], ipAddr[1], ipAddr[2], ipAddr[3]);
    // IPAddress gateway(ipAddr[0], ipAddr[1], ipAddr[2], 1);
    // IPAddress subnet(255, 255, 255, 0);
    hostName = sketchConfig;
    int n = WiFi.scanNetworks();
    for (int i=0; i<min(n,25); i++){
        for (int si=0; si<3;si++){
          
          if (WiFi.SSID(i) == ssids[si]){
            ssid = ssids[si];
            password = passwords[si];
            Serial.print("\tWifi IP: ");
            Serial.println(ssid);
            
          }
        }  
    }
    if (ssid == ""){
        Serial.println("\tNo WiFi Found");
        return 2;
    }
    
    // if (!WiFi.config(local_IP, gateway, subnet)) {
    //           Serial.println("STA Failed to configure");
    // }
    // WiFi.mode(WIFI_AP);
    WiFi.begin(ssid, password);
    while (WiFi.waitForConnectResult() != WL_CONNECTED) {
        delay(1000);
    }
    Serial.println(WiFi.localIP().toString());
    IPAddress ip = WiFi.localIP();
    IPAddress local_IP(ip[0],ip[1],ip[2],ipAddr[3]);
    IPAddress gateway(ip[0],ip[1],ip[2],1);
    IPAddress subnet(255,255,255,0);
    WiFi.config(local_IP,gateway,subnet);
    Serial.print("\tIP Address: ");
    Serial.println(WiFi.localIP());
    if(!MDNS.begin("GPS_Receiver")) {
      Serial.println("Error starting mDNS");
    }
    startMonitor();
    return 1;
}

uint8_t MyWifi::makeAP(uint8_t * ipAddr, char * sketchConfig){
    IPAddress local_IP(ipAddr[0], ipAddr[1], ipAddr[2], ipAddr[3]);
    IPAddress gateway(ipAddr[0], ipAddr[1], ipAddr[2], 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.mode(WIFI_AP);   
    WiFi.softAP(sketchConfig, "123456789");
    delay(100);
    WiFi.softAPConfig(local_IP, local_IP, subnet);
    return 3;
}
// Task handler, runs in a separate task
void MyWifi::taskHandler(void *param) {
    // Cast the param back to the ClassA object
    MyWifi *instance = static_cast<MyWifi *>(param);
    instance->continuousLoop();  // Call the member function
}

// Start the FreeRTOS task
void MyWifi::startMonitor() {
    xTaskCreate(
        taskHandler,   // Task function
        "TaskA",       // Name of the task
        4096,          // Stack size (in words)
        this,          // Pass the current instance as the task parameter
        1,             // Priority of the task
        NULL           // Task handle (not needed)
    );
}

// Function to run in parallel
void MyWifi::continuousLoop() {
  while (true) {
    if (WiFi.status() != WL_CONNECTED){
      // ESP.restart();
      Serial.println("Reconnecting to WiFi...");
      WiFi.disconnect();
      WiFi.reconnect();
    }
    vTaskDelay(1000);
    }
  }
