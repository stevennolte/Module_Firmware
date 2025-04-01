

#include <Arduino.h>

#include <Wire.h>
#include <MyWifi_h.h>
#include <Adafruit_ADS1X15.h>
#include <ArduinoOTA.h>
#include <SPI.h>
#include "driver/Ledc.h"
// #include <UMS3.h>
#include "driver/timer.h"
#include "driver/gpio.h"
#include "driver/twai.h"
#include "StatusLed.h"
#include "IncomingUDPstructs.h"
#include "AsyncUDP.h"
#include "InternalStructs.h"
#include "OutgoingUDPstructs.h"
#include "CanFlowMeterStructs.h"
#include "definitions.h"
#include "SparkFun_BNO080_Arduino_Library.h" // Click here to get the library: http://librarymanager/All#SparkFun_BNO080
#include <ImuStructs.h>
#include <SD.h>
#include "ESP32OTAPull.h"
#include "ArduinoJson.h"
#include <EEPROM.h>
#include <ESPAsyncWebServer.h>

// #define CONFIG_URL   "http://192.168.0.151:5500/OTA_Config.json" // this is where you'll post your JSON filter file
 // The current version of this program
struct Config {
  char sketchConfig[64];
  char configLocation[64];
  int ip_address_1;
  int ip_address_2;
  int ip_address_3;
  int ip_address_4;
};
  // <- SD library uses 8.3 filenames
Config config;  
SPIClass *spi = NULL;
MyWifi myWifi;
StatusLED statusLed;
File myFile;
File uploadLogFile;
File dataLogFile;
hw_timer_t *My_timer = NULL;
std::vector<String> debugVars;
AsyncWebServer server(80);

class DirectionalValve{
  public:
    DirectionalValve(){}
    uint8_t pin1;
    uint8_t valveState;
    void init(){
      pinMode(pin1, OUTPUT);
      digitalWrite(pin1, LOW);
    }
    void loop(){
      if (valveState > 0){
        digitalWrite(pin1, HIGH);
      } else {
        digitalWrite(pin1, LOW);
      }
    }
};
DirectionalValve dirValve = DirectionalValve();

class FoldValve{
  public:
    uint8_t pin1;
    uint8_t pin2;
    uint8_t valveState;
    FoldValve(){}
    void init(){
      pinMode(pin1, OUTPUT);
      pinMode(pin2, OUTPUT);
      digitalWrite(pin1, LOW);
      digitalWrite(pin2, LOW);
    }
    void loop(){
      if (valveState == 0){
        digitalWrite(pin1, LOW);
        digitalWrite(pin2, LOW);
      } else if (valveState == 1){
        digitalWrite(pin1, HIGH);
        digitalWrite(pin2, HIGH);
      }
    }
};
FoldValve foldValves[7];

class FoldController{
  private:
    
  public:
    FoldController(){}

    void init(){
    }

    void loop(){
      logicController();
    }
    
    void logicController(){
      int stateCnt = 0;
      for (int i=0; i<7;i++){
        foldCmds[i]=udpFoldCmds[i];
        if (foldCmds[i]==1){
          foldValves[i].valveState = 1;
        } else if (foldCmds[i] == 0 ) {
          foldValves[i].valveState = 0;
        } else if (foldCmds[i] == 2) {
          foldValves[i].valveState = 1;
          stateCnt++;
        }
      }
      if (stateCnt > 0){
        dirValve.valveState = 1;
      } else if (stateCnt == 0){
        dirValve.valveState = 0;
      }
    }
};
FoldController foldCtrl = FoldController();

class UDPMethods{
  private:
    int heartbeatTimePrevious=0;
    int heartbeatTimeTrip=500;
    int flowDataTimePrevious=0;
    int flowDataTimeTrip=500000;
  public:
    AsyncUDP udp;
    UDPMethods(){
    }
    
