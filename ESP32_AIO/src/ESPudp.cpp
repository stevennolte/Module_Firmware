/**
 * @file ESPudp.cpp
 * @brief Implementation of UDP communication services for agricultural guidance
 * 
 * @details This file implements the ESPudp class functionality for managing
 *          multiple UDP communication channels including AgOpen GPS protocol,
 *          NTRIP corrections, wireless sensors, and control interfaces.
 * 
 * @author Steve Gavel
 * @date 2024
 * @version 4.6.1
 * 
 * @see ESPudp.h for class interface definition
 */

#include "ESPudp.h"
#include <Update.h>

/**
 * @brief Constructor initializing UDP communication manager
 * 
 * @param vars Pointer to ESPdata singleton for configuration access
 * 
 * @details Initializes all UDP service objects and links to central
 *          data management for protocol configuration and data sharing.
 */
ESPudp::ESPudp(ESPdata* vars) : udp(), udpNtrip(), udpGPS(), udpWAS(){
    espData = vars;
    
    // NTRIP task and buffering disabled for direct forwarding mode
    // Initialize NTRIP circular buffer
    // ntripBufferHead = 0;
    // ntripBufferTail = 0;
    
    // Create NTRIP queue for notifications (small queue, just for wake-up signals)
    // ntripQueue = xQueueCreate(5, sizeof(uint8_t));
    
    // Create NTRIP processing task
    // xTaskCreatePinnedToCore(
    //     ntripTask,           // Task function
    //     "NTRIP_Task",        // Task name
    //     4096,                // Stack size
    //     this,                // Parameter passed to task
    //     2,                   // Task priority (lower than GPS task)
    //     &ntripTaskHandle,    // Task handle
    //     0                    // Core to run on (core 0)
    // );
}

/**
 * @brief Calculate checksum for AgOpen GPS protocol packets
 * 
 * @param data Pointer to packet data buffer
 * @param size Total size of data buffer
 * @return uint8_t Calculated checksum value
 * 
 * @details Computes XOR-based checksum for packet validation according to
 *          AgOpen GPS protocol specifications. Checksum covers data payload
 *          excluding header and checksum fields.
 */
uint8_t ESPudp::calcChecksum(uint8_t* data, size_t size){
  // Serial.print("Calculating Checksum: ");
  // Serial.println(size);  
  uint8_t checksum = 0;
  // for (int i = 0; i < size; i++){
  //   Serial.print(data[i]);
  //   Serial.print(" ");
  // }
  // Serial.println();
    for (int i = 2; i < data[4] + 5; i++){
        checksum += data[i];
    }
    return checksum;
}

/**
 * @brief Initialize all UDP communication services
 * 
 * @param gps Pointer to GPS manager for NMEA data integration
 * 
 * @details Sets up UDP listeners on configured ports and establishes
 *          packet handlers for each service type including joystick control,
 *          NTRIP corrections, wireless sensors, and main AgOpen GPS communication.
 */

