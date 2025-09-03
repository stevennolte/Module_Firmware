#include "WebServer.h"
#include "Version.h"
#include <WiFi.h>

WebServerManager::WebServerManager(AsyncWebServer* serverPtr, ESPconfig* configPtr) 
    : server(serverPtr), espConfig(configPtr) {
}

void WebServerManager::begin() {
    setupRoutes();
    server->begin();
}

void WebServerManager::setupRoutes() {
    // Serve the main HTML page
    server->on("/", static_cast<WebRequestMethodComposite>(HTTP_GET), [](AsyncWebServerRequest *request){
        Serial.println("getting index file");
        request->send(LittleFS, "/index.html");
    });
    
    // Route to get debug variables as JSON
    server->on("/getDebugVars", static_cast<WebRequestMethodComposite>(HTTP_GET), [this](AsyncWebServerRequest *request){
        this->handleDebugVars(request);
    });
    
    // Route to list files as JSON
    server->on("/getFiles", static_cast<WebRequestMethodComposite>(HTTP_GET), [this](AsyncWebServerRequest *request){
        this->handleFileList(request);
    });
    
    // Route to download files
    server->on("/download", static_cast<WebRequestMethodComposite>(HTTP_GET), [this](AsyncWebServerRequest *request){
        this->handleFileDownload(request);
    });

    // Handle file upload
    server->on("/upload", static_cast<WebRequestMethodComposite>(HTTP_POST), [](AsyncWebServerRequest *request) {}, 
        [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
            this->handleFileUpload(request, filename, index, data, len, final);
        });

    // Handle firmware update
    server->on("/update", static_cast<WebRequestMethodComposite>(HTTP_POST), [](AsyncWebServerRequest *request) {}, 
        [this](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
            this->handleFirmwareUpload(request, filename, index, data, len, final);
        });

    // Reboot handler
    server->on("/reboot", static_cast<WebRequestMethodComposite>(HTTP_GET), [this](AsyncWebServerRequest *request){
        this->handleReboot(request);
    });
    
    // Toggle AP Mode
    server->on("/toggleAPMode", static_cast<WebRequestMethodComposite>(HTTP_POST), [this](AsyncWebServerRequest *request){
        this->handleToggleAPMode(request);
    });
    
    // WAS zero handler
    server->on("/zeroWAS", static_cast<WebRequestMethodComposite>(HTTP_GET), [this](AsyncWebServerRequest *request){
        this->handleWASzero(request);
    });
    
    // SVG file handlers
    server->on("/Module_Disconnected", static_cast<WebRequestMethodComposite>(HTTP_GET), [](AsyncWebServerRequest *request){
        Serial.println("Sending Module_Disconnected.svg");
        request->send(LittleFS, "/Module_Disconnected.svg", "image/svg+xml");
    });
    
    server->on("/Module_Connected", static_cast<WebRequestMethodComposite>(HTTP_GET), [](AsyncWebServerRequest *request){
        request->send(LittleFS, "/Module_Connected.svg", "image/svg+xml");
    });
    
    // GPS source selection handler
    server->on("/setGpsSource", static_cast<WebRequestMethodComposite>(HTTP_POST), [](AsyncWebServerRequest *request){},
        NULL,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            StaticJsonDocument<128> doc;
            DeserializationError error = deserializeJson(doc, data, len);
            if (error) {
                request->send(400, "text/plain", "Invalid JSON");
                return;
            }
            String source = doc["source"] | "";
            if (source == "um982") {
                espConfig->gpsCfg.externalGPS = false;
                request->send(200, "text/plain", "UM982 GPS selected");
            } else if (source == "external") {
                espConfig->gpsCfg.externalGPS = true;
                request->send(200, "text/plain", "External GPS selected");
            } else {
                request->send(400, "text/plain", "Unknown GPS source");
            }
        }
    );
}