    void begin(){
      udp.listen(8888);
      Serial.println("UDP Listening");
      
      udp.onPacket([](AsyncUDPPacket packet) {
        if (packet.data()[0]==0x80 & packet.data()[1]==0x81){
          programStates.udpTimer=esp_timer_get_time();
          switch(packet.data()[3]){
            case 157:
              // if (packet.data()[5] == myWifi.address_4 & packet.data()[6]==1){
                ESP.restart();
              // } else if (packet.data()[5] == myWifi.address_4 & packet.data()[6]==2){
              //   programStates.freqLoopback=true;
              // }
              // break;
            case 254:
              //cmdData.avgSpeed = (uint16_t)(packet.data()[6])*256+(uint16_t)(packet.data()[5]);
              break;
            case 229:
              break;
            case 151:
              break;
            case 150:
              for (int i = 0; i< sizeof(udpFoldCmds);i++){
                udpFoldCmds[i] = packet.data()[i+6];
              }
              break;
            case 161:
              for (int i = 0; i < 4; i++){
                timestampUnion.bytes[i] = packet.data()[i+5];
              }
              timeStamp = timestampUnion.timestampStruct.timestamp;
              break;
            case 162:   // Joystick Cmds
              for (int i = 0; i < 4; i++){
                joystickCmds[i] = packet.data()[i+5];
              }
              break;
          }
        }
      });
    }

    void loop(){
      udpCheck();
      sendHeartbeat();
    }

    void udpCheck(){
      if(millis()-programStates.udpTimer < 1000){
        programStates.udpConnected=true;
      } else {
        programStates.udpConnected=false;
      }
    }

    

    void sendHeartbeat(){
      if (millis()-heartbeatTimePrevious > heartbeatTimeTrip){
        heartbeat.heartbeatStruct.aogByte1 = 0x80;
        heartbeat.heartbeatStruct.aogByte2 = 0x81;        
        int cksum=0;
        for (int i=2;i<=sizeof(heartbeatStruct_t)-2;i++)
        {
          cksum += heartbeat.bytes[i];
          // Serial.print(heartbeat.bytes[i]);
          // Serial.print(" ");
        }
        // Serial.println((byte)(cksum));
    
        
        heartbeat.heartbeatStruct.checksum = (byte)(cksum);
        udp.writeTo(heartbeat.bytes,sizeof(heartbeatStruct_t),IPAddress(30,30,30,255),9999);
        heartbeatTimePrevious = millis();
        // Serial.println("Sent UPD");
      }
    }
};
UDPMethods udpMethods = UDPMethods();

class OtaMethods{
  private:
    
  public:
    OtaMethods(){
    }
    void begin(){
      ArduinoOTA
        .onStart([]() {
          String type;
          if (ArduinoOTA.getCommand() == U_FLASH)
            type = "sketch";
          else // U_SPIFFS
            type = "filesystem";

          // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
          Serial.println("Start updating " + type);
        })
        .onEnd([]() {
          Serial.println("\nEnd");
        })
        .onProgress([](unsigned int progress, unsigned int total) {
          Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
        })
        .onError([](ota_error_t error) {
          Serial.printf("Error[%u]: ", error);
          if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
          else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
          else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
          else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
          else if (error == OTA_END_ERROR) Serial.println("End Failed");
      });
      ArduinoOTA.begin();
      int startupTimestamp = esp_timer_get_time();
      while (esp_timer_get_time() - startupTimestamp < STARTUP_DELAY*1000){
        ArduinoOTA.handle();
      delay(3);
      }
    }
    void handle(){
      ArduinoOTA.handle();
    }
};
OtaMethods otaMethods = OtaMethods();


void OtaPullCallback(int offset, int totallength)
{
	Serial.printf("Updating %d of %d (%02d%%)...\r", offset, totallength, 100 * offset / totallength);
#if defined(LED_BUILTIN) // flicker LED on update
	static int status = LOW;
	status = status == LOW && offset < totallength ? HIGH : LOW;
	digitalWrite(LED_BUILTIN, status);
#endif
}