void ESPudp::begin(ESPGPS* gps){
    Serial.println("Starting UDP Services");
    _gps = gps;

    Serial.println("Setting up Joystick UDP on port 8887");
    udpJoystick.listen(8887);
    udpJoystick.onPacket([this](AsyncUDPPacket packet) {
      if (packet.data()[0]==0x80 && packet.data()[1]==0x81){
        //   espData.udpTimer = millis();
        
        switch (packet.data()[3]){
          case 162:
          // if(espData->joystickData.joyStickActive){
              espData->joystick.lastMsgRecieved = millis();
            for (uint8_t i = 0; i<8; i++){
              espData->joystick.switchStates[i] = packet.data()[i+5];
            }
          // }
            break;
        }}
    });
    Serial.println("Joystick UDP listener setup complete");
    

    Serial.println("Setting up NTRIP UDP on port 2233");
    udpNtrip.listen(2233);
    udpNtrip.onPacket([this](AsyncUDPPacket packet) {
      size_t packetLength = packet.length();
      uint8_t *_data = packet.data();
      
      // Track NTRIP statistics
      espData->gps.ntripPacketCount++;
      espData->gps.ntripTotalBytes += packetLength;
      uint32_t currentTime = millis();
      if (espData->gps.ntripPacketCount == 1) {
        espData->gps.firstNtripTime = currentTime;
      }
      espData->gps.lastNtripTime = currentTime;
      
      // Direct forwarding mode: send raw data to GPS
      // Serial.print("NTRIP direct mode - forwarding: ");
      // Serial.print(packetLength);
      // Serial.print(" bytes - ");
      // for (size_t i = 0; i < min(packetLength, (size_t)16); i++) {
      //   Serial.printf("%02X ", _data[i]);
      // }
      // if (packetLength > 16) Serial.print("...");
      // Serial.println();
      
      if (_gps) {
        _gps->sendNTRIP(_data, packetLength);
      }
    });
    Serial.println("NTRIP UDP listener setup complete");
    
    Serial.println("Setting up WAS UDP on port 8889");
    udpWAS.listen(8889);
    udpWAS.onPacket([this](AsyncUDPPacket packet){
      if (packet.data()[0]==0x80 && packet.data()[1]==0x81){
        switch (packet.data()[3]){
          case 180:
            //TODO: change pgn
            espData->steer.lastWAStime = millis();
            union {
              uint32_t angle;
              uint8_t bytes[4];
            } wirelessWASunion;
            wirelessWASunion.bytes[0] = packet.data()[5];
            wirelessWASunion.bytes[1] = packet.data()[6];
            wirelessWASunion.bytes[2] = packet.data()[7];
            wirelessWASunion.bytes[3] = packet.data()[8];
            espData->steer.byte1 = packet.data()[5];
            espData->steer.byte2 = packet.data()[6];
            espData->steer.byte3 = packet.data()[7];
            espData->steer.byte4 = packet.data()[8];
            // espData->steerData.wasZeroAngle = float(espData->steerCfg.steerOffset)/float(espData->steerCfg.countsPerDeg);
            if (wirelessWASunion.angle > 2147483647){
              this->espData->steer.absAngle  = float(wirelessWASunion.angle - 4294967295)/100.0;
            } else {
              this->espData->steer.absAngle = float(wirelessWASunion.angle)/100.0;
            }
            this->espData->steer.actSteerAngle = this->espData->steer.absAngle - this->espData->steer.wasZeroAngle;

            break;
        }
      }
    });
    Serial.println("WAS UDP listener setup complete");
    
    Serial.println("Setting up main UDP on port 8888");
    
    udp.listen(8888);
    udp.onPacket([this](AsyncUDPPacket packet) {
        // Serial.println("Received UDP");
        
        if (packet.data()[0]==0x80 && packet.data()[1]==0x81){
        //   espData.udpTimer = millis();
        // Serial.println("Received AIO");  
        // Serial.println(packet.data()[3]);
        switch (packet.data()[3]){
            case 120:  //GPS reply to Hello Message, disable AIO GPS
              espData->gps.externalGPS = true;
              break;
            
            case 200:  //Hello from AgIO
              // Serial.println("Hello from AgIO");
              aioReply[0] = 0x80;
              aioReply[1] = 0x81;
              aioReply[2] = espData->wifi.ips[3];
              aioReply[3] = espData->wifi.ips[3];
              aioReply[4] = 5;

              // // Convert actSteerAngle to bytes and store in aioReply[5] and aioReply[6]
              // union {
              //     uint16_t angle;
              //     uint8_t bytes[2];
              // } angleUnion1;
              // angleUnion1.angle = static_cast<uint16_t>(espData->steerData.actSteerAngle*100);
              aioReply[5] = static_cast<uint16_t>(espData->steer.actSteerAngle*100) & 0xFF;
              aioReply[6] = static_cast<uint16_t>(espData->steer.actSteerAngle*100) >> 8;
              aioReply[7] = espData->steer.countsPerDeg & 0xFF;
              aioReply[8] = espData->steer.countsPerDeg >> 8;
              aioReply[9] = espData->steer.switchState;
              aioReply[10] = calcChecksum(aioReply, sizeof(aioReply));
              // Serial.println(sizeof(aioReply));
              udp.writeTo(aioReply, sizeof(aioReply), IPAddress(espData->wifi.ips[0],espData->wifi.ips[1], espData->wifi.ips[2],255) , espData->wifi.aioPort);
              // sendUDP(aioReply);
              // udp.writeTo(aioReply, sizeof(aioReply), espData->wifiCfg.moduleIP, espData->wifiCfg.aioPort);
              // aioReply[2] = 126;
              delay(10);
              if (espData->gps.imuState == 1){
              aioReply[2] = 79;
              aioReply[3] = 121;
              aioReply[4] = 5;
              aioReply[5] = 0;
              aioReply[6] = 0;
              aioReply[7] = 0;
              aioReply[8] = 0;  
              aioReply[9] = 0;
              aioReply[10] = calcChecksum(aioReply, sizeof(aioReply));
              udp.writeTo(aioReply, sizeof(aioReply), IPAddress(espData->wifi.ips[0],espData->wifi.ips[1], espData->wifi.ips[2],255) , espData->wifi.aioPort);
              }// TODO: Send back a hello packet
              break;
            case 201:
              
              this->espData->wifi.ips[0] = packet.data()[7];
              this->espData->wifi.ips[1] = packet.data()[8];
              this->espData->wifi.ips[2] = packet.data()[9];
              espData->updateIP();
              ESP.restart();
              break;
            case 251:

              this->espData->steer.set0 = packet.data()[5];
              this->espData->steer.pulseCount = packet.data()[6];
              this->espData->steer.minSpeed = packet.data()[7];
              this->espData->steer.set1 = packet.data()[8];
              this->espData->steer.settingsUpdated = 1;
              Serial.print("Got Steer Settings ");
              Serial.println(packet.data()[3]);
              espData->updateSteer();
              break;
            case 252:
              this->espData->steer.gainP = packet.data()[5];
              this->espData->steer.highPWM = packet.data()[6];
              this->espData->steer.lowPWM = packet.data()[7];
              this->espData->steer.minPWM = packet.data()[8];
              this->espData->steer.countsPerDeg = packet.data()[9];
              // Serial.print("counts per deg: ");
              // Serial.println(this->espData->steer.countsPerDeg);
              this->espData->steer.steerOffset = packet.data()[11] << 8 | packet.data()[10];
              this->espData->steer.ackermanFix = packet.data()[12];
              this->espData->steer.settingsUpdated = 1;
              Serial.print("Got Steer Settings ");
              Serial.println(packet.data()[3]);
             
              espData->updateSteer();
              break;
            case 254:  //GPS reply to Hello Message, disable AIO GPS
              // Serial.println("got steerdata");
              this->espData->steer.watchdog = millis();
                union {
                  uint16_t angle;
                  uint8_t bytes[2];
                } angleUnion;
                angleUnion.bytes[0] = packet.data()[8];
                angleUnion.bytes[1] = packet.data()[9];
                if (angleUnion.angle > 32767){
                  this->espData->steer.targetSteerAngle  = float(angleUnion.angle - 65536)/100.0;
                } else {
                  this->espData->steer.targetSteerAngle = float(angleUnion.angle)/100.0;
                }
                this->espData->steer.status = packet.data()[7];

              break;
            case 180:  //Wireless WAS 
            //TODO: change pgn
              espData->steer.lastWAStime = millis();
              union {
                uint32_t angle;
                uint8_t bytes[4];
              } wirelessWASunion;
              wirelessWASunion.bytes[0] = packet.data()[5];
              wirelessWASunion.bytes[1] = packet.data()[6];
              wirelessWASunion.bytes[2] = packet.data()[7];
              wirelessWASunion.bytes[3] = packet.data()[8];
              if (wirelessWASunion.angle > 32767){
                this->espData->steer.absAngle  = float(wirelessWASunion.angle - 65536)/100.0;
              } else {
                this->espData->steer.absAngle = float(wirelessWASunion.angle)/100.0;
              }
              this->espData->steer.actSteerAngle = this->espData->steer.absAngle - this->espData->steer.wasZeroAngle;
              // this->espData->steerData.actSteerAngle = this->espData->steerData.actSteerAngle + float(espData->steerCfg.steerOffset/espData->steerCfg.countsPerDeg);
              break;
          }
        }
    });
    Serial.println("Main UDP listener setup complete");
    Serial.println("All UDP Services Started Successfully");
    return;   
}



