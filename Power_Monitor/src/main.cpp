#define CAN2

#include <Arduino.h>
#include <myWifi.h>
#include "Adafruit_INA219.h"
#include "ESP32OTAPull.h"
#include "Version.h"
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "AsyncUDP.h"
#include "messageStructs.h"
#include "myLED.h"
#include "configLoad.h"
#include "vector"
#include "Wire.h"
#include <driver/twai.h>
#include <driver/gpio.h>
#include "driver/temp_sensor.h"

#define SDA_0 5
#define SCL_0 4
#ifdef CAN1
  #define CAN1_RX_PIN 2
  #define CAN1_TX_PIN 1
#endif
#ifdef CAN2
  #define CAN1_RX_PIN 41
  #define CAN1_TX_PIN 42
#endif

#define keyPowerPin 12
#define batPowerPin 14
#define debugPrintInterval 250
#define HEARBEAT_PGN 5
#define MY_PGN 149

Heartbeat_u heartbeat;
Heartbeat_t * hbData = &heartbeat.heartbeat_t;
Shutdown_u shutdownMsg;

std::vector<String> debugVars;
ProgramData_t progData;
MyLED myLED(progData, 48, 25);
ConfigLoad config(progData);
MyWifi myWifi;
AsyncWebServer server(80);
ESP32OTAPull ota;

class UDPMethods{
  private:
    
    uint32_t udpTimeout = 1000;
    
    int heartbeatTimePrevious=0;
    int heartbeatTimeTrip=250;
    int flowDataTimePrevious=0;
    int flowDataTimeTrip=1000;
  public:
    AsyncUDP udp;
    
    UDPMethods(){
    }
    
    void begin(){
      udp.listen(8888);
      Serial.println("UDP Listening");
      
      char version[64];
      strcpy(version, VERSION);
      char *token = strtok(version, ".");
      int i = 0;
      while (token != NULL) {
        int intValue = atoi(token);
        switch (i){
          case 0:
            hbData->version1 = intValue;
            break;
          case 1:
            hbData->version2 = intValue;
            break;
          case 2:
            hbData->version3 = intValue;
            break;
        }
        i++;
        token = strtok(NULL, ".");
      }
      Serial.println("testbreak");
      
      hbData->aogByte1 = 0x80;
      hbData->aogByte2 = 0x81;
      hbData->sourceAddress = progData.ips[3];
      hbData->PGN = MY_PGN;
      hbData->length = 10;
      hbData->myPGN = HEARBEAT_PGN;
      udp.onPacket([](AsyncUDPPacket packet) {
        
        if (packet.data()[0]==0x80 & packet.data()[1]==0x81){
          progData.udpTimer = millis();
          switch (packet.data()[3]){
            case 201:
              
              progData.ips[0] = packet.data()[7];
              progData.ips[1] = packet.data()[8];
              progData.ips[2] = packet.data()[9];
              config.updateConfig();
              ESP.restart();
              break;
          
            case 149:
              switch (packet.data()[5])
                {
                case 5:
                  progData.shutdown = true;
                  break;
                case 6:
                  Serial.print("Update Cmd from Server: ");
                  Serial.println(packet.remoteIP()[3]);
                  Serial.println(packet.data()[6]);
                  Serial.println((packet.data()[8] << 8) + packet.data()[7]);
                  progData.serverAddress=packet.remoteIP()[3];
                  progData.serverPort = (packet.data()[8] << 8) + packet.data()[7];
                  
                  config.updateConfig();
                  ESP.restart();
                  break;
                
                default:
                  break;
                }
                break;
              }
              
        
        }
      });
    }

    void udpCheck(){    // 📌  udpCheck 📝 🗑️
      if (millis() - progData.udpTimer < udpTimeout){
        progData.AOGconnected == 1;
      } else
      {
        progData.AOGconnected == 2;
      }
      
    }

