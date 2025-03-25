#include "ESPconfig.h"

ESPconfig::ESPconfig() : progCfg(), wifiCfg(), otaCfg() {}

uint8_t ESPconfig::getStrapping(){
    pinMode(4, INPUT);
    uint32_t measurement = analogReadMilliVolts(4);
    //TODO: add strapping logic
    return 1;
}

uint8_t ESPconfig::loadConfig(){
    
    wifiCfg.ips[0] = 192;
    wifiCfg.ips[1] = 168;
    wifiCfg.ips[2] = 5;
    wifiCfg.ips[3] = 11;
    if (!LittleFS.begin(true)){
        return 2;
    }
    File file = LittleFS.open("/config.json","r");
    if (!file) {
        return 3;
    }
    String jsonString;
    while (file.available()){
        jsonString += char(file.read());
    }
    file.close();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    if (error){
        return 4;
    }
#pragma region NAME_VERIFICATION
    // Verify the name of the program, config, and board matches
    strlcpy(progCfg.name, doc["Name"],sizeof(progCfg.name));
    // progCfg.name = doc["Name"];
    
    if (doc["Name"] != NAME){
        return 5;
    }
#pragma endregion

    for (int i = 0; i < 4; i++){
        wifiCfg.ips[i] = uint8_t(doc["ipAddress"][i]);
    }
    // for (int i = 0; i < 4; i++){
    //     wifiCfg.ssids[i] = doc["ssids"][i];
    //     wifiCfg.passwords[i] = doc["passwords"][i];
    //     // strlcpy(wifiCfg.ssids[i],doc["ssids"][i],sizeof(wifiCfg.ssids[i]));
    //     // strlcpy(wifiCfg.passwords[i],doc["passwords"][i],sizeof(wifiCfg.passwords[i]));
    // }
    char version[64];
    strcpy(version, VERSION);
    char *token = strtok(version, ".");
    int i = 0;
    while (token != NULL) {
        int intValue = atoi(token);
        switch (i){
        case 0:
            progCfg.version[0] = intValue;
            break;
        case 1:
            progCfg.version[1] = intValue;
            break;
        case 2:
            progCfg.version[2] = intValue;
            break;
        }
        i++;
        token = strtok(NULL, ".");
    }
    // i2cDefs.ADS_ADDRESS = uint8_t(doc["adsAddress"]);
    
    return 1;
}

uint8_t ESPconfig::updateIP(){
    File file = LittleFS.open("/config.json","r");
    if (!file) {
        return 3;
    }
    String jsonString;
    while (file.available()){
        jsonString += char(file.read());
    }
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    if (error){
        return 4;
    }
    for (int i = 0; i < 4; i++){
        doc["ipAddress"][i] = wifiCfg.ips[i];
    }
    if (serializeJson(doc, file) == 0) {
        Serial.println(F("Failed to write to file"));
      }

      // Close the file
      file.close();
      return true;
}

uint8_t ESPconfig::updateServer(){
    File file = LittleFS.open("/config.json","r");
    if (!file) {
        return 3;
    }
    String jsonString;
    while (file.available()){
        jsonString += char(file.read());
    }
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    if (error){
        return 4;
    }
    doc["serverAdr"] = otaCfg.ipAddr;
    doc["serverPort"] = otaCfg.port;
    if (serializeJson(doc, file) == 0) {
        Serial.println(F("Failed to write to file"));
      }

      // Close the file
      file.close();
      return true;
}
// TODO: Config Update Function

// TODO: Config Update Function