void ESPudp::sendUDP(uint8_t* _data, size_t size) {
    // Serial.println("Sent data:");
    // Serial.print("\tLen: ");
    // Serial.println(sizeof(_data));
    udp.writeTo(_data, sizeof(_data), IPAddress(espData->wifi.ips[0],espData->wifi.ips[1], espData->wifi.ips[2],255) , espData->wifi.aioPort);
}

void ESPudp::sendUDPgps(const char * data){
    // Use explicit broadcast address instead of broadcastTo() for better AP mode compatibility
    IPAddress broadcastIP(espData->wifi.ips[0], espData->wifi.ips[1], espData->wifi.ips[2], 255);
    udpGPS.writeTo((const uint8_t*)data, strlen(data), broadcastIP, 9999);
    // Yield to allow WiFi stack to transmit immediately
    taskYIELD();
}

/**
 * @brief Buffer NTRIP data for processing by the dedicated task
 * 
 * @param data Pointer to NTRIP correction data
 * @param length Length of data in bytes
 * 
 * @details Thread-safe method to buffer incoming NTRIP correction data
 *          using a circular buffer. Preserves exact byte order for proper
 *          RTCM message parsing.
 * 
 * DISABLED FOR DIRECT FORWARDING MODE
 */
/*
void ESPudp::bufferNTRIPData(const uint8_t* data, size_t length) {
    if (!data || length == 0) {
        return;
    }
    
    // Add data to circular buffer
    for (size_t i = 0; i < length; i++) {
        size_t nextHead = (ntripBufferHead + 1) % NTRIP_BUFFER_SIZE;
        
        // Check for buffer overflow
        if (nextHead == ntripBufferTail) {
            Serial.println("NTRIP buffer overflow - dropping data");
            break;
        }
        
        ntripBuffer[ntripBufferHead] = data[i];
        ntripBufferHead = nextHead;
    }
    
    // Signal the task that data is available
    uint8_t signal = 1;
    xQueueSend(ntripQueue, &signal, 0);
}
*/

