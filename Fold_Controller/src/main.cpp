#include <Arduino.h>
#include <myWifi.h>

#include "ESP32OTAPull.h"
#include "Version.h"
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "AsyncUDP.h"
#include "messageStructs.h"
#include "myLED.h"
#include "configLoad.h"
#include "vector"
#include "BLEcomms.h"

Heartbeat_u heartbeat;
Heartbeat_t * hbData = &heartbeat.heartbeat_t;

std::vector<String> debugVars;
ProgramData_t progData;
MyLED myLED(progData, 48, 25);
ConfigLoad config(progData);
MyWifi myWifi;
AsyncWebServer server(80);
ESP32OTAPull ota;


class UDPMethods{
  private:
    
    uint32_t udpTimeout = 1000;
    
    int heartbeatTimePrevious=0;
    int heartbeatTimeTrip=250;
    int flowDataTimePrevious=0;
    int flowDataTimeTrip=1000;
  public:
    AsyncUDP udp;
    
    UDPMethods(){
    }
    
    void begin(){
      udp.listen(8888);
      Serial.println("UDP Listening");
      #pragma region Get_Version
      char version[64];
      strcpy(version, VERSION);
      char *token = strtok(version, ".");
      int i = 0;
      while (token != NULL) {
        int intValue = atoi(token);
        switch (i){
          case 0:
            hbData->version1 = intValue;
            break;
          case 1:
            hbData->version2 = intValue;
            break;
          case 2:
            hbData->version3 = intValue;
            break;
        }
        i++;
        token = strtok(NULL, ".");
      }
      #pragma endregion
      #pragma region Set Heartbeat Static Info
      hbData->aogByte1 = 0x80;
      hbData->aogByte2 = 0x81;
      hbData->sourceAddress = 51;
      hbData->PGN = MY_PGN;
      hbData->length = 10;
      hbData->myPGN = HEARBEAT_PGN;
      #pragma endregion
      udp.onPacket([](AsyncUDPPacket packet) {
        if (packet.data()[0]==0x80 & packet.data()[1]==0x81){
          progData.udpTimer = millis();
          switch (packet.data()[3]){
            case 201:
              
              progData.ips[0] = packet.data()[7];
              progData.ips[1] = packet.data()[8];
              progData.ips[2] = packet.data()[9];
              config.updateConfig();
              ESP.restart();
              break;
          
            case 149:
              switch (packet.data()[5])
                {
                case 5:
                  progData.shutdown = true;
                  break;
                case 6:
                  Serial.print("Update Cmd from Server: ");
                  Serial.println(packet.remoteIP()[3]);
                  Serial.println(packet.data()[6]);
                  Serial.println((packet.data()[8] << 8) + packet.data()[7]);
                  progData.serverAddress=packet.remoteIP()[3];
                  progData.serverPort = (packet.data()[8] << 8) + packet.data()[7];
                  
                  config.updateConfig();
                  ESP.restart();
                  break;
                
                default:
                  break;
                }
                break;
              }
              
        
        }
      });
    }
    
    void udpCheck(){  // checks to see with the last message was recieved
      if (millis() - progData.udpTimer < udpTimeout){
        progData.AOGconnected == 1;
      } else
      {
        progData.AOGconnected == 2;
      }
      
    }
    void sendHeartbeat(){
  
      if (millis()-heartbeatTimePrevious > heartbeatTimeTrip){
        heartbeatTimePrevious = millis();
        
        hbData->isConnectedAOG = progData.AOGconnected;
        hbData->isConnectedCAN1 = progData.can1connected;
        hbData->isConnectedCAN2 = progData.can2connected;
        hbData->isConnectedINA219 = progData.ina219Connected;
        
        
        // int cksum=0;
        // for (int i=2;i<=sizeof(Heartbeat_t)-1;i++)
        // {
        //   cksum += heartbeat.bytes[i];
        //   // Serial.print(heartbeat.bytes[i]);
        //   // Serial.print(" ");
        // }
        udp.writeTo(heartbeat.bytes,sizeof(Heartbeat_t),IPAddress(progData.ips[0],progData.ips[1],progData.ips[2],255),9999);
        
      }
    }
};
UDPMethods udpMethods = UDPMethods();

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
// #if defined(LED_BUILTIN) // flicker LED on update
// 	static int status = LOW;
// 	status = status == LOW && offset < totallength ? HIGH : LOW;
// 	digitalWrite(LED_BUILTIN, status);
// #endif
}


