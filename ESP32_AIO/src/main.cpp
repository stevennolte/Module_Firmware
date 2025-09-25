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
#include <WiFi.h>
#include <ArduinoJson.h>
#include "esp_system.h"

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

// Function declarations
void updateDebugVars();
void updateRecoveryDebugVars();
void recoveryBoot();
void normalboot();

bool I2Csetup(){
  if(!twoWire.setPins(espData.pins.SDA_PIN, espData.pins.SCL_PIN)){
    Serial.println("Wire failed to set pins");
    return false;
  }
 
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
  JsonDocument doc;
  JsonArray array = doc.to<JsonArray>();

  // Open LittleFS root directory and list files
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  
  while (file) {
    JsonObject fileObject = array.add<JsonObject>();
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

// Recovery mode specific debug variables
void updateRecoveryDebugVars() {
  debugVars.clear(); // Clear the list to update it dynamically
  debugVars.push_back("=== RECOVERY MODE ===");
  debugVars.push_back("Program: " + String(NAME));
  debugVars.push_back("Recovery Boot Timestamp [s]: " + String((float)(millis())/1000.0));
  debugVars.push_back("Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
  debugVars.push_back("Used PSRAM: " + String(ESP.getPsramSize() - ESP.getFreePsram()) + " bytes");
  debugVars.push_back("Version: " + String(VERSION));
  debugVars.push_back("Chip Model: " + String(ESP.getChipModel()));
  debugVars.push_back("Chip Revision: " + String(ESP.getChipRevision()));
  debugVars.push_back("CPU Frequency: " + String(ESP.getCpuFreqMHz()) + " MHz");
  debugVars.push_back("Flash Size: " + String(ESP.getFlashChipSize()) + " bytes");
  debugVars.push_back("Flash Speed: " + String(ESP.getFlashChipSpeed()) + " Hz");
  debugVars.push_back("MAC Address: " + WiFi.macAddress());
  debugVars.push_back("");
  debugVars.push_back("=== RECOVERY WIFI ===");
  debugVars.push_back("AP SSID: " + String(NAME) + "_RECOVERY");
  debugVars.push_back("AP Password: recovery123");
  debugVars.push_back("AP IP: 192.168.4.1");
  debugVars.push_back("WiFi Mode: AP (Access Point)");
  debugVars.push_back("Connected Clients: " + String(WiFi.softAPgetStationNum()));
  debugVars.push_back("");
  debugVars.push_back("=== SYSTEM STATUS ===");
  debugVars.push_back("Config Load Result: " + String(espData.program.confRes));
  debugVars.push_back("Program State: " + String(espData.program.state));
  debugVars.push_back("Reset Reason Core 0: " + String(esp_reset_reason()));
  debugVars.push_back("Reset Reason Core 1: " + String(esp_reset_reason()));
  debugVars.push_back("Wake Reason: " + String(esp_sleep_get_wakeup_cause()));
  debugVars.push_back("");
  debugVars.push_back("=== HARDWARE STATUS ===");
  debugVars.push_back("MCP23017 State: " + String(espData.program.mcpState));
  debugVars.push_back("ADS1115 State: " + String(espData.program.adsState));
  debugVars.push_back("IMU State: " + String(espData.gps.imuState));
  debugVars.push_back("I2C0 SDA/SCL: " + String(SDA) + "/" + String(SCL));
  debugVars.push_back("I2C0 SDA/SCL: " + String(espData.pins.SDA_PIN) + "/" + String(espData.pins.SCL_PIN));
  debugVars.push_back("");
  debugVars.push_back("=== FILE SYSTEM ===");
  debugVars.push_back("LittleFS Total: " + String(LittleFS.totalBytes()) + " bytes");
  debugVars.push_back("LittleFS Used: " + String(LittleFS.usedBytes()) + " bytes");
  debugVars.push_back("LittleFS Free: " + String(LittleFS.totalBytes() - LittleFS.usedBytes()) + " bytes");
  debugVars.push_back("");
  debugVars.push_back("=== RECOVERY ACTIONS ===");
  debugVars.push_back("• Upload new firmware via /update");
  debugVars.push_back("• Download/upload config files via /upload");
  debugVars.push_back("• Factory reset via /factoryReset");
  debugVars.push_back("• Force normal boot via /forceNormalBoot");
  debugVars.push_back("• Reboot system via /reboot");
}

// Function to serve the debug variables as JSON
void handleDebugVars(AsyncWebServerRequest *request) {
  updateDebugVars();  // Update the debug variables just before sending
  JsonDocument doc;
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

// Settings page handler
void handleSettingsPage(AsyncWebServerRequest *request) {
  Serial.println("=== SETTINGS PAGE REQUEST ===");
  Serial.println("Client IP: " + request->client()->remoteIP().toString());
  Serial.println("Request Method: " + String(request->methodToString()));
  Serial.println("Request URL: " + request->url());
  Serial.println("Serving settings page HTML...");
  
  String html = "<!DOCTYPE html><html><head><title>ESP32 Settings</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 20px; background-color: #f0f0f0; }";
  html += ".container { max-width: 800px; margin: 0 auto; background-color: white; padding: 20px; border-radius: 10px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }";
  html += ".header { text-align: center; color: #333; margin-bottom: 30px; }";
  html += ".section { margin-bottom: 25px; padding: 15px; border: 1px solid #ddd; border-radius: 8px; background-color: #f9f9f9; }";
  html += ".section-title { font-weight: bold; color: #0066cc; margin-bottom: 15px; font-size: 18px; }";
  html += ".form-group { margin-bottom: 15px; display: flex; align-items: center; }";
  html += ".form-group label { min-width: 200px; font-weight: bold; }";
  html += ".form-group input, .form-group select { flex: 1; padding: 8px; border: 1px solid #ccc; border-radius: 4px; }";
  html += ".button { background-color: #0066cc; color: white; padding: 12px 24px; border: none; border-radius: 5px; cursor: pointer; margin: 10px 5px; font-size: 16px; }";
  html += ".button:hover { background-color: #0052a3; }";
  html += ".button.danger { background-color: #cc0000; }";
  html += ".button.danger:hover { background-color: #a30000; }";
  html += ".status { margin-top: 20px; padding: 10px; border-radius: 5px; text-align: center; }";
  html += ".status.success { background-color: #d4edda; color: #155724; border: 1px solid #c3e6cb; }";
  html += ".status.error { background-color: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }";
  html += "@media (max-width: 600px) { .form-group { flex-direction: column; align-items: flex-start; } .form-group label { min-width: auto; margin-bottom: 5px; } }";
  html += "</style></head><body>";
  
  html += "<div class='container'>";
  html += "<div class='header'><h1>ESP32-AIO Settings</h1></div>";
  
  // Network Settings Section
  html += "<div class='section'>";
  html += "<div class='section-title'>Network Configuration</div>";
  html += "<div class='form-group'><label>IP Address:</label><input type='text' id='ipAddress' placeholder='192.168.1.100'></div>";
  html += "<div class='form-group'><label>WiFi SSID 1:</label><input type='text' id='ssid1' placeholder='Network Name'></div>";
  html += "<div class='form-group'><label>WiFi Password 1:</label><input type='password' id='password1' placeholder='Network Password'></div>";
  html += "<div class='form-group'><label>WiFi SSID 2:</label><input type='text' id='ssid2' placeholder='Backup Network'></div>";
  html += "<div class='form-group'><label>WiFi Password 2:</label><input type='password' id='password2' placeholder='Backup Password'></div>";
  html += "</div>";
  
  // Steering Settings Section
  html += "<div class='section'>";
  html += "<div class='section-title'>Steering Configuration</div>";
  html += "<div class='form-group'><label>PID Gain (Kp):</label><input type='number' id='kp' step='0.1' min='0' max='255'></div>";
  html += "<div class='form-group'><label>High PWM:</label><input type='number' id='highPWM' min='0' max='255'></div>";
  html += "<div class='form-group'><label>Low PWM:</label><input type='number' id='lowPWM' min='0' max='255'></div>";
  html += "<div class='form-group'><label>Min PWM:</label><input type='number' id='minPWM' min='0' max='255'></div>";
  html += "<div class='form-group'><label>Counts Per Degree:</label><input type='number' id='countsPerDeg' step='0.1' min='0'></div>";
  html += "<div class='form-group'><label>WAS Offset:</label><input type='number' id='wasOffset' step='1' min='-2048' max='2048'></div>";
  html += "<div class='form-group'><label>PID Input Filter:</label><input type='number' id='pidInputFilt' step='0.01' min='0' max='1'></div>";
  html += "<div class='form-group'><label>PID Output Filter:</label><input type='number' id='pidOutputFilt' step='0.01' min='0' max='1'></div>";
  html += "<div class='form-group'><label>Use ADS1115:</label><select id='useADS'><option value='1'>Yes</option><option value='0'>No</option></select></div>";
  html += "</div>";
  
  // System Settings Section
  html += "<div class='section'>";
  html += "<div class='section-title'>System Configuration</div>";
  html += "<div class='form-group'><label>LED Brightness (0-255):</label><input type='number' id='ledBrightness' min='0' max='255'></div>";
  html += "<div class='form-group'><label>ADS Address:</label><select id='adsAddress'><option value='0x48'>0x48</option><option value='0x49'>0x49</option><option value='0x4A'>0x4A</option><option value='0x4B'>0x4B</option></select></div>";
  html += "</div>";
  
  // Action buttons
  html += "<div style='text-align: center; margin-top: 30px;'>";
  html += "<button class='button' onclick='loadSettings()'>Load Current Settings</button>";
  html += "<button class='button' onclick='saveSettings()'>Save Settings</button>";
  html += "<button class='button' onclick='resetToDefaults()'>Reset to Defaults</button>";
  html += "<button class='button danger' onclick='window.history.back()'>Back</button>";
  html += "</div>";
  
  html += "<div id='status' class='status' style='display: none;'></div>";
  html += "</div>";
  
  // JavaScript
  html += "<script>";
  html += "function showStatus(message, isError) {";
  html += "  const status = document.getElementById('status');";
  html += "  status.textContent = message;";
  html += "  status.className = 'status ' + (isError ? 'error' : 'success');";
  html += "  status.style.display = 'block';";
  html += "  setTimeout(() => status.style.display = 'none', 5000);";
  html += "}";
  
  html += "function loadSettings() {";
  html += "  console.log('Loading settings from server...');";
  html += "  fetch('/getSettings')";
  html += "    .then(response => {";
  html += "      console.log('Settings response status:', response.status);";
  html += "      if (!response.ok) throw new Error('Network response was not ok');";
  html += "      return response.json();";
  html += "    })";
  html += "    .then(data => {";
  html += "      console.log('Settings data received:', data);";
  html += "      document.getElementById('ipAddress').value = data.ip0 + '.' + data.ip1 + '.' + data.ip2 + '.' + data.ip3;";
  html += "      document.getElementById('kp').value = data.kp;";
  html += "      document.getElementById('highPWM').value = data.highPWM;";
  html += "      document.getElementById('lowPWM').value = data.lowPWM;";
  html += "      document.getElementById('minPWM').value = data.minPWM;";
  html += "      document.getElementById('countsPerDeg').value = data.countsPerDeg;";
  html += "      document.getElementById('wasOffset').value = data.wasOffset;";
  html += "      document.getElementById('pidInputFilt').value = data.pidInputFilt;";
  html += "      document.getElementById('pidOutputFilt').value = data.pidOutputFilt;";
  html += "      document.getElementById('useADS').value = data.useADS;";
  html += "      document.getElementById('ledBrightness').value = data.ledBrightness;";
  html += "      document.getElementById('adsAddress').value = data.adsAddress;";
  html += "      showStatus('Settings loaded successfully', false);";
  html += "    })";
  html += "    .catch(error => { console.log('Settings load error:', error); showStatus('Failed to load settings: ' + error, true); });";
  html += "}";
  
  html += "function saveSettings() {";
  html += "  const ipParts = document.getElementById('ipAddress').value.split('.');";
  html += "  if (ipParts.length !== 4) { showStatus('Invalid IP address format', true); return; }";
  html += "  const formData = new FormData();";
  html += "  formData.append('ip0', parseInt(ipParts[0]) || 192);";
  html += "  formData.append('ip1', parseInt(ipParts[1]) || 168);";
  html += "  formData.append('ip2', parseInt(ipParts[2]) || 1);";
  html += "  formData.append('ip3', parseInt(ipParts[3]) || 100);";
  html += "  formData.append('kp', parseFloat(document.getElementById('kp').value) || 50);";
  html += "  formData.append('highPWM', parseInt(document.getElementById('highPWM').value) || 255);";
  html += "  formData.append('lowPWM', parseInt(document.getElementById('lowPWM').value) || 10);";
  html += "  formData.append('minPWM', parseInt(document.getElementById('minPWM').value) || 5);";
  html += "  formData.append('countsPerDeg', parseFloat(document.getElementById('countsPerDeg').value) || 10);";
  html += "  formData.append('wasOffset', parseInt(document.getElementById('wasOffset').value) || 0);";
  html += "  formData.append('pidInputFilt', parseFloat(document.getElementById('pidInputFilt').value) || 0.1);";
  html += "  formData.append('pidOutputFilt', parseFloat(document.getElementById('pidOutputFilt').value) || 0.1);";
  html += "  formData.append('useADS', document.getElementById('useADS').value);";
  html += "  formData.append('ledBrightness', parseInt(document.getElementById('ledBrightness').value) || 100);";
  html += "  fetch('/saveSettings', {";
  html += "    method: 'POST',";
  html += "    body: formData";
  html += "  })";
  html += "    .then(response => response.text())";
  html += "    .then(data => showStatus(data, false))";
  html += "    .catch(error => showStatus('Failed to save settings: ' + error, true));";
  html += "}";
  
  html += "function resetToDefaults() {";
  html += "  if (confirm('Reset all settings to defaults? This will require a reboot.')) {";
  html += "    document.getElementById('ipAddress').value = '192.168.1.100';";
  html += "    document.getElementById('kp').value = '50';";
  html += "    document.getElementById('highPWM').value = '255';";
  html += "    document.getElementById('lowPWM').value = '10';";
  html += "    document.getElementById('minPWM').value = '5';";
  html += "    document.getElementById('countsPerDeg').value = '10';";
  html += "    document.getElementById('wasOffset').value = '0';";
  html += "    document.getElementById('pidInputFilt').value = '0.1';";
  html += "    document.getElementById('pidOutputFilt').value = '0.1';";
  html += "    document.getElementById('useADS').value = '1';";
  html += "    document.getElementById('ledBrightness').value = '100';";
  html += "    document.getElementById('adsAddress').value = '0x48';";
  html += "    showStatus('Default values loaded - click Save to apply', false);";
  html += "  }";
  html += "}";
  
  html += "window.onload = loadSettings;";
  html += "</script>";
  html += "</body></html>";
  
  Serial.println("Settings page HTML generated, size: " + String(html.length()) + " bytes");
  request->send(200, "text/html", html);
  Serial.println("Settings page sent successfully");
}

// Get current settings as JSON
void handleGetSettings(AsyncWebServerRequest *request) {
  Serial.println("=== GET SETTINGS REQUEST ===");
  Serial.println("Client IP: " + request->client()->remoteIP().toString());
  Serial.println("Preparing settings JSON response...");
  
  StaticJsonDocument<1024> doc;
  
  // Network settings
  doc["ip0"] = espData.wifi.ips[0];
  doc["ip1"] = espData.wifi.ips[1];
  doc["ip2"] = espData.wifi.ips[2];
  doc["ip3"] = espData.wifi.ips[3];
  
  // Steering settings
  doc["kp"] = espData.steer.gainP;
  doc["highPWM"] = espData.steer.highPWM;
  doc["lowPWM"] = espData.steer.lowPWM;
  doc["minPWM"] = espData.steer.minPWM;
  doc["countsPerDeg"] = espData.steer.countsPerDeg;
  doc["wasOffset"] = espData.steer.steerOffset;
  doc["pidInputFilt"] = espData.steer.pidInputFilt;
  doc["pidOutputFilt"] = espData.steer.pidOutputFilt;
  doc["useADS"] = espData.steer.useADS;
  
  // System settings
  doc["ledBrightness"] = espData.program.ledBrht;
  doc["adsAddress"] = "0x48"; // Default, could be made configurable
  
  String response;
  serializeJson(doc, response);
  
  Serial.println("Settings JSON prepared:");
  Serial.println("  Network: " + String(espData.wifi.ips[0]) + "." + String(espData.wifi.ips[1]) + "." + String(espData.wifi.ips[2]) + "." + String(espData.wifi.ips[3]));
  Serial.println("  Kp: " + String(espData.steer.gainP));
  Serial.println("  High PWM: " + String(espData.steer.highPWM));
  Serial.println("  Low PWM: " + String(espData.steer.lowPWM));
  Serial.println("  Min PWM: " + String(espData.steer.minPWM));
  Serial.println("  Use ADS: " + String(espData.steer.useADS ? "true" : "false"));
  Serial.println("JSON Response size: " + String(response.length()) + " bytes");
  
  request->send(200, "application/json", response);
  Serial.println("Settings JSON sent successfully");
}

// Save settings from JSON POST
void handleSaveSettings(AsyncWebServerRequest *request) {
  Serial.println("=== SAVE SETTINGS REQUEST ===");
  Serial.println("Client IP: " + request->client()->remoteIP().toString());
  Serial.println("Request Method: " + String(request->methodToString()));
  Serial.println("Number of parameters: " + String(request->params()));
  
  // Log all received parameters
  for (int i = 0; i < request->params(); i++) {
    const AsyncWebParameter* p = request->getParam(i);
    Serial.println("Parameter [" + String(i) + "]: " + p->name() + " = " + p->value());
  }
  
  // This will be called when the request has parameters
  // We'll use a simpler approach with URL parameters for now
  
  Serial.println("Processing network settings...");
  // Update network settings if provided
  if (request->hasParam("ip0", true)) {
    int oldValue = espData.wifi.ips[0];
    espData.wifi.ips[0] = request->getParam("ip0", true)->value().toInt();
    Serial.println("  IP0: " + String(oldValue) + " -> " + String(espData.wifi.ips[0]));
  }
  if (request->hasParam("ip1", true)) {
    int oldValue = espData.wifi.ips[1];
    espData.wifi.ips[1] = request->getParam("ip1", true)->value().toInt();
    Serial.println("  IP1: " + String(oldValue) + " -> " + String(espData.wifi.ips[1]));
  }
  if (request->hasParam("ip2", true)) {
    int oldValue = espData.wifi.ips[2];
    espData.wifi.ips[2] = request->getParam("ip2", true)->value().toInt();
    Serial.println("  IP2: " + String(oldValue) + " -> " + String(espData.wifi.ips[2]));
  }
  if (request->hasParam("ip3", true)) {
    int oldValue = espData.wifi.ips[3];
    espData.wifi.ips[3] = request->getParam("ip3", true)->value().toInt();
    Serial.println("  IP3: " + String(oldValue) + " -> " + String(espData.wifi.ips[3]));
  }
  
  Serial.println("Processing steering settings...");
  // Update steering settings if provided
  if (request->hasParam("kp", true)) {
    float oldValue = espData.steer.gainP;
    espData.steer.gainP = request->getParam("kp", true)->value().toFloat();
    Serial.println("  Kp: " + String(oldValue) + " -> " + String(espData.steer.gainP));
  }
  if (request->hasParam("highPWM", true)) {
    int oldValue = espData.steer.highPWM;
    espData.steer.highPWM = request->getParam("highPWM", true)->value().toInt();
    Serial.println("  High PWM: " + String(oldValue) + " -> " + String(espData.steer.highPWM));
  }
  if (request->hasParam("lowPWM", true)) {
    int oldValue = espData.steer.lowPWM;
    espData.steer.lowPWM = request->getParam("lowPWM", true)->value().toInt();
    Serial.println("  Low PWM: " + String(oldValue) + " -> " + String(espData.steer.lowPWM));
  }
  if (request->hasParam("minPWM", true)) {
    int oldValue = espData.steer.minPWM;
    espData.steer.minPWM = request->getParam("minPWM", true)->value().toInt();
    Serial.println("  Min PWM: " + String(oldValue) + " -> " + String(espData.steer.minPWM));
  }
  if (request->hasParam("countsPerDeg", true)) {
    float oldValue = espData.steer.countsPerDeg;
    espData.steer.countsPerDeg = request->getParam("countsPerDeg", true)->value().toFloat();
    Serial.println("  Counts Per Deg: " + String(oldValue) + " -> " + String(espData.steer.countsPerDeg));
  }
  if (request->hasParam("wasOffset", true)) {
    int oldValue = espData.steer.steerOffset;
    espData.steer.steerOffset = request->getParam("wasOffset", true)->value().toInt();
    Serial.println("  WAS Offset: " + String(oldValue) + " -> " + String(espData.steer.steerOffset));
  }
  if (request->hasParam("pidInputFilt", true)) {
    float oldValue = espData.steer.pidInputFilt;
    espData.steer.pidInputFilt = request->getParam("pidInputFilt", true)->value().toFloat();
    Serial.println("  PID Input Filter: " + String(oldValue) + " -> " + String(espData.steer.pidInputFilt));
  }
  if (request->hasParam("pidOutputFilt", true)) {
    float oldValue = espData.steer.pidOutputFilt;
    espData.steer.pidOutputFilt = request->getParam("pidOutputFilt", true)->value().toFloat();
    Serial.println("  PID Output Filter: " + String(oldValue) + " -> " + String(espData.steer.pidOutputFilt));
  }
  if (request->hasParam("useADS", true)) {
    bool oldValue = espData.steer.useADS;
    String rawValue = request->getParam("useADS", true)->value();
    espData.steer.useADS = rawValue.toInt();
    Serial.println("  Use ADS raw value: '" + rawValue + "'");
    Serial.println("  Use ADS: " + String(oldValue ? "true" : "false") + " -> " + String(espData.steer.useADS ? "true" : "false"));
  }
  
  Serial.println("Processing system settings...");
  // Update system settings if provided
  if (request->hasParam("ledBrightness", true)) {
    int oldValue = espData.program.ledBrht;
    espData.program.ledBrht = request->getParam("ledBrightness", true)->value().toInt();
    Serial.println("  LED Brightness: " + String(oldValue) + " -> " + String(espData.program.ledBrht));
    // Apply the new brightness immediately
    myLED.updateBrightness();
    Serial.println("  LED brightness updated immediately");
  }
  
  Serial.println("Attempting to save configuration to NVS...");
  // Save to preferences
  bool saveResult = espData.saveConfig();
  
  if (saveResult) {
    Serial.println("Configuration saved successfully to NVS");
    request->send(200, "text/plain", "Settings saved successfully! Some changes may require a reboot.");
  } else {
    Serial.println("ERROR: Failed to save configuration to NVS");
    request->send(500, "text/plain", "Failed to save settings to memory");
  }
  Serial.println("=== SAVE SETTINGS COMPLETE ===");
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


// Recovery boot function with WiFi AP and debug web server
void recoveryBoot() {
  Serial.println("=== ENTERING RECOVERY MODE ===");
  
  // Set all LEDs to error flash pattern to indicate recovery mode
  // mcpManager.setAllLEDs(MCPLEDState::ERROR_FLASH);
  
  // Initialize LittleFS for file operations
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed - Formatting...");
    LittleFS.format();
    LittleFS.begin();
  }
  
  // Setup WiFi AP for recovery access
  String recoverySSID = String(NAME) + "_RECOVERY";
  const char* recoveryPassword = "recovery123";
  
  Serial.println("Setting up Recovery WiFi AP...");
  Serial.println("SSID: " + recoverySSID);
  Serial.println("Password: " + String(recoveryPassword));
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(recoverySSID.c_str(), recoveryPassword);
  
  // Configure AP IP address
  IPAddress local_IP(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  
  Serial.println("Recovery AP IP: " + WiFi.softAPIP().toString());
  
  // Setup recovery web server
  Serial.println("Starting Recovery Web Server...");
  
  // Serve recovery main page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = "<!DOCTYPE html><html><head><title>ESP32 Recovery Mode</title>";
    html += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<style>body { font-family: Arial; margin: 20px; background: #f0f0f0; }";
    html += ".container { max-width: 1200px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; }";
    html += ".header { background: #d32f2f; color: white; padding: 15px; margin: -20px -20px 20px -20px; border-radius: 8px 8px 0 0; }";
    html += ".section { margin: 20px 0; padding: 15px; background: #f9f9f9; border-radius: 5px; }";
    html += ".debug-vars { font-family: monospace; white-space: pre-wrap; background: #000; color: #0f0; padding: 10px; border-radius: 5px; max-height: 400px; overflow-y: auto; }";
    html += ".button { background: #1976d2; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; margin: 5px; }";
    html += ".button:hover { background: #1565c0; } .button.danger { background: #d32f2f; }";
    html += ".button.danger:hover { background: #c62828; } .file-input { margin: 10px 0; }</style></head>";
    html += "<body><div class='container'><div class='header'>";
    html += "<h1>🔧 ESP32 Recovery Mode</h1><p>System failed to boot normally - Recovery interface active</p></div>";
    html += "<div class='section'><h2>🔧 Recovery Actions</h2>";
    html += "<button class='button' onclick='location.href=\"/settings\"'>⚙️ Settings</button>";
    html += "<button class='button' onclick='location.href=\"/factoryReset\"'>Factory Reset</button>";
    html += "<button class='button' onclick='location.href=\"/forceNormalBoot\"'>Force Normal Boot</button>";
    html += "<button class='button' onclick='location.href=\"/reboot\"'>Reboot System</button>";
    html += "<button class='button' onclick='refreshDebug()'>Refresh Debug Info</button></div>";
    html += "<div class='section'><h2>📤 Firmware Update</h2>";
    html += "<form id='updateForm' enctype='multipart/form-data'>";
    html += "<div class='file-input'><input type='file' id='firmwareFile' accept='.bin' required>";
    html += "<button type='submit' class='button'>Upload Firmware</button></div></form></div>";
    html += "<div class='section'><h2>📁 File Management</h2><div>";
    html += "<input type='file' id='configFile' accept='.json'>";
    html += "<button class='button' onclick='uploadConfig()'>Upload Config</button>";
    html += "<button class='button' onclick='downloadConfig()'>Download Config</button></div></div>";
    html += "<div class='section'><h2>🔍 System Debug Information</h2>";
    html += "<div id='debugInfo' class='debug-vars'>Loading debug information...</div></div></div>";
    html += "<script>function refreshDebug() { fetch('/getRecoveryDebug').then(response => response.json())";
    html += ".then(data => { document.getElementById('debugInfo').textContent = data.join('\\n'); }); }";
    html += "setInterval(refreshDebug, 5000); refreshDebug();";
    html += "document.getElementById('updateForm').addEventListener('submit', function(e) {";
    html += "e.preventDefault(); const fileInput = document.getElementById('firmwareFile');";
    html += "if (fileInput.files[0]) { const formData = new FormData();";
    html += "formData.append('firmware', fileInput.files[0]);";
    html += "fetch('/update', { method: 'POST', body: formData }).then(response => {";
    html += "alert('Firmware upload ' + (response.ok ? 'successful!' : 'failed!')); }); } });";
    html += "function uploadConfig() { const fileInput = document.getElementById('configFile');";
    html += "if (fileInput.files[0]) { const formData = new FormData();";
    html += "formData.append('config', fileInput.files[0]);";
    html += "fetch('/upload', { method: 'POST', body: formData }).then(response => {";
    html += "alert('Config upload ' + (response.ok ? 'successful!' : 'failed!')); }); } }";
    html += "function downloadConfig() { window.open('/download?file=config.json', '_blank'); }";
    html += "</script></body></html>";
    
    request->send(200, "text/html", html);
  });
  
  // Recovery-specific debug endpoint
  server.on("/getRecoveryDebug", HTTP_GET, [](AsyncWebServerRequest *request) {
    updateRecoveryDebugVars();
    JsonDocument doc;
    JsonArray array = doc.to<JsonArray>();
    
    for (const auto& var : debugVars) {
      array.add(var);
    }
    
    String jsonResponse;
    serializeJson(doc, jsonResponse);
    request->send(200, "application/json", jsonResponse);
  });
  
  // Factory reset endpoint
  // server.on("/factoryReset", HTTP_GET, [](AsyncWebServerRequest *request) {
  //   Serial.println("Factory reset requested");
  //   request->send(200, "text/plain", "Factory reset initiated. Device will reboot...");
  //   delay(1000);
  //   LittleFS.format();
  //   ESP.restart();
  // });
  
  // Force normal boot endpoint
  server.on("/forceNormalBoot", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("Force normal boot requested");
    espData.program.state = 1; // Set to normal boot state
    espData.saveConfig();
    request->send(200, "text/plain", "Normal boot forced. Device will reboot...");
    delay(1000);
    ESP.restart();
  });
  
  // Existing handlers for compatibility
  server.on("/getDebugVars", HTTP_GET, [](AsyncWebServerRequest *request) {
    updateRecoveryDebugVars();
    JsonDocument doc;
    JsonArray array = doc.to<JsonArray>();
    
    for (const auto& var : debugVars) {
      array.add(var);
    }
    
    String jsonResponse;
    serializeJson(doc, jsonResponse);
    request->send(200, "application/json", jsonResponse);
  });
  
  Serial.println("Registering settings endpoints for recovery mode...");
  server.on("/settings", HTTP_GET, handleSettingsPage);
  server.on("/getSettings", HTTP_GET, handleGetSettings);
  server.on("/saveSettings", HTTP_POST, handleSaveSettings);
  Serial.println("Settings endpoints registered: /settings, /getSettings, /saveSettings");
  server.on("/getFiles", HTTP_GET, handleFileList);
  server.on("/download", HTTP_GET, handleFileDownload);
  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {}, handleFileUpload);
  server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {}, handleFirmwareUpload);
  server.on("/reboot", HTTP_GET, handleReboot);
  
  // Start the recovery server
  server.begin();
  Serial.println("Recovery Web Server started!");
  Serial.println("Connect to WiFi: " + recoverySSID);
  Serial.println("Password: " + String(recoveryPassword));
  Serial.println("Open browser to: http://192.168.4.1");
  Serial.println("=====================================");
  
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

  espData.setState(2);
  
  // Start other Serial Ports
  bnoSerial.setPins(espData.pins.BNO_PIN, 10);
  bnoSerial.begin(115200);
  gpsSerial.setPins(espData.pins.GPS_RX, espData.pins.GPS_TX);
  gpsSerial.begin(115200);
  
  // Grab the config
  

  // Start I2C and check for hardware
  
  if(!I2Csetup()){
    Serial.println("I2C setup failed");
    ESP.restart();
  }
  if (espData.program.mcpState == 1){
    if (mcpManager.begin(&espData, 0x20, &twoWire)) {
      Serial.println("MCPManager initialized successfully");
    } else {
      Serial.println("MCPManager initialization failed");
    }
  }
  if (espData.program.adsState == 1){
    ads.begin(0x48, &twoWire);
  }
  
  // Start GPS
  // Using MCPManager singleton approach (auto-detected when no MCP pointer provided):
  gps.init(&espUdp);
    // If everything is good, turn on power to autosteer
  mainPower.startTask();

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
        Serial.println("Registering settings endpoints for normal mode...");
        server.on("/settings", HTTP_GET, handleSettingsPage);
        server.on("/getSettings", HTTP_GET, handleGetSettings);
        server.on("/saveSettings", HTTP_POST, handleSaveSettings);
        Serial.println("Settings endpoints registered: /settings, /getSettings, /saveSettings");
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
            JsonDocument doc;
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


  espSteer.begin(&espUdp);
  

  // UDP setup
  espUdp.begin(&gps);
  Serial.println("Network setup complete");
  // delay(5000);
  espData.setState(1);

  // Add your normal boot logic here
}

void setup(){
  Serial.begin(115200);
  myLED.startTask();
  // myLED.setLEDState(LEDState::NO_ERROR);
  delay(1000); // Give time for serial to initialize
  Serial.println("\n\nStarting up...");
  
  espData.program.confRes = espData.loadConfig();
  // Update LED brightness from loaded configuration
  myLED.updateBrightness();
  Serial.println("LED brightness set to: " + String(espData.program.ledBrht));
  
  if (espData.program.state != 1){
    Serial.println(" Booting into Recovery Mode");
    myLED.setLEDState(LEDState::RECOVERY_MODE);
    recoveryBoot();
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