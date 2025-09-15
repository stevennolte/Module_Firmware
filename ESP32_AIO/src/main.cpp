#include <Arduino.h>
#include "nvs_flash.h"
#include "ESPdata.h"
#include "ESPWifi.h"
#include "myLED.h"
#include "Wire.h"
#include "ESPudp.h"
// #include "Adafruit_MCP23X17.h"
#include "MCPManager.h"
#include <Adafruit_ADS1X15.h>
// #include "Indicators.h"
#include "MainPower.h"
#include "GPS.h"
#include "HardwareSerial.h"
#include "ESP32OTAPull.h"
#include "ESPsteer.h"
#include <ESPAsyncWebServer.h>
#include "littlefs.h"

//TODO: add wifi connect timer to ap mode

TwoWire twoWire = TwoWire(0);
TwoWire twoWire1 = TwoWire(1);
HardwareSerial bnoSerial(2);
HardwareSerial gpsSerial(1);

// Adafruit_MCP23X17 mcp;
Adafruit_ADS1115 ads;

// Using singleton pattern - single access point for configuration
ESPdata& espData = ESPdata::getInstance();
MCPManager& mcpManager = MCPManager::getInstance();
// MCPManager singleton instance will be initialized in setup()
// Usage example:
// MCPManager& mcpMgr = MCPManager::getInstance();
// mcpMgr.setupMotorPins(pin1, pin2);
// mcpMgr.enableMotor(pin1, pin2);

// Components using singleton instance
ESPGPS gps(&espData, &gpsSerial, &bnoSerial);  // MCPManager accessed via singleton inside class
MyLED myLED(&espData);
MainPower mainPower(&espData, &ads);  // MCPManager accessed via singleton inside class
ESPWifi espWifi(&espData);
ESPudp espUdp(&espData);
ESP32OTAPull ota;
AsyncWebServer server(80);

ESPsteer espSteer(&espData, &ads);  // MCPManager accessed via singleton inside class
std::vector<String> debugVars;


bool I2Csetup(){
  if(!twoWire.setPins(espData.pins.SDA_PIN, espData.pins.SCL_PIN)){
    Serial.println("Wire failed to set pins");
    return false;
  }
  // if(!twoWire.setClock(1000000)){
  //   Serial.println("Wire failed to set clock");
  //   return false;
  // }
  if(!twoWire.begin()){
    Serial.println("Wire failed to begin");
    return false;
  }
  byte error, address;
  int nDevices;

  Serial.println("Scanning...");

  twoWire.beginTransmission(0x20);
  error = twoWire.endTransmission();
  if (error == 0)
  {
    Serial.println("MCP23017 found at address 0x20  !");
    espData.program.mcpState = 1;
  }
  else 
  {
    Serial.println("MCP23017 not found at address 0x20");
    espData.program.mcpState = 2;
  }
  
  twoWire.beginTransmission(0x48);
  error = twoWire.endTransmission();
  if (error == 0)
  {
    Serial.println("ADS1115 found at address 0x48  !");
    espData.program.adsState = 1;
  }
  else 
  {
    Serial.println("ADS1115 not found at address 0x48");
    espData.program.adsState = 2;
  }
  if (espData.program.mcpState == 2 || espData.program.adsState == 2){
    return false;
  } else {
    return true;
  }
  
}

// #pragma region OTA
// const char *errtext(int code)
// {
// 	switch(code)
// 	{
// 		case ESP32OTAPull::UPDATE_AVAILABLE:
// 			return "An update is available but wasn't installed";
// 		case ESP32OTAPull::NO_UPDATE_PROFILE_FOUND:
// 			return "No profile matches";
// 		case ESP32OTAPull::NO_UPDATE_AVAILABLE:
// 			return "Profile matched, but update not applicable";
// 		case ESP32OTAPull::UPDATE_OK:
// 			return "An update was done, but no reboot";
// 		case ESP32OTAPull::HTTP_FAILED:
// 			return "HTTP GET failure";
// 		case ESP32OTAPull::WRITE_ERROR:
// 			return "Write error";
// 		case ESP32OTAPull::JSON_PROBLEM:
// 			return "Invalid JSON";
// 		case ESP32OTAPull::OTA_UPDATE_FAIL:
// 			return "Update fail (no OTA partition?)";
// 		default:
// 			if (code > 0)
// 				return "Unexpected HTTP response code";
// 			break;
// 	}
// 	return "Unknown error";
// }

// void OtaPullCallback(int offset, int totallength)
// {
// 	Serial.printf("Updating %d of %d (%02d%%)...\r", offset, totallength, 100 * offset / totallength);
// }

