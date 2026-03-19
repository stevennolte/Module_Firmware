// #include "WiFi.h"
// #include <Common.h>
#include "AsyncUDP.h"
#include "ESPWifi.h"
#include <Preferences.h>
#include <ESPAsyncWebServer.h>
#include "ESPconfig.h"
#include "driver/temp_sensor.h"
#include "Version.h"
#include "ESP32OTAPull.h"
#include "Joystick.h"

ESPconfig espConfig;
// MyWifi myWifi;
ESPWifi espWifi(&espConfig);
AsyncEventSource events("/events");
AsyncWebServer server(80);
std::vector<String> debugVars;
Joystick joystick(&espConfig);
// Preferences preferences;

auto& progData = espConfig.progData;
auto& progCfg = espConfig.progCfg;
auto& progState = espConfig.progData.state;
auto& wifiCfg = espConfig.wifiCfg;
auto& gpioDefs = espConfig.gpioDefs;
auto& joyCmds = espConfig.joyCmds;

uint32_t lastMessageSent = 0;

class UDPMethods{
  private:
    int commandTimer = 0;
  public:
    AsyncUDP udp;
    UDPMethods(){
    }
    
    void begin(){
      udp.listen(8888);
      Serial.println("UDP Listening");
      udp.onPacket([](AsyncUDPPacket packet) {
        if (packet.data()[0]==0x80 & packet.data()[1]==0x81){
          // Serial.println(packet.remoteIP()[3]);
          switch(packet.data()[3]){
            case 157:
                ESP.restart();
                break;
            case 201:
              // preferences.putUInt("ip_one", packet.data()[7]);
              // preferences.putUInt("ip_two", packet.data()[8]);
              // preferences.putUInt("ip_three", packet.data()[9]);
              ESP.restart();
          }
        }
      });
    }


    void sendCommands(){
      if (millis()-commandTimer > 30){
        commandTimer = millis();
        // Serial.println("Sending Commands");
        uint8_t data[14];
        data[0] = 0x80;
        data[1] = 0x81;
        data[2] = 61;
        data[3] = 162;
        data[4] = 10;
        data[5] = joyCmds.leftLift;
        data[6] = joyCmds.leftLower;
        data[7] = joyCmds.centerLift;
        data[8] = joyCmds.centerLower;
        data[9] = joyCmds.rightLift;
        data[10] = joyCmds.rightLower;
        data[11] = joyCmds.sectionControl;
        data[12] = joyCmds.autoSteer;
        // joyCmds.aogByte1 = 0x80;
        // joyCmds.aogByte2 = 0x81;
        // joyCmds.sourceAddress = 61;
        // joyCmds.PGN = 162;
        // joyCmds.length = 10;
        
        // joyCmds.switch1 = !digitalRead(gpioDefs.inputPins[joyCmds.leftLift]);
        // joyCmds.switch2 = !digitalRead(gpioDefs.inputPins[joyCmds.leftLower]);
        // joyCmds.switch3 = !digitalRead(gpioDefs.inputPins[joyCmds.centerLift]);
        // joyCmds.switch4 = !digitalRead(gpioDefs.inputPins[joyCmds.centerLower]);
        // joyCmds.switch5 = !digitalRead(gpioDefs.inputPins[joyCmds.rightLift]);
        // joyCmds.switch6 = !digitalRead(gpioDefs.inputPins[joyCmds.rightLower]);
        // joyCmds.switch7 = !digitalRead(gpioDefs.inputPins[joyCmds.sectionControl]);
        // joyCmds.switch8 = !digitalRead(gpioDefs.inputPins[joyCmds.autoSteer]);

        // joyCmds->sourceAddress = 61;
        // joyCmds->PGN = 162;
        // joyCmds->length = 10;
        // joyCmds->switch1 = !digitalRead(inputPins[0]);
        // joyCmds->switch2 = !digitalRead(inputPins[1]);
        // joyCmds->switch3 = !digitalRead(inputPins[2]);
        // joyCmds->switch4 = !digitalRead(inputPins[3]);
        // joyCmds->switch5 = !digitalRead(inputPins[4]);
        // joyCmds->switch6 = !digitalRead(inputPins[5]);
        // joyCmds->switch7 = !digitalRead(inputPins[6]);
        // joyCmds->switch8 = !digitalRead(inputPins[7]);
        // for (int i = 0; i<8;i++){
        //   Serial.print(cmds[i]);
        //   Serial.print("\t");
        // }
        // Serial.println();
        // for (int i = 0; i<8;i++){
        //   Serial.print(!digitalRead(inputPins[i]));
        //   Serial.print("\t\t");
        // }
        // Serial.println();
        udp.writeTo(data,sizeof(data),IPAddress(wifiCfg.ips[0],wifiCfg.ips[1],wifiCfg.ips[2],255),8887);
        }
      }    
    
    
};

UDPMethods udpMethods = UDPMethods();


#pragma region Webserver

