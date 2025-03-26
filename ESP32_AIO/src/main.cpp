#include <Arduino.h>
#include "ESPconfig.h"
#include "ESPWifi.h"
#include "myLED.h"
#include "Wire.h"
#include "ESPudp.h"
#include "Adafruit_MCP23X17.h"
#include <Adafruit_ADS1X15.h>
// #include "Indicators.h"
#include "MainPower.h"
#include "GPS.h"
#include "HardwareSerial.h"
#include "ESP32OTAPull.h"
#include "ESPsteer.h"
#include <ESPAsyncWebServer.h>
#include "littlefs.h"




TwoWire twoWire = TwoWire(0);
TwoWire twoWire1 = TwoWire(1);
HardwareSerial bnoSerial(2);
HardwareSerial gpsSerial(1);

Adafruit_MCP23X17 mcp;
Adafruit_ADS1115 ads;

ESPconfig espConfig;

// Indicators indicators(&espConfig, &mcp);
GPS gps(&espConfig, &gpsSerial, &bnoSerial, &mcp);
MyLED myLED(&espConfig);
MainPower mainPower(&espConfig, &mcp, &ads);
ESPWifi espWifi(&espConfig);
ESPudp espUdp(&espConfig);
ESP32OTAPull ota;
AsyncWebServer server(80);

ESPsteer espSteer(&espConfig, &ads, &mcp);
std::vector<String> debugVars;

auto& progData = espConfig.progData;
auto& progCfg = espConfig.progCfg;
auto& progState = espConfig.progData.state;
auto& wifiCfg = espConfig.wifiCfg;