    void sendHeartbeat(){
      // TODO: add heartbeat
      if (millis()-heartbeatTimePrevious > heartbeatTimeTrip){
        heartbeatTimePrevious = millis();
        
        hbData->isConnectedAOG = progData.AOGconnected;
        hbData->isConnectedCAN1 = progData.can1connected;
        hbData->isConnectedCAN2 = progData.can2connected;
        hbData->isConnectedINA219 = progData.ina219Connected;
        
        
        // int cksum=0;
        // for (int i=2;i<=sizeof(Heartbeat_t)-1;i++)
        // {
        //   cksum += heartbeat.bytes[i];
        //   // Serial.print(heartbeat.bytes[i]);
        //   // Serial.print(" ");
        // }
        udp.writeTo(heartbeat.bytes,sizeof(Heartbeat_t),IPAddress(progData.ips[0],progData.ips[1],progData.ips[2],255),9999);
        
      }
    }
 
    void sendShutdown(){
      shutdownMsg.shutdown_t.aogByte1 = 0x80;
      shutdownMsg.shutdown_t.aogByte2 = 0x81;
      shutdownMsg.shutdown_t.PGN = 149;
      shutdownMsg.shutdown_t.length = 3;
      shutdownMsg.shutdown_t.myPGN = 3;
      shutdownMsg.shutdown_t.confirm = 1;
      // udp.writeTo(shutdown.bytes,sizeof(Shutdown_t),IPAddress(Config.ips[0],Config.ips[1],Config.ips[2],255),9999);
    }
};
UDPMethods udpMethods = UDPMethods();


class CanHandler{
  private:
    
    byte TX_PIN = 14;
    byte RX_PIN = 13;

  public:
    
    byte udpBuffer[512];
    uint16_t cnt = 0;
    #pragma region UDPgrainFlow
      struct __attribute__ ((packed)) GrainFlow_t{
        uint8_t aogByte1 : 8;
        uint8_t aogByte2 : 8;
        uint8_t srcAddrs : 8;
        uint8_t PGN : 8;
        uint8_t length : 8;
        uint8_t myPGN : 8;
        uint16_t massFlow : 16;
        uint16_t moisture : 16;
        uint8_t active : 16;
        uint8_t checksum : 8;
      };

      union GrainFlow_u{
        GrainFlow_t grainFlow_t;
        uint8_t bytes[sizeof(GrainFlow_t)];
      } grainFlow;
    #pragma endregion
    #pragma region CANID
      union
      {
        struct
        {
          uint8_t byte0;
          uint8_t byte1;
          uint8_t byte2;
          uint8_t byte3;
        };
        uint32_t longint;
      } canID;
    #pragma endregion
    #pragma region GrainFlowMsg
      struct __attribute__ ((packed)) CanGrainFlow_t{
        uint16_t optocode : 16;
        uint16_t grainFlow : 16;
        uint16_t moisture : 16;
      };

      union CanGrainFlow_u{
        CanGrainFlow_t canGrainFlow_t;
        uint8_t bytes[sizeof(CanGrainFlow_t)];
      } canGrainFlow;
    #pragma endregion
    
    uint32_t scaleID = 0xCCBFF90;
    uint8_t scaleCANdata[8];
    uint32_t messageTimestamp;
    uint32_t messageTimeout;
    CanHandler(){}


