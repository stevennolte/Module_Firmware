#include "Arduino.h"
#include "configLoad.h"



ConfigLoad::ConfigLoad(ProgramData_t &data) : data(data) {}


bool ConfigLoad::begin(){
    if (!LittleFS.begin(true)) {
        Serial.println("An error has occurred while mounting LittleFS");
        return false;
      }
      Serial.println("LittleFS mounted successfully");
      File file = LittleFS.open("/config.json", "r");
      if (!file) {
        Serial.println("Failed to open file for reading");
        return false;
      }

      String jsonString;
      while (file.available()) {
          jsonString += char(file.read());
      }
      file.close();
      
      // Print the JSON string to verify
      Serial.println(jsonString);

      
      DynamicJsonDocument doc(capacity);

      // Parse the JSON string
      DeserializationError error = deserializeJson(doc, jsonString);
      if (error) {
          Serial.print("Failed to parse JSON: ");
          Serial.println(error.c_str());
          return false;
      }
      // Access the JSON data
      
      strlcpy(data.sketchConfig, doc["Config"],sizeof(data.sketchConfig)); 
      Serial.print("Sketch Config: ");
      Serial.println(data.sketchConfig);
      data.ips[0] = int(doc["ip1"]);
      data.ips[1] = int(doc["ip2"]);
      data.ips[2] = int(doc["ip3"]);
      data.ips[3] = int(doc["ip4"]);
      data.serverAddress = int(doc["serverAdr"]);
      data.serverPort = int(doc["serverPort"]);
      return true;
}

bool ConfigLoad::updateConfig(){
      File file = LittleFS.open("/config.json", "w");
      if (!file) {
        Serial.println("Failed to open file for writing");
        return false;
      }
      DynamicJsonDocument doc(capacity);
      doc["Config"] = data.sketchConfig;
      doc["ip1"] = data.ips[0];
      doc["ip2"] = data.ips[1];
      doc["ip3"] = data.ips[2];
      doc["ip4"] = data.ips[3];
      doc["serverAdr"] = data.serverAddress;
      doc["serverPort"] = data.serverPort;
      // Parse the JSON string
      
      
      if (serializeJson(doc, file) == 0) {
        Serial.println(F("Failed to write to file"));
      }

      // Close the file
      file.close();
      return true;
    }