bool I2Csetup(){
  if(!twoWire.setPins(espConfig.gpioDefs.SDA_PIN, espConfig.gpioDefs.SCL_PIN)){
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
}

void softwareUpdate(){
  char basePath[] = "/%s/Releases/OTA_Config.json";
  char CONFIG_URL[150];
  sprintf(CONFIG_URL, basePath, NAME);
  Serial.println(CONFIG_URL);
  char SERVER[150];
  sprintf(SERVER, "http://%d.%d.%d.%d:%d",espConfig.wifiCfg.ips[0],espConfig.wifiCfg.ips[1],espConfig.wifiCfg.ips[2],espConfig.otaCfg.ipAddr,espConfig.otaCfg.port);
  Serial.print("CONFIG_URL: ");
  Serial.println(CONFIG_URL);
  Serial.print("SERVER: ");
  Serial.println(SERVER);
  
  ota.SetConfig(NAME);
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
  debugVars.push_back("IMU State: " + String(espConfig.gpsData.imuState));
  debugVars.push_back("GPS Data: ");
  debugVars.push_back("..Timestamp: " + String(espConfig.gpsData.fixTime));
  debugVars.push_back("..Position Type: " + String(espConfig.gpsData.positionType));
  debugVars.push_back("..Latitude: " + String(espConfig.gpsData.latitude));
  debugVars.push_back("..Longitude: " + String(espConfig.gpsData.longitude));
  debugVars.push_back("..Altitude: " + String(espConfig.gpsData.altitude));
  debugVars.push_back("..Speed: " + String(espConfig.gpsData.speedKnots));
  debugVars.push_back("..Heading: " + String(espConfig.gpsData.imuHeading));
  debugVars.push_back("..Roll: " + String(espConfig.gpsData.imuRoll));
  debugVars.push_back("..Pitch: " + String(espConfig.gpsData.imuPitch));
  debugVars.push_back("..Yaw Rate: " + String(espConfig.gpsData.imuYawRate));
  debugVars.push_back("..Fix Quality: " + String(espConfig.gpsData.fixQuality));
  debugVars.push_back("..Number of Satellites: " + String(espConfig.gpsData.numSats));
  debugVars.push_back("..HDOP: " + String(espConfig.gpsData.HDOP));
  debugVars.push_back("..Age of DGPS: " + String(espConfig.gpsData.ageDGPS));
  debugVars.push_back("..NMEA: " + String(espConfig.gpsData.nmea));
  debugVars.push_back("Steer Data: ");
  debugVars.push_back("..Target Steer Angle: " + String(espConfig.steerData.targetSteerAngle));
  debugVars.push_back("..Steer Angle: " + String(espConfig.steerData.actSteerAngle));
  debugVars.push_back("..Test State: " + String(espConfig.steerData.testState));
  debugVars.push_back("..Steer Current: " + String(espConfig.steerData.steerCurrent));
  debugVars.push_back("..Switch State: " + String(espConfig.steerData.switchState));
  debugVars.push_back("..PID Command: " + String(espConfig.steerData.pwmCmd));
  debugVars.push_back("..Status: " + String(espConfig.steerData.status));
  debugVars.push_back("..Wireless WAS: " + String(espConfig.steerCfg.wirelessWAS));
  // debugVars.push_back("..WAS byte1: " + String(espConfig.steerData.byte1));
  // debugVars.push_back("..WAS byte2: " + String(espConfig.steerData.byte2));
  // debugVars.push_back("..WAS byte3: " + String(espConfig.steerData.byte3));
  // debugVars.push_back("..WAS byte4: " + String(espConfig.steerData.byte4));
  debugVars.push_back("Steer Config: ");
  debugVars.push_back("..Settings Updated: " + String(espConfig.steerCfg.settingsUpdated));
  debugVars.push_back("..Gain P: " + String(espConfig.steerCfg.gainP));
  debugVars.push_back("..High PWM: " + String(espConfig.steerCfg.highPWM));
  debugVars.push_back("..Low PWM: " + String(espConfig.steerCfg.lowPWM));
  debugVars.push_back("..Min PWM: " + String(espConfig.steerCfg.minPWM));
  debugVars.push_back("..Counts per Degree: " + String(espConfig.steerCfg.countsPerDeg));
  debugVars.push_back("..Steer Offset: " + String(espConfig.steerCfg.steerOffset));
  debugVars.push_back("..Ackerman Fix: " + String(espConfig.steerCfg.ackermanFix));
  debugVars.push_back("..Set0: " + String(espConfig.steerCfg.set0));
  debugVars.push_back("..Pulse Count: " + String(espConfig.steerCfg.pulseCount));
  debugVars.push_back("..Min Speed: " + String(espConfig.steerCfg.minSpeed));
  debugVars.push_back("..Set1: " + String(espConfig.steerCfg.set1));
  debugVars.push_back("..Steer Msg Rate: " + String(espConfig.steerCfg.steerMsgRate));
  debugVars.push_back("..PID Input Filter: " + String(espConfig.steerCfg.pidInputFilt));

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

void setup(){
  progData.state = 0;
  myLED.startTask();
  progData.state = 2;
  
  // Start USB Serial Port
  Serial.begin(115200);
  delay(5000);   // Wait for the usb to connect so you can see the outputs at startup
  Serial.println("Starting up...");
  espConfig.progCfg.confRes = espConfig.loadConfig();
  // Start Wifi AP and Webserver for diagnostics
  // espConfig.wifiCfg.state = espWifi.connect();
  wifiCfg.state = espWifi.connect();
  // espConfig.wifiCfg.state = espWifi.makeAP();
  Serial.println("Wifi State: " + String(espConfig.wifiCfg.state));
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
        
        // Start server
        server.begin();
      #pragma endregion

  // Start other Serial Ports
  bnoSerial.setPins(espConfig.gpioDefs.BNO_PIN, 10);
  bnoSerial.begin(115200);
  gpsSerial.setPins(espConfig.gpioDefs.GPS_RX, espConfig.gpioDefs.GPS_TX);
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
    // mcp.pinMode(espConfig.gpioDefs.rtkFix, OUTPUT);
    // mcp.digitalWrite(espConfig.gpioDefs.rtkFix, HIGH);
    // delay(1000);
    // mcp.digitalWrite(espConfig.gpioDefs.rtkFix, LOW);
  }
  if (progData.adsState == 1){
    ads.begin(0x48, &twoWire);
  }
  
  
  // Start GPS
  // gps.init(&espUdp);

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
  Serial.printf(" progName: %s", espConfig.progCfg.name);
  Serial.printf(" progState: %lu", progState);
  Serial.printf(" confRes: %lu", espConfig.progCfg.confRes);
  Serial.printf(" wifiRes: %lu", espConfig.wifiCfg.state);
  Serial.printf(" gpsFix: %lu", espConfig.gpsData.fixQualityInt);
  Serial.printf(" ip[0]: %d", espConfig.wifiCfg.ips[0]);
  Serial.printf(" ip[1]: %d", espConfig.wifiCfg.ips[1]);
  Serial.printf(" ip[2]: %d", espConfig.wifiCfg.ips[2]);
  Serial.printf(" ip[3]: %d", espConfig.wifiCfg.ips[3]);
  
  
  
  // Serial.printf(" gpsAge: %lu", espConfig.gpsData.);
  Serial.println();
  // Serial.println(twoWire.requestFrom(0x22, 0x01));
  // Serial.printf("Mag x: %.2f mT, y: %.2f mT, z: %.2f mT, Temp: %.2f °C\n", espConfig.magData.x, espConfig.magData.y, espConfig.magData.z, (espConfig.magData.t*1.8)+32);
  // Serial.println();
  // Serial.println(espConfig.progCfg.name);
  // Serial.println();
}

void loop(){
  
  //Read in NMEA from the UM980
  debugPrint();
  
  delay(5000);
}