#include <Arduino.h>
#include "ESPconfig.h"
#include "ESPWifi.h"
#include "myLED.h"
#include "Wire.h"
#include <ESPAsyncWebServer.h>
#include "Mag_Sensor.h"
#include "ESPudp.h"
#include "MCP4725.h"
#include "Signal_Gen.h"
#include "SparkFun_TMAG5273_Arduino_Library.h"
#include "ESP32OTAPull.h"
#include "driver/temp_sensor.h"


// TwoWire twoWire = TwoWire(0);
// TwoWire twoWire1 = TwoWire(0);

TMAG5273 tmag5273;


ESPconfig espConfig;

MCP4725 mcp4725(espConfig.i2cDefs.MCP_ADDRESS, &Wire1);

MyLED myLED(&espConfig);
ESPWifi espWifi(&espConfig);
ESPudp espUdp(&espConfig);
Signal_Gen sigGen(&espConfig, &mcp4725);
Mag_Sensor magSensor(&espConfig, &tmag5273);
std::vector<String> debugVars;
ESP32OTAPull ota;
AsyncWebServer server(80);
// uint8_t count = 0;

auto& progData = espConfig.progData;
auto& progCfg = espConfig.progCfg;
auto& progState = espConfig.progData.state;
auto& wifiCfg = espConfig.wifiCfg;

uint8_t conversionAverage = TMAG5273_X32_CONVERSION;
uint8_t magneticChannel = TMAG5273_YXY_ENABLE;
uint8_t angleCalculation = TMAG5273_XY_ANGLE_CALCULATION;