    byte startCAN(byte _RX_PIN, byte _TX_PIN){
      
      TX_PIN = _TX_PIN;
      RX_PIN = _RX_PIN;
      Serial.println("StartingCan");
      twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)TX_PIN, (gpio_num_t)RX_PIN, TWAI_MODE_NO_ACK);  // TWAI_MODE_NORMAL, TWAI_MODE_NO_ACK or TWAI_MODE_LISTEN_ONLY
      
      twai_timing_config_t t_config  = TWAI_TIMING_CONFIG_250KBITS();
      #ifndef CANFILTER
        twai_filter_config_t f_config  = TWAI_FILTER_CONFIG_ACCEPT_ALL();
      #endif
      
      #ifdef CANFILTER
        twai_filter_config_t f_config = {
          .acceptance_code = (0x18EEFF01<<3),    //Bit shift our target ID to the 29 MSBits
          .acceptance_mask = 0x7,    //Mask out our don't care bits (i.e., the 3 LSBits)
          .single_filter = true
        };
        twai_filter_config_t f_config;
        f_config.acceptance_code =   0x10EFFFD3<<3;
        f_config.acceptance_mask = ~(0x10111111<<3);
        f_config.single_filter = true;
       #endif
      
      // g_config.
      twai_driver_install(&g_config, &t_config, &f_config);
      
      if (twai_start() == ESP_OK) {
        printf("Driver started\n");
        
      } else {
        printf("Failed to start driver\n");
        return 2;
      }
      
      twai_status_info_t status;
      twai_get_status_info(&status);
      Serial.print("TWAI state ");
      Serial.println(status.state);
      return 1;
    }
    
        
    void handle_tx_message(twai_message_t message)
    {
      esp_err_t result = twai_transmit(&message, pdMS_TO_TICKS(100));
      if (result == ESP_OK){
      }
      else {
        Serial.printf("\n%s: Failed to queue the message for transmission.\n", esp_err_to_name(result));
      }
    }

    void transmit_normal_message(uint32_t identifier, uint8_t data[], uint8_t data_length_code = TWAI_FRAME_MAX_DLC)
      {
        // configure message to transmit
        twai_message_t message = {
          .flags = TWAI_MSG_FLAG_EXTD,
          .identifier = identifier,
          .data_length_code = data_length_code,
        };

        memcpy(message.data, data, data_length_code);

        //Transmit messages using self reception request
        handle_tx_message(message);
      }

    
    void checkCAN(){
      if (millis()-messageTimestamp < 1000){
        hbData->isConnectedISOVehicle = true;
      } else{
        hbData->isConnectedISOVehicle = false;
      }
    }

    void canRecieve(){
      twai_message_t message;
      if (twai_receive(&message, pdMS_TO_TICKS(1)) == ESP_OK) {
          
          canID.longint = message.identifier;
          // if (canID.byte1 == 0xFF && canID.byte2 == 0xFE && message.data[0] == 0x47){
          if (canID.byte0 == 0xEA && canID.byte1 == 0xFE && canID.byte2 == 0xFF && message.data[0] == 0x47){
              grainFlow.grainFlow_t.active = message.data[5];
              progData.candiag = message.identifier;
              // if (message.data[5] > 150){
              //   grainFlow.grainFlow_t.active = 2;
              // } else {
              //   grainFlow.grainFlow_t.active = 1;
              // }
          }
          
          if (canID.byte1 == 0xFF && canID.byte2 == 0xEF){
            messageTimestamp = millis();
            for (int i=0;i<message.data_length_code; i++){
              canGrainFlow.bytes[i] = message.data[i];
            }
            if (canGrainFlow.canGrainFlow_t.optocode == 2383){
              grainFlow.grainFlow_t.aogByte1 = 0x80;
              grainFlow.grainFlow_t.aogByte2 = 0x81;
              grainFlow.grainFlow_t.length = 10;
              grainFlow.grainFlow_t.PGN = 149;
              grainFlow.grainFlow_t.myPGN = 1;

              
              grainFlow.grainFlow_t.massFlow = canGrainFlow.canGrainFlow_t.grainFlow;
              grainFlow.grainFlow_t.moisture = canGrainFlow.canGrainFlow_t.moisture;
              udpMethods.udp.writeTo(grainFlow.bytes,sizeof(GrainFlow_t),IPAddress(progData.ips[0],progData.ips[1],progData.ips[2],255),9999);
             
            }
            
            
          }
         

          #pragma region UDPcanbuffer
            
            udpBuffer[cnt * 12 + 0] = canID.byte0;
            udpBuffer[cnt * 12 + 1] = canID.byte1;
            udpBuffer[cnt * 12 + 2] = canID.byte2;
            udpBuffer[cnt * 12 + 3] = canID.byte3;
            for (int i=0;i<message.data_length_code; i++){
              udpBuffer[cnt*12+4+i] = message.data[i];
            }
            cnt++;

            if (cnt == 40){
              Serial.print(millis());
              Serial.print(" ");
              Serial.println("dataLoadFull ");
              udpMethods.udp.writeTo(udpBuffer, sizeof(udpBuffer),IPAddress(progData.ips[0],progData.ips[1],progData.ips[2],255),9999);
              cnt = 0;
              for (int i = 0; i < sizeof(udpBuffer); i++){
                udpBuffer[i] = 0;
              }
            }
          #pragma endregion
        }
      }

    void sendCANbuffer(){
      uint8_t outgoingData[512];
      outgoingData[0] = 0x80;
      outgoingData[1] = 0x81;
      outgoingData[2] = 0;
      outgoingData[3] = 149;
      outgoingData[4] = 512;
      outgoingData[5] = 2;
      for (int i = 6; i < 512; i++){
        outgoingData[i] = udpBuffer[i-6];
      }
      udpMethods.udp.writeTo(outgoingData,512,IPAddress(progData.ips[0],progData.ips[1],progData.ips[2],255),9999);
    }
    
};
CanHandler canHandler = CanHandler();


