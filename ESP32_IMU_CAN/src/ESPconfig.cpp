#include "ESPconfig.h"

ESPconfig::ESPconfig() : progCfg(), wifiCfg(), otaCfg() {}

uint8_t ESPconfig::loadConfig() {
    wifiCfg.ips[0] = 192;
    wifiCfg.ips[1] = 168;
    wifiCfg.ips[2] = 5;
    wifiCfg.ips[3] = 20;

    if (!LittleFS.begin(true)) {
        return 2;
    }
    File file = LittleFS.open("/config.json", "r");
    if (!file) {
        return 3;
    }
    String jsonString;
    while (file.available()) {
        jsonString += char(file.read());
    }
    file.close();

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    if (error) {
        return 4;
    }

    // Verify module name
    strlcpy(progCfg.name, doc["Name"] | NAME, sizeof(progCfg.name));
    if (doc["Name"] != NAME) {
        return 5;
    }

    // IP address
    for (int i = 0; i < 4; i++) {
        wifiCfg.ips[i] = uint8_t(doc["ipAddress"][i]);
    }

    // CAN / J1939 settings
    canCfg.j1939SA  = doc["j1939SA"]        | (uint8_t)0x80;
    canCfg.txFreq   = doc["canTxFreq"]       | (uint32_t)50;

    // Magnetic declination for true-north heading
    imuData.magDeclination = doc["magDeclination"] | 0.0f;
    imuData.headingOffset  = doc["headingOffset"]  | -260.0f;
    imuData.reverseHeading = doc["reverseHeading"] | false;

    // Parse version from VERSION string
    char version[64];
    strcpy(version, VERSION);
    char *token = strtok(version, ".");
    int i = 0;
    while (token != NULL) {
        int val = atoi(token);
        if (i < 3) progCfg.version[i] = (uint8_t)val;
        i++;
        token = strtok(NULL, ".");
    }

    return 1;
}

uint8_t ESPconfig::updateIP() {
    File file = LittleFS.open("/config.json", "r");
    if (!file) return 3;

    String jsonString;
    while (file.available()) jsonString += char(file.read());
    file.close();

    JsonDocument doc;
    if (deserializeJson(doc, jsonString)) return 4;

    for (int i = 0; i < 4; i++) {
        doc["ipAddress"][i] = wifiCfg.ips[i];
    }

    File out = LittleFS.open("/config.json", "w");
    if (!out) return 3;
    serializeJson(doc, out);
    out.close();
    return 1;
}

uint8_t ESPconfig::saveConfig() {
    File file = LittleFS.open("/config.json", "r");
    if (!file) return 3;

    String jsonString;
    while (file.available()) jsonString += char(file.read());
    file.close();

    JsonDocument doc;
    if (deserializeJson(doc, jsonString)) return 4;

    for (int i = 0; i < 4; i++) {
        doc["ipAddress"][i] = wifiCfg.ips[i];
    }
    doc["j1939SA"]       = canCfg.j1939SA;
    doc["canTxFreq"]     = canCfg.txFreq;
    doc["magDeclination"] = imuData.magDeclination;
    doc["headingOffset"]  = imuData.headingOffset;
    doc["reverseHeading"] = imuData.reverseHeading;
    doc["bnoAddress"]    = i2cDefs.BNO_ADDRESS;

    File out = LittleFS.open("/config.json", "w");
    if (!out) return 3;
    serializeJson(doc, out);
    out.close();
    return 1;
}