void updateDebugVars() {
  debugVars.clear(); // Clear the list to update it dynamically
  debugVars.push_back("Timestamp since boot [ms]: " + String(millis()));
  debugVars.push_back("Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
  float tempReading;
  temp_sensor_read_celsius(&tempReading);
  debugVars.push_back("Temp: " + String(tempReading));
  debugVars.push_back("Version: " + String(VERSION));
  debugVars.push_back("Wifi IP: " + String(WiFi.SSID()));
  debugVars.push_back("Left Lift: " + String(espConfig.joyCmds.leftLift));
  debugVars.push_back("Left Lower: " + String(espConfig.joyCmds.leftLower));
  debugVars.push_back("Right Lift: " + String(espConfig.joyCmds.rightLift));
  debugVars.push_back("Right Lower: " + String(espConfig.joyCmds.rightLower));
  debugVars.push_back("Center Lift: " + String(espConfig.joyCmds.centerLift));
  debugVars.push_back("Center Lower: " + String(espConfig.joyCmds.centerLower));
  debugVars.push_back("Auto Steer: " + String(espConfig.joyCmds.autoSteer));
  debugVars.push_back("Section Control: " + String(espConfig.joyCmds.sectionControl));
  debugVars.push_back("Changed Commands: " + String(espConfig.joyCmds.changeInCmd));
  std::string ipValue = "Sensor: " + std::to_string(WiFi.localIP());
  // bleRemote.sendIPData(ipValue);d
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
static bool fwUpdateSkip = false;

void handleFirmwareUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!index) {
    fwUpdateSkip = false;
    Serial.printf("Update Start: %s\n", filename.c_str());
    if (!filename.startsWith(NAME)) {
      Serial.printf("Firmware rejected: filename '%s' does not match module '%s'\n", filename.c_str(), NAME);
      fwUpdateSkip = true;
    } else if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { // Start with max available size
      Update.printError(Serial);
    }
  }

  if (fwUpdateSkip) {
    if (final) {
      request->send(400, "text/plain", "Wrong firmware file! Expected a file starting with: " + String(NAME));
    }
    return;
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

void handleFilesystemUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (!index) {
    Serial.printf("Filesystem Update Start: %s\n", filename.c_str());
    if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS)) {
      Update.printError(Serial);
      request->send(500, "text/plain", "Filesystem update failed to start.");
      return;
    }
  }
  if (Update.write(data, len) != len) {
    Update.printError(Serial);
  }
  if (final) {
    if (Update.end(true)) {
      Serial.printf("Filesystem Update Success: %u bytes\n", index + len);
      request->send(200, "text/html", "Filesystem update complete! Rebooting...");
      delay(1000);
      ESP.restart();
    } else {
      Update.printError(Serial);
      request->send(500, "text/html", "Filesystem update failed.");
    }
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
   Serial.begin(115200);
  //  preferences.begin("my-app", false);
   pinMode(LED_BUILTIN, OUTPUT);
   delay(2000);
   Serial.println("Starting");
  
   espConfig.loadConfig();
   Serial.println("Connecting to Wifi");
   while (wifiCfg.state != 1){
    wifiCfg.state = espWifi.connect();
    if (millis()>60000){
      Serial.println("Failed to connect to wifi, starting AP");
      wifiCfg.state = espWifi.makeAP();
      break;
    }
  }
    udpMethods.begin();
   joystick.initialize();
  //  ++bootCount;
//   if (!LittleFS.begin()) {
//     Serial.println("Failed to mount LittleFS");
//     return;
// }
// Serial.println(LittleFS.exists("/config.json") ? "File exists" : "File does not exist");
  
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    if (LittleFS.exists("/index.html")) {
        request->send(LittleFS, "/index.html");
    } else {
        request->send(404, "text/plain", "File not found");
    }
    });
    server.on("/getDebugVars", HTTP_GET, handleDebugVars);
        // Route to list files as JSON
        server.on("/listFiles", HTTP_GET, handleFileList);
        // Route to download files
        server.on("/download", HTTP_GET, handleFileDownload);

        // Handle file upload
        server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {}, handleFileUpload);
        server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {}, 
        handleFirmwareUpload);
        server.on("/updatefs", HTTP_POST, [](AsyncWebServerRequest *request) {},
        handleFilesystemUpload);
        server.on("/reboot", HTTP_GET, handleReboot);
    server.begin();
   
   digitalWrite(LED_BUILTIN, HIGH);
  //  esp_sleep_enable_ext0_wakeup(GPIO_NUM_4,0);
  //  Serial.println("Sleep");
  //  esp_deep_sleep_start();
}



void loop() {
  // udpMethods.sendCommands();
  if (espConfig.joyCmds.changeInCmd){
    espConfig.joyCmds.changeInCmd = false;
    udpMethods.sendCommands();
  }
  if (millis() - lastMessageSent > 500) {
    lastMessageSent = millis();
    // udpMethods.sendCommands();
  }
  // udpMethods.sendCommands();
  if (digitalRead(LED_BUILTIN)){
    digitalWrite(LED_BUILTIN, LOW);
  } else {
    digitalWrite(LED_BUILTIN, HIGH);
  }
  // Serial.println(joyCmds.leftLift);
  delay(10);
}