void softwareUpdate(){
  char basePath[] = "/%s/Releases/OTA_Config.json";
  char CONFIG_URL[150];
  sprintf(CONFIG_URL, basePath, progData.sketchConfig);
  Serial.println(CONFIG_URL);
  

  char SERVER[150];
  sprintf(SERVER, "http://%d.%d.%d.%d:%d",progData.ips[0],progData.ips[1],progData.ips[2],progData.serverAddress,progData.serverPort);

  Serial.print("CONFIG_URL: ");
  Serial.println(CONFIG_URL);
  Serial.print("SERVER: ");
  Serial.println(SERVER);
  
  ota.SetConfig(progData.sketchConfig);
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

void updateDebugVars() {
  debugVars.clear(); // Clear the list to update it dynamically
  debugVars.push_back("Program: " + String(progData.sketchConfig));
  debugVars.push_back("Version: " + String(VERSION));
  debugVars.push_back("Timestamp since boot [ms]: " + String(millis()));
  debugVars.push_back("Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
  debugVars.push_back("Wifi IP: " + String("FERT"));
  debugVars.push_back("Wifi State: " + String(progData.wifiConnected));
  debugVars.push_back("IP Address: " + String(progData.ips[0])+"."+String(progData.ips[1])+"."+String(progData.ips[2])+"."+String(progData.ips[3]));

  // debugVars.push_back("Key Power State: " + String(hbData->keyPowerState));
  // debugVars.push_back("Key Power Voltage: " + String(keyMonitor.voltage));
  // debugVars.push_back("PC Power State: " + String(hbData->pcPowerState));
  // debugVars.push_back("ISO 1 State: " + String(hbData->isConnectedISOVehicle));
  // debugVars.push_back("PC Voltage: " + String(busMonitor.busvoltage));
  // debugVars.push_back("PC Current: " + String(busMonitor.current_mA));
  debugVars.push_back("Shutdown Cmd: " + String(progData.shutdown));
  debugVars.push_back("Program State: " + String(progData.programState));
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

void serverSetup(){
      // Serve the main HTML page
      server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/index.html");
      });
      // Route to get debug variables as JSON
      server.on("/getDebugVars", HTTP_GET, handleDebugVars);
      // Route to list files as JSON
      server.on("/listFiles", HTTP_GET, handleFileList);
      // Route to download files
      server.on("/download", HTTP_GET, handleFileDownload);

      // Handle file upload
      server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {}, handleFileUpload);

      server.on("/reboot", HTTP_GET, handleReboot);
      // Start server
      server.begin();
}
#pragma endregion
#pragma region Timers
hw_timer_t * hbTimer = NULL;

void IRAM_ATTR onHBtimer(){
  if (progData.wifiConnected == 1){
    udpMethods.sendHeartbeat();
  }
}

void timerSetups(){
  // Initialize heartbeat send timer
    hbTimer = timerBegin(0, 80, true);  // Use timer 0, prescaler 80 (1 microsecond ticks), count up
    timerAttachInterrupt(hbTimer, &onHBtimer, true);  // Attach onTimer function as interrupt handler
    timerAlarmWrite(hbTimer, 1000000, true);  // Set alarm to trigger every 1 second (1000000 microseconds)
    timerAlarmEnable(hbTimer);  // Enable the timer
}
#pragma endregion

void setup(){
  Serial.begin(115200);
  // Get Config Variables from json
  config.begin();
  // Set program state to "Startup"
  progData.programState = 1;
  // Inititialize LED controller
  myLED.startTask();
  // Initialize Wifi
  while (true){
    progData.wifiConnected = myWifi.connect(progData.ips, progData.sketchConfig);
    if (progData.wifiConnected == 1){
      // Setup webserver
      serverSetup();
      progData.programState = 3;
      //Check for software updates
      softwareUpdate();
      progData.programState = 1;
      // setup udp comms
      udpMethods.begin();
      
      break;
    }
  }
  Serial.println("Setup Complete");
}


void loop(){
 delay(1000);
}