class BusMonitor{
  private:
    float filterWeight = 0.95;
    float currentThreshold = 700;
  public:
    float current_mA = 0.0;
    float busvoltage = 0.0;
    byte busState = 0.0;
    Adafruit_INA219 ina219;
    BusMonitor(){}

    byte begin(){
      if (! ina219.begin()) {
        Serial.println("Failed to find INA219 chip");
        return 2;
      }
      return 1;
    }
    
    void sampleBus(){
      busvoltage = ina219.getBusVoltage_V()*(1.0-filterWeight) + busvoltage * filterWeight;
      current_mA = ina219.getCurrent_mA()*(1.0-filterWeight) + current_mA * filterWeight;
      // current_mA = ina219.getCurrent_mA();
      if (current_mA < currentThreshold){
        busState = 2;
      } else if (current_mA > currentThreshold){
        busState = 1;
      }
      return;
    }
    
};
BusMonitor busMonitor = BusMonitor();

class KeyPowerMonitor{
  private:
    byte keyPin;
    byte batPin;
    uint16_t delayTime = 10000;
    uint32_t keyOffTime;
    uint32_t keyOnTime;
    bool turnOff = false;
  public:
    uint32_t voltage;
    KeyPowerMonitor(){}

    void begin(byte _keyPin, byte _batPin){
      keyPin = _keyPin;
      batPin = _batPin;
      pinMode(keyPin, INPUT);
      pinMode(batPin, OUTPUT);
      keyOnTime = millis();
      
    }
    void turnOnBattery(){
      digitalWrite(batPin, HIGH);
    }

    void turnOffBattery(){
      
      digitalWrite(batPin, LOW);
    }

    void checkInput(){
      voltage = analogReadMilliVolts(keyPin);
      if (hbData->keyPowerState != 2){
        keyOffTime = millis();
      }
      if (voltage > 500){
        hbData->keyPowerState = 1;
        turnOnBattery();
      } else {
        hbData->keyPowerState = 2;
        udpMethods.sendShutdown();
        if (busMonitor.busState == 2 && progData.shutdown){
          turnOffBattery();
        }
        
      }
    }
};
KeyPowerMonitor keyMonitor = KeyPowerMonitor();

void initTempSensor(){
    temp_sensor_config_t temp_sensor = TSENS_CONFIG_DEFAULT();
    temp_sensor.dac_offset = TSENS_DAC_L2;  // TSENS_DAC_L2 is default; L4(-40°C ~ 20°C), L2(-10°C ~ 80°C), L1(20°C ~ 100°C), L0(50°C ~ 125°C)
    temp_sensor_set_config(temp_sensor);
    temp_sensor_start();
}

#pragma region OTA
const char *errtext(int code)
{
	switch(code)
	{
		case ESP32OTAPull::UPDATE_AVAILABLE:
			return "An update is available but wasn't installed";
		case ESP32OTAPull::NO_UPDATE_PROFILE_FOUND:
			return "No profile matches";
		case ESP32OTAPull::NO_UPDATE_AVAILABLE:
			return "Profile matched, but update not applicable";
		case ESP32OTAPull::UPDATE_OK:
			return "An update was done, but no reboot";
		case ESP32OTAPull::HTTP_FAILED:
			return "HTTP GET failure";
		case ESP32OTAPull::WRITE_ERROR:
			return "Write error";
		case ESP32OTAPull::JSON_PROBLEM:
			return "Invalid JSON";
		case ESP32OTAPull::OTA_UPDATE_FAIL:
			return "Update fail (no OTA partition?)";
		default:
			if (code > 0)
				return "Unexpected HTTP response code";
			break;
	}
	return "Unknown error";
}



