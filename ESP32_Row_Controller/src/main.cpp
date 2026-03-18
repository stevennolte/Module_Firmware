#include <Arduino.h>
#include "ESPconfig.h"
#include "ESPWifi.h"
#include "myLED.h"
#include <ESPAsyncWebServer.h>
#include "ESP32OTAPull.h"
#include "ESPudp.h"
#include "ArduinoJson.h"
#include <Update.h>

ESPconfig espConfig;
MyLED myLED(&espConfig);
ESPWifi espWifi(&espConfig);
ESPudp espUdp(&espConfig);
std::vector<String> debugVars;
AsyncWebServer server(80);

auto& progData = espConfig.progData;
auto& progCfg = espConfig.progCfg;
auto& wifiCfg = espConfig.wifiCfg;
auto& sectionData = espConfig.sectionData;
auto& gpioDefs = espConfig.gpioDefs;

// -----------------------------------------------------------------------
// Row output control
// -----------------------------------------------------------------------

void initRowOutputs() {
    for (int i = 0; i < NUM_ROWS; i++) {
        pinMode(gpioDefs.rowPins[i], OUTPUT);
        digitalWrite(gpioDefs.rowPins[i], LOW);
    }
}

// Apply section states to physical MOSFET outputs.
// If the toolbar is up all outputs are forced off regardless of section state.
void updateRowOutputs() {
    bool toolbarUp;
    
    // Check if manual override is enabled
    if (sectionData.toolbarOverrideEnabled) {
        toolbarUp = sectionData.toolbarOverrideValue;
    } else {
        toolbarUp = digitalRead(gpioDefs.TOOLBAR_PIN) == HIGH;
    }
    
    sectionData.toolbarUp = toolbarUp;

    for (int i = 0; i < NUM_ROWS; i++) {
        bool active = !toolbarUp && (sectionData.rowStates[i] == 1);
        digitalWrite(gpioDefs.rowPins[i], active ? HIGH : LOW);
    }

    // Update LED state: toolbar up = state 3, normal operation = state 1
    progData.state = toolbarUp ? 3 : 1;
}

// -----------------------------------------------------------------------
// Webserver helpers
// -----------------------------------------------------------------------

static bool fwUpdateSkip = false;