/**
 * @brief FreeRTOS task for processing NTRIP correction data
 * 
 * @param pvParameters Pointer to ESPudp instance
 * 
 * @details Dedicated task that processes buffered NTRIP data and parses complete
 *          RTCM messages before sending them to the GPS receiver. Ensures proper
 *          message boundaries and validates RTCM format (D3 00 preamble).
 * 
 * DISABLED FOR DIRECT FORWARDING MODE
 */
/*
void ESPudp::ntripTask(void* pvParameters) {
    ESPudp* udpInstance = (ESPudp*)pvParameters;
    uint8_t signal;
    static uint8_t rtcmBuffer[1024];  // Increased buffer for larger RTCM messages (was 512)
    static size_t rtcmIndex = 0;
    static bool inMessage = false;
    static uint16_t expectedLength = 0;
    
    Serial.println("NTRIP processing task started");
    
    while (true) {
        // Wait for signal that data is available
        if (xQueueReceive(udpInstance->ntripQueue, &signal, portMAX_DELAY) == pdTRUE) {
            
            // Process all available data in circular buffer
            while (udpInstance->ntripBufferTail != udpInstance->ntripBufferHead) {
                uint8_t byte = udpInstance->ntripBuffer[udpInstance->ntripBufferTail];
                udpInstance->ntripBufferTail = (udpInstance->ntripBufferTail + 1) % udpInstance->NTRIP_BUFFER_SIZE;
                
                if (!inMessage) {
                    // Look for RTCM preamble D3 00
                    if (rtcmIndex == 0 && byte == 0xD3) {
                        rtcmBuffer[rtcmIndex++] = byte;
                    } else if (rtcmIndex == 1 && byte == 0x00) {
                        rtcmBuffer[rtcmIndex++] = byte;
                    } else if (rtcmIndex == 2) {
                        // Third byte: high byte of length
                        rtcmBuffer[rtcmIndex++] = byte;
                        expectedLength = (byte & 0x03) << 8;  // Only lower 2 bits
                    } else if (rtcmIndex == 3) {
                        // Fourth byte: low byte of length
                        rtcmBuffer[rtcmIndex++] = byte;
                        expectedLength |= byte;
                        expectedLength += 6;  // Add header (3) + CRC (3) bytes
                        inMessage = true;
                        
                        Serial.printf("RTCM message started, expected length: %d\n", expectedLength);
                    } else {
                        // Reset if we don't find proper preamble
                        rtcmIndex = 0;
                        if (byte == 0xD3) {
                            rtcmBuffer[rtcmIndex++] = byte;
                        }
                    }
                } else {
                    // We're in a message, collect bytes until complete
                    if (rtcmIndex < sizeof(rtcmBuffer)) {
                        rtcmBuffer[rtcmIndex++] = byte;
                        
                        // Check if message is complete
                        if (rtcmIndex >= expectedLength) {
                            // Send complete RTCM message to GPS
                            if (udpInstance->_gps) {
                                udpInstance->_gps->sendNTRIP(rtcmBuffer, rtcmIndex);
                                Serial.printf("Complete RTCM message sent: %d bytes\n", rtcmIndex);
                            }
                            
                            // Reset for next message
                            rtcmIndex = 0;
                            inMessage = false;
                            expectedLength = 0;
                        }
                    } else {
                        // Buffer overflow - reset
                        Serial.println("RTCM buffer overflow, resetting");
                        rtcmIndex = 0;
                        inMessage = false;
                        expectedLength = 0;
                    }
                }
            }
        }
    }
}
*/