// void softwareUpdate(){
//   char basePath[] = "/%s/Releases/OTA_Config.json";
//   char CONFIG_URL[150];
//   sprintf(CONFIG_URL, basePath, NAME);
//   Serial.println(CONFIG_URL);
//   char SERVER[150];
//   sprintf(SERVER, "http://%d.%d.%d.%d:%d",espData.wifiCfg.ips[0],espData.wifiCfg.ips[1],espData.wifiCfg.ips[2],espData.otaCfg.ipAddr,espData.otaCfg.port);
//   Serial.print("CONFIG_URL: ");
//   Serial.println(CONFIG_URL);
//   Serial.print("SERVER: ");
//   Serial.println(SERVER);
  
//   ota.SetConfig(NAME);
//   ota.SetCallback(OtaPullCallback);
  
//   Serial.printf("We are running version %s of the sketch, Board='%s', Device='%s', IP='%s \n", VERSION, ARDUINO_BOARD, WiFi.macAddress().c_str(),(String)(WiFi.localIP()[3]));
//   Serial.println();
//   // Serial.printf("Checking %s to see if an update is available...\n", CONFIG_URL);
//   Serial.println();
//   int ret = ota.CheckForOTAUpdate(SERVER, CONFIG_URL, VERSION);
//   Serial.printf("CheckForOTAUpdate returned %d (%s)\n\n", ret, errtext(ret));
// }

// #pragma endregion

#pragma region Webserver

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
  Serial.println("Sent File List");
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

void handleWASzero(AsyncWebServerRequest *request) {
  // espSteer.was.zeroSteerAngle();
  espData.steer.wasZeroAngle = espData.steer.absAngle;
  Serial.println(espData.steer.absAngle);
  Serial.println(espData.steer.wasZeroAngle);
  Serial.println("WAS zeroed");
  uint8_t res = espData.saveWASzero();
  if (res == 1){
    request->send(200, "text/plain", "WAS zeroed and saved to config.json");
  } else {
    request->send(200, "text/plain", "WAS zeroed");
  }
}

void handleFirmwareUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!index) {
    Serial.printf("Update Start: %s\n", filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { // Start with max available size
      Update.printError(Serial);
    }
  }
  
  // Write the received data to the flash memory
  if (Update.write(data, len) != len) {
    Update.printError(Serial);
  }

  // If the upload is complete
  if (final) {
    if (Update.end(true)) { // True to set the size correctly
      Serial.printf("Update Success: %u bytes\n", index + len);
      request->send(200, "text/html", "Update complete! Rebooting...");
      delay(1000);
      ESP.restart();
    } else {
      Update.printError(Serial);
      request->send(500, "text/html", "Update failed.");
    }
  }
}