void handleFirmwareUpload(AsyncWebServerRequest *request, String filename,
                          size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
        fwUpdateSkip = false;
        Serial.printf("OTA Start: %s\n", filename.c_str());
        if (!filename.startsWith(NAME)) {
            Serial.printf("Firmware rejected: filename '%s' does not match module '%s'\n", filename.c_str(), NAME);
            fwUpdateSkip = true;
        } else if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    }

    if (fwUpdateSkip) {
        if (final) {
            request->send(400, "text/plain", "Wrong firmware file! Expected a file starting with: " + String(NAME));
        }
        return;
    }

    if (Update.write(data, len) != len) {
        Update.printError(Serial);
    }
    if (final) {
        if (Update.end(true)) {
            Serial.printf("OTA Success: %u bytes\n", index + len);
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
    debugVars.clear();
    debugVars.push_back("Program: " + String(NAME));
    debugVars.push_back("Version: " + String(VERSION));
    debugVars.push_back("Uptime [s]: " + String((float)millis() / 1000.0f));
    debugVars.push_back("Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
    debugVars.push_back("Wifi SSID: " + WiFi.SSID());
    debugVars.push_back("IP Address: " + String(wifiCfg.ips[0]) + "." +
                                         String(wifiCfg.ips[1]) + "." +
                                         String(wifiCfg.ips[2]) + "." +
                                         String(wifiCfg.ips[3]));
    debugVars.push_back("Wifi State: " + String(wifiCfg.state));
    debugVars.push_back("Program State: " + String(progData.state));
    debugVars.push_back("Speed: " + String(sectionData.speed));
    debugVars.push_back("Toolbar Up: " + String(sectionData.toolbarUp ? "YES" : "NO"));
    debugVars.push_back("Last Section Msg [ms ago]: " +
                        String(millis() - sectionData.lastSectionMsg));
    for (int i = 0; i < NUM_ROWS; i++) {
        debugVars.push_back("Row " + String(i + 1) + ": " +
                            String(sectionData.rowStates[i] ? "ON" : "OFF"));
    }
}

void handleDebugVars(AsyncWebServerRequest *request) {
    updateDebugVars();
    DynamicJsonDocument doc(2048);
    JsonArray array = doc.to<JsonArray>();
    for (const auto& var : debugVars) {
        array.add(var);
    }
    String jsonResponse;
    serializeJson(doc, jsonResponse);
    request->send(200, "application/json", jsonResponse);
}

void handleFileList(AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(1024);
    JsonArray array = doc.to<JsonArray>();
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

void handleReboot(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Rebooting...");
    delay(100);
    ESP.restart();
}

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

void handleFileUpload(AsyncWebServerRequest *request, String filename,
                      size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
        Serial.printf("UploadStart: %s\n", filename.c_str());
        request->_tempFile = LittleFS.open("/" + filename, "w");
    }
    if (len) {
        request->_tempFile.write(data, len);
    }
    if (final) {
        request->_tempFile.close();
        Serial.printf("UploadEnd: %s, %u B\n", filename.c_str(), index + len);
        request->send(200, "text/plain", "File Uploaded");
    }
}

// Save settings (WiFi SSID/password, device IP last octet) and reboot
void handleSaveSettings(AsyncWebServerRequest *request) {
    if (request->hasParam("ssid", true)) {
        String ssid = request->getParam("ssid", true)->value();
        espConfig.preferences.putString("ssid", ssid);
    }
    if (request->hasParam("password", true)) {
        String password = request->getParam("password", true)->value();
        espConfig.preferences.putString("password", password);
    }
    if (request->hasParam("ip3", true)) {
        int ip3 = request->getParam("ip3", true)->value().toInt();
        if (ip3 >= 0 && ip3 <= 255) {
            espConfig.preferences.putInt("ip3", ip3);
        }
    }
    request->send(200, "text/plain", "Settings saved. Rebooting...");
    delay(500);
    ESP.restart();
}

// Settings can also come as query params (GET) from the settings page form
void handleSaveSettingsGet(AsyncWebServerRequest *request) {
    if (request->hasParam("ssid")) {
        espConfig.preferences.putString("ssid", request->getParam("ssid")->value());
    }
    if (request->hasParam("password")) {
        espConfig.preferences.putString("password", request->getParam("password")->value());
    }
    if (request->hasParam("ip3")) {
        int ip3 = request->getParam("ip3")->value().toInt();
        if (ip3 >= 0 && ip3 <= 255) {
            espConfig.preferences.putInt("ip3", ip3);
        }
    }
    request->send(200, "text/plain", "Settings saved. Rebooting...");
    delay(500);
    ESP.restart();
}

// Return current section states as JSON for the web UI
void handleGetSections(AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(512);
    doc["toolbarUp"] = sectionData.toolbarUp;
    doc["speed"] = sectionData.speed;
    doc["toolbarOverrideEnabled"] = sectionData.toolbarOverrideEnabled;
    doc["toolbarOverrideValue"] = sectionData.toolbarOverrideValue;
    JsonArray rows = doc.createNestedArray("rows");
    for (int i = 0; i < NUM_ROWS; i++) {
        rows.add(sectionData.rowStates[i]);
    }
    String jsonResponse;
    serializeJson(doc, jsonResponse);
    request->send(200, "application/json", jsonResponse);
}

// Set toolbar manual override
void handleSetToolbarOverride(AsyncWebServerRequest *request) {
    if (request->hasParam("enabled")) {
        String enabled = request->getParam("enabled")->value();
        sectionData.toolbarOverrideEnabled = (enabled == "1" || enabled == "true");
    }
    if (request->hasParam("value")) {
        String value = request->getParam("value")->value();
        sectionData.toolbarOverrideValue = (value == "1" || value == "true");
    }
    request->send(200, "text/plain", "OK");
}

// -----------------------------------------------------------------------
// Setup
// -----------------------------------------------------------------------

void setup() {
    progData.state = 0;
    myLED.startTask();
    progData.state = 2;

    Serial.begin(115200);
    delay(3000);
    Serial.println("ESP32 Row Controller starting...");

    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed");
    }

    espConfig.progData.confRes = espConfig.loadConfig();

    // Setup row MOSFET outputs and toolbar input
    initRowOutputs();
    pinMode(gpioDefs.TOOLBAR_PIN, INPUT_PULLDOWN);
    
    // Setup power relay (starts LOW, will turn on after boot)
    pinMode(gpioDefs.POWER_RELAY_PIN, OUTPUT);
    digitalWrite(gpioDefs.POWER_RELAY_PIN, LOW);

    // Connect to WiFi; fall back to AP mode after 60 s
    while (wifiCfg.state != 1) {
        wifiCfg.state = espWifi.connect();
        if (millis() > 60000) {
            Serial.println("WiFi connect timeout - starting AP");
            wifiCfg.state = espWifi.makeAP();
            break;
        }
    }
    Serial.println("Wifi State: " + String(wifiCfg.state));
    
    // Wait for WiFi connection and print IP address
    if (wifiCfg.state == 1) {
        Serial.print("Waiting for WiFi connection");
        uint32_t connectStart = millis();
        while (!WiFi.isConnected() && (millis() - connectStart < 15000)) {
            delay(500);
            Serial.print(".");
        }
        if (WiFi.isConnected()) {
            Serial.println("\nConnected to WiFi!");
            Serial.print("IP address: ");
            Serial.println(WiFi.localIP());
            espWifi.startMonitor();
        } else {
            Serial.println("\nFailed to connect to WiFi");
        }
    }

    // ----- Webserver routes -----
    // Page routes
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/index.html");
    });
    server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/index.html");
    });
    server.on("/debug.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/debug.html");
    });
    server.on("/settings.html", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/settings.html");
    });

    // Data / action routes
    server.on("/getDebugVars", HTTP_GET, handleDebugVars);
    server.on("/getFiles", HTTP_GET, handleFileList);
    server.on("/download", HTTP_GET, handleFileDownload);
    server.on("/reboot", HTTP_GET, handleReboot);
    server.on("/getSections", HTTP_GET, handleGetSections);
    server.on("/setToolbarOverride", HTTP_GET, handleSetToolbarOverride);
    server.on("/saveSettings", HTTP_POST, handleSaveSettings);
    server.on("/saveSettings", HTTP_GET, handleSaveSettingsGet);

    // Upload & OTA
    server.on("/upload", HTTP_POST,
              [](AsyncWebServerRequest *request) {},
              handleFileUpload);
    server.on("/update", HTTP_POST,
              [](AsyncWebServerRequest *request) {},
              handleFirmwareUpload);

    // Module identification endpoint – used by the PC management server
    server.on("/version", HTTP_GET, [](AsyncWebServerRequest *request) {
        DynamicJsonDocument doc(128);
        doc["name"] = NAME;
        doc["version"] = VERSION;
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    server.begin();

    // Start UDP listener for AgOpenGPS messages
    espUdp.begin();

    progData.state = 1;
    
    // Turn on main power relay after boot process is complete
    digitalWrite(gpioDefs.POWER_RELAY_PIN, HIGH);
    Serial.println("Power relay activated.");
    
    Serial.println("Setup complete.");
}

// -----------------------------------------------------------------------
// Loop
// -----------------------------------------------------------------------

void debugPrint() {
    Serial.printf("[%lu ms] State:%d WiFi:%d ToolbarUp:%d Speed:%.1f\n",
                  millis(), progData.state, wifiCfg.state,
                  sectionData.toolbarUp, sectionData.speed);
    for (int i = 0; i < NUM_ROWS; i++) {
        Serial.printf("  Row%d:%d", i + 1, sectionData.rowStates[i]);
    }
    Serial.println();
}

void loop() {
    updateRowOutputs();
    
    // Send section control data (PGN 234) to AgOpenGPS at 10Hz
    static uint32_t lastUdpSend = 0;
    if (millis() - lastUdpSend >= 100) {
        lastUdpSend = millis();
        espUdp.sendUDP();
    }
    
    delay(50);

    static uint32_t lastDebugPrint = 0;
    if (millis() - lastDebugPrint >= 1000) {
        lastDebugPrint = millis();
        debugPrint();
    }
}
