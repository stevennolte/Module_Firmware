#include "ESPudp.h"
#include <Update.h>


ESPudp::ESPudp(ESPconfig* vars) : udp() {
    espConfig = vars;
}

void ESPudp::begin(){
    
    udp.listen(8888);
    udp.onPacket([this](AsyncUDPPacket packet) {
        if (packet.data()[0]==0x80 && packet.data()[1]==0x81){
        //   espConfig.udpTimer = millis();
        
        switch (packet.data()[3]){
            case 162: //Joystick
              espConfig->joystickData.lastMsgRecieved = millis();
              for (uint8_t i = 0; i<8; i++){
                espConfig->joystickData.switchStates[i] = packet.data()[i+5];
              }
              break;
            case 200:  //Hello from AgIO
              // TODO: Send back a hello packet
              break;
            case 201:
              
              this->espConfig->wifiCfg.ips[0] = packet.data()[7];
              this->espConfig->wifiCfg.ips[1] = packet.data()[8];
              this->espConfig->wifiCfg.ips[2] = packet.data()[9];
              espConfig->updateIP();
              ESP.restart();
              break;
            case 254:
              
              break;
           
          }
        }
    });
    
}



void ESPudp::sendUDP() {
}