void updateDebugVars() {
  debugVars.clear(); // Clear the list to update it dynamically
  debugVars.push_back("Program: " + String(NAME));
  debugVars.push_back("Timestamp since boot [s]: " + String((float)(millis())/1000.0));
  debugVars.push_back("Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
  debugVars.push_back("Version: " + String(VERSION));
  debugVars.push_back("Wifi SSID: " + WiFi.SSID());
  debugVars.push_back("IP Address: " + String(espData.wifi.ips[0])+"."+String(espData.wifi.ips[1])+"."+String(espData.wifi.ips[2])+"."+String(espData.wifi.ips[3]));
  debugVars.push_back("Wifi State: " + String(espData.wifi.state));
  debugVars.push_back("Program State: " + String(espData.program.state));
  debugVars.push_back("MCP23017 State: " + String(espData.program.mcpState));
  debugVars.push_back("ADS1115 State: " + String(espData.program.adsState));
  debugVars.push_back("IMU State: " + String(espData.gps.imuState));
  debugVars.push_back("External GPS: " + String(espData.gps.externalGPS ? "Enabled" : "Disabled"));
  debugVars.push_back("GPS Data: ");
  debugVars.push_back("..Timestamp: " + String(espData.gps.fixTime));
  debugVars.push_back("..Position Type: " + String(espData.gps.positionType));
  debugVars.push_back("..Latitude: " + String(espData.gps.latitude));
  debugVars.push_back("..Longitude: " + String(espData.gps.longitude));
  debugVars.push_back("..Altitude: " + String(espData.gps.altitude));
  debugVars.push_back("..Speed: " + String(espData.gps.speedKnots));
  debugVars.push_back("..Heading: " + String(espData.gps.imuHeading));
  debugVars.push_back("..Roll: " + String(espData.gps.imuRoll));
  debugVars.push_back("..Pitch: " + String(espData.gps.imuPitch));
  debugVars.push_back("..Yaw Rate: " + String(espData.gps.imuYawRate));
  debugVars.push_back("..Fix Quality: " + String(espData.gps.fixQuality));
  debugVars.push_back("..Number of Satellites: " + String(espData.gps.numSats));
  debugVars.push_back("..HDOP: " + String(espData.gps.HDOP));
  debugVars.push_back("..Age of DGPS: " + String(espData.gps.ageDGPS));
  debugVars.push_back("..NMEA: " + String(espData.gps.nmea));
  debugVars.push_back("..Last Ntrip Data: " + String(espData.gps.lastNtripData));
  debugVars.push_back("..Last Ntrip Data Length: " + String(espData.gps.lastNtripDataLen));
  debugVars.push_back("Steer Data: ");
  debugVars.push_back("..Target Steer Angle: " + String(espData.steer.targetSteerAngle));
  debugVars.push_back("..Steer Angle: " + String(espData.steer.actSteerAngle));
  debugVars.push_back("..Absolute Angle: " + String(espData.steer.absAngle));
  debugVars.push_back("..ZeroValue: " + String(espData.steer.wasZeroAngle));
  debugVars.push_back("..Test State: " + String(espData.steer.testState));
  debugVars.push_back("..Steer Current: " + String(espData.steer.steerCurrent));
  debugVars.push_back("..Switch State: " + String(espData.steer.switchState));
  debugVars.push_back("..PWM Command: " + String(espData.steer.pwmCmd));
  debugVars.push_back("..PID Input: " + String(espData.steer.pidCmd));
  debugVars.push_back("..Status: " + String(espData.steer.status));
  debugVars.push_back("..Wireless WAS: " + String(espData.steer.wirelessWAS));
  // debugVars.push_back("..WAS byte1: " + String(espConfig.steerData.byte1));
  // debugVars.push_back("..WAS byte2: " + String(espConfig.steerData.byte2));
  // debugVars.push_back("..WAS byte3: " + String(espConfig.steerData.byte3));
  // debugVars.push_back("..WAS byte4: " + String(espConfig.steerData.byte4));
  debugVars.push_back("Steer Config: ");
  debugVars.push_back("..Settings Updated: " + String(espData.steer.settingsUpdated));
  debugVars.push_back("..Gain P: " + String(espData.steer.gainP));
  debugVars.push_back("..High PWM: " + String(espData.steer.highPWM));
  debugVars.push_back("..Low PWM: " + String(espData.steer.lowPWM));
  debugVars.push_back("..Min PWM: " + String(espData.steer.minPWM));
  debugVars.push_back("..Counts per Degree: " + String(espData.steer.countsPerDeg));
  debugVars.push_back("..Steer Offset: " + String(espData.steer.steerOffset));
  debugVars.push_back("..Ackerman Fix: " + String(espData.steer.ackermanFix));
  debugVars.push_back("..Set0: " + String(espData.steer.set0));
  debugVars.push_back("..Pulse Count: " + String(espData.steer.pulseCount));
  debugVars.push_back("..Min Speed: " + String(espData.steer.minSpeed));
  debugVars.push_back("..Set1: " + String(espData.steer.set1));
  debugVars.push_back("..Steer Msg Rate: " + String(espData.steer.steerMsgRate));
  debugVars.push_back("..PID Input Filter: " + String(espData.steer.pidInputFilt));
  debugVars.push_back("Switches: ");
  debugVars.push_back("..Steer Switch: " + String(espData.switches.steerSwitch));
  debugVars.push_back("..Work Switch: " + String(espData.switches.workSwitch));

  String sipValue = String(espData.wifi.ips[0])+"."+String(espData.wifi.ips[1])+"."+String(espData.wifi.ips[2])+"."+String(espData.wifi.ips[3]);
  int   ArrayLength  =sipValue.length()+1;    //The +1 is for the 0x00h Terminator
  char  CharArray[ArrayLength];
  sipValue.toCharArray(CharArray,ArrayLength);
  // std::string ipValue = sipValue.toCharArray();
  
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
// void handleFileList(AsyncWebServerRequest *request) {
//   DynamicJsonDocument doc(1024);
//   JsonArray array = doc.to<JsonArray>();

//   // Open LittleFS root directory and list files
//   File root = LittleFS.open("/");
//   File file = root.openNextFile();
  
//   while (file) {
//     JsonObject fileObject = array.createNestedObject();
//     fileObject["name"] = String(file.name());
//     fileObject["size"] = file.size();
//     file = root.openNextFile();
//   }

//   String jsonResponse;
//   serializeJson(doc, jsonResponse);
//   request->send(200, "application/json", jsonResponse);
//   Serial.println("Sent File List");
// }

// Reboot handler
void handleReboot(AsyncWebServerRequest *request) {
  request->send(200, "text/plain", "Rebooting...");
  delay(100); // Give some time for the response to be sent
  ESP.restart();
}

// File download handler
// void handleFileDownload(AsyncWebServerRequest *request) {
//   if (request->hasParam("filename")) {
//     String filename = request->getParam("filename")->value();
//     if (LittleFS.exists("/" + filename)) {
//       request->send(LittleFS, "/" + filename, "application/octet-stream");
//     } else {
//       request->send(404, "text/plain", "File not found");
//     }
//   } else {
//     request->send(400, "text/plain", "Filename not provided");
//   }
// }

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


void handleToggleAPMode(AsyncWebServerRequest *request) {
  static bool apModeState = false;
  apModeState = !apModeState;
  espData.wifi.apMode = apModeState ? 1 : 0;
  Serial.printf("AP Mode State: %s\n", apModeState ? "ON" : "OFF");
  request->send(200, "text/plain", apModeState ? "AP_Mode is ON" : "AP_Mode is OFF");
}

#pragma endregion

#pragma region Buttons
void IRAM_ATTR handleSteerSwitch() {
  unsigned long currentTime = millis();
  if (currentTime - espData.steer.steerSwitchLastTime > 100) {
      espData.steer.steerSwitch = true; // Set the flag
      espData.steer.steerSwitchLastTime = currentTime; // Update the debounce timestamp
    }
}

void IRAM_ATTR handleWorkSwitch() {
  unsigned long currentTime = millis();
  if (currentTime - espData.switches.workSwitchLastTime > 100) {
      espData.switches.workSwitch = true; // Set the flag
      espData.switches.workSwitchLastTime = currentTime; // Update the debounce timestamp
  }
}

void buttonSetup(){
  // Set up the GPIO pins for the buttons
  pinMode(espData.pins.STEER_SWITCH_PIN, INPUT_PULLUP);
  pinMode(espData.pins.WORK_SWITCH_PIN, INPUT_PULLUP);

  // Attach interrupts to the buttons
  attachInterrupt(digitalPinToInterrupt(espData.pins.STEER_SWITCH_PIN), handleSteerSwitch, FALLING);
  attachInterrupt(digitalPinToInterrupt(espData.pins.WORK_SWITCH_PIN), handleWorkSwitch, FALLING);
}
#pragma endregion


void normalboot(){
  // Normal boot sequence
  /** @brief Normal boot sequence */
  Serial.println("Normal Boot Sequence Initiated");
  espData.program.state = 2;
  while (espData.wifi.state != 1){
    espData.wifi.state = espWifi.connect();
    /** @brief Check for WiFi connection, if times out, create AP */
    if (millis() > 120000){
      Serial.println("Wifi connection timed out");
      Serial.println("Switching to AP mode");
      espData.wifi.state = espWifi.makeAP();
      break;
    }
  }
  // espConfig.wifiCfg.state = espWifi.makeAP();
  Serial.println("Wifi State: " + String(espData.wifi.state));
  #pragma region Server Setup
        // Serve the main HTML page
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
          Serial.println("getting index file");
          request->send(LittleFS, "/index.html");
        });
        // Route to get debug variables as JSON
        server.on("/getDebugVars", HTTP_GET, handleDebugVars);
        // Route to list files as JSON
        server.on("/getFiles", HTTP_GET, handleFileList);
        // Route to download files
        server.on("/download", HTTP_GET, handleFileDownload);

        // Handle file upload
        server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {}, handleFileUpload);

        server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {}, 
        handleFirmwareUpload);

        server.on("/reboot", HTTP_GET, handleReboot);
        
        server.on("/toggleAPMode", HTTP_POST, handleToggleAPMode); // Add this line
        // Handle toggle state update
        server.on("/zeroWAS", HTTP_GET, handleWASzero);
        // Start server
        server.on("/Module_Disconnected", HTTP_GET, [](AsyncWebServerRequest *request){
          Serial.println("Sending Module_Disconnected.svg");
          request->send(LittleFS, "/Module_Disconnected.svg", "image/svg+xml");
        });
        server.on("/Module_Connected", HTTP_GET, [](AsyncWebServerRequest *request){
          request->send(LittleFS, "/Module_Connected.svg", "image/svg+xml");
        });
        server.on("/setGpsSource", HTTP_POST, [](AsyncWebServerRequest *request){},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            StaticJsonDocument<128> doc;
            DeserializationError error = deserializeJson(doc, data, len);
            if (error) {
                request->send(400, "text/plain", "Invalid JSON");
                return;
            }
            String source = doc["source"] | "";
            if (source == "um982") {

                espData.gps.externalGPS = false;
                request->send(200, "text/plain", "UM982 GPS selected");
            } else if (source == "external") {
                espData.gps.externalGPS = true;
                request->send(200, "text/plain", "External GPS selected");
            } else {
                request->send(400, "text/plain", "Unknown GPS source");
            }
        }
    );
        // server.on("/isConnected", HTTP_GET, [](AsyncWebServerRequest *request){
        //   request->send(LittleFS, "/Modu.svg", "image/svg+xml");
        // });
        server.begin();
      #pragma endregion

  // Start other Serial Ports
  bnoSerial.setPins(espData.pins.BNO_PIN, 10);
  bnoSerial.begin(115200);
  gpsSerial.setPins(espData.pins.GPS_RX, espData.pins.GPS_TX);
  gpsSerial.begin(115200);
  
  // Grab the config
  

  // Start I2C and check for hardware
  
  if(!I2Csetup()){
    Serial.println("I2C setup failed");
    espData.program.state = 3;
    while(1){
      delay(1000);
    }
  }
  if (espData.program.mcpState == 1){
    // Initialize the original MCP instance for backward compatibility
    // mcp.begin_I2C(0x20, &twoWire);
    
    // Initialize the MCPManager singleton
    // MCPManager& mcpManager = MCPManager::getInstance();
    if (mcpManager.begin(&espData, 0x20, &twoWire)) {
      Serial.println("MCPManager initialized successfully");
    } else {
      Serial.println("MCPManager initialization failed");
    }
    
    // mcp.pinMode(espConfig.gpioDefs.rtkFix, OUTPUT);
    // mcp.digitalWrite(espConfig.gpioDefs.rtkFix, HIGH);
    // delay(1000);
    // mcp.digitalWrite(espConfig.gpioDefs.rtkFix, LOW);
  }
  if (espData.program.adsState == 1){
    ads.begin(0x48, &twoWire);
  }
  
  
  // Start GPS
  // Using MCPManager singleton approach (auto-detected when no MCP pointer provided):
  gps.init(&espUdp);
  
  // Traditional approach using MCP pointer injection:
  // gps.init(&espUdp);  // Would use MCP pointer if provided in constructor
  
  // Alternative explicit singleton method:
  // gps.initWithSingleton(&espUdp);

  // If everything is good, turn on power to autosteer
  mainPower.startTask();
  espSteer.begin(&espUdp);
  
  // Scan for Wifi networks
  // espWifi.connect();
  
  // UDP setup
  espUdp.begin(&gps);
  Serial.println("Network setup complete");
  // delay(5000);
  espData.program.state = 1;

  // Add your normal boot logic here
}

void setup(){
  Serial.begin(115200);
  delay(1000); // Give time for serial to initialize
  Serial.println("\n\nStarting up...");
  myLED.startTask();
  espData.program.confRes = espData.loadConfig();
  if (espData.program.state != 1){
    Serial.println(" Booting into Recovery Mode");
    // recoveryBoot();
  } else {
    normalboot();
  }
  // Start Wifi AP and Webserver for diagnostics
  // espConfig.wifiCfg.state = espWifi.connect();

}

void debugPrint(){
  Serial.printf("Timestamp since boot [ms]: %lu", millis());
  Serial.printf(" progName: %s", espData.program.name);
  Serial.printf(" progState: %lu", espData.program.state);
  Serial.printf(" confRes: %lu", espData.program.confRes);
  Serial.printf(" wifiRes: %lu", espData.wifi.state);
  Serial.printf(" gpsFix: %lu", espData.gps.fixQualityInt);
  Serial.printf(" ip[0]: %d", espData.wifi.ips[0]);
  Serial.printf(" ip[1]: %d", espData.wifi.ips[1]);
  Serial.printf(" ip[2]: %d", espData.wifi.ips[2]);
  Serial.printf(" ip[3]: %d", espData.wifi.ips[3]);


  
  // Serial.printf(" gpsAge: %lu", espConfig.gpsData.);
  Serial.println();
  // Serial.println(twoWire.requestFrom(0x22, 0x01));
  // Serial.printf("Mag x: %.2f mT, y: %.2f mT, z: %.2f mT, Temp: %.2f °C\n", espConfig.magData.x, espConfig.magData.y, espConfig.magData.z, (espConfig.magData.t*1.8)+32);
  // Serial.println();
  // Serial.println(espConfig.progCfg.name);
  // Serial.println();
}

void loop(){
  
  debugPrint();
  
  delay(5000);
}