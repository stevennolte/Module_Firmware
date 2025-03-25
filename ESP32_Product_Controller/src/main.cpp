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
      foldOuterWingsState = (action == "on");
      Serial.printf("Fold Outer Wings: %s\n", foldOuterWingsState ? "ON" : "OFF");
      // Add your logic here to control hardware for "Fold Outer Wings"
  } else if (command == "foldCenterWings") {
      foldCenterWingsState = (action == "on");
      Serial.printf("Fold Center Wings: %s\n", foldCenterWingsState ? "ON" : "OFF");
      // Add your logic here to control hardware for "Fold Center Wings"
  } else if (command == "raiseWings") {
      raiseWingsState = (action == "on");
      Serial.printf("Raise Wings: %s\n", raiseWingsState ? "ON" : "OFF");
      // Add your logic here to control hardware for "Raise Wings"
  } else {
      Serial.println("Unknown command received.");
  }
}

// Function to handle setting the application rate
void handleSetApplicationRate(AsyncWebServerRequest *request) {
  Serial.println("Received set application rate request");
  if (request->hasParam("rate")){
    String rateStr = request->getParam("rate")->value();
    espConfig.rateData.targetRate = rateStr.toFloat();
    Serial.println(espConfig.updateRate());
    Serial.printf("Received rate: %f\n", espConfig.rateData.targetRate);
    if (espConfig.rateData.targetRate > 0) {
      // Add your logic here to set the application rate
      Serial.printf("Application rate set to: %f\n", espConfig.rateData.targetRate);
      request->send(200, "text/plain", "Application rate set successfully");
    } else {
      request->send(400, "text/plain", "Invalid rate value");
    }
    
  }
   
}
void handleGetApplicationRate(AsyncWebServerRequest *request) {
  // Create a JSON response with the current application rate
  StaticJsonDocument<100> doc;
  doc["rate"] = espConfig.rateData.targetRate; // Replace with your variable holding the application rate

  String jsonResponse;
  serializeJson(doc, jsonResponse);

  request->send(200, "application/json", jsonResponse);
}

void handleGetModuleState(AsyncWebServerRequest *request) {
    // Example variables for module state, target position, and current position
    int moduleState = 2; // Replace with your actual variable (1 = good, 2 = disconnected) TODO: Change to actual module state
    float targetPosition = 50.0; // Replace with your actual target position TODO: Change to actual target position
    float currentPosition = 48.5; // Replace with your actual current position TODO: Change to actual current position

    // Map the module state to a string
    String stateString = (moduleState == 1) ? "good" : "disconnected";

    // Create a JSON response
    StaticJsonDocument<200> doc;
    doc["state"] = stateString;
    doc["targetPosition"] = targetPosition;
    doc["currentPosition"] = currentPosition;
    doc["pressure"] = 0.0; // Replace with your actual pressure value TODO: Change to actual pressure value
    doc["flowRate"] = 0.0; // Replace with your actual flow rate value TODO: Change to actual flow rate value
    doc["level"] = 0.0; // Replace with your actual level value TODO: Change to actual level value

    // Serialize the JSON response
    String jsonResponse;
    serializeJson(doc, jsonResponse);

    // Send the JSON response
    request->send(200, "application/json", jsonResponse);
}

void handleGetPerformanceVariables(AsyncWebServerRequest *request) {
    Serial.println("Handling /performance/variables request...");

    // Example variables for performance data
    float pressureCurrent = 75.0; // Replace with your actual pressure current value
    float pressureTarget = 100.0; // Replace with your actual pressure target value
    float flowrateCurrent = 50.0; // Replace with your actual flowrate current value
    float flowrateTarget = 60.0; // Replace with your actual flowrate target value
    float levelCurrent = 30.0; // Replace with your actual level current value
    float levelTarget = 40.0; // Replace with your actual level target value

    // Create a JSON response
    StaticJsonDocument<256> doc; // Adjust size based on the number of variables
    doc["Pressure"]["current"] = pressureCurrent;
    doc["Pressure"]["target"] = pressureTarget;
    doc["Flowrate"]["current"] = flowrateCurrent;
    doc["Flowrate"]["target"] = flowrateTarget;
    doc["Level"]["current"] = levelCurrent;
    doc["Level"]["target"] = levelTarget;

    // Serialize the JSON response
    String jsonResponse;
    if (serializeJson(doc, jsonResponse) == 0) {
        Serial.println("Failed to serialize JSON");
        request->send(500, "application/json", "{\"error\":\"Failed to serialize JSON\"}");
        return;
    }

    Serial.println("JSON response:");
    Serial.println(jsonResponse);

    // Send the JSON response
    request->send(200, "application/json", jsonResponse);
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
  // espConfig.wifiCfg.state = espWifi.makeAP();
  wifiCfg.state = espWifi.connect();
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