void WebServerManager::handleFileList(AsyncWebServerRequest *request) {
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

void WebServerManager::handleFileDownload(AsyncWebServerRequest *request) {
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

void WebServerManager::handleWASzero(AsyncWebServerRequest *request) {
    espConfig->steerData.wasZeroAngle = espConfig->steerData.absAngle;
    Serial.println(espConfig->steerData.absAngle);
    Serial.println(espConfig->steerData.wasZeroAngle);
    Serial.println("WAS zeroed");
    uint8_t res = espConfig->saveWASzero();
    if (res == 1){
        request->send(200, "text/plain", "WAS zeroed and saved to config.json");
    } else {
        request->send(200, "text/plain", "WAS zeroed");
    }
}

void WebServerManager::handleFirmwareUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
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

void WebServerManager::updateDebugVars() {
    debugVars.clear(); // Clear the list to update it dynamically
    
    // Get reference shortcuts
    auto& progData = espConfig->progData;
    auto& wifiCfg = espConfig->wifiCfg;
    
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
    debugVars.push_back("IMU State: " + String(espConfig->gpsData.imuState));
    debugVars.push_back("External GPS: " + String(espConfig->gpsCfg.externalGPS ? "Enabled" : "Disabled"));
    debugVars.push_back("GPS Data: ");
    debugVars.push_back("..Timestamp: " + String(espConfig->gpsData.fixTime));
    debugVars.push_back("..Position Type: " + String(espConfig->gpsData.positionType));
    debugVars.push_back("..Latitude: " + String(espConfig->gpsData.latitude));
    debugVars.push_back("..Longitude: " + String(espConfig->gpsData.longitude));
    debugVars.push_back("..Altitude: " + String(espConfig->gpsData.altitude));
    debugVars.push_back("..Speed: " + String(espConfig->gpsData.speedKnots));
    debugVars.push_back("..Heading: " + String(espConfig->gpsData.imuHeading));
    debugVars.push_back("..Roll: " + String(espConfig->gpsData.imuRoll));
    debugVars.push_back("..Pitch: " + String(espConfig->gpsData.imuPitch));
    debugVars.push_back("..Yaw Rate: " + String(espConfig->gpsData.imuYawRate));
    debugVars.push_back("..Fix Quality: " + String(espConfig->gpsData.fixQuality));
    debugVars.push_back("..Number of Satellites: " + String(espConfig->gpsData.numSats));
    debugVars.push_back("..HDOP: " + String(espConfig->gpsData.HDOP));
    debugVars.push_back("..Age of DGPS: " + String(espConfig->gpsData.ageDGPS));
    debugVars.push_back("..NMEA: " + String(espConfig->gpsData.nmea));
    debugVars.push_back("..Last Ntrip Data: " + String(espConfig->gpsData.lastNtripData));
    debugVars.push_back("..Last Ntrip Data Length: " + String(espConfig->gpsData.lastNtripDataLen));
    debugVars.push_back("Steer Data: ");
    debugVars.push_back("..Target Steer Angle: " + String(espConfig->steerData.targetSteerAngle));
    debugVars.push_back("..Steer Angle: " + String(espConfig->steerData.actSteerAngle));
    debugVars.push_back("..Absolute Angle: " + String(espConfig->steerData.absAngle));
    debugVars.push_back("..ZeroValue: " + String(espConfig->steerData.wasZeroAngle));
    debugVars.push_back("..Test State: " + String(espConfig->steerData.testState));
    debugVars.push_back("..Steer Current: " + String(espConfig->steerData.steerCurrent));
    debugVars.push_back("..Switch State: " + String(espConfig->steerData.switchState));
    debugVars.push_back("..PWM Command: " + String(espConfig->steerData.pwmCmd));
    debugVars.push_back("..PID Input: " + String(espConfig->steerData.pidCmd));
    debugVars.push_back("..Status: " + String(espConfig->steerData.status));
    debugVars.push_back("..Wireless WAS: " + String(espConfig->steerCfg.wirelessWAS));
    debugVars.push_back("Steer Config: ");
    debugVars.push_back("..Settings Updated: " + String(espConfig->steerCfg.settingsUpdated));
    debugVars.push_back("..Gain P: " + String(espConfig->steerCfg.gainP));
    debugVars.push_back("..High PWM: " + String(espConfig->steerCfg.highPWM));
    debugVars.push_back("..Low PWM: " + String(espConfig->steerCfg.lowPWM));
    debugVars.push_back("..Min PWM: " + String(espConfig->steerCfg.minPWM));
    debugVars.push_back("..Counts per Degree: " + String(espConfig->steerCfg.countsPerDeg));
    debugVars.push_back("..Steer Offset: " + String(espConfig->steerCfg.steerOffset));
    debugVars.push_back("..Ackerman Fix: " + String(espConfig->steerCfg.ackermanFix));
    debugVars.push_back("..Set0: " + String(espConfig->steerCfg.set0));
    debugVars.push_back("..Pulse Count: " + String(espConfig->steerCfg.pulseCount));
    debugVars.push_back("..Min Speed: " + String(espConfig->steerCfg.minSpeed));
    debugVars.push_back("..Set1: " + String(espConfig->steerCfg.set1));
    debugVars.push_back("..Steer Msg Rate: " + String(espConfig->steerCfg.steerMsgRate));
    debugVars.push_back("..PID Input Filter: " + String(espConfig->steerCfg.pidInputFilt));
    debugVars.push_back("Switches: ");
    debugVars.push_back("..Steer Switch: " + String(espConfig->switchData.steerSwitch));
    debugVars.push_back("..Work Switch: " + String(espConfig->switchData.workSwitch)); 
    for (int i = 0; i < 8; i++){
        debugVars.push_back("..Joystick switch " + String(i) + ": " + String(espConfig->joystickData.switchStates[i]));
    }
}

void WebServerManager::handleDebugVars(AsyncWebServerRequest *request) {
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

void WebServerManager::handleReboot(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Rebooting...");
    delay(100); // Give some time for the response to be sent
    ESP.restart();
}

void WebServerManager::handleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
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

void WebServerManager::handleToggleAPMode(AsyncWebServerRequest *request) {
    static bool apModeState = false;
    apModeState = !apModeState;
    espConfig->wifiCfg.apMode = apModeState ? 1 : 0;
    Serial.printf("AP Mode State: %s\n", apModeState ? "ON" : "OFF");
    request->send(200, "text/plain", apModeState ? "AP_Mode is ON" : "AP_Mode is OFF");
}

void WebServerManager::handleClient() {
    // This method can be used for any periodic webserver maintenance if needed
    // Currently, AsyncWebServer handles everything automatically
}
