#include <Arduino.h>
#include "ESPdata.h"
#include "ESPWifi.h"
#include "myLED.h"
#include "Wire.h"
#include "ESPudp.h"
#include "Adafruit_MCP23X17.h"
#include <Adafruit_ADS1X15.h>
#include "Indicators.h"
#include "MainPower.h"
#include "GPS.h"
#include "HardwareSerial.h"
#include "ESP32OTAPull.h"
#include "ESPsteer.h"
#include <ESPAsyncWebServer.h>
#include "littlefs.h"
#include "CANBUS.h"

// #include "ESPOTAUpdater.h"
#include "nvs_flash.h"
#include "esp_task_wdt.h"

//TODO: add wifi connect timer to ap mode

TwoWire twoWire = TwoWire(0);
TwoWire twoWire1 = TwoWire(1);
HardwareSerial bnoSerial(2);
HardwareSerial gpsSerial(1);

Adafruit_MCP23X17 mcp;
Adafruit_ADS1115 ads;

// Simple global objects - initialized after NVS is ready in setup()
// This avoids early singleton instantiation issues
ESPdata* espDataPtr = nullptr;
#define espData (*espDataPtr)  // Simple macro for accessing singleton

// Component objects - will be created in setup()
ESPGPS* gps = nullptr;
MyLED* myLED = nullptr;
MainPower* mainPower = nullptr;
ESPWifi* espWifi = nullptr;
ESPudp* espUdp = nullptr;
ESPsteer* espSteer = nullptr;
Indicators* indicators = nullptr;
CANBUS* canbus = nullptr;
// Objects that don't depend on ESPdata
ESP32OTAPull ota;
AsyncWebServer server(80);
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
    Serial.println("Unknown error at address 0x20");
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
    Serial.println("Unknown error at address 0x48");
    espData.program.adsState = 2;
  }
  if (espData.program.mcpState == 2 || espData.program.adsState == 2){
    return false;
  } else {
    return true;
  }
  
}


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
  espData.saveBootState(1); // Set to normal boot
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

void recoveryServer(){
  // Start Wifi AP and Webserver for diagnostics
  espData.wifi.state = espWifi->makeAP();
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

        // Start server
        server.begin();
      #pragma endregion
}

void normalBoot(){
  Serial.println("Starting normal boot with 30-second watchdog timer...");
  esp_task_wdt_init(30, true); // 30 seconds timeout, panic on timeout
  esp_task_wdt_add(NULL); // Add current task to watchdog

  // Initialize all components that depend on ESPdata
  gps = new ESPGPS(&espData, &gpsSerial, &bnoSerial);
  mainPower = new MainPower(&espData, &mcp, &ads);
  espUdp = new ESPudp(&espData);
  espSteer = new ESPsteer(&espData, &ads, &mcp);
  indicators = new Indicators(&espData, &mcp);
  canbus = new CANBUS(&espData);
  Serial.println("All components initialized successfully");
  
  
  espData.program.state = 1;
  Serial.println("Set program state to 1");
  
  espData.saveBootState(2);
  Serial.println("Saved boot state 2");
  
  espData.program.state = 0;
  Serial.println("Set program state to 0");
  
  myLED->startTask();
  Serial.println("Started LED task");
  
  espData.program.state = 2;
  Serial.println("Set program state to 2");
  
  // Feed watchdog after initial setup
  esp_task_wdt_reset();


  while (espData.wifi.state != 1){
    espData.wifi.state = espWifi->connect();
    // Feed watchdog during wifi connection attempts
    esp_task_wdt_reset();
    if (millis() > 120000){
      Serial.println("Wifi connection timed out");
      espData.wifi.state = espWifi->makeAP();
      break;
    }
  }
  // espConfig.wifiCfg.state = espWifi.makeAP();
  Serial.println("Wifi State: " + String(espData.wifi.state));
  
  // Feed watchdog after WiFi setup
  esp_task_wdt_reset();
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
        [&](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
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
  
  // Feed watchdog before I2C setup
  esp_task_wdt_reset();
  
  Serial.println("Starting I2C setup...");
  
  if(!I2Csetup()){
    Serial.println("I2C setup failed");
    espData.program.state = 3;
    // Remove from watchdog before entering infinite loop
    esp_task_wdt_delete(NULL);
    while(1){
      delay(1000);
    }
  }
  
  Serial.println("I2C setup successful");
  
  // Feed watchdog after I2C setup
  esp_task_wdt_reset();
  
  if (espData.program.mcpState == 1){
    mcp.begin_I2C(0x20, &twoWire);
    
    Serial.println("Initializing indicators...");
    
    // Initialize indicators after MCP is ready
    indicators->init();
    indicators->testSequence();  // Optional: run test sequence at startup
    indicators->startTask(4096, 1, 0);  // Start FreeRTOS task: 4KB stack, priority 1, core 0
    
    // mcp.pinMode(espConfig.gpioDefs.rtkFix, OUTPUT);
    // mcp.digitalWrite(espConfig.gpioDefs.rtkFix, HIGH);
    // delay(1000);
    // mcp.digitalWrite(espConfig.gpioDefs.rtkFix, LOW);
  }
  if (espData.program.adsState == 1){
    ads.begin(0x48, &twoWire);
  }
  
  Serial.println("Hardware initialization complete");
  
  // Feed watchdog after hardware initialization
  // esp_task_wdt_reset();  // COMMENTED OUT - WATCHDOG DISABLED
  
  Serial.println("Starting GPS...");
  
  // Start GPS
  gps->init(espUdp);
  
  Serial.println("Starting main power and steering...");
  
  // If everything is good, turn on power to autosteer
  mainPower->startTask();
  espSteer->begin(espUdp);

  Serial.println("Starting UDP...");
  
  // UDP setup
  espUdp->begin(gps);
  Serial.println("Network setup complete");
  
  // Feed watchdog before final operations
  esp_task_wdt_reset();
  
  // delay(5000);
  espData.program.state = 1;
  espData.saveBootState(1);
  
  // Boot completed successfully - remove from watchdog
  esp_task_wdt_delete(NULL);
  Serial.println("Normal boot completed successfully - watchdog disabled");
}

void recoveryBoot(){
  espData.program.state = 4;
  myLED->startTask();
  recoveryServer();
}


void setup(){
  // Start USB Serial Port - ESP32-S3 USB CDC mode
  Serial.begin(115200);
  
  // For ESP32-S3 with USB CDC, wait for Serial connection
  unsigned long startTime = millis();
  while (!Serial && (millis() - startTime) < 5000) {
    delay(10); // Wait up to 5 seconds for Serial connection
  }
  
  // Additional delay to ensure stability
  delay(1000);
  
  // Force flush and ensure connection
  Serial.println();
  Serial.println("=== ESP32 AIO Starting Up ===");
  
  // Now safe to get ESPdata instance
  espDataPtr = &ESPdata::getInstance();
  
  Serial.println("Initializing ESPdata Preferences...");
  espData.initPreferences();
  espData.loadConfig();


  myLED = new MyLED(&espData);
  espWifi = new ESPWifi(&espData);

  if (espData.getBootState() == 1){
    Serial.println("Previous boot was clean");
    normalBoot();
  } else {
    Serial.println("Previous boot was NOT clean");
    Serial.printf("Boot state: %d\n", espData.getBootState());
    recoveryBoot();
  }

  

  
  Serial.println("Checking bootState...");

  Serial.printf("Boot state: %d\n", espData.getBootState());

 
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
  // Indicators now run in their own FreeRTOS task
  // No need to call indicators.loop() here anymore
  
  debugPrint();
  
  delay(5000);
}