void debugPrint()
{
  
}

void loadConfiguration(const char* filename, Config& config) {
  // Open file for reading
  File file = SD.open(filename);

  // Allocate a temporary JsonDocument
  JsonDocument doc;

  // Deserialize the JSON document
  DeserializationError error = deserializeJson(doc, file);
  if (error)
    Serial.println(F("Failed to read file, using default configuration"));

  // Copy values from the JsonDocument to the Config
  config.ip_address_1 = doc["ip_address_1"];
  config.ip_address_2 = doc["ip_address_2"];
  config.ip_address_3 = doc["ip_address_3"];
  config.ip_address_4 = doc["ip_address_4"];
  strlcpy(config.sketchConfig,                  // <- destination
          doc["Config"],sizeof(config.sketchConfig));         // <- destination's capacity
  strlcpy(CONFIG_URL,                  // <- destination
          doc["Config_Location"],sizeof(CONFIG_URL));
  Serial.println("SD CARD CONFIG DATA:");
  Serial.print("\tSketch Config: ");
  Serial.println(config.sketchConfig);
  Serial.print("\tConfig URL: ");
  Serial.println(CONFIG_URL);
  // Close the file (Curiously, File's destructor doesn't close the file)
  file.close();
}

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

void setup(){
  EEPROM.begin(EEPROM_SIZE);
  logCnt = EEPROM.read(0)+1;
  EEPROM.write(0,logCnt);
  EEPROM.commit();
  statusLed.setup();
  pinMode(ADDRESS_PIN, INPUT);
  pinMode(POWER_RELAY_PIN, OUTPUT);
  Serial.begin(115000);
  Serial.println("Initializing");
  spi = new SPIClass(HSPI);
  spi->begin(SPI_CLK, SPI_MISO, SPI_MOSI, SPI_CS);
  if (!SD.begin(SPI_CS, *spi)) {
    Serial.println("initialization failed!");
    return;
  }
  Serial.println(F("Loading configuration..."));
  loadConfiguration(filename, config);
  myWifi.address_1 = config.ip_address_1;
  myWifi.address_2 = config.ip_address_2;
  myWifi.address_3 = config.ip_address_3;
  myWifi.address_4 = config.ip_address_4;
  myWifi.addressPin = ADDRESS_PIN;
  if(myWifi.connect()){
    statusLed.connectedToWifi();
    otaMethods.begin();
  } else {
    ESP.restart();
  }
  ESP32OTAPull ota;
  ota.SetConfig(config.sketchConfig);
  ota.SetCallback(OtaPullCallback);
  Serial.printf("We are running version %s of the sketch, Board='%s', Device='%s', IP='%s \n\r", VERSION, ARDUINO_BOARD, WiFi.macAddress().c_str(),(String)(WiFi.localIP()[3]));
	Serial.println();
  Serial.printf("Checking %s to see if an update is available...\n\r", CONFIG_URL);
  Serial.println();
  int ret = ota.CheckForOTAUpdate(CONFIG_URL, VERSION);
  Serial.print("OTA Result: ");
  Serial.println(ret);
  Serial.printf("CheckForOTAUpdate returned %d (%s)\n\r", ret, errtext(ret));
  udpMethods.begin();
  for (int i=0; i<7;i++){
    foldValves[i]=FoldValve();
    foldValves[i].pin1 = foldPins1[i];
    foldValves[i].pin2 = foldPins2[i];
    foldValves[i].init();
  }
  dirValve.pin1 = directionalValvePin;
  dirValve.init();
  digitalWrite(POWER_RELAY_PIN, HIGH);
  foldCtrl.init();
  statusLed.connectionsGood(); 
}

void loop(){
  otaMethods.handle();
  udpMethods.loop();
  foldCtrl.loop();
  dirValve.loop();
  for (int i=0; i<7; i++){
    foldValves[i].loop();
  } 
}