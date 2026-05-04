#include <Arduino.h>
#include "ESPconfig.h"
#include "ESPWifi.h"
#include "myLED.h"
#include "CANBUS.h"
#include "IMUSensor.h"
#include "ESP32OTAPull.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "WiFi.h"
#include "driver/temp_sensor.h"
#include <vector>

ESPconfig  espConfig;
MyLED      myLED(&espConfig);
ESPWifi    espWifi(&espConfig);
CANBUS     canBus(&espConfig);
IMUSensor  imuSensor(&espConfig);
ESP32OTAPull ota;
AsyncWebServer server(80);

std::vector<String> debugVars;

auto& progData  = espConfig.progData;
auto& progCfg   = espConfig.progCfg;
auto& wifiCfg   = espConfig.wifiCfg;
auto& imuData   = espConfig.imuData;
auto& canCfg    = espConfig.canCfg;

// ─── OTA ──────────────────────────────────────────────────────────────────────

const char *otaErrText(int code) {
    switch (code) {
        case ESP32OTAPull::UPDATE_AVAILABLE:        return "Update available but not installed";
        case ESP32OTAPull::NO_UPDATE_PROFILE_FOUND: return "No profile matches";
        case ESP32OTAPull::NO_UPDATE_AVAILABLE:     return "No update applicable";
        case ESP32OTAPull::UPDATE_OK:               return "Update done, no reboot";
        case ESP32OTAPull::HTTP_FAILED:             return "HTTP GET failure";
        case ESP32OTAPull::WRITE_ERROR:             return "Write error";
        case ESP32OTAPull::JSON_PROBLEM:            return "Invalid JSON";
        case ESP32OTAPull::OTA_UPDATE_FAIL:         return "Update fail (no OTA partition?)";
        default: return code > 0 ? "Unexpected HTTP code" : "Unknown error";
    }
}

// ─── Firmware / filesystem upload ─────────────────────────────────────────────

static bool fwUpdateSkip = false;

void handleFirmwareUpload(AsyncWebServerRequest *request, String filename,
                           size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
        fwUpdateSkip = false;
        if (!filename.startsWith(NAME)) {
            fwUpdateSkip = true;
            Serial.printf("Firmware rejected: '%s' does not match module '%s'\n",
                          filename.c_str(), NAME);
        } else if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    }
    if (fwUpdateSkip) {
        if (final) request->send(400, "text/plain",
                                 "Wrong firmware file! Expected: " + String(NAME) + ".bin");
        return;
    }
    if (Update.write(data, len) != len) Update.printError(Serial);
    if (final) {
        if (Update.end(true)) {
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
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS)) {
            Update.printError(Serial);
            request->send(500, "text/plain", "Filesystem update failed to start.");
            return;
        }
    }
    if (Update.write(data, len) != len) Update.printError(Serial);
    if (final) {
        if (Update.end(true)) {
            request->send(200, "text/html", "Filesystem update complete! Rebooting...");
            delay(1000);
            ESP.restart();
        } else {
            Update.printError(Serial);
            request->send(500, "text/html", "Filesystem update failed.");
        }
    }
}

// ─── File upload/download ──────────────────────────────────────────────────────

void handleFileUpload(AsyncWebServerRequest *request, String filename,
                       size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
        Serial.printf("UploadStart: %s\n", filename.c_str());
        request->_tempFile = LittleFS.open("/" + filename, "w");
    }
    if (len) request->_tempFile.write(data, len);
    if (final) {
        request->_tempFile.close();
        Serial.printf("UploadEnd: %s, %u B\n", filename.c_str(), index + len);
        request->send(200, "text/plain", "File Uploaded");
    }
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

