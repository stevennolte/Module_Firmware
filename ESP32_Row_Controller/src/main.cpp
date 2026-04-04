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

auto& progData    = espConfig.progData;
auto& progCfg     = espConfig.progCfg;
auto& apCfg       = espConfig.apCfg;
auto& staCfg      = espConfig.staCfg;
auto& sectionData = espConfig.sectionData;
auto& gpioDefs    = espConfig.gpioDefs;

// -----------------------------------------------------------------------
// Row output control
// -----------------------------------------------------------------------

void initRowOutputs() {
    for (int i = 0; i < NUM_ROWS; i++) {
        pinMode(gpioDefs.rowPins[i], OUTPUT);
        // Set to inactive state (opposite of active state)
        digitalWrite(gpioDefs.rowPins[i], gpioDefs.rowActiveHigh ? LOW : HIGH);
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
        // Check both toolbar sensors - if either is HIGH, toolbar is up
        toolbarUp = (digitalRead(gpioDefs.toolbarPins[0]) == HIGH) || 
                    (digitalRead(gpioDefs.toolbarPins[1]) == HIGH);
    }
    
    sectionData.toolbarUp = toolbarUp;

    for (int i = 0; i < NUM_ROWS; i++) {
        bool active = !toolbarUp && (sectionData.rowStates[i] == 1);
        // Write active or inactive state based on rowActiveHigh setting
        uint8_t activeState = gpioDefs.rowActiveHigh ? HIGH : LOW;
        uint8_t inactiveState = gpioDefs.rowActiveHigh ? LOW : HIGH;
        digitalWrite(gpioDefs.rowPins[i], active ? activeState : inactiveState);
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

void handleFilesystemUpload(AsyncWebServerRequest *request, String filename,
                            size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
        Serial.printf("Filesystem OTA Start: %s\n", filename.c_str());
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
            Serial.printf("Filesystem OTA Success: %u bytes\n", index + len);
            request->send(200, "text/html", "Filesystem update complete! Rebooting...");
            delay(1000);
            ESP.restart();
        } else {
            Update.printError(Serial);
            request->send(500, "text/html", "Filesystem update failed.");
        }
    }
}

void updateDebugVars() {
    debugVars.clear();
    debugVars.push_back("Program: " + String(NAME));
    debugVars.push_back("Version: " + String(VERSION));
    debugVars.push_back("Uptime [s]: " + String((float)millis() / 1000.0f));
    debugVars.push_back("Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
    debugVars.push_back("AP SSID: "    + String(apCfg.ssid));
    debugVars.push_back("AP IP: "      + String(apCfg.ips[0]) + "." +
                                         String(apCfg.ips[1]) + "." +
                                         String(apCfg.ips[2]) + "." +
                                         String(apCfg.ips[3]));
    debugVars.push_back("Connected Clients: " + String(espWifi.getConnectedClients()));
    debugVars.push_back("STA Enabled: "   + String(staCfg.count > 0 && espConfig.wifiMode != 0 ? "YES" : "NO"));
    debugVars.push_back("STA Networks: "  + String(staCfg.count));
    debugVars.push_back("STA Connected: " + String(staCfg.state == 1 ? "YES" : "NO"));
    if (staCfg.state == 1) {
        debugVars.push_back("STA IP: " + WiFi.localIP().toString());
    }
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

// Return current settings as JSON for the settings page
void handleGetSettings(AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(1024);
    doc["wifi_mode"] = espConfig.wifiMode;
    doc["ap_ssid"]   = espConfig.apCfg.ssid;
    doc["ap_pass"]   = espConfig.apCfg.password;
    doc["ap_ch"]     = espConfig.apCfg.channel;
    doc["ap_ip3"]    = espConfig.apCfg.ips[3];
    JsonArray networks = doc.createNestedArray("sta_networks");
    for (int i = 0; i < espConfig.staCfg.count; i++) {
        JsonObject net = networks.createNestedObject();
        net["ssid"] = espConfig.staCfg.ssids[i];
        net["pass"] = espConfig.staCfg.passwords[i];
    }
    JsonArray rowPins = doc.createNestedArray("row_pins");
    for (int i = 0; i < NUM_ROWS; i++) {
        rowPins.add(espConfig.gpioDefs.rowPins[i]);
    }
    JsonArray toolbarPins = doc.createNestedArray("toolbar_pins");
    for (int i = 0; i < 2; i++) {
        toolbarPins.add(espConfig.gpioDefs.toolbarPins[i]);
    }
    doc["row_active_high"] = espConfig.gpioDefs.rowActiveHigh;
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
}

// Save settings (WiFi SSID/password, device IP last octet) and reboot
void handleSaveSettings(AsyncWebServerRequest *request) {
    bool doReboot = false;
    if (request->hasParam("ap_ssid", true)) {
        String v = request->getParam("ap_ssid", true)->value();
        if (v.length() > 0 && v.length() < 64) { espConfig.preferences.putString("ap_ssid", v); doReboot = true; }
    }
    if (request->hasParam("ap_pass", true)) {
        String v = request->getParam("ap_pass", true)->value();
        if (v.length() > 0 && v.length() < 64) { espConfig.preferences.putString("ap_pass", v); doReboot = true; }
    }
    if (request->hasParam("ap_ip3", true)) {
        int ip3 = request->getParam("ap_ip3", true)->value().toInt();
        if (ip3 >= 1 && ip3 <= 254) { espConfig.preferences.putInt("ap_ip3", ip3); doReboot = true; }
    }
    if (request->hasParam("wifi_mode", true)) {
        int mode = request->getParam("wifi_mode", true)->value().toInt();
        if (mode >= 0 && mode <= 2) { espConfig.preferences.putInt("wifi_mode", mode); doReboot = true; }
    }
    request->send(200, "text/plain", doReboot ? "Settings saved. Rebooting..." : "No changes.");
    if (doReboot) { delay(500); ESP.restart(); }
}

void handleAddNetwork(AsyncWebServerRequest *request) {
    if (!request->hasParam("ssid", true)) {
        request->send(400, "text/plain", "Missing ssid");
        return;
    }
    String ssid = request->getParam("ssid", true)->value();
    String pass = request->hasParam("pass", true) ? request->getParam("pass", true)->value() : "";
    ssid.trim();
    if (ssid.length() == 0 || ssid.length() >= 64) {
        request->send(400, "text/plain", "SSID must be 1–63 characters");
        return;
    }
    if (pass.length() >= 64) {
        request->send(400, "text/plain", "Password must be fewer than 64 characters");
        return;
    }
    int count = espConfig.preferences.getInt("sta_count", 0);
    if (count >= ESPconfig::STAConfig::MAX_NETWORKS) {
        request->send(400, "text/plain", "Maximum number of networks reached");
        return;
    }
    // Check for duplicate
    for (int i = 0; i < count; i++) {
        String keySSID = "sta_ssid_" + String(i);
        if (espConfig.preferences.getString(keySSID.c_str(), "") == ssid) {
            request->send(400, "text/plain", "Network already exists");
            return;
        }
    }
    String keySSID = "sta_ssid_" + String(count);
    String keyPass = "sta_pass_" + String(count);
    espConfig.preferences.putString(keySSID.c_str(), ssid);
    espConfig.preferences.putString(keyPass.c_str(), pass);
    espConfig.preferences.putInt("sta_count", count + 1);
    request->send(200, "text/plain", "Network added. Rebooting...");
    delay(500);
    ESP.restart();
}

void handleRemoveNetwork(AsyncWebServerRequest *request) {
    if (!request->hasParam("index", true)) {
        request->send(400, "text/plain", "Missing index");
        return;
    }
    int idx   = request->getParam("index", true)->value().toInt();
    int count = espConfig.preferences.getInt("sta_count", 0);
    if (idx < 0 || idx >= count) {
        request->send(400, "text/plain", "Invalid index");
        return;
    }
    // Shift remaining entries down
    for (int i = idx; i < count - 1; i++) {
        String keySSID_src = "sta_ssid_" + String(i + 1);
        String keyPass_src = "sta_pass_" + String(i + 1);
        String keySSID_dst = "sta_ssid_" + String(i);
        String keyPass_dst = "sta_pass_" + String(i);
        espConfig.preferences.putString(keySSID_dst.c_str(),
            espConfig.preferences.getString(keySSID_src.c_str(), ""));
        espConfig.preferences.putString(keyPass_dst.c_str(),
            espConfig.preferences.getString(keyPass_src.c_str(), ""));
    }
    // Clear the last slot
    String keySSID_last = "sta_ssid_" + String(count - 1);
    String keyPass_last = "sta_pass_" + String(count - 1);
    espConfig.preferences.remove(keySSID_last.c_str());
    espConfig.preferences.remove(keyPass_last.c_str());
    espConfig.preferences.putInt("sta_count", count - 1);
    request->send(200, "text/plain", "Network removed. Rebooting...");
    delay(500);
    ESP.restart();
}

// Save row pins configuration and reboot
void handleSaveRowPins(AsyncWebServerRequest *request) {
    bool valid = true;
    uint8_t newRowPins[NUM_ROWS];
    
    // Parse and validate all row pin parameters
    for (int i = 0; i < NUM_ROWS; i++) {
        String paramName = "row_pin_" + String(i);
        if (request->hasParam(paramName, true)) {
            int pinValue = request->getParam(paramName, true)->value().toInt();
            // Validate pin number (ESP32 GPIO range, excluding some restricted pins)
            if (pinValue < 0 || pinValue > 48) {
                valid = false;
                break;
            }
            newRowPins[i] = (uint8_t)pinValue;
        } else {
            valid = false;
            break;
        }
    }
    
    if (!valid) {
        request->send(400, "text/plain", "Invalid row pin configuration");
        return;
    }
    
    // Save to preferences and update runtime configuration
    for (int i = 0; i < NUM_ROWS; i++) {
        espConfig.gpioDefs.rowPins[i] = newRowPins[i];
    }
    
    // Handle row active high/low setting
    if (request->hasParam("row_active_high", true)) {
        String value = request->getParam("row_active_high", true)->value();
        espConfig.gpioDefs.rowActiveHigh = (value == "1" || value == "true");
    }
    
    espConfig.updateRowPins();
    
    request->send(200, "text/plain", "Row pins saved. Rebooting...");
    delay(500);
    ESP.restart();
}

// Save toolbar pins configuration and reboot
void handleSaveToolbarPins(AsyncWebServerRequest *request) {
    bool valid = true;
    uint8_t newToolbarPins[2];
    
    // Parse and validate toolbar pin parameters
    for (int i = 0; i < 2; i++) {
        String paramName = "toolbar_pin_" + String(i);
        if (request->hasParam(paramName, true)) {
            int pinValue = request->getParam(paramName, true)->value().toInt();
            // Validate pin number (ESP32 GPIO range)
            if (pinValue < 0 || pinValue > 48) {
                valid = false;
                break;
            }
            newToolbarPins[i] = (uint8_t)pinValue;
        } else {
            valid = false;
            break;
        }
    }
    
    if (!valid) {
        request->send(400, "text/plain", "Invalid toolbar pin configuration");
        return;
    }
    
    // Save to preferences and update runtime configuration
    for (int i = 0; i < 2; i++) {
        espConfig.gpioDefs.toolbarPins[i] = newToolbarPins[i];
    }
    espConfig.updateToolbarPins();
    
    request->send(200, "text/plain", "Toolbar pins saved. Rebooting...");
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

    // Setup row MOSFET outputs and toolbar inputs
    initRowOutputs();
    pinMode(gpioDefs.toolbarPins[0], INPUT_PULLDOWN);
    pinMode(gpioDefs.toolbarPins[1], INPUT_PULLDOWN);
    
    // Setup power relay (starts LOW, will turn on after boot)
    pinMode(gpioDefs.POWER_RELAY_PIN, OUTPUT);
    digitalWrite(gpioDefs.POWER_RELAY_PIN, LOW);

    // Start WiFi access point (always) and optionally connect as STA
    espWifi.startAP();
    if (espConfig.wifiMode != 0 && staCfg.count > 0) {
        espWifi.connectSTA();
    }
    espWifi.startMonitor();

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
    server.on("/getSettings",  HTTP_GET, handleGetSettings);
    server.on("/getFiles", HTTP_GET, handleFileList);
    server.on("/download", HTTP_GET, handleFileDownload);
    server.on("/reboot", HTTP_GET, handleReboot);
    server.on("/getSections", HTTP_GET, handleGetSections);
    server.on("/setToolbarOverride", HTTP_GET, handleSetToolbarOverride);
    server.on("/saveSettings",   HTTP_POST, handleSaveSettings);
    server.on("/addNetwork",     HTTP_POST, handleAddNetwork);
    server.on("/removeNetwork",  HTTP_POST, handleRemoveNetwork);
    server.on("/saveRowPins",    HTTP_POST, handleSaveRowPins);
    server.on("/saveToolbarPins", HTTP_POST, handleSaveToolbarPins);

    // Upload & OTA
    server.on("/upload", HTTP_POST,
              [](AsyncWebServerRequest *request) {},
              handleFileUpload);
    server.on("/update", HTTP_POST,
              [](AsyncWebServerRequest *request) {},
              handleFirmwareUpload);
    server.on("/updatefs", HTTP_POST,
              [](AsyncWebServerRequest *request) {},
              handleFilesystemUpload);

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
    Serial.printf("[%lu ms] State:%d WiFiMode:%d STA:%d ToolbarUp:%d Speed:%.1f\n",
                  millis(), progData.state, espConfig.wifiMode,
                  staCfg.state, sectionData.toolbarUp, sectionData.speed);
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
