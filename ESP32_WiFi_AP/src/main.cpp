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
    debugVars.push_back("STA Enabled: "   + String(staCfg.enabled ? "YES" : "NO"));
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

void handleSaveSettings(AsyncWebServerRequest* request) {
    bool doReboot = false;
    if (request->hasParam("ap_ssid", true)) {
        espConfig.preferences.putString("ap_ssid", request->getParam("ap_ssid", true)->value());
        doReboot = true;
    }
    if (request->hasParam("ap_pass", true)) {
        espConfig.preferences.putString("ap_pass", request->getParam("ap_pass", true)->value());
        doReboot = true;
    }
    if (request->hasParam("ap_ch", true)) {
        int ch = request->getParam("ap_ch", true)->value().toInt();
        if (ch >= 1 && ch <= 13) espConfig.preferences.putInt("ap_ch", ch);
        doReboot = true;
    }
    if (request->hasParam("ap_ip3", true)) {
        int ip3 = request->getParam("ap_ip3", true)->value().toInt();
        if (ip3 >= 1 && ip3 <= 254) espConfig.preferences.putInt("ap_ip3", ip3);
        doReboot = true;
    }
    if (request->hasParam("sta_en", true)) {
        bool en = (request->getParam("sta_en", true)->value() == "1");
        espConfig.preferences.putBool("sta_en", en);
        doReboot = true;
    }
    if (request->hasParam("sta_ssid", true)) {
        espConfig.preferences.putString("sta_ssid", request->getParam("sta_ssid", true)->value());
        doReboot = true;
    }
    if (request->hasParam("sta_pass", true)) {
        espConfig.preferences.putString("sta_pass", request->getParam("sta_pass", true)->value());
        doReboot = true;
    }
    request->send(200, "text/plain", doReboot ? "Settings saved. Rebooting..." : "No changes.");
    if (doReboot) { delay(500); ESP.restart(); }
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
    if (staCfg.enabled) {
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
    server.on("/getFiles",      HTTP_GET, handleFileList);
    server.on("/download",      HTTP_GET, handleFileDownload);
    server.on("/reboot",        HTTP_GET, handleReboot);
    server.on("/saveSettings",  HTTP_POST, handleSaveSettings);

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