void OtaPullCallback(int offset, int totallength)
{
	Serial.printf("Updating %d of %d (%02d%%)...\r", offset, totallength, 100 * offset / totallength);
// #if defined(LED_BUILTIN) // flicker LED on update
// 	static int status = LOW;
// 	status = status == LOW && offset < totallength ? HIGH : LOW;
// 	digitalWrite(LED_BUILTIN, status);
// #endif
}


void softwareUpdate(){
  char basePath[] = "/%s/Releases/OTA_Config.json";
  char CONFIG_URL[150];
  sprintf(CONFIG_URL, basePath, progData.sketchConfig);
  Serial.println(CONFIG_URL);
  

  char SERVER[150];
  sprintf(SERVER, "http://%d.%d.%d.%d:%d",progData.ips[0],progData.ips[1],progData.ips[2],progData.serverAddress,progData.serverPort);

  Serial.print("CONFIG_URL: ");
  Serial.println(CONFIG_URL);
  Serial.print("SERVER: ");
  Serial.println(SERVER);
  
  ota.SetConfig(progData.sketchConfig);
  ota.SetCallback(OtaPullCallback);
  
  Serial.printf("We are running version %s of the sketch, Board='%s', Device='%s', IP='%s \n", VERSION, ARDUINO_BOARD, WiFi.macAddress().c_str(),(String)(WiFi.localIP()[3]));
  Serial.println();
  // Serial.printf("Checking %s to see if an update is available...\n", CONFIG_URL);
  Serial.println();
  int ret = ota.CheckForOTAUpdate(SERVER, CONFIG_URL, VERSION);
  Serial.printf("CheckForOTAUpdate returned %d (%s)\n\n", ret, errtext(ret));
}

#pragma endregion
#pragma region Webserver

void updateDebugVars() {
  debugVars.clear(); // Clear the list to update it dynamically
  debugVars.push_back("Timestamp since boot [ms]: " + String(millis()));
  debugVars.push_back("Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
  debugVars.push_back("Version: " + String(VERSION));
  debugVars.push_back("Wifi IP: " + String("FERT"));
  debugVars.push_back("Wifi State: " + String(progData.wifiConnected));
  debugVars.push_back("IP Address: " + String(progData.ips[0])+"."+String(progData.ips[1])+"."+String(progData.ips[2])+"."+String(progData.ips[3]));
  debugVars.push_back("Bus Monitor: " + String(progData.ina219Connected));
  debugVars.push_back("CAN 1 State: " + String(progData.can1connected));
  debugVars.push_back("CAN 2 State: " + String(progData.can2connected));
  debugVars.push_back("CAN Diag: " + String(progData.candiag));
  debugVars.push_back("MassFlow: " + String(canHandler.grainFlow.grainFlow_t.massFlow));
  debugVars.push_back("Moisture: " + String(canHandler.grainFlow.grainFlow_t.moisture));
  debugVars.push_back("Active: " + String(canHandler.grainFlow.grainFlow_t.active));
  // debugVars.push_back("Key Power State: " + String(hbData->keyPowerState));
  // debugVars.push_back("Key Power Voltage: " + String(keyMonitor.voltage));
  // debugVars.push_back("PC Power State: " + String(hbData->pcPowerState));
  // debugVars.push_back("ISO 1 State: " + String(hbData->isConnectedISOVehicle));
  // debugVars.push_back("PC Voltage: " + String(busMonitor.busvoltage));
  // debugVars.push_back("PC Current: " + String(busMonitor.current_mA));
  debugVars.push_back("Shutdown Cmd: " + String(progData.shutdown));
  debugVars.push_back("Program State: " + String(progData.programState));
}

// Function to serve the debug variables as JSON
void handleDebugVars(AsyncWebServerRequest *request) {
  updateDebugVars();  // Update the debug variables just before sending
  DynamicJsonDocument doc(1024);
  JsonArray array = doc.to<JsonArray>();
  
  for (const auto& var : debugVars) {
    array.add(var);
  }
  
  String jsonResponse;
  serializeJson(doc, jsonResponse);
  request->send(200, "application/json", jsonResponse);
}


// Function to serve the file list as JSON
void handleFileList(AsyncWebServerRequest *request) {
  DynamicJsonDocument doc(1024);
  JsonArray array = doc.to<JsonArray>();

  // Open LittleFS root directory and list files
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  
  while (file) {
    JsonObject fileObject = array.createNestedObject();
    fileObject["name"] = String(file.name());
    fileObject["size"] = file.size();
    file = root.openNextFile();
  }

  String jsonResponse;
  serializeJson(doc, jsonResponse);
  request->send(200, "application/json", jsonResponse);
}

// Reboot handler
void handleReboot(AsyncWebServerRequest *request) {
  request->send(200, "text/plain", "Rebooting...");
  delay(100); // Give some time for the response to be sent
  ESP.restart();
}

// File download handler
void handleFileDownload(AsyncWebServerRequest *request) {
  if (request->hasParam("filename")) {
    String filename = request->getParam("filename")->value();
    if (LittleFS.exists("/" + filename)) {
      request->send(LittleFS, "/" + filename, "application/octet-stream");
    } else {
      request->send(404, "text/plain", "File not found");
    }
  } else {
    request->send(400, "text/plain", "Filename not provided");
  }
}

// File upload handler
void handleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!index) {
    // Open file for writing
    Serial.printf("UploadStart: %s\n", filename.c_str());
    request->_tempFile = LittleFS.open("/" + filename, "w");
  }
  if (len) {
    // Write the file content
    request->_tempFile.write(data, len);
  }
  if (final) {
    // Close the file
    request->_tempFile.close();
    Serial.printf("UploadEnd: %s, %u B\n", filename.c_str(), index + len);
    request->send(200, "text/plain", "File Uploaded");
  }
}


