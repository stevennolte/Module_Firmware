#include <Arduino.h>
#include <Arduino.h>
#include "ESPconfig.h"
#include "ESPWifi.h"
#include "myLED.h"
#include "Wire.h"
#include <ESPAsyncWebServer.h>
#include "ESP32OTAPull.h"
#include "ESPudp.h"
#include "ArduinoJson.h"
#include "Product_Ctrl.h"
#include "driver/temp_sensor.h"
#include "CANBUS.h"


// TwoWire twoWire = TwoWire(0);
// TwoWire twoWire1 = TwoWire(0);

// Variables to store the state of the switches
bool foldOuterWingsState = true;
bool foldCenterWingsState = false;
bool raiseWingsState = false;



ESPconfig espConfig;

CANBUS canbus(&espConfig);
MyLED myLED(&espConfig);
ESPWifi espWifi(&espConfig);
ESPudp espUdp(&espConfig);
Product_Ctrl productCtrl(&espConfig, &canbus);
std::vector<String> debugVars;
AsyncWebServer server(80);

auto& progData = espConfig.progData;
auto& progCfg = espConfig.progCfg;
auto& progState = espConfig.progData.state;
auto& wifiCfg = espConfig.wifiCfg;



void IRAM_ATTR ISR()
{
  espConfig.rateData.pulseCount++;
}

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
  debugVars.push_back("TargetRate: " + String(espConfig.rateData.targetRate));
  debugVars.push_back("Flow Rate: " + String(espConfig.flowCfg.flowRate));
  debugVars.push_back("Fold Outer Wings: " + String(foldOuterWingsState ? "ON" : "OFF"));
  debugVars.push_back("Sec 1: " + String(espConfig.rateData.sectionStates[0]));
  debugVars.push_back("Sec 2: " + String(espConfig.rateData.sectionStates[1]));
  debugVars.push_back("Sec 3: " + String(espConfig.rateData.sectionStates[2]));
  debugVars.push_back("Sec 4: " + String(espConfig.rateData.sectionStates[3]));
  debugVars.push_back("Sec 5: " + String(espConfig.rateData.sectionStates[4]));
  debugVars.push_back("Flow Freq: " + String(espConfig.rateData.frequency));
  debugVars.push_back("Target Pressure: " + String(espConfig.rateData.targetPressure));
  debugVars.push_back("Target Flow Rate: " + String(espConfig.rateData.targetFlowRate));
  debugVars.push_back("Actual Flow Rate: " + String(espConfig.rateData.actualFlowRate));
  debugVars.push_back("Target Row Flow Rate: " + String(espConfig.rateData.targetRowFlowRate));
  debugVars.push_back("Speed: " + String(espConfig.rateData.speed));
  debugVars.push_back("Last Section Msg: " + String(espConfig.rateData.lastSectionMsg));
  debugVars.push_back("Fold State 1: " + String(espConfig.foldData.foldStates[0]));
  debugVars.push_back("Fold State 2: " + String(espConfig.foldData.foldStates[1]));
  debugVars.push_back("Fold State 3: " + String(espConfig.foldData.foldStates[2]));
  debugVars.push_back("Fold State 4: " + String(espConfig.foldData.foldStates[3]));
  debugVars.push_back("Fold State 5: " + String(espConfig.foldData.foldStates[4]));
  debugVars.push_back("Fold State 6: " + String(espConfig.foldData.foldStates[5]));
  debugVars.push_back("Fold State 7: " + String(espConfig.foldData.foldStates[6]));
  debugVars.push_back("Fold State 8: " + String(espConfig.foldData.foldStates[7]));
  

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


#pragma endregion

void setup() {
  progData.state = 0;
  myLED.startTask();
  progData.state = 2;

  // Start USB Serial Port
  Serial.begin(115200);
  delay(5000);   // Wait for the usb to connect so you can see the outputs at startup
  Serial.println("Starting up...");
  espConfig.progData.confRes = espConfig.loadConfig();
  // Start Wifi AP and Webserver for diagnostics
  
  while (wifiCfg.state != 1){
    wifiCfg.state = espWifi.connect();
    if (millis()>60000){
      Serial.println("Failed to connect to wifi, starting AP");
      wifiCfg.state = espWifi.makeAP();
      break;
    }
  }
  Serial.println("Wifi State: " + String(espConfig.wifiCfg.state));
  #pragma region Server Setup
        // Serve the main HTML page
        #pragma region Page Handlers
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
          Serial.println("getting Home file");
          request->send(LittleFS, "/product.html");
        });
        server.on("/product.html", HTTP_GET, [](AsyncWebServerRequest *request){
          Serial.println("getting Home file");
          request->send(LittleFS, "/product.html");
        });
        server.on("/settings.html", HTTP_GET, [](AsyncWebServerRequest *request){
          Serial.println("getting settings file");
          request->send(LittleFS, "/settings.html");
        });
        server.on("/debug.html", HTTP_GET, [](AsyncWebServerRequest *request){
          Serial.println("getting index file");
          request->send(LittleFS, "/debug.html");
        });
        server.on("/boom.html", HTTP_GET, [](AsyncWebServerRequest *request){
          Serial.println("getting boom file");
          request->send(LittleFS, "/boom.html");
        });
        #pragma endregion

        #pragma region Request Handlers
        server.on("/getDebugVars", HTTP_GET, handleDebugVars);
        server.on("/getFiles", HTTP_GET, handleFileList);
        server.on("/download", HTTP_GET, handleFileDownload);
        server.on("/reboot", HTTP_GET, handleReboot);
        
        #pragma endregion

        #pragma region Post Handlers
        server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {}, handleFileUpload);
        server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {}, handleFirmwareUpload);

        
        server.on("/foldOuterWings/on", HTTP_POST, [](AsyncWebServerRequest *request) {
          handleToggleCommand("foldOuterWings", "on");
          request->send(200, "text/plain", "Fold Outer Wings ON");
        });

        server.on("/foldOuterWings/off", HTTP_POST, [](AsyncWebServerRequest *request) {
            handleToggleCommand("foldOuterWings", "off");
            request->send(200, "text/plain", "Fold Outer Wings OFF");
        });

        server.on("/foldCenterWings/on", HTTP_POST, [](AsyncWebServerRequest *request) {
            handleToggleCommand("foldCenterWings", "on");
            request->send(200, "text/plain", "Fold Center Wings ON");
        });

        server.on("/foldCenterWings/off", HTTP_POST, [](AsyncWebServerRequest *request) {
            handleToggleCommand("foldCenterWings", "off");
            request->send(200, "text/plain", "Fold Center Wings OFF");
        });

        server.on("/raiseWings/on", HTTP_POST, [](AsyncWebServerRequest *request) {
            handleToggleCommand("raiseWings", "on");
            request->send(200, "text/plain", "Raise Wings ON");
        });

        server.on("/raiseWings/off", HTTP_POST, [](AsyncWebServerRequest *request) {
            handleToggleCommand("raiseWings", "off");
            request->send(200, "text/plain", "Raise Wings OFF");
        });
        server.on("/momentary", HTTP_POST, handleMomentaryCommand);
        server.on("/setApplicationRate", HTTP_POST, handleSetApplicationRate);
        server.on("/getApplicationRate", HTTP_GET, handleGetApplicationRate);
        server.on("/module", HTTP_GET, handleGetModuleState);
        server.on("/performance", HTTP_GET, handleGetPerformanceVariables);
        
        #pragma endregion
        
        
        // Start server
        server.begin();
      #pragma endregion
  temp_sensor_start();
  espUdp.begin();
  canbus.begin();
  productCtrl.begin();
  progState = 1;
}

void debugPrint(){
  Serial.printf("Timestamp since boot [ms]: %lu", millis());
  Serial.printf(" progName: %s", espConfig.progCfg.name);
  Serial.printf(" progState: %lu", progState);
  Serial.printf(" confRes: %lu", espConfig.progData.confRes);
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
  
  delay(1000);
  debugPrint();
}


