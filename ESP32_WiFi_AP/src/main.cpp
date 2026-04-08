#include <Arduino.h>
#include "ESPconfig.h"
#include "ESPWifi.h"
#include "myLED.h"
#include "ESPudp.h"
#include <ESPAsyncWebServer.h>
#include "ArduinoJson.h"
#include <Update.h>

ESPconfig espConfig;
MyLED     myLED(&espConfig);
ESPWifi   espWifi(&espConfig);
ESPudp    espUdp(&espConfig);

std::vector<String> debugVars;
AsyncWebServer server(80);

auto& progData = espConfig.progData;
auto& apCfg    = espConfig.apCfg;
auto& staCfg   = espConfig.staCfg;
auto& udpStats = espConfig.udpStats;

// ── OTA handlers ─────────────────────────────────────────────────────────

static bool fwUpdateSkip = false;

void handleFirmwareUpload(AsyncWebServerRequest* request, String filename,
                          size_t index, uint8_t* data, size_t len, bool final) {
    if (!index) {
        fwUpdateSkip = false;
        Serial.printf("OTA Start: %s\n", filename.c_str());
        if (!filename.startsWith(NAME)) {
            Serial.printf("Firmware rejected: '%s' does not match '%s'\n",
                          filename.c_str(), NAME);
            fwUpdateSkip = true;
        } else if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
            fwUpdateSkip = true;
        }
    }
    if (fwUpdateSkip) {
        if (final)
            request->send(400, "text/plain",
                          "Wrong firmware file! Expected a file starting with: " + String(NAME));
        return;
    }
    if (Update.write(data, len) != len) Update.printError(Serial);
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

void handleFilesystemUpload(AsyncWebServerRequest* request, String filename,
                            size_t index, uint8_t* data, size_t len, bool final) {
    if (!index) {
        Serial.printf("Filesystem OTA Start: %s\n", filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS)) {
            Update.printError(Serial);
            request->send(500, "text/plain", "Filesystem update failed to start.");
            return;
        }
    }
    if (Update.write(data, len) != len) Update.printError(Serial);
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

// ── Debug vars ────────────────────────────────────────────────────────────