#pragma endregion


void setup(){
  Serial.begin(115200);
  config.begin();
  Serial.println("ESP32 Module:");
    Serial.print("\tSketch Name: ");
    Serial.println(progData.sketchConfig);
    Serial.print("\tVersion: ");
    Serial.println(VERSION);
  
  myLED.startTask();
  progData.programState = 1;
  Wire.setPins(5,4);
  Wire.begin();
  keyMonitor.begin(keyPowerPin, batPowerPin);
  while (true){
    progData.wifiConnected = myWifi.connect(progData.ips);
    if (progData.wifiConnected == 1){
      
      progData.programState = 3;
      softwareUpdate();
      progData.programState = 1;
      udpMethods.begin();
      #pragma region Server Setup
        // Serve the main HTML page
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
          request->send(LittleFS, "/index.html");
        });
        // Route to get debug variables as JSON
        server.on("/getDebugVars", HTTP_GET, handleDebugVars);
        // Route to list files as JSON
        server.on("/listFiles", HTTP_GET, handleFileList);
        // Route to download files
        server.on("/download", HTTP_GET, handleFileDownload);

        // Handle file upload
        server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {}, handleFileUpload);

        server.on("/reboot", HTTP_GET, handleReboot);
        // Start server
        server.begin();
      #pragma endregion
      break;
    }
  }
  progData.can1connected = canHandler.startCAN(CAN1_RX_PIN, CAN1_TX_PIN);
  // progData.can2connected = canHandler2.startCAN(CAN2_RX_PIN, CAN2_TX_PIN);
  progData.ina219Connected = busMonitor.begin();
  initTempSensor();
  progData.programState = 2;
  progData.programState = 2;

  Serial.println("Setup Complete");
}


void loop(){
  if (progData.can1connected == 1){
    canHandler.canRecieve();
    canHandler.checkCAN();
  }
  // if (progData.can2connected == 1){
  //   canHandler2.canRecieve();
  //   canHandler2.checkCAN();
  // }
  if (progData.ina219Connected == 1){
    busMonitor.sampleBus();
  }
  if (progData.wifiConnected == 1){
    udpMethods.udpCheck();
    udpMethods.sendHeartbeat();
  }
  keyMonitor.checkInput();

 
}