void handleFileList(AsyncWebServerRequest *request) {
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

// ─── Debug vars ───────────────────────────────────────────────────────────────

void updateDebugVars() {
    debugVars.clear();
    debugVars.push_back("Program: "   + String(NAME));
    debugVars.push_back("Version: "   + String(VERSION));
    debugVars.push_back("Uptime [s]: " + String((float)millis() / 1000.0f));
    debugVars.push_back("Free Heap: " + String(ESP.getFreeHeap()) + " bytes");

    float tempC;
    temp_sensor_read_celsius(&tempC);
    debugVars.push_back("CPU Temp: " + String(tempC, 1) + " °C");

    debugVars.push_back("WiFi SSID: "  + WiFi.SSID());
    debugVars.push_back("IP Address: " +
        String(wifiCfg.ips[0]) + "." + String(wifiCfg.ips[1]) + "." +
        String(wifiCfg.ips[2]) + "." + String(wifiCfg.ips[3]));
    debugVars.push_back("WiFi State: " + String(wifiCfg.state));
    debugVars.push_back("Prog State: " + String(progData.state));
    debugVars.push_back("IMU State: "  + String(progData.imuState));
    debugVars.push_back("CAN State: "  + String(progData.canState));
    debugVars.push_back("--- IMU Data ---");
    debugVars.push_back("Roll:  " + String(imuData.roll,  2) + " deg");
    debugVars.push_back("Pitch: " + String(imuData.pitch, 2) + " deg");
    debugVars.push_back("Yaw:   " + String(imuData.yaw,   2) + " deg");
    debugVars.push_back("Accuracy: " + String(imuData.accuracy));
    debugVars.push_back("Last Update: " + String(imuData.lastUpdate) + " ms");
    debugVars.push_back("--- CAN Config ---");
    debugVars.push_back("CAN TX ID: 0x" + String(canCfg.txID, HEX));
    debugVars.push_back("CAN TX Freq: " + String(canCfg.txFreq) + " ms");
}

void handleDebugVars(AsyncWebServerRequest *request) {
    updateDebugVars();
    DynamicJsonDocument doc(2048);
    JsonArray array = doc.to<JsonArray>();
    for (const auto& var : debugVars) array.add(var);
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
}

// ─── Settings API ─────────────────────────────────────────────────────────────

void handleGetSettings(AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(512);
    doc["canTxID"]   = canCfg.txID;
    doc["canTxFreq"] = canCfg.txFreq;
    doc["bnoAddress"] = espConfig.i2cDefs.BNO_ADDRESS;
    doc["ipOctet4"]  = wifiCfg.ips[3];
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
}

void handleSaveSettings(AsyncWebServerRequest *request) {
    if (request->hasParam("canTxID",   true)) canCfg.txID   = strtoul(request->getParam("canTxID",   true)->value().c_str(), nullptr, 0);
    if (request->hasParam("canTxFreq", true)) canCfg.txFreq = request->getParam("canTxFreq", true)->value().toInt();
    if (request->hasParam("ipOctet4",  true)) wifiCfg.ips[3] = request->getParam("ipOctet4",  true)->value().toInt();
    espConfig.saveConfig();
    request->send(200, "text/plain", "Settings saved. Rebooting...");
    delay(500);
    ESP.restart();
}

// ─── Misc handlers ────────────────────────────────────────────────────────────

void handleReboot(AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Rebooting...");
    delay(100);
    ESP.restart();
}

void handleToggleAPMode(AsyncWebServerRequest *request) {
    wifiCfg.apMode = wifiCfg.apMode ? 0 : 1;
    request->send(200, "text/plain", wifiCfg.apMode ? "AP_Mode is ON" : "AP_Mode is OFF");
}

// ─── Setup ────────────────────────────────────────────────────────────────────

void setup() {
    progData.state = 2;
    myLED.startTask();

    Serial.begin(115200);
    delay(5000);
    Serial.println("ESP32_IMU_CAN starting...");

    progCfg.confRes = espConfig.loadConfig();
    Serial.printf("Config load result: %d\n", progCfg.confRes);

    // Connect WiFi
    while (wifiCfg.state != 1) {
        wifiCfg.state = espWifi.connect();
        if (millis() > 30000) {
            Serial.println("WiFi timeout – starting AP");
            wifiCfg.state = espWifi.makeAP();
            break;
        }
    }
    Serial.println("WiFi State: " + String(wifiCfg.state));

    // Start CPU temperature sensor
    temp_sensor_start();

    // ── Web server routes ──────────────────────────────────────────────────────
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send(LittleFS, "/index.html");
    });
    server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send(LittleFS, "/index.html");
    });
    server.on("/settings.html", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send(LittleFS, "/settings.html");
    });

    server.on("/getDebugVars",  HTTP_GET,  handleDebugVars);
    server.on("/getFiles",      HTTP_GET,  handleFileList);
    server.on("/download",      HTTP_GET,  handleFileDownload);
    server.on("/getSettings",   HTTP_GET,  handleGetSettings);
    server.on("/reboot",        HTTP_GET,  handleReboot);

    server.on("/upload",     HTTP_POST, [](AsyncWebServerRequest *req) {}, handleFileUpload);
    server.on("/update",     HTTP_POST, [](AsyncWebServerRequest *req) {}, handleFirmwareUpload);
    server.on("/updatefs",   HTTP_POST, [](AsyncWebServerRequest *req) {}, handleFilesystemUpload);
    server.on("/saveSettings", HTTP_POST, handleSaveSettings);
    server.on("/toggleAPMode", HTTP_POST, handleToggleAPMode);

    server.begin();

    // ── CAN bus ────────────────────────────────────────────────────────────────
    if (!canBus.begin()) {
        Serial.println("CAN bus init failed");
        progData.state = 3;
    }

    // ── BNO085 ─────────────────────────────────────────────────────────────────
    if (!imuSensor.begin()) {
        Serial.println("IMU init failed");
        progData.state = 3;
        // Keep running; debug page still accessible
    } else {
        imuSensor.startTask();
    }

    if (progData.state != 3) {
        progData.state = 1;
    }
    Serial.println("Setup complete");
}

// ─── Loop ─────────────────────────────────────────────────────────────────────

void loop() {
    // Debug print
    if (millis() - progData.debugTimestamp > progCfg.debugPrintDelay) {
        progData.debugTimestamp = millis();
        Serial.printf("[%lu] Roll=%.2f  Pitch=%.2f  Yaw=%.2f  acc=%d  imu=%d  can=%d\n",
                      millis(),
                      imuData.roll, imuData.pitch, imuData.yaw, imuData.accuracy,
                      progData.imuState, progData.canState);
    }

    // Send IMU data over CAN bus at configured frequency
    if (progData.imuState == 1 && progData.canState == 1) {
        if (millis() - canCfg.txTimestamp >= canCfg.txFreq) {
            canCfg.txTimestamp = millis();
            canBus.sendIMUData(imuData.roll, imuData.pitch, imuData.yaw, imuData.accuracy);
        }
    }

    delay(3);
}