bool I2Csetup(){
  // Start Wire setup
  if(!Wire.setPins(espConfig.gpioDefs.SDA_PIN, espConfig.gpioDefs.SCL_PIN)){
    Serial.println("Wire failed to set pins");
    return false;
  }
  if(!Wire.setClock(100000)){
    Serial.println("Failed to set wire clock");
  }
  if(!Wire.begin()){
    Serial.println("Wire failed to begin");
    return false;
  }
  byte error, address;
  int nDevices;

  Serial.println("Scanning...");

  Wire.beginTransmission(espConfig.i2cDefs.TMAG_ADDRESS);
  error = Wire.endTransmission();
  if (error == 0)
  {
    Serial.println("TMAG found at address 0x22  !");
    progData.tmagState = 1;
  }
  else 
  {
    Serial.println("Unknown error at address 0x22");
    progData.tmagState = 2;
  }
  // End Wire setup
  
  // Start twoWire1 setup
  Wire1.setPins(espConfig.gpioDefs.SDA_H_PIN, espConfig.gpioDefs.SCL_H_PIN);
  Wire1.begin();
  Wire1.setClock(100000);
  Wire1.beginTransmission(0x60);
  error = Wire1.endTransmission();
  if (error == 0)
  {
    Serial.println("ADS1115 found at address 0x48  !");
    progData.dacState = 1;
  }
  else 
  {
    Serial.println("Unknown error at address 0x48");
    progData.dacState = 2;
  }
  // byte address;
  // int nDevices;

  // Serial.println("Scanning...");

  // nDevices = 0;
  // for(address = 1; address < 127; address++ )
  // {
  //   // The i2c_scanner uses the return value of
  //   // the Write.endTransmisstion to see if
  //   // a device did acknowledge to the address.
  //   Wire1.beginTransmission(address);
  //   error = Wire1.endTransmission();

  //   if (error == 0)
  //   {
  //     Serial.print("I2C device found at address 0x");
  //     if (address<16)
  //       Serial.print("0");
  //     Serial.print(address,HEX);
  //     Serial.println("  !");

  //     nDevices++;
  //   }
  //   else if (error==4)
  //   {
  //     Serial.print("Unknown error at address 0x");
  //     if (address<16)
  //       Serial.print("0");
  //     Serial.println(address,HEX);
  //   }
  // }
  // if (nDevices == 0)
  //   Serial.println("No I2C devices found\n");
  // else
  //   Serial.println("done\n");

  // delay(5000);       
  if (progData.tmagState == 2 || progData.dacState == 2){
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
  float tempReading;
  temp_sensor_read_celsius(&tempReading);
  debugVars.push_back("Temp: " + String(tempReading));
  debugVars.push_back("Version: " + String(VERSION));
  debugVars.push_back("Wifi SSID: " + WiFi.SSID());
  debugVars.push_back("IP Address: " + String(wifiCfg.ips[0])+"."+String(wifiCfg.ips[1])+"."+String(wifiCfg.ips[2])+"."+String(wifiCfg.ips[3]));
  debugVars.push_back("Wifi State: " + String(wifiCfg.state));
  debugVars.push_back("Program State: " + String(progData.state));
  debugVars.push_back("MCP23017 State: " + String(progData.tmagState));
  debugVars.push_back("DAC State: " + String(progData.dacState));
  debugVars.push_back("Mag Readings:");
  debugVars.push_back("--X: " + String(espConfig.wasData.x));
  debugVars.push_back("--Y: " + String(espConfig.wasData.y));
  debugVars.push_back("--Z: " + String(espConfig.wasData.z));
  debugVars.push_back("--T: " + String(espConfig.wasData.t));
  debugVars.push_back("--Angle: " + String(espConfig.wasData.sensorAngle));
  debugVars.push_back("Sig Readings:");
  debugVars.push_back("--Value: " + String(espConfig.wasData.sigReading));


  
  

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
  // espConfig.wifiCfg.state = espWifi.makeAP();
  while (wifiCfg.state != 1){
    wifiCfg.state = espWifi.connect();
  }
  
  espUdp.begin();
  Serial.println("Wifi State: " + String(espConfig.wifiCfg.state));
  #pragma region Server Setup
        // Serve the main HTML page
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
          Serial.println("getting index file");
          request->send(LittleFS, "/index.html");
        });
        server.on("/settings.html", HTTP_GET, [](AsyncWebServerRequest *request){
          Serial.println("getting settings file");
          request->send(LittleFS, "/settings.html");
        });
        server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request){
          Serial.println("getting index file");
          request->send(LittleFS, "/index.html");
        });
        server.on("/controls.html", HTTP_GET, [](AsyncWebServerRequest *request){
          Serial.println("getting controls file");
          request->send(LittleFS, "/controls.html");
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
      temp_sensor_start();
      if(!I2Csetup()){
        Serial.println("I2C setup failed");
        progData.state = 3;
        while(1){
          delay(1000);
    }
  }
  tmag5273.begin(espConfig.i2cDefs.TMAG_ADDRESS, Wire);
  tmag5273.setConvAvg(conversionAverage);
  tmag5273.setTemperatureEn(true);
  // tmag5273.setMagneticChannel(magneticChannel);
  // tmag5273.setReadMode(TMAG5273_I2C_MODE_1BYTE_16BIT);
  // tmag5273.setAngleEn(angleCalculation);
  
  mcp4725.begin();
  magSensor.startTask();
  sigGen.startTask();
  progState = 1;
}

void debugPrint(){
  Serial.printf("Timestamp since boot [ms]: %lu", millis());
  Serial.printf(" progName: %s", espConfig.progCfg.name);
  Serial.printf(" progState: %lu", progState);
  Serial.printf(" confRes: %lu", espConfig.progCfg.confRes);
  Serial.printf(" wifiRes: %lu", espConfig.wifiCfg.state);
  float reading;
  temp_sensor_read_celsius(&reading);
  Serial.printf(" temp: %f", reading);
  Serial.println();
  // Serial.println(twoWire.requestFrom(0x22, 0x01));
  // Serial.printf("Mag x: %.2f mT, y: %.2f mT, z: %.2f mT, Temp: %.2f °C\n", espConfig.magData.x, espConfig.magData.y, espConfig.magData.z, (espConfig.magData.t*1.8)+32);
  // Serial.println();
  // Serial.println(espConfig.progCfg.name);
  // Serial.println();
}

void loop(){
  
  // please note that the value of status should be checked and properly handler
  if (millis()-espConfig.progData.debugTimestamp > 1000){
    espConfig.progData.debugTimestamp = millis();
    debugPrint();
  }
  if (millis()-espConfig.wasData.udpTimestamp > espConfig.wasData.udpFreq){
    espConfig.wasData.udpTimestamp = millis();
    espUdp.sendSteerData();
  }
  delay(3);

}