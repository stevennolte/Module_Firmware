#include <Arduino.h>
#include "ESPconfig.h"
#include "ESPWifi.h"
#include "myLED.h"
#include "Wire.h"
#include <ESPAsyncWebServer.h>
#include "ESP32OTAPull.h"
#include "ESPudp.h"
#include "ArduinoJson.h"
#include "driver/temp_sensor.h"


ESPconfig espConfig;
MyLED myLED(&espConfig);
ESPWifi espWifi(&espConfig);
ESPudp espUdp(&espConfig);
std::vector<String> debugVars;
AsyncWebServer server(80);

auto& progData = espConfig.progData;
auto& progCfg = espConfig.progCfg;
auto& progState = espConfig.progData.state;
auto& wifiCfg = espConfig.wifiCfg;


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

  debugVars.push_back("Fold Outer Wings: " + String(espConfig.foldData.foldOuterWingsState));
  debugVars.push_back("Fold Center Wings: " + String(espConfig.foldData.foldCenterWingsState));
  debugVars.push_back("Raise Wings: " + String(espConfig.foldData.raiseWingsState));
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

void handleMomentaryCommand(AsyncWebServerRequest *request) {
  if (request->hasParam("button") && request->hasParam("action")) {
    String button = request->getParam("button")->value();
    String action = request->getParam("action")->value();

    Serial.printf("Momentary Command: Button=%s, Action=%s\n", button.c_str(), action.c_str());

    // Add your logic here to handle the momentary button actions
    // LEFT FLIP 
    if (button == "leftFlipOut") {
      if (action == "start") {
        espConfig.foldData.foldStates[espConfig.foldData.leftFlip] = 1;
        
      } else if (action == "stop") {
        espConfig.foldData.foldStates[espConfig.foldData.leftFlip] = 0;
      } else {
        Serial.println("Unknown action received for LeftFlipOut.");
      }
    } else if (button == "leftFlipIn") {
      if (action == "start") {
        espConfig.foldData.foldStates[espConfig.foldData.leftFlip] = 2;
      } else if (action == "stop") {
        espConfig.foldData.foldStates[espConfig.foldData.leftFlip] = 0;
      } else {
        Serial.println("Unknown action received for LeftFlipIn.");
      }
    } else if (button == "leftLiftUp") {
      if (action == "start") {
        espConfig.foldData.foldStates[espConfig.foldData.leftLift] = 1;
      } else if (action == "stop") {
        espConfig.foldData.foldStates[espConfig.foldData.leftLift] = 0;
      } else {
        Serial.println("Unknown action received for LeftLiftOut.");
      }
    } else if (button == "leftLiftDown") {
      if (action == "start") {
        espConfig.foldData.foldStates[espConfig.foldData.leftLift] = 2;
      } else if (action == "stop") {
        espConfig.foldData.foldStates[espConfig.foldData.leftLift] = 0;
      } else {
        Serial.println("Unknown action received for LeftLiftIn.");
      }
    } else if (button == "leftFoldOut") {
      if (action == "start") {
        espConfig.foldData.foldStates[espConfig.foldData.leftFold] = 1;
      } else if (action == "stop") {
        espConfig.foldData.foldStates[espConfig.foldData.leftFold] = 0;
      } else {
        Serial.println("Unknown action received for LeftFoldOut.");
      }
    } else if (button == "leftFoldIn") {
      if (action == "start") {
        espConfig.foldData.foldStates[espConfig.foldData.leftFold] = 2;
      } else if (action == "stop") {
        espConfig.foldData.foldStates[espConfig.foldData.leftFold] = 0;
      } else {
        Serial.println("Unknown action received for LeftFoldIn.");
      }
    } else if (button == "centerUp") {
      if (action == "start") {
        espConfig.foldData.foldStates[espConfig.foldData.center] = 1;
      } else if (action == "stop") {
        espConfig.foldData.foldStates[espConfig.foldData.center] = 0;
      } else {
        Serial.println("Unknown action received for CenterOut.");
      }
    } else if (button == "centerDown") {
      if (action == "start") {
        espConfig.foldData.foldStates[espConfig.foldData.center] = 2;
      } else if (action == "stop") {
        espConfig.foldData.foldStates[espConfig.foldData.center] = 0;
      } else {
        Serial.println("Unknown action received for CenterIn.");
      }
    } else if (button == "rightFoldOut") {
      if (action == "start") {
        espConfig.foldData.foldStates[espConfig.foldData.rightFold] = 1;
      } else if (action == "stop") {
        espConfig.foldData.foldStates[espConfig.foldData.rightFold] = 0;
      } else {
        Serial.println("Unknown action received for RightFoldOut.");
      }
    } else if (button == "rightFoldIn") {
      if (action == "start") {
        espConfig.foldData.foldStates[espConfig.foldData.rightFold] = 2;
      } else if (action == "stop") {
        espConfig.foldData.foldStates[espConfig.foldData.rightFold] = 0;
      } else {
        Serial.println("Unknown action received for RightFoldIn.");
      }
    } else if (button == "rightLiftUp") {
      if (action == "start") {
        espConfig.foldData.foldStates[espConfig.foldData.rightLift] = 1;
      } else if (action == "stop") {
        espConfig.foldData.foldStates[espConfig.foldData.rightLift] = 0;
      } else {
        Serial.println("Unknown action received for RightLiftOut.");
      }
    } else if (button == "rightLiftDown") {
      if (action == "start") {
        espConfig.foldData.foldStates[espConfig.foldData.rightLift] = 2;
      } else if (action == "stop") {
        espConfig.foldData.foldStates[espConfig.foldData.rightLift] = 0;
      } else {
        Serial.println("Unknown action received for RightLiftIn.");
      }
    } else if (button == "rightFlipOut") {
      if (action == "start") {
        espConfig.foldData.foldStates[espConfig.foldData.rightFlip] = 1;
      } else if (action == "stop") {
        espConfig.foldData.foldStates[espConfig.foldData.rightFlip] = 0;
      } else {
        Serial.println("Unknown action received for RightFlipOut.");
      }
    } else if (button == "rightFlipIn") {
      if (action == "start") {
        espConfig.foldData.foldStates[espConfig.foldData.rightFlip] = 2;
      } else if (action == "stop") {
        espConfig.foldData.foldStates[espConfig.foldData.rightFlip] = 0;
      } else {
        Serial.println("Unknown action received for RightFlipIn.");
      }
    } else {


      Serial.println("Unknown button received.");
    }

    request->send(200, "text/plain", "Momentary command processed");
  } else {
    request->send(400, "text/plain", "Invalid parameters");
  }
}

// Function to handle toggle switch commands
void handleToggleCommand(const String& command, const String& action) {
  if (command == "foldOuterWings") {
      espConfig.foldData.foldOuterWingsState = (action == "on");
      Serial.printf("Fold Outer Wings: %s\n", espConfig.foldData.foldOuterWingsState ? "ON" : "OFF");
      // Add your logic here to control hardware for "Fold Outer Wings"
  } else if (command == "foldCenterWings") {
      espConfig.foldData.foldCenterWingsState = (action == "on");
      Serial.printf("Fold Center Wings: %s\n", espConfig.foldData.foldCenterWingsState ? "ON" : "OFF");
      // Add your logic here to control hardware for "Fold Center Wings"
  } else if (command == "raiseWings") {
    espConfig.foldData.raiseWingsState = (action == "on");
      Serial.printf("Raise Wings: %s\n", espConfig.foldData.raiseWingsState ? "ON" : "OFF");
      // Add your logic here to control hardware for "Raise Wings"
  } else if (command == "joystick") {
      espConfig.foldData.joyStickActive = (action == "on");
      Serial.printf("Joystick: %s\n", espConfig.foldData.joyStickActive ? "ON" : "OFF");
      // Add your logic here to control hardware for "Joystick"
    } else {
      Serial.println("Unknown command received.");
  }
}



#pragma endregion Webserver

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
  // espConfig.wifiCfg.state = espWifi.makeAP();
  wifiCfg.state = espWifi.connect();
  Serial.println("Wifi State: " + String(espConfig.wifiCfg.state));
  #pragma region Server Setup
        // Serve the main HTML page
        #pragma region Page Handlers
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
          Serial.println("getting Home file");
          request->send(LittleFS, "/boom.html");
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
        server.on("/joystick/on", HTTP_POST, [](AsyncWebServerRequest *request) {
            handleToggleCommand("joystick", "on");
            request->send(200, "text/plain", "Joystick ON");
        });
        server.on("/joystick/off", HTTP_POST, [](AsyncWebServerRequest *request) {
            handleToggleCommand("joystick", "off");
            request->send(200, "text/plain", "Joystick OFF");
        });
        server.on("/momentary", HTTP_POST, handleMomentaryCommand);
        
        
        #pragma endregion
        
        
        // Start server
        server.begin();
      #pragma endregion
  temp_sensor_start();
  espUdp.begin();
  
  
  
  progState = 1;
}

void debugPrint(){
  Serial.printf("Timestamp since boot [ms]: %lu", millis());
  Serial.println();
  Serial.printf("\tprogName: %s", espConfig.progCfg.name);
  Serial.printf(" progState: %lu", progState);
  Serial.printf(" confRes: %lu", espConfig.progData.confRes);
  Serial.printf(" wifiRes: %lu", espConfig.wifiCfg.state);
  float reading;
  temp_sensor_read_celsius(&reading);
  Serial.printf(" temp: %f", reading);
  Serial.println();
  Serial.printf("\tFree Heap: %lu", ESP.getFreeHeap());
  Serial.printf(" SSID: %s", WiFi.SSID().c_str());
  Serial.printf(" IP: %d.%d.%d.%d", wifiCfg.ips[0], wifiCfg.ips[1], wifiCfg.ips[2], wifiCfg.ips[3]);
  // Serial.println(twoWire.requestFrom(0x22, 0x01));
  // Serial.printf("Mag x: %.2f mT, y: %.2f mT, z: %.2f mT, Temp: %.2f °C\n", espConfig.magData.x, espConfig.magData.y, espConfig.magData.z, (espConfig.magData.t*1.8)+32);
  // Serial.println();
  // Serial.println(espConfig.progCfg.name);
  Serial.println();
}

void loop(){
  
  // please note that the value of status should be checked and properly handler
  
  delay(1000);
  debugPrint();
}