void updateDebugVars() {
    debugVars.clear();
    debugVars.push_back("Program: " + String(NAME));
    debugVars.push_back("Version: " + String(VERSION));
    debugVars.push_back("Uptime [s]: " + String((float)millis() / 1000.0f));
    debugVars.push_back("Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
    debugVars.push_back("AP SSID: "   + String(apCfg.ssid));
    debugVars.push_back("AP IP: "     + String(apCfg.ips[0]) + "." +
                                        String(apCfg.ips[1]) + "." +
                                        String(apCfg.ips[2]) + "." +
                                        String(apCfg.ips[3]));
    debugVars.push_back("AP Channel: " + String(apCfg.channel));
    debugVars.push_back("Connected Clients: " + String(espWifi.getConnectedClients()));
    debugVars.push_back("STA Enabled: "   + String(staCfg.count > 0 && espConfig.wifiMode != 0 ? "YES" : "NO"));
    debugVars.push_back("STA Networks: "  + String(staCfg.count));
    debugVars.push_back("STA Connected: " + String(staCfg.state == 1 ? "YES" : "NO"));
    if (staCfg.state == 1) {
        debugVars.push_back("STA IP: " + WiFi.localIP().toString());
    }
    debugVars.push_back("UDP Packets Relayed: " + String(udpStats.packetsRelayed));
    debugVars.push_back("UDP Bytes Relayed: "   + String(udpStats.bytesRelayed));
    debugVars.push_back("Last UDP Packet [ms ago]: " +
                        String(udpStats.lastPacketMs ? millis() - udpStats.lastPacketMs : 0));
}

void handleDebugVars(AsyncWebServerRequest* request) {
    updateDebugVars();
    DynamicJsonDocument doc(2048);
    JsonArray array = doc.to<JsonArray>();
    for (const auto& var : debugVars) array.add(var);
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
}

// ── Settings handlers ────────────────────────────────────────────────────

void handleGetSettings(AsyncWebServerRequest* request) {
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
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
}

void handleSaveSettings(AsyncWebServerRequest* request) {
    bool doReboot = false;
    if (request->hasParam("ap_ssid", true)) {
        String v = request->getParam("ap_ssid", true)->value();
        if (v.length() > 0 && v.length() < 64) { espConfig.preferences.putString("ap_ssid", v); doReboot = true; }
    }
    if (request->hasParam("ap_pass", true)) {
        String v = request->getParam("ap_pass", true)->value();
        if (v.length() > 0 && v.length() < 64) { espConfig.preferences.putString("ap_pass", v); doReboot = true; }
    }
    if (request->hasParam("ap_ch", true)) {
        int ch = request->getParam("ap_ch", true)->value().toInt();
        if (ch >= 1 && ch <= 13) { espConfig.preferences.putInt("ap_ch", ch); doReboot = true; }
    }
    if (request->hasParam("ap_ip3", true)) {
        int ip3 = request->getParam("ap_ip3", true)->value().toInt();
        if (ip3 >= 1 && ip3 <= 254) { espConfig.preferences.putInt("ap_ip3", ip3); doReboot = true; }
    }
    if (request->hasParam("wifi_mode", true)) {
        int mode = request->getParam("wifi_mode", true)->value().toInt();
        if (mode >= 0 && mode <= 2) {
            espConfig.preferences.putInt("wifi_mode", mode);
            doReboot = true;
        }
    }
    request->send(200, "text/plain", doReboot ? "Settings saved. Rebooting..." : "No changes.");
    if (doReboot) { delay(500); ESP.restart(); }
}

void handleAddNetwork(AsyncWebServerRequest* request) {
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

void handleRemoveNetwork(AsyncWebServerRequest* request) {
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

void handleReboot(AsyncWebServerRequest* request) {
    request->send(200, "text/plain", "Rebooting...");
    delay(100);
    ESP.restart();
}

void handleFileList(AsyncWebServerRequest* request) {
    DynamicJsonDocument doc(1024);
    JsonArray array = doc.to<JsonArray>();
    File root = LittleFS.open("/");
    File file = root.openNextFile();
    while (file) {
        JsonObject obj = array.createNestedObject();
        obj["name"] = String(file.name());
        obj["size"] = file.size();
        file = root.openNextFile();
    }
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
}

void handleFileDownload(AsyncWebServerRequest* request) {
    if (request->hasParam("filename")) {
        String fn = "/" + request->getParam("filename")->value();
        if (LittleFS.exists(fn))
            request->send(LittleFS, fn, "application/octet-stream");
        else
            request->send(404, "text/plain", "File not found");
    } else {
        request->send(400, "text/plain", "Filename not provided");
    }
}

void handleFileUpload(AsyncWebServerRequest* request, String filename,
                      size_t index, uint8_t* data, size_t len, bool final) {
    if (!index) {
        Serial.printf("UploadStart: %s\n", filename.c_str());
        request->_tempFile = LittleFS.open("/" + filename, "w");
    }
    if (len) request->_tempFile.write(data, len);
    if (final) {
        request->_tempFile.close();
        Serial.printf("UploadEnd: %s  %u B\n", filename.c_str(), index + len);
        request->send(200, "text/plain", "File Uploaded");
    }
}

// ── Setup ─────────────────────────────────────────────────────────────────

void setup() {
    progData.state = 0;
    myLED.startTask();
    progData.state = 2;

    Serial.begin(115200);
    delay(1000);
    Serial.println("ESP32 WiFi AP starting...");

    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed");
    }

    espConfig.progData.confRes = espConfig.loadConfig();

    // Start WiFi access point (always)
    espWifi.startAP();

    // Optionally connect to upstream STA network
    if (espConfig.wifiMode != 0 && staCfg.count > 0) {
        espWifi.connectSTA();
    }

    espWifi.startMonitor();

    // ── Web server routes ──────────────────────────────────────────────
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(LittleFS, "/index.html");
    });
    server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(LittleFS, "/index.html");
    });
    server.on("/debug.html", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(LittleFS, "/debug.html");
    });
    server.on("/settings.html", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(LittleFS, "/settings.html");
    });

    server.on("/getDebugVars",  HTTP_GET, handleDebugVars);
    server.on("/getSettings",   HTTP_GET, handleGetSettings);
    server.on("/getFiles",      HTTP_GET, handleFileList);
    server.on("/download",      HTTP_GET, handleFileDownload);
    server.on("/reboot",        HTTP_GET, handleReboot);
    server.on("/saveSettings",  HTTP_POST, handleSaveSettings);
    server.on("/addNetwork",    HTTP_POST, handleAddNetwork);
    server.on("/removeNetwork", HTTP_POST, handleRemoveNetwork);

    server.on("/upload", HTTP_POST,
              [](AsyncWebServerRequest* r) {},
              handleFileUpload);
    server.on("/update", HTTP_POST,
              [](AsyncWebServerRequest* r) {},
              handleFirmwareUpload);
    server.on("/updatefs", HTTP_POST,
              [](AsyncWebServerRequest* r) {},
              handleFilesystemUpload);

    // Module identification – used by the PC management server
    server.on("/version", HTTP_GET, [](AsyncWebServerRequest* request) {
        DynamicJsonDocument doc(128);
        doc["name"]    = NAME;
        doc["version"] = VERSION;
        String json;
        serializeJson(doc, json);
        request->send(200, "application/json", json);
    });

    server.begin();

    // Start UDP relay (high-speed broadcast forwarding)
    espUdp.begin();

    progData.state = 1;
    Serial.println("Setup complete.");
}

// ── Loop ──────────────────────────────────────────────────────────────────

void loop() {
    // Update LED state based on connected client count
    progData.state = (espWifi.getConnectedClients() > 0) ? 3 : 1;

    static uint32_t lastDebug = 0;
    if (millis() - lastDebug >= 5000) {
        lastDebug = millis();
        Serial.printf("[%lu s] Clients:%d  Relayed:%lu pkts  Heap:%u\n",
                      millis() / 1000,
                      espWifi.getConnectedClients(),
                      (unsigned long)udpStats.packetsRelayed,
                      ESP.getFreeHeap());
    }

    delay(100);
}
