#include "Arduino.h"
#include "MyWifi_h.h"
#include <WiFi.h>
// #include "definitions.h"



uint16_t addressMeasures[] = {29,83,175,299,425,541,663,789,911,1034,1162,1289,1424,1561,1704,1855,1992,2130,2268,2408,2542,2671,2795,2928,3056,3191,3337,3486,3653,3829};
uint8_t addressList[] {38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67};

MyWifi::MyWifi()
{
    
};

bool MyWifi::connect(){
    // IPAddress local_IP(192, 168, 0, getAddress());
    IPAddress local_IP(address_1, address_2, address_3, address_4);
    IPAddress gateway(address_1, address_2, address_3, 1);
    IPAddress subnet(255, 255, 255, 0);
    
    int n = WiFi.scanNetworks();
    for (int i=0; i<min(n,25); i++){
        for (int si=0; si<3;si++){
          
          if (WiFi.SSID(i) == ssids[si]){
            ssid = ssids[si];
            password = passwords[si];
          }
        }  
    }
    if (ssid == ""){
        Serial.println("No WiFi Found, REBOOTING");
        ESP.restart();
    }
    
    if (!WiFi.config(local_IP, gateway, subnet)) {
              Serial.println("STA Failed to configure");
    }
    WiFi.mode(WIFI_AP);
    WiFi.begin(ssid, password);
    while (WiFi.waitForConnectResult() != WL_CONNECTED) {
        delay(1000);
    }
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    return true;
}

uint8_t MyWifi::getAddress(){
    address_4 = 37;
    uint16_t val=0;
    uint64_t voltageSum=0;
    for (int i=0; i<100000; i++){
        voltageSum+=analogRead(addressPin);
    }
    val = voltageSum/100000;
    Serial.print("voltageSum: ");
    Serial.println(voltageSum);
    for (int i=0; i<29;i++){
        int tolerance = val*0.04;
        Serial.print("Testing val: ");
        Serial.print(val);
        Serial.print(" Tolerance: ");
        Serial.print(tolerance);
        Serial.println();
        if ((val >= addressMeasures[i]) && (val <= addressMeasures[i+1])){
            address_4 = addressList[i];
            break;
        }
    }
    // Serial.print("Tolerance: ");
    // Serial.print(tolerance);
    Serial.print(" Address Measure: ");
    Serial.print(val);
    Serial.print(" Address Assigned: ");
    Serial.println(address_4);
    // Serial.println(address);
    // address = 37 + val;
    return address_4;
}