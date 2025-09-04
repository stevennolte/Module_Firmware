#include <Arduino.h>
#include "espData.h"
#include "ESPdata.h"
#include "ESPWifi.h"
#include "myLED.h"
#include "Wire.h"
#include "ESPudp.h"
#include "Adafruit_MCP23X17.h"
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

Adafruit_MCP23X17 mcp;
Adafruit_ADS1115 ads;

// Using singleton pattern - single access point for configuration
espData& espData = espData::getInstance();

// Using singleton pattern - single access point for program data
ESPdata& espData = ESPdata::getInstance();

// Get MCPManager singleton instance (alternative approach)
MCPManager& mcpManager = MCPManager::getInstance();

// Components using singleton instance
GPS gps(&espData, &gpsSerial, &bnoSerial);  // Using MCPManager singleton, no MCP pointer needed
MyLED myLED(&espData);
MainPower mainPower(&espData, &mcp, &ads);
ESPWifi espWifi(&espData);
ESPudp espUdp(&espData);
ESP32OTAPull ota;
AsyncWebServer server(80);

ESPsteer espSteer(&espData, &ads, &mcp);
std::vector<String> debugVars;

// Reference shortcuts using singleton
auto& progData = espData.progData;
auto& progCfg = espData.progCfg;
auto& progState = espData.progData.state;
auto& wifiCfg = espData.wifiCfg;

bool I2Csetup(){
  if(!twoWire.setPins(espData.gpioDefs.SDA_PIN, espData.gpioDefs.SCL_PIN)){
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
    progData.mcpState = 1;
  }
  else 
  {
    Serial.println("Unknown error at address 0x20");
    progData.mcpState = 2;
  }
  
  twoWire.beginTransmission(0x48);
  error = twoWire.endTransmission();
  if (error == 0)
  {
    Serial.println("ADS1115 found at address 0x48  !");
    progData.adsState = 1;
  }
  else 
  {
    Serial.println("Unknown error at address 0x48");
    progData.adsState = 2;
  }
  if (progData.mcpState == 2 || progData.adsState == 2){
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
  espData.steerData.wasZeroAngle = espData.steerData.absAngle;
  Serial.println(espData.steerData.absAngle);
  Serial.println(espData.steerData.wasZeroAngle);
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
  debugVars.push_back("IP Address: " + String(wifiCfg.ips[0])+"."+String(wifiCfg.ips[1])+"."+String(wifiCfg.ips[2])+"."+String(wifiCfg.ips[3]));
  debugVars.push_back("Wifi State: " + String(wifiCfg.state));
  debugVars.push_back("Program State: " + String(progData.state));
  debugVars.push_back("MCP23017 State: " + String(progData.mcpState));
  debugVars.push_back("ADS1115 State: " + String(progData.adsState));
  debugVars.push_back("IMU State: " + String(espData.gpsData.imuState));
  debugVars.push_back("External GPS: " + String(espData.gpsCfg.externalGPS ? "Enabled" : "Disabled"));
  debugVars.push_back("GPS Data: ");
  debugVars.push_back("..Timestamp: " + String(espData.gpsData.fixTime));
  debugVars.push_back("..Position Type: " + String(espData.gpsData.positionType));
  debugVars.push_back("..Latitude: " + String(espData.gpsData.latitude));
  debugVars.push_back("..Longitude: " + String(espData.gpsData.longitude));
  debugVars.push_back("..Altitude: " + String(espData.gpsData.altitude));
  debugVars.push_back("..Speed: " + String(espData.gpsData.speedKnots));
  debugVars.push_back("..Heading: " + String(espData.gpsData.imuHeading));
  debugVars.push_back("..Roll: " + String(espData.gpsData.imuRoll));
  debugVars.push_back("..Pitch: " + String(espData.gpsData.imuPitch));
  debugVars.push_back("..Yaw Rate: " + String(espData.gpsData.imuYawRate));
  debugVars.push_back("..Fix Quality: " + String(espData.gpsData.fixQuality));
  debugVars.push_back("..Number of Satellites: " + String(espData.gpsData.numSats));
  debugVars.push_back("..HDOP: " + String(espData.gpsData.HDOP));
  debugVars.push_back("..Age of DGPS: " + String(espData.gpsData.ageDGPS));
  debugVars.push_back("..NMEA: " + String(espData.gpsData.nmea));
  debugVars.push_back("..Last Ntrip Data: " + String(espData.gpsData.lastNtripData));
  debugVars.push_back("..Last Ntrip Data Length: " + String(espData.gpsData.lastNtripDataLen));
  debugVars.push_back("Steer Data: ");
  debugVars.push_back("..Target Steer Angle: " + String(espData.steerData.targetSteerAngle));
  debugVars.push_back("..Steer Angle: " + String(espData.steerData.actSteerAngle));
  debugVars.push_back("..Absolute Angle: " + String(espData.steerData.absAngle));
  debugVars.push_back("..ZeroValue: " + String(espData.steerData.wasZeroAngle));
  debugVars.push_back("..Test State: " + String(espData.steerData.testState));
  debugVars.push_back("..Steer Current: " + String(espData.steerData.steerCurrent));
  debugVars.push_back("..Switch State: " + String(espData.steerData.switchState));
  debugVars.push_back("..PWM Command: " + String(espData.steerData.pwmCmd));
  debugVars.push_back("..PID Input: " + String(espData.steerData.pidCmd));
  debugVars.push_back("..Status: " + String(espData.steerData.status));
  debugVars.push_back("..Wireless WAS: " + String(espData.steerCfg.wirelessWAS));
  // debugVars.push_back("..WAS byte1: " + String(espData.steerData.byte1));
  // debugVars.push_back("..WAS byte2: " + String(espData.steerData.byte2));
  // debugVars.push_back("..WAS byte3: " + String(espData.steerData.byte3));
  // debugVars.push_back("..WAS byte4: " + String(espData.steerData.byte4));
  debugVars.push_back("Steer Config: ");
  debugVars.push_back("..Settings Updated: " + String(espData.steerCfg.settingsUpdated));
  debugVars.push_back("..Gain P: " + String(espData.steerCfg.gainP));
  debugVars.push_back("..High PWM: " + String(espData.steerCfg.highPWM));
  debugVars.push_back("..Low PWM: " + String(espData.steerCfg.lowPWM));
  debugVars.push_back("..Min PWM: " + String(espData.steerCfg.minPWM));
  debugVars.push_back("..Counts per Degree: " + String(espData.steerCfg.countsPerDeg));
  debugVars.push_back("..Steer Offset: " + String(espData.steerCfg.steerOffset));
  debugVars.push_back("..Ackerman Fix: " + String(espData.steerCfg.ackermanFix));
  debugVars.push_back("..Set0: " + String(espData.steerCfg.set0));
  debugVars.push_back("..Pulse Count: " + String(espData.steerCfg.pulseCount));
  debugVars.push_back("..Min Speed: " + String(espData.steerCfg.minSpeed));
  debugVars.push_back("..Set1: " + String(espData.steerCfg.set1));
  debugVars.push_back("..Steer Msg Rate: " + String(espData.steerCfg.steerMsgRate));
  debugVars.push_back("..PID Input Filter: " + String(espData.steerCfg.pidInputFilt));
  debugVars.push_back("Switches: ");
  debugVars.push_back("..Steer Switch: " + String(espData.switchData.steerSwitch));
  debugVars.push_back("..Work Switch: " + String(espData.switchData.workSwitch)); 
  for (int i = 0; i < 8; i++){
    debugVars.push_back("..Joystick switch " + String(i) + ": " + String(espData.joystickData.switchStates[i]));

  }
  String sipValue = String(wifiCfg.ips[0])+"."+String(wifiCfg.ips[1])+"."+String(wifiCfg.ips[2])+"."+String(wifiCfg.ips[3]);
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
  wifiCfg.apMode = apModeState ? 1 : 0;
  Serial.printf("AP Mode State: %s\n", apModeState ? "ON" : "OFF");
  request->send(200, "text/plain", apModeState ? "AP_Mode is ON" : "AP_Mode is OFF");
}

#pragma endregion

#pragma region Buttons
void IRAM_ATTR handleSteerSwitch() {
  unsigned long currentTime = millis();
  if (currentTime - espData.switchData.steerSwitchLastTime > 100) {
      espData.switchData.steerSwitch = true; // Set the flag
      espData.switchData.steerSwitchLastTime = currentTime; // Update the debounce timestamp
  }
}

void IRAM_ATTR handleWorkSwitch() {
  unsigned long currentTime = millis();
  if (currentTime - espData.switchData.workSwitchLastTime > 100) {
      espData.switchData.workSwitch = true; // Set the flag
      espData.switchData.workSwitchLastTime = currentTime; // Update the debounce timestamp
  }
}

void buttonSetup(){
  // Set up the GPIO pins for the buttons
  pinMode(espData.gpioDefs.STEER_SWITCH_PIN, INPUT_PULLUP);
  pinMode(espData.gpioDefs.WORK_SWITCH_PIN, INPUT_PULLUP);
  
  // Attach interrupts to the buttons
  attachInterrupt(digitalPinToInterrupt(espData.gpioDefs.STEER_SWITCH_PIN), handleSteerSwitch, FALLING);
  attachInterrupt(digitalPinToInterrupt(espData.gpioDefs.WORK_SWITCH_PIN), handleWorkSwitch, FALLING);
}
#pragma endregion

void setup(){
  progData.state = 0;
  myLED.startTask();
  progData.state = 2;
  
  // Start USB Serial Port
  Serial.begin(115200);
  delay(5000);   // Wait for the usb to connect so you can see the outputs at startup
  Serial.println("Starting up...");
  espData.progCfg.confRes = espData.loadConfig();
  
  // Load program data from Preferences
  espData.loadData();
  Serial.println("Program data loaded from Preferences");
  
  // Start Wifi AP and Webserver for diagnostics
  // espData.wifiCfg.state = espWifi.connect();
  
  while (wifiCfg.state != 1){
    wifiCfg.state = espWifi.connect();
    if (millis() > 120000){
      Serial.println("Wifi connection timed out");
      wifiCfg.state = espWifi.makeAP();
      break;
    }
  }
  // espData.wifiCfg.state = espWifi.makeAP();
  Serial.println("Wifi State: " + String(espData.wifiCfg.state));
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

                espData.gpsCfg.externalGPS = false;
                request->send(200, "text/plain", "UM982 GPS selected");
            } else if (source == "external") {
                espData.gpsCfg.externalGPS = true;
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
  bnoSerial.setPins(espData.gpioDefs.BNO_PIN, 10);
  bnoSerial.begin(115200);
  gpsSerial.setPins(espData.gpioDefs.GPS_RX, espData.gpioDefs.GPS_TX);
  gpsSerial.begin(115200);
  
  // Grab the config
  

  // Start I2C and check for hardware
  
  if(!I2Csetup()){
    Serial.println("I2C setup failed");
    progData.state = 3;
    while(1){
      delay(1000);
    }
  }
  if (progData.mcpState == 1){
    mcp.begin_I2C(0x20, &twoWire);
    
    // Also initialize the MCPManager singleton (alternative approach)
    mcpManager.begin(0x20, &twoWire);
    
    // mcp.pinMode(espData.gpioDefs.rtkFix, OUTPUT);
    // mcp.digitalWrite(espData.gpioDefs.rtkFix, HIGH);
    // delay(1000);
    // mcp.digitalWrite(espData.gpioDefs.rtkFix, LOW);
  }
  if (progData.adsState == 1){
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
  progData.state = 1;
  
}

void debugPrint(){
  Serial.printf("Timestamp since boot [ms]: %lu", millis());
  Serial.printf(" progName: %s", espData.progCfg.name);
  Serial.printf(" progState: %lu", progState);
  Serial.printf(" confRes: %lu", espData.progCfg.confRes);
  Serial.printf(" wifiRes: %lu", espData.wifiCfg.state);
  Serial.printf(" gpsFix: %lu", espData.gpsData.fixQualityInt);
  Serial.printf(" ip[0]: %d", espData.wifiCfg.ips[0]);
  Serial.printf(" ip[1]: %d", espData.wifiCfg.ips[1]);
  Serial.printf(" ip[2]: %d", espData.wifiCfg.ips[2]);
  Serial.printf(" ip[3]: %d", espData.wifiCfg.ips[3]);
  
  
  
  // Serial.printf(" gpsAge: %lu", espData.gpsData.);
  Serial.println();
  // Serial.println(twoWire.requestFrom(0x22, 0x01));
  // Serial.printf("Mag x: %.2f mT, y: %.2f mT, z: %.2f mT, Temp: %.2f °C\n", espData.magData.x, espData.magData.y, espData.magData.z, (espData.magData.t*1.8)+32);
  // Serial.println();
  // Serial.println(espData.progCfg.name);
  // Serial.println();
}

void loop(){
  
  //Read in NMEA from the UM980
  debugPrint();
  
  // Update program data
  espData.setUptime(millis());
  
  // Example: Save data every 30 seconds (30000 ms / 5000 ms delay = 6 cycles)
  static uint8_t saveCounter = 0;
  saveCounter++;
  if (saveCounter >= 6) {
    espData.saveData();
    saveCounter = 0;
    Serial.println("Program data saved to Preferences");
  }
  
  delay(5000);
}
