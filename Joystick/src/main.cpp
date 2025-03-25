#include "WiFi.h"
#include <Common.h>
#include "AsyncUDP.h"
#include "MyWifi_h.h"
#include <Preferences.h>

MyWifi myWifi;

RTC_DATA_ATTR int bootCount = 0;
Preferences preferences;

class UDPMethods{
  private:
    int commandTimer = 0;
  public:
    AsyncUDP udp;
    UDPMethods(){
    }
    
    void begin(){
      udp.listen(8888);
      Serial.println("UDP Listening");
      udp.onPacket([](AsyncUDPPacket packet) {
        if (packet.data()[0]==0x80 & packet.data()[1]==0x81){
          // Serial.println(packet.remoteIP()[3]);
          switch(packet.data()[3]){
            case 157:
                ESP.restart();
                break;
            case 201:
              preferences.putUInt("ip_one", packet.data()[7]);
              preferences.putUInt("ip_two", packet.data()[8]);
              preferences.putUInt("ip_three", packet.data()[9]);
              ESP.restart();
          }
        }
      });
    }


    void sendCommands(){
      if (millis()-commandTimer > 30){
        commandTimer = millis();
        // Serial.println("Sending Commands");
        joyCmds->aogByte1 = 0x80;
        joyCmds->aogByte2 = 0x81;
        joyCmds->sourceAddress = 61;
        joyCmds->PGN = 162;
        joyCmds->length = 10;
        joyCmds->switch1 = !digitalRead(inputPins[0]);
        joyCmds->switch2 = !digitalRead(inputPins[1]);
        joyCmds->switch3 = !digitalRead(inputPins[2]);
        joyCmds->switch4 = !digitalRead(inputPins[3]);
        joyCmds->switch5 = !digitalRead(inputPins[4]);
        joyCmds->switch6 = !digitalRead(inputPins[5]);
        joyCmds->switch7 = !digitalRead(inputPins[6]);
        joyCmds->switch8 = !digitalRead(inputPins[7]);
        for (int i = 0; i<8;i++){
          Serial.print(cmds[i]);
          Serial.print("\t");
        }
        Serial.println();
        for (int i = 0; i<8;i++){
          Serial.print(!digitalRead(inputPins[i]));
          Serial.print("\t\t");
        }
        Serial.println();
        udp.writeTo(joystickCmds.bytes,sizeof(joystickCommandsStruct_t),IPAddress(myWifi.address_1,myWifi.address_2,myWifi.address_3,255),9999);
        }
      }    
    
    
};

UDPMethods udpMethods = UDPMethods();
void setup() {
   Serial.begin(115200);
   preferences.begin("my-app", false);
   pinMode(LED_BUILTIN, OUTPUT);
   delay(2000);
   Serial.println("Starting");
   myWifi.address_1 = preferences.getUInt("ip_one",192);
   myWifi.address_2 = preferences.getUInt("ip_two", 168);
   
   myWifi.address_3 = preferences.getUInt("ip_three",1);

   myWifi.address_4 = 61;
   Serial.println("Connecting to Wifi");
   if (myWifi.connect()){
    Serial.println("Connected");
    udpMethods.begin();
    
   } else {
    Serial.println("Rebooting");
    delay(1000);
    ESP.restart();
   }
   for (int i = 0; i < sizeof(inputPins); i++){
    pinMode(inputPins[i], INPUT_PULLUP);
   }
   ++bootCount;
   Serial.println("Boot number: " + String(bootCount));

   
   digitalWrite(LED_BUILTIN, HIGH);
  //  esp_sleep_enable_ext0_wakeup(GPIO_NUM_4,0);
  //  Serial.println("Sleep");
  //  esp_deep_sleep_start();
}

void loop() {
  udpMethods.sendCommands();
  if (digitalRead(LED_BUILTIN)){
    digitalWrite(LED_BUILTIN, LOW);
  } else {
    digitalWrite(LED_BUILTIN, HIGH);
  }
  delay(100);
}

