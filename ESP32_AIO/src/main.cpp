/**
 * @file main.cpp
 * @brief ESP32 All-In-One Agricultural Controller Main Application
 * 
 * This file contains the main application logic for the ESP32-AIO agricultural controller.
 * It manages GPS, steering control, wireless communication, web interface, and various
 * hardware peripherals for precision agriculture applications.
 * 
 * @author Steve Nolte
 * @date 2025
 * @version 1.0
 * 
 * Features:
 * - GPS/IMU integration with UM982 and BNO08x
 * - Wheel Angle Sensor (WAS) support (wired/wireless)
 * - PID-based steering control
 * - Web-based configuration interface
 * - Serial command interface
 * - Recovery mode for field servicing
 * - Power cycle detection using RTC memory
 * - UDP communication for AgOpen GPS integration
 */

#include <Arduino.h>
#include "nvs_flash.h"
#include "ESPdata.h"
#include "ESPWifi.h"
#include "myLED.h"
#include "Wire.h"
#include "ESPudp.h"
// #include "Adafruit_MCP23X17.h"
#include "I2C_Manager.h"
// #include "Indicators.h"
#include "MainPower.h"
#include "GPS.h"
#include "HardwareSerial.h"
#include "ESP32OTAPull.h"
#include "ESPsteer.h"
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include "esp_system.h"

//TODO: add wifi connect timer to ap mode

/// @brief I2C bus 0 for primary peripherals
TwoWire twoWire = TwoWire(0);
/// @brief I2C bus 1 for secondary peripherals  
// TwoWire twoWire1 = TwoWire(1);
// /// @brief Hardware serial port 2 for BNO08x IMU communication
HardwareSerial bnoSerial(2);
/// @brief Hardware serial port 1 for GPS communication
HardwareSerial gpsSerial(1);

// Using singleton pattern - single access point for configuration
/// @brief Global configuration and data storage singleton instance
ESPdata& espData = ESPdata::getInstance();
/// @brief I2C Manager singleton instance for centralized bus management
I2CManager& i2cManager = I2CManager::getInstance();
/// @brief GPS/IMU manager with UM982 GPS and BNO08x IMU support
ESPGPS gps(&espData, &gpsSerial, &bnoSerial);  // MCPManager accessed via singleton inside class
/// @brief LED status indicator controller with error state management
MyLED myLED(&espData);
/// @brief Main power control and monitoring system
MainPower mainPower(&espData);  // Will be migrated to use ADSManager
/// @brief WiFi connection and AP mode manager
ESPWifi espWifi(&espData);
/// @brief UDP communication handler for AgOpen GPS protocol
ESPudp espUdp(&espData);
/// @brief Over-The-Air firmware update manager
ESP32OTAPull ota;
/// @brief Async web server for configuration interface (port 80)
AsyncWebServer server(80);
/// @brief WebSocket server for real-time serial monitoring
// AsyncWebSocket ws("/ws");

/// @brief Steering control system with PID feedback and motor control
ESPsteer espSteer(&espData);  // Will be migrated to use ADSManager
/// @brief Simple I2C Manager for centralized bus access

/// @brief Debug variables vector for web interface display
std::vector<String> debugVars;

/// @brief Debug mode flag - true for full debug, false for minimal
bool fullDebugMode = false;

// WebSocket Serial Monitor Variables
// String serialBuffer = "";
// const size_t MAX_SERIAL_BUFFER = 8192;  // 8KB buffer

/**
 * @brief Custom serial print function that also sends to WebSocket clients
 * @param message Message to print to both serial and WebSocket
 */
void webSerial(const String& message) {
    // Print to hardware serial
    // Serial.print(message);
    
    // // Add to buffer for WebSocket clients
    // serialBuffer += message;
    
    // // Limit buffer size
    // if (serialBuffer.length() > MAX_SERIAL_BUFFER) {
    //     serialBuffer = serialBuffer.substring(serialBuffer.length() - MAX_SERIAL_BUFFER);
    // }
    
    // // Send to all connected WebSocket clients
    // ws.textAll(message);
}

// /**
//  * @brief WebSocket event handler for serial monitor
//  */
// void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
//     if (type == WS_EVT_CONNECT) {
//         webSerial("WebSocket client #" + String(client->id()) + " connected from " + client->remoteIP().toString() + "\n");
        
//         // Send welcome message to new WebSocket client
//         String welcomeMsg = "\n===== ESP32-AIO Serial Monitor =====\n";
//         welcomeMsg += "System: " + String(ARDUINO_BOARD) + "\n";
//         welcomeMsg += "MAC: " + WiFi.macAddress() + "\n";
//         welcomeMsg += "IP: " + WiFi.localIP().toString() + "\n";
//         welcomeMsg += "Version: " + String(VERSION) + "\n";
//         welcomeMsg += "Available commands: clear, restart, status\n";
//         welcomeMsg += "====================================\n\n";
//         client->text(welcomeMsg);
        
//         // Send current buffer to newly connected client
//         if (serialBuffer.length() > 0) {
//             client->text("=== ESP32-AIO Serial Monitor Connected ===\n");
//             client->text("System: " + String(NAME) + " v" + String(VERSION) + "\n");
//             client->text("Uptime: " + String(millis()/1000) + " seconds\n");
//             client->text("=========================================\n");
//             client->text(serialBuffer);
//         }
        
//     } else if (type == WS_EVT_DISCONNECT) {
//         webSerial("WebSocket client #" + String(client->id()) + " disconnected\n");
        
//     } else if (type == WS_EVT_DATA) {
//         // Handle incoming WebSocket data (commands from web interface)
//         AwsFrameInfo *info = (AwsFrameInfo*)arg;
//         if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
//             data[len] = 0;  // Null terminate
//             String command = String((char*)data);
//             command.trim();
            
//             // Process web-based serial commands
//             webSerial("Web Command: " + command + "\n");
            
//             // Process commands
//             if (command == "clear") {
//                 serialBuffer = "";
//                 // ws.textAll(""); // Clear all clients
//             } else if (command == "restart") {
//                 webSerial("Restarting ESP32...\n");
//                 delay(1000);
//                 ESP.restart();
//             } else if (command == "status") {
//                 webSerial("System Status:\n");
//                 webSerial("  Uptime: " + String(millis()/1000) + " seconds\n");
//                 webSerial("  Free Heap: " + String(ESP.getFreeHeap()) + " bytes\n");
//                 webSerial("  WiFi: " + String(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected") + "\n");
//                 webSerial("  Program State: " + String(espData.program.state) + "\n");
//             }
//         }
//     }
// }

// Function declarations
/**
 * @brief Update debug variables for normal operation mode
 */
void updateDebugVars();

/**
 * @brief Update debug variables for recovery mode
 */
void updateRecoveryDebugVars();

/**
 * @brief Initialize and start recovery mode with minimal functionality
 */
void recoveryBoot();

/**
 * @brief Initialize and start normal operation mode with full functionality
 */
void normalboot();

/**
 * @brief Initializes I2C bus and detects connected hardware components
 * 
 * @details This function configures the I2C interface and scans for essential hardware:
 *          - Sets up I2C pins (SDA/SCL) from configuration
 *          - Initializes the Wire library for I2C communication
 *          - Scans for MCP23017 I/O expander at address 0x20
 *          - Scans for ADS1115 ADC at address 0x48
 *          - Updates component state flags in espData structure
 * 
 * @return true if all required hardware components are detected
 * @return false if any essential component is missing or I2C setup fails
 * 
 * @note Both MCP23017 and ADS1115 are required for normal operation
 * @see normalboot(), espData.program.mcpState, espData.program.adsState
 */
bool I2Csetup(){
  if(!twoWire.setPins(espData.pins.SDA_PIN, espData.pins.SCL_PIN)){
    webSerial("Wire failed to set pins\n");
    return false;
  }
 
  if(!twoWire.begin()){
    webSerial("Wire failed to begin\n");
    return false;
  }
  byte error, address;
  int nDevices;

  webSerial("Scanning...\n");

  twoWire.beginTransmission(0x20);
  error = twoWire.endTransmission();
  if (error == 0)
  {
    webSerial("MCP23017 found at address 0x20  !\n");
    espData.program.mcpState = 1;
  }
  else 
  {
    webSerial("MCP23017 not found at address 0x20\n");
    espData.program.mcpState = 2;
  }
  
  twoWire.beginTransmission(0x48);
  error = twoWire.endTransmission();
  if (error == 0)
  {
    webSerial("ADS1115 found at address 0x48  !\n");
    espData.program.adsState = 1;
  }
  else 
  {
    webSerial("ADS1115 not found at address 0x48\n");
    espData.program.adsState = 2;
  }
  if (espData.program.mcpState == 2 || espData.program.adsState == 2){
    return false;
  } else {
    return true;
  }
  
}

#pragma region OTA
// const char *errtext(int code)
// {
// 	switch(code)
// 	{
// 		case ESP32OTAPull::UPDATE_AVAILABLE:
// 			return "An update is available but wasn't installed";
// 		case ESP32OTAPull::NO_UPDATE_PROFILE_FOUND:
// 			return "No profile matches";
// 		case ESP32OTAPull::NO_UPDATE_AVAILABLE:
// 			return "Profile matched, but update not applicable";
// 		case ESP32OTAPull::UPDATE_OK:
// 			return "An update was done, but no reboot";
// 		case ESP32OTAPull::HTTP_FAILED:
// 			return "HTTP GET failure";
// 		case ESP32OTAPull::WRITE_ERROR:
// 			return "Write error";
// 		case ESP32OTAPull::JSON_PROBLEM:
// 			return "Invalid JSON";
// 		case ESP32OTAPull::OTA_UPDATE_FAIL:
// 			return "Update fail (no OTA partition?)";
// 		default:
// 			if (code > 0)
// 				return "Unexpected HTTP response code";
// 			break;
// 	}
// 	return "Unknown error";
// }

// void OtaPullCallback(int offset, int totallength)
// {
// 	Serial.printf("Updating %d of %d (%02d%%)...\r", offset, totallength, 100 * offset / totallength);
// }

// void softwareUpdate(){
//   char basePath[] = "/%s/Releases/OTA_Config.json";
//   char CONFIG_URL[150];
//   sprintf(CONFIG_URL, basePath, NAME);
//   Serial.println(CONFIG_URL);
//   char SERVER[150];
//   sprintf(SERVER, "http://%d.%d.%d.%d:%d",espData.wifiCfg.ips[0],espData.wifiCfg.ips[1],espData.wifiCfg.ips[2],espData.otaCfg.ipAddr,espData.otaCfg.port);
//   Serial.print("CONFIG_URL: ");
//   Serial.println(CONFIG_URL);
//   Serial.print("SERVER: ");
//   Serial.println(SERVER);
  
//   ota.SetConfig(NAME);
//   ota.SetCallback(OtaPullCallback);
  
//   Serial.printf("We are running version %s of the sketch, Board='%s', Device='%s', IP='%s \n", VERSION, ARDUINO_BOARD, WiFi.macAddress().c_str(),(String)(WiFi.localIP()[3]));
//   Serial.println();
//   // Serial.printf("Checking %s to see if an update is available...\n", CONFIG_URL);
//   Serial.println();
//   int ret = ota.CheckForOTAUpdate(SERVER, CONFIG_URL, VERSION);
//   Serial.printf("CheckForOTAUpdate returned %d (%s)\n\n", ret, errtext(ret));
// }

#pragma endregion

#pragma region Webserver

// Function to serve the file list as JSON
void handleFileList(AsyncWebServerRequest *request) {
  JsonDocument doc;
  JsonArray array = doc.to<JsonArray>();

  // Open LittleFS root directory and list files
  File root = LittleFS.open("/");
  File file = root.openNextFile();
  
  while (file) {
    JsonObject fileObject = array.add<JsonObject>();
    fileObject["name"] = String(file.name());
    fileObject["size"] = file.size();
    file = root.openNextFile();
  }

  String jsonResponse;
  serializeJson(doc, jsonResponse);
  request->send(200, "application/json", jsonResponse);
  Serial.println("Sent File List");
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

void handleWASzero(AsyncWebServerRequest *request) {
  // espSteer.was.zeroSteerAngle();
  espData.steer.wasZeroAngle = espData.steer.absAngle;
  Serial.println(espData.steer.absAngle);
  Serial.println(espData.steer.wasZeroAngle);
  Serial.println("WAS zeroed");
  uint8_t res = espData.saveWASzero();
  if (res == 1){
    request->send(200, "text/plain", "WAS zeroed and saved to config.json");
  } else {
    request->send(200, "text/plain", "WAS zeroed");
  }
}

void handleFirmwareUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
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


/**
 * @brief Updates the debug variables vector with current system status information
 * 
 * @details This function clears and repopulates the debugVars vector with real-time
 *          system information including:
 *          - Program name, version, and uptime
 *          - Memory usage and heap information
 *          - WiFi connection status and IP address
 *          - Hardware component states (MCP23017, ADS1115, IMU)
 *          - GPS data (position, satellites, fix quality)
 *          - Steering system status and configuration
 *          - Power management information
 *          Used for web interface diagnostics and system monitoring
 */
void updateDebugVars() {
  // Check available heap before proceeding
  uint32_t freeHeap = ESP.getFreeHeap();
  if (freeHeap < 30000) {
    webSerial("Skipping debug update - low memory: " + String(freeHeap) + " bytes\n");
    debugVars.clear();
    debugVars.push_back("Low Memory - Debug Disabled");
    debugVars.push_back("Free Heap: " + String(freeHeap) + " bytes");
    return;
  }
  
  // Reserve capacity to avoid multiple reallocations
  debugVars.clear();
  
  // Check if minimal debug mode is enabled
  if (!fullDebugMode) {
    // Minimal debug mode - only show basic info
    Serial.println("DEBUG: Using minimal debug mode");
    debugVars.reserve(3);
    debugVars.push_back("Program: " + String(NAME));
    debugVars.push_back("Timestamp since boot [s]: " + String((float)(millis())/1000.0, 2));
    return;
  }
  
  // Full debug mode - show all variables
  Serial.println("DEBUG: Using full debug mode");
  debugVars.reserve(40); // Reduced from 80 to minimize memory usage
  
  webSerial("Updating normal mode debug variables...\n");
  
  // Basic system info
  debugVars.push_back("Program: " + String(NAME));
  debugVars.push_back("Timestamp since boot [s]: " + String((float)(millis())/1000.0, 2));
  debugVars.push_back("Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
  debugVars.push_back("Version: " + String(VERSION));
  
  // WiFi info  
  debugVars.push_back("Wifi SSID: " + WiFi.SSID());
  debugVars.push_back("IP Address: " + String(espData.wifi.ips[0])+"."+String(espData.wifi.ips[1])+"."+String(espData.wifi.ips[2])+"."+String(espData.wifi.ips[3]));
  debugVars.push_back("Wifi State: " + String(espData.wifi.state) + " (" + (espData.wifi.state == 1 ? "Connected" : "Disconnected") + ")");
  debugVars.push_back("Program State: " + String(espData.program.state));
  
  // I2C device status
  debugVars.push_back("MCP23017 State: " + String(espData.program.mcpState));
  debugVars.push_back("ADS1115 State: " + String(espData.program.adsState));
  debugVars.push_back("MCP Ready: " + String(i2cManager.isMcpReady() ? "Yes" : "No"));
  debugVars.push_back("ADS Ready: " + String(i2cManager.isAdsReady() ? "Yes" : "No"));
  
  // GPS data - simplified to reduce stack usage
  debugVars.push_back("GPS Data: ");
  debugVars.push_back("..GPS Mode: " + String(espData.gps.ntripPandaMode ? "PANDA Generation" : "Raw NMEA"));
  debugVars.push_back("..Fix Time: " + String(espData.gps.fixTime));
  debugVars.push_back("..Fix Quality: " + String(espData.gps.fixQuality));
  debugVars.push_back("..Satellites: " + String(espData.gps.numSats));
  debugVars.push_back("..Latitude: " + String(espData.gps.latitude));
  debugVars.push_back("..Longitude: " + String(espData.gps.longitude));
  debugVars.push_back("..Altitude: " + String(espData.gps.altitude));
  debugVars.push_back("..Speed: " + String(espData.gps.speedKnots));
  debugVars.push_back("..Heading: " + String(espData.gps.vtgHeading));
  debugVars.push_back("..HDOP: " + String(espData.gps.HDOP));
  debugVars.push_back("..Last PANDA: " + String(espData.gps.nmea));
  debugVars.push_back("..Message Counts");
  debugVars.push_back("....GGA: " + String(espData.gps.ggaMessageCount));
  debugVars.push_back("....RMC: " + String(espData.gps.rmcMessageCount));
  debugVars.push_back("....GSA: " + String(espData.gps.gsaMessageCount));
  debugVars.push_back("....VTG: " + String(espData.gps.vtgMessageCount));
  debugVars.push_back("....Other:" + String(espData.gps.otherMessageCount)); 
  debugVars.push_back("....PANDA Sent: " + String(espData.gps.pandaMessageCount));
  debugVars.push_back("..Last PANDA Time: " + String((float)(espData.gps.lastPandaTime)/1000.0, 2) + "s"); 
  debugVars.push_back("..IMU State: " + String(espData.gps.imuState));
  debugVars.push_back("....IMU Heading: " + String(espData.gps.imuHeading));
  debugVars.push_back("....IMU Pitch: " + String(espData.gps.imuPitch));
  debugVars.push_back("....IMU Roll: " + String(espData.gps.imuRoll));
  // Steering data - essential only
  debugVars.push_back("Steer Data: ");
  debugVars.push_back("..Target Angle: " + String(espData.steer.targetSteerAngle, 2));
  debugVars.push_back("..Actual Angle: " + String(espData.steer.actSteerAngle, 2));
  debugVars.push_back("..Raw ADS: " + String(espData.steer.rawADS));
  debugVars.push_back("..PWM Command: " + String(espData.steer.pwmCmd));
  debugVars.push_back("..Switch State: " + String(espData.steer.switchState));
  
  // ADS readings - simplified
  debugVars.push_back("ADS1115 INFO");
  for(int i = 0; i < 4; i++) {
    debugVars.push_back("..Ch" + String(i) + ": " + String(i2cManager.getVoltage(i), 3) + "V");
  }
  
}

/**
 * @brief Updates debug variables specifically for recovery mode display
 * 
 * @details This function populates the debugVars vector with recovery mode
 *          specific information including:
 *          - Recovery mode indicators and timestamps
 *          - Hardware specifications (chip model, CPU frequency, flash)
 *          - Recovery WiFi AP configuration details
 *          - System status and reset reason information
 *          - Memory usage statistics including PSRAM
 *          Used for diagnostics when the system is in recovery mode
 */
void updateRecoveryDebugVars() {
  debugVars.clear(); // Clear the list to update it dynamically
  debugVars.push_back("=== RECOVERY MODE ===");
  debugVars.push_back("Program: " + String(NAME));
  debugVars.push_back("Recovery Boot Timestamp [s]: " + String((float)(millis())/1000.0));
  debugVars.push_back("Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
  debugVars.push_back("Used PSRAM: " + String(ESP.getPsramSize() - ESP.getFreePsram()) + " bytes");
  debugVars.push_back("Version: " + String(VERSION));
  debugVars.push_back("Chip Model: " + String(ESP.getChipModel()));
  debugVars.push_back("Chip Revision: " + String(ESP.getChipRevision()));
  debugVars.push_back("CPU Frequency: " + String(ESP.getCpuFreqMHz()) + " MHz");
  debugVars.push_back("Flash Size: " + String(ESP.getFlashChipSize()) + " bytes");
  debugVars.push_back("Flash Speed: " + String(ESP.getFlashChipSpeed()) + " Hz");
  debugVars.push_back("MAC Address: " + WiFi.macAddress());
  debugVars.push_back("");
  debugVars.push_back("=== RECOVERY WIFI ===");
  debugVars.push_back("AP SSID: " + String(NAME) + "_RECOVERY");
  debugVars.push_back("AP Password: recovery123");
  debugVars.push_back("AP IP: 192.168.4.1");
  debugVars.push_back("WiFi Mode: AP (Access Point)");
  debugVars.push_back("Connected Clients: " + String(WiFi.softAPgetStationNum()));
  debugVars.push_back("");
  debugVars.push_back("=== SYSTEM STATUS ===");
  debugVars.push_back("Config Load Result: " + String(espData.program.confRes));
  debugVars.push_back("Program State: " + String(espData.program.state));
  debugVars.push_back("Reset Reason Core 0: " + String(esp_reset_reason()));
  debugVars.push_back("Reset Reason Core 1: " + String(esp_reset_reason()));
  debugVars.push_back("Wake Reason: " + String(esp_sleep_get_wakeup_cause()));
  debugVars.push_back("");
  debugVars.push_back("=== HARDWARE STATUS ===");
  debugVars.push_back("MCP23017 State: " + String(espData.program.mcpState));
  debugVars.push_back("ADS1115 State: " + String(espData.program.adsState));
  debugVars.push_back("IMU State: " + String(espData.gps.imuState));
  debugVars.push_back("I2C0 SDA/SCL: " + String(SDA) + "/" + String(SCL));
  debugVars.push_back("I2C0 SDA/SCL: " + String(espData.pins.SDA_PIN) + "/" + String(espData.pins.SCL_PIN));
  debugVars.push_back("");
  debugVars.push_back("=== FILE SYSTEM ===");
  debugVars.push_back("LittleFS Total: " + String(LittleFS.totalBytes()) + " bytes");
  debugVars.push_back("LittleFS Used: " + String(LittleFS.usedBytes()) + " bytes");
  debugVars.push_back("LittleFS Free: " + String(LittleFS.totalBytes() - LittleFS.usedBytes()) + " bytes");
  debugVars.push_back("");
  debugVars.push_back("=== RECOVERY ACTIONS ===");
  debugVars.push_back("• Upload new firmware via /update");
  debugVars.push_back("• Download/upload config files via /upload");
  debugVars.push_back("• Factory reset via /factoryReset");
  debugVars.push_back("• Force normal boot via /forceNormalBoot");
  debugVars.push_back("• Reboot system via /reboot");
}

// Function to serve the debug variables as JSON
void handleDebugVars(AsyncWebServerRequest *request) {
  webSerial("Normal mode debug handler called\n");
  updateDebugVars();  // Update the debug variables just before sending
  JsonDocument doc;
  JsonArray array = doc.to<JsonArray>();
  
  for (const auto& var : debugVars) {
    array.add(var);
  }
  
  String jsonResponse;
  serializeJson(doc, jsonResponse);
  webSerial("Sending " + String(debugVars.size()) + " debug variables\n");
  request->send(200, "application/json", jsonResponse);
}


// Function to serve the file list as JSON
// void handleFileList(AsyncWebServerRequest *request) {
//   DynamicJsonDocument doc(1024);
//   JsonArray array = doc.to<JsonArray>();

//   // Open LittleFS root directory and list files
//   File root = LittleFS.open("/");
//   File file = root.openNextFile();
  
//   while (file) {
//     JsonObject fileObject = array.createNestedObject();
//     fileObject["name"] = String(file.name());
//     fileObject["size"] = file.size();
//     file = root.openNextFile();
//   }

//   String jsonResponse;
//   serializeJson(doc, jsonResponse);
//   request->send(200, "application/json", jsonResponse);
//   Serial.println("Sent File List");
// }

// Reboot handler
void handleReboot(AsyncWebServerRequest *request) {
  request->send(200, "text/plain", "Rebooting...");
  delay(100); // Give some time for the response to be sent
  ESP.restart();
}

// Settings page handler
void handleSettingsPage(AsyncWebServerRequest *request) {
  Serial.println("=== SETTINGS PAGE REQUEST ===");
  Serial.println("Client IP: " + request->client()->remoteIP().toString());
  Serial.println("Request Method: " + String(request->methodToString()));
  Serial.println("Request URL: " + request->url());
  Serial.println("Serving settings page HTML...");
  
  String html = "<!DOCTYPE html><html><head><title>ESP32 Settings</title>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 20px; background-color: #f0f0f0; }";
  html += ".container { max-width: 800px; margin: 0 auto; background-color: white; padding: 20px; border-radius: 10px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }";
  html += ".header { text-align: center; color: #333; margin-bottom: 30px; }";
  html += ".section { margin-bottom: 25px; padding: 15px; border: 1px solid #ddd; border-radius: 8px; background-color: #f9f9f9; }";
  html += ".section-title { font-weight: bold; color: #0066cc; margin-bottom: 15px; font-size: 18px; }";
  html += ".form-group { margin-bottom: 15px; display: flex; align-items: center; }";
  html += ".form-group label { min-width: 200px; font-weight: bold; }";
  html += ".form-group input, .form-group select { flex: 1; padding: 8px; border: 1px solid #ccc; border-radius: 4px; }";
  html += ".button { background-color: #0066cc; color: white; padding: 12px 24px; border: none; border-radius: 5px; cursor: pointer; margin: 10px 5px; font-size: 16px; }";
  html += ".button:hover { background-color: #0052a3; }";
  html += ".button.danger { background-color: #cc0000; }";
  html += ".button.danger:hover { background-color: #a30000; }";
  html += ".status { margin-top: 20px; padding: 10px; border-radius: 5px; text-align: center; }";
  html += ".status.success { background-color: #d4edda; color: #155724; border: 1px solid #c3e6cb; }";
  html += ".status.error { background-color: #f8d7da; color: #721c24; border: 1px solid #f5c6cb; }";
  html += "@media (max-width: 600px) { .form-group { flex-direction: column; align-items: flex-start; } .form-group label { min-width: auto; margin-bottom: 5px; } }";
  html += "</style></head><body>";
  
  html += "<div class='container'>";
  html += "<div class='header'><h1>ESP32-AIO Settings</h1></div>";
  
  // Network Settings Section
  html += "<div class='section'>";
  html += "<div class='section-title'>Network Configuration</div>";
  html += "<div class='form-group'><label>IP Address:</label><input type='text' id='ipAddress' placeholder='192.168.1.100'></div>";
  html += "<div class='form-group'><label>WiFi SSID 1:</label><input type='text' id='ssid1' placeholder='Network Name'></div>";
  html += "<div class='form-group'><label>WiFi Password 1:</label><input type='password' id='password1' placeholder='Network Password'></div>";
  html += "<div class='form-group'><label>WiFi SSID 2:</label><input type='text' id='ssid2' placeholder='Backup Network'></div>";
  html += "<div class='form-group'><label>WiFi Password 2:</label><input type='password' id='password2' placeholder='Backup Password'></div>";
  html += "</div>";
  
  // Steering Settings Section
  html += "<div class='section'>";
  html += "<div class='section-title'>Steering Configuration</div>";
  html += "<div class='form-group'><label>PID Gain (Kp):</label><input type='number' id='kp' step='0.1' min='0' max='255'></div>";
  html += "<div class='form-group'><label>High PWM:</label><input type='number' id='highPWM' min='0' max='255'></div>";
  html += "<div class='form-group'><label>Low PWM:</label><input type='number' id='lowPWM' min='0' max='255'></div>";
  html += "<div class='form-group'><label>Min PWM:</label><input type='number' id='minPWM' min='0' max='255'></div>";
  html += "<div class='form-group'><label>Counts Per Degree:</label><input type='number' id='countsPerDeg' step='0.1' min='0'></div>";
  html += "<div class='form-group'><label>WAS Offset:</label><input type='number' id='wasOffset' step='1' min='-2048' max='2048'></div>";
  html += "<div class='form-group'><label>PID Input Filter:</label><input type='number' id='pidInputFilt' step='0.01' min='0' max='1'></div>";
  html += "<div class='form-group'><label>PID Output Filter:</label><input type='number' id='pidOutputFilt' step='0.01' min='0' max='1'></div>";
  html += "<div class='form-group'><label>Use ADS1115:</label><select id='useADS'><option value='1'>Yes</option><option value='0'>No</option></select></div>";
  html += "</div>";
  
  // System Settings Section
  html += "<div class='section'>";
  html += "<div class='section-title'>System Configuration</div>";
  html += "<div class='form-group'><label>GPS Source:</label><select id='gpsSource'><option value='0'>On-board GPS</option><option value='1'>Wireless GPS</option></select></div>";
  html += "<div class='form-group'><label>GPS Output Mode:</label><select id='pandaMode'><option value='0'>Raw NMEA Forward</option><option value='1'>PANDA Generation</option></select></div>";
  html += "<div class='form-group'><label>WAS Source:</label><select id='wasSource'><option value='0'>Wired WAS</option><option value='1'>Wireless WAS</option></select></div>";
  html += "<div class='form-group'><label>LED Brightness (0-255):</label><input type='number' id='ledBrightness' min='0' max='255'></div>";
  html += "<div class='form-group'><label>ADS Address:</label><select id='adsAddress'><option value='0x48'>0x48</option><option value='0x49'>0x49</option><option value='0x4A'>0x4A</option><option value='0x4B'>0x4B</option></select></div>";
  html += "</div>";
  
  // Action buttons
  html += "<div style='text-align: center; margin-top: 30px;'>";
  html += "<button class='button' onclick='loadSettings()'>Load Current Settings</button>";
  html += "<button class='button' onclick='saveSettings()'>Save Settings</button>";
  html += "<button class='button' onclick='resetToDefaults()'>Reset to Defaults</button>";
  html += "<button class='button danger' onclick='window.history.back()'>Back</button>";
  html += "</div>";
  
  html += "<div id='status' class='status' style='display: none;'></div>";
  html += "</div>";
  
  // JavaScript
  html += "<script>";
  html += "function showStatus(message, isError) {";
  html += "  const status = document.getElementById('status');";
  html += "  status.textContent = message;";
  html += "  status.className = 'status ' + (isError ? 'error' : 'success');";
  html += "  status.style.display = 'block';";
  html += "  setTimeout(() => status.style.display = 'none', 5000);";
  html += "}";
  
  html += "function loadSettings() {";
  html += "  console.log('Loading settings from server...');";
  html += "  fetch('/getSettings')";
  html += "    .then(response => {";
  html += "      console.log('Settings response status:', response.status);";
  html += "      if (!response.ok) throw new Error('Network response was not ok');";
  html += "      return response.json();";
  html += "    })";
  html += "    .then(data => {";
  html += "      console.log('Settings data received:', data);";
  html += "      document.getElementById('ipAddress').value = data.ip0 + '.' + data.ip1 + '.' + data.ip2 + '.' + data.ip3;";
  html += "      document.getElementById('kp').value = data.kp;";
  html += "      document.getElementById('highPWM').value = data.highPWM;";
  html += "      document.getElementById('lowPWM').value = data.lowPWM;";
  html += "      document.getElementById('minPWM').value = data.minPWM;";
  html += "      document.getElementById('countsPerDeg').value = data.countsPerDeg;";
  html += "      document.getElementById('wasOffset').value = data.wasOffset;";
  html += "      document.getElementById('pidInputFilt').value = data.pidInputFilt;";
  html += "      document.getElementById('pidOutputFilt').value = data.pidOutputFilt;";
  html += "      document.getElementById('useADS').value = data.useADS;";
  html += "      document.getElementById('gpsSource').value = data.gpsSource;";
  html += "      document.getElementById('pandaMode').value = data.pandaMode;";
  html += "      document.getElementById('wasSource').value = data.wasSource;";
  html += "      document.getElementById('ledBrightness').value = data.ledBrightness;";
  html += "      document.getElementById('adsAddress').value = data.adsAddress;";
  html += "      showStatus('Settings loaded successfully', false);";
  html += "    })";
  html += "    .catch(error => { console.log('Settings load error:', error); showStatus('Failed to load settings: ' + error, true); });";
  html += "}";
  
  html += "function saveSettings() {";
  html += "  const ipParts = document.getElementById('ipAddress').value.split('.');";
  html += "  if (ipParts.length !== 4) { showStatus('Invalid IP address format', true); return; }";
  html += "  const formData = new FormData();";
  html += "  formData.append('ip0', parseInt(ipParts[0]) || 192);";
  html += "  formData.append('ip1', parseInt(ipParts[1]) || 168);";
  html += "  formData.append('ip2', parseInt(ipParts[2]) || 1);";
  html += "  formData.append('ip3', parseInt(ipParts[3]) || 100);";
  html += "  formData.append('kp', parseFloat(document.getElementById('kp').value) || 50);";
  html += "  formData.append('highPWM', parseInt(document.getElementById('highPWM').value) || 255);";
  html += "  formData.append('lowPWM', parseInt(document.getElementById('lowPWM').value) || 10);";
  html += "  formData.append('minPWM', parseInt(document.getElementById('minPWM').value) || 5);";
  html += "  formData.append('countsPerDeg', parseFloat(document.getElementById('countsPerDeg').value) || 10);";
  html += "  formData.append('wasOffset', parseInt(document.getElementById('wasOffset').value) || 0);";
  html += "  formData.append('pidInputFilt', parseFloat(document.getElementById('pidInputFilt').value) || 0.1);";
  html += "  formData.append('pidOutputFilt', parseFloat(document.getElementById('pidOutputFilt').value) || 0.1);";
  html += "  formData.append('useADS', document.getElementById('useADS').value);";
  html += "  formData.append('gpsSource', document.getElementById('gpsSource').value);";
  html += "  formData.append('pandaMode', document.getElementById('pandaMode').value);";
  html += "  formData.append('wasSource', document.getElementById('wasSource').value);";
  html += "  formData.append('ledBrightness', parseInt(document.getElementById('ledBrightness').value) || 100);";
  html += "  fetch('/saveSettings', {";
  html += "    method: 'POST',";
  html += "    body: formData";
  html += "  })";
  html += "    .then(response => response.text())";
  html += "    .then(data => showStatus(data, false))";
  html += "    .catch(error => showStatus('Failed to save settings: ' + error, true));";
  html += "}";
  
  html += "function resetToDefaults() {";
  html += "  if (confirm('Reset all settings to defaults? This will require a reboot.')) {";
  html += "    document.getElementById('ipAddress').value = '192.168.1.100';";
  html += "    document.getElementById('kp').value = '50';";
  html += "    document.getElementById('highPWM').value = '255';";
  html += "    document.getElementById('lowPWM').value = '10';";
  html += "    document.getElementById('minPWM').value = '5';";
  html += "    document.getElementById('countsPerDeg').value = '10';";
  html += "    document.getElementById('wasOffset').value = '0';";
  html += "    document.getElementById('pidInputFilt').value = '0.1';";
  html += "    document.getElementById('pidOutputFilt').value = '0.1';";
  html += "    document.getElementById('useADS').value = '1';";
  html += "    document.getElementById('gpsSource').value = '0';";
  html += "    document.getElementById('pandaMode').value = '1';";
  html += "    document.getElementById('wasSource').value = '0';";
  html += "    document.getElementById('ledBrightness').value = '100';";
  html += "    document.getElementById('adsAddress').value = '0x48';";
  html += "    showStatus('Default values loaded - click Save to apply', false);";
  html += "  }";
  html += "}";
  
  html += "window.onload = loadSettings;";
  html += "</script>";
  html += "</body></html>";
  
  Serial.println("Settings page HTML generated, size: " + String(html.length()) + " bytes");
  request->send(200, "text/html", html);
  Serial.println("Settings page sent successfully");
}

// Get current settings as JSON
void handleGetSettings(AsyncWebServerRequest *request) {
  Serial.println("=== GET SETTINGS REQUEST ===");
  Serial.println("Client IP: " + request->client()->remoteIP().toString());
  Serial.println("Preparing settings JSON response...");
  
  JsonDocument doc;
  
  // Network settings
  doc["ip0"] = espData.wifi.ips[0];
  doc["ip1"] = espData.wifi.ips[1];
  doc["ip2"] = espData.wifi.ips[2];
  doc["ip3"] = espData.wifi.ips[3];
  
  // Steering settings
  doc["kp"] = espData.steer.gainP;
  doc["highPWM"] = espData.steer.highPWM;
  doc["lowPWM"] = espData.steer.lowPWM;
  doc["minPWM"] = espData.steer.minPWM;
  doc["countsPerDeg"] = espData.steer.countsPerDeg;
  doc["wasOffset"] = espData.steer.steerOffset;
  doc["pidInputFilt"] = espData.steer.pidInputFilt;
  doc["pidOutputFilt"] = espData.steer.pidOutputFilt;
  doc["useADS"] = espData.steer.useADS;
  
  // System settings
  doc["gpsSource"] = espData.gps.externalGPS ? 1 : 0;
  doc["wasSource"] = espData.steer.wirelessWAS ? 1 : 0;
  doc["ledBrightness"] = espData.program.ledBrht;
  doc["adsAddress"] = "0x48"; // Default, could be made configurable
  doc["pandaMode"] = espData.gps.ntripPandaMode ? 1 : 0;
  
  String response;
  serializeJson(doc, response);
  
  Serial.println("Settings JSON prepared:");
  Serial.println("  Network: " + String(espData.wifi.ips[0]) + "." + String(espData.wifi.ips[1]) + "." + String(espData.wifi.ips[2]) + "." + String(espData.wifi.ips[3]));
  Serial.println("  Kp: " + String(espData.steer.gainP));
  Serial.println("  High PWM: " + String(espData.steer.highPWM));
  Serial.println("  Low PWM: " + String(espData.steer.lowPWM));
  Serial.println("  Min PWM: " + String(espData.steer.minPWM));
  Serial.println("  Use ADS: " + String(espData.steer.useADS ? "true" : "false"));
  Serial.println("  PANDA Mode: " + String(espData.gps.ntripPandaMode ? "Enabled" : "Disabled"));
  Serial.println("JSON Response size: " + String(response.length()) + " bytes");
  
  request->send(200, "application/json", response);
  Serial.println("Settings JSON sent successfully");
}

// Save settings from JSON POST
void handleSaveSettings(AsyncWebServerRequest *request) {
  Serial.println("=== SAVE SETTINGS REQUEST ===");
  Serial.println("Client IP: " + request->client()->remoteIP().toString());
  Serial.println("Request Method: " + String(request->methodToString()));
  Serial.println("Number of parameters: " + String(request->params()));
  
  // Log all received parameters
  for (int i = 0; i < request->params(); i++) {
    const AsyncWebParameter* p = request->getParam(i);
    Serial.println("Parameter [" + String(i) + "]: " + p->name() + " = " + p->value());
  }
  
  // This will be called when the request has parameters
  // We'll use a simpler approach with URL parameters for now
  
  Serial.println("Processing network settings...");
  // Update network settings if provided
  if (request->hasParam("ip0", true)) {
    int oldValue = espData.wifi.ips[0];
    espData.wifi.ips[0] = request->getParam("ip0", true)->value().toInt();
    Serial.println("  IP0: " + String(oldValue) + " -> " + String(espData.wifi.ips[0]));
  }
  if (request->hasParam("ip1", true)) {
    int oldValue = espData.wifi.ips[1];
    espData.wifi.ips[1] = request->getParam("ip1", true)->value().toInt();
    Serial.println("  IP1: " + String(oldValue) + " -> " + String(espData.wifi.ips[1]));
  }
  if (request->hasParam("ip2", true)) {
    int oldValue = espData.wifi.ips[2];
    espData.wifi.ips[2] = request->getParam("ip2", true)->value().toInt();
    Serial.println("  IP2: " + String(oldValue) + " -> " + String(espData.wifi.ips[2]));
  }
  if (request->hasParam("ip3", true)) {
    int oldValue = espData.wifi.ips[3];
    espData.wifi.ips[3] = request->getParam("ip3", true)->value().toInt();
    Serial.println("  IP3: " + String(oldValue) + " -> " + String(espData.wifi.ips[3]));
  }
  
  Serial.println("Processing steering settings...");
  // Update steering settings if provided
  if (request->hasParam("kp", true)) {
    float oldValue = espData.steer.gainP;
    espData.steer.gainP = request->getParam("kp", true)->value().toFloat();
    Serial.println("  Kp: " + String(oldValue) + " -> " + String(espData.steer.gainP));
  }
  if (request->hasParam("highPWM", true)) {
    int oldValue = espData.steer.highPWM;
    espData.steer.highPWM = request->getParam("highPWM", true)->value().toInt();
    Serial.println("  High PWM: " + String(oldValue) + " -> " + String(espData.steer.highPWM));
  }
  if (request->hasParam("lowPWM", true)) {
    int oldValue = espData.steer.lowPWM;
    espData.steer.lowPWM = request->getParam("lowPWM", true)->value().toInt();
    Serial.println("  Low PWM: " + String(oldValue) + " -> " + String(espData.steer.lowPWM));
  }
  if (request->hasParam("minPWM", true)) {
    int oldValue = espData.steer.minPWM;
    espData.steer.minPWM = request->getParam("minPWM", true)->value().toInt();
    Serial.println("  Min PWM: " + String(oldValue) + " -> " + String(espData.steer.minPWM));
  }
  if (request->hasParam("countsPerDeg", true)) {
    float oldValue = espData.steer.countsPerDeg;
    espData.steer.countsPerDeg = request->getParam("countsPerDeg", true)->value().toFloat();
    Serial.println("  Counts Per Deg: " + String(oldValue) + " -> " + String(espData.steer.countsPerDeg));
  }
  if (request->hasParam("wasOffset", true)) {
    int oldValue = espData.steer.steerOffset;
    espData.steer.steerOffset = request->getParam("wasOffset", true)->value().toInt();
    Serial.println("  WAS Offset: " + String(oldValue) + " -> " + String(espData.steer.steerOffset));
  }
  if (request->hasParam("pidInputFilt", true)) {
    float oldValue = espData.steer.pidInputFilt;
    espData.steer.pidInputFilt = request->getParam("pidInputFilt", true)->value().toFloat();
    Serial.println("  PID Input Filter: " + String(oldValue) + " -> " + String(espData.steer.pidInputFilt));
  }
  if (request->hasParam("pidOutputFilt", true)) {
    float oldValue = espData.steer.pidOutputFilt;
    espData.steer.pidOutputFilt = request->getParam("pidOutputFilt", true)->value().toFloat();
    Serial.println("  PID Output Filter: " + String(oldValue) + " -> " + String(espData.steer.pidOutputFilt));
  }
  if (request->hasParam("useADS", true)) {
    bool oldValue = espData.steer.useADS;
    String rawValue = request->getParam("useADS", true)->value();
    espData.steer.useADS = rawValue.toInt();
    Serial.println("  Use ADS raw value: '" + rawValue + "'");
    Serial.println("  Use ADS: " + String(oldValue ? "true" : "false") + " -> " + String(espData.steer.useADS ? "true" : "false"));
  }
  
  Serial.println("Processing system settings...");
  // Update system settings if provided
  if (request->hasParam("gpsSource", true)) {
    bool oldValue = espData.gps.externalGPS;
    espData.gps.externalGPS = request->getParam("gpsSource", true)->value().toInt();
    Serial.println("  GPS Source: " + String(oldValue ? "Wireless" : "On-board") + " -> " + String(espData.gps.externalGPS ? "Wireless" : "On-board"));
  }
  if (request->hasParam("wasSource", true)) {
    bool oldValue = espData.steer.wirelessWAS;
    espData.steer.wirelessWAS = request->getParam("wasSource", true)->value().toInt();
    Serial.println("  WAS Source: " + String(oldValue ? "Wireless" : "Wired") + " -> " + String(espData.steer.wirelessWAS ? "Wireless" : "Wired"));
  }
  if (request->hasParam("ledBrightness", true)) {
    int oldValue = espData.program.ledBrht;
    espData.program.ledBrht = request->getParam("ledBrightness", true)->value().toInt();
    Serial.println("  LED Brightness: " + String(oldValue) + " -> " + String(espData.program.ledBrht));
    // Apply the new brightness immediately
    myLED.updateBrightness();
    Serial.println("  LED brightness updated immediately");
  }
  if (request->hasParam("pandaMode", true)) {
    bool oldValue = espData.gps.ntripPandaMode;
    espData.gps.ntripPandaMode = request->getParam("pandaMode", true)->value().toInt();
    Serial.println("  PANDA Mode: " + String(oldValue ? "Enabled" : "Disabled") + " -> " + String(espData.gps.ntripPandaMode ? "Enabled" : "Disabled"));
  }
  
  Serial.println("Attempting to save configuration to NVS...");
  // Save to preferences
  bool saveResult = espData.saveConfig();
  
  if (saveResult) {
    Serial.println("Configuration saved successfully to NVS");
    request->send(200, "text/plain", "Settings saved successfully! Some changes may require a reboot.");
  } else {
    Serial.println("ERROR: Failed to save configuration to NVS");
    request->send(500, "text/plain", "Failed to save settings to memory");
  }
  Serial.println("=== SAVE SETTINGS COMPLETE ===");
}

// File download handler
// void handleFileDownload(AsyncWebServerRequest *request) {
//   if (request->hasParam("filename")) {
//     String filename = request->getParam("filename")->value();
//     if (LittleFS.exists("/" + filename)) {
//       request->send(LittleFS, "/" + filename, "application/octet-stream");
//     } else {
//       request->send(404, "text/plain", "File not found");
//     }
//   } else {
//     request->send(400, "text/plain", "Filename not provided");
//   }
// }

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


void handleToggleAPMode(AsyncWebServerRequest *request) {
  static bool apModeState = false;
  apModeState = !apModeState;
  espData.wifi.apMode = apModeState ? 1 : 0;
  Serial.printf("AP Mode State: %s\n", apModeState ? "ON" : "OFF");
  request->send(200, "text/plain", apModeState ? "AP_Mode is ON" : "AP_Mode is OFF");
}



#pragma endregion

#pragma region Buttons
/**
 * @brief Interrupt service routine for handling steering switch activation
 * 
 * @details This ISR is triggered when the steering switch is pressed. It implements
 *          debouncing logic with a 100ms minimum interval between triggers.
 *          Sets the steerSwitch flag and updates the last trigger timestamp.
 *          Must be declared with IRAM_ATTR for interrupt handling on ESP32.
 * 
 * @note This function runs in interrupt context - keep execution time minimal
 * @see handleWorkSwitch(), buttonSetup()
 */
void IRAM_ATTR handleSteerSwitch() {
  unsigned long currentTime = millis();
  if (currentTime - espData.steer.steerSwitchLastTime > 100) {
      espData.steer.steerSwitch = true; // Set the flag
      espData.steer.steerSwitchLastTime = currentTime; // Update the debounce timestamp
    }
}

/**
 * @brief Interrupt service routine for handling work switch activation
 * 
 * @details This ISR is triggered when the work switch is pressed. It implements
 *          debouncing logic with a 100ms minimum interval between triggers.
 *          Sets the workSwitch flag and updates the last trigger timestamp.
 *          Must be declared with IRAM_ATTR for interrupt handling on ESP32.
 * 
 * @note This function runs in interrupt context - keep execution time minimal
 * @see handleSteerSwitch(), buttonSetup()
 */
void IRAM_ATTR handleWorkSwitch() {
  unsigned long currentTime = millis();
  if (currentTime - espData.switches.workSwitchLastTime > 100) {
      espData.switches.workSwitch = true; // Set the flag
      espData.switches.workSwitchLastTime = currentTime; // Update the debounce timestamp
  }
}

/**
 * @brief Configures GPIO pins and interrupt handlers for physical switches
 * 
 * @details This function initializes the hardware button interface by:
 *          - Setting steering and work switch pins as INPUT_PULLUP
 *          - Attaching interrupt handlers for FALLING edge detection
 *          - Enabling hardware debouncing through interrupt configuration
 * 
 * @note Called once during system initialization in setup()
 * @see handleSteerSwitch(), handleWorkSwitch()
 */
void buttonSetup(){
  // Set up the GPIO pins for the buttons
  pinMode(espData.pins.STEER_SWITCH_PIN, INPUT_PULLUP);
  pinMode(espData.pins.WORK_SWITCH_PIN, INPUT_PULLUP);

  // Attach interrupts to the buttons
  attachInterrupt(digitalPinToInterrupt(espData.pins.STEER_SWITCH_PIN), handleSteerSwitch, FALLING);
  attachInterrupt(digitalPinToInterrupt(espData.pins.WORK_SWITCH_PIN), handleWorkSwitch, FALLING);
}
#pragma endregion


/**
 * @brief Handles runtime serial commands for system control and diagnostics
 * 
 * @details This function processes serial input commands for remote system management:
 *          - "help" - Display available commands and usage
 *          - "status" - Show current system status and debug information
 *          - "reboot" - Perform immediate system restart
 *          - "recovery" - Set recovery mode flag and restart
 *          - "normal" - Clear recovery mode flag and restart
 * 
 *          Uses a static input buffer for command line parsing with newline termination.
 *          Commands are case-insensitive and provide immediate feedback.
 * 
 * @note Called continuously in main loop() for real-time command processing
 * @see setup(), loop()
 */
void handleSerialCommands() {
  static String inputBuffer = "";
  
  while (Serial.available()) {
    char c = Serial.read();
    
    if (c == '\n' || c == '\r') {
      if (inputBuffer.length() > 0) {
        inputBuffer.trim();
        inputBuffer.toLowerCase();
        
        Serial.println(""); // New line after command
        
        if (inputBuffer == "help" || inputBuffer == "?") {
          Serial.println("=== Available Serial Commands ===");
          Serial.println("help or ?         - Show this help");
          Serial.println("reboot            - Restart ESP32");
          Serial.println("recovery          - Reboot into recovery mode");
          Serial.println("normal            - Reboot into normal mode");
          Serial.println("status            - Show system status");
          Serial.println("config            - Show configuration");
          Serial.println("gps raw           - Set GPS to raw NMEA forwarding mode");
          Serial.println("gps panda         - Set GPS to PANDA generation mode");
          Serial.println("====================================");
          
        } else if (inputBuffer == "reboot") {
          Serial.println("SERIAL COMMAND: Rebooting ESP32...");
          delay(100);
          ESP.restart();
          
        } else if (inputBuffer == "recovery") {
          Serial.println("SERIAL COMMAND: Setting recovery mode and rebooting...");
          espData.program.state = 0; // Force recovery state
          espData.updateRTCBeforeReboot(0); // Update RTC before reboot
          espData.saveConfig();
          delay(100);
          ESP.restart();
          
        } else if (inputBuffer == "normal") {
          Serial.println("SERIAL COMMAND: Setting normal mode and rebooting...");
          espData.program.state = 1; // Force normal state
          espData.updateRTCBeforeReboot(1); // Update RTC before reboot
          espData.saveConfig();
          delay(100);
          ESP.restart();
          
        } else if (inputBuffer == "status") {
          Serial.println("=== System Status ===");
          Serial.println("Program: " + String(NAME));
          Serial.println("Boot Mode: " + String(espData.program.state == 1 ? "Normal" : "Recovery"));
          Serial.println("NVS Boot Count: " + String(espData.program.bootcount));
          Serial.println("Software Boots (since power cycle): " + String(espData.getSoftwareBootCount()));
          Serial.println("Uptime: " + String(millis()/1000) + " seconds");
          Serial.println("Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
          Serial.println("WiFi State: " + String(espData.wifi.state));
          Serial.println("GPS Fix: " + String(espData.gps.fixQuality));
          Serial.println("====================");
          
        } else if (inputBuffer == "config") {
          Serial.println("=== Configuration ===");
          Serial.println("IP: " + String(espData.wifi.ips[0]) + "." + String(espData.wifi.ips[1]) + "." + String(espData.wifi.ips[2]) + "." + String(espData.wifi.ips[3]));
          Serial.println("GPS Source: " + String(espData.gps.externalGPS ? "Wireless" : "On-board"));
          Serial.println("GPS Output: " + String(espData.gps.ntripPandaMode ? "PANDA Generation" : "Raw NMEA Forwarding"));
          Serial.println("WAS Source: " + String(espData.steer.wirelessWAS ? "Wireless" : "Wired"));
          Serial.println("Use ADS: " + String(espData.steer.useADS ? "Yes" : "No"));
          Serial.println("LED Brightness: " + String(espData.program.ledBrht));
          Serial.println("PID Kp: " + String(espData.steer.gainP));
          Serial.println("====================");
          
        } else if (inputBuffer == "gps raw") {
          Serial.println("SERIAL COMMAND: Setting GPS to raw NMEA forwarding mode");
          espData.gps.ntripPandaMode = false;
          espData.saveConfig();
          Serial.println("GPS output set to: Raw NMEA Forwarding");
          Serial.println("GPS GGA/VTG sentences will be forwarded as-is via UDP");
          
        } else if (inputBuffer == "gps panda") {
          Serial.println("SERIAL COMMAND: Setting GPS to PANDA generation mode");
          espData.gps.ntripPandaMode = true;
          espData.saveConfig();
          Serial.println("GPS output set to: PANDA Generation");
          Serial.println("GPS GGA/VTG sentences will be parsed to generate PANDA messages");
          
        } else if (inputBuffer.length() > 0) {
          Serial.println("Unknown command: " + inputBuffer);
          Serial.println("Type 'help' for available commands");
        }
        
        inputBuffer = "";
        Serial.print("> "); // Command prompt
      }
    } else if (c >= 32 && c <= 126) { // Printable characters only
      inputBuffer += c;
      Serial.print(c); // Echo character
    }
  }
}

/**
 * @brief Outputs basic system debug information to serial console
 * 
 * @details This function prints essential system status to the serial interface:
 *          - Current timestamp (milliseconds since boot)
 *          - Program name and current operational state
 *          - Steering angle and target steering values
 *          - WiFi connection status and assigned IP address
 * 
 *          Uses tab-separated format for easy parsing and debugging.
 *          Called periodically for monitoring system operation.
 * 
 * @note Less detailed than status command output, optimized for frequent calls
 * @see handleSerialCommands()
 */
void debugPrint(){
  webSerial("Timestamp: " + String(millis()) + "\n");
  webSerial("\tprogName: " + String(NAME));
  webSerial("\tprogState: " + String(espData.program.state));
  webSerial("\tconfRes: " + String(espData.program.confRes));
  webSerial("\twifiRes: " + String(espData.wifi.state));
  webSerial("\tgpsFix: " + String(espData.gps.fixQualityInt));
  webSerial("\tip: " + String(espData.wifi.ips[0]) + "." + 
                     String(espData.wifi.ips[1]) + "." + 
                     String(espData.wifi.ips[2]) + "." + 
                     String(espData.wifi.ips[3]) + "\n");
  // webSerial("\timu roll: " + String(espData.gps.imuRoll) + 
  //           "\timu pitch: " + String(espData.gps.imuPitch) + 
  //           "\timu yaw: " + String(espData.gps.imuHeading) + "\n");
}

/**
 * @brief Write startup log entry to file system
 * @param logEntry The log entry to write
 * @param isNewBoot True if this is a new boot session, false for additional entries
 */
void writeStartupLog(String logEntry, bool isNewBoot = false) {
  File logFile = LittleFS.open("/startup.log", isNewBoot ? "w" : "a");
  if (logFile) {
    if (isNewBoot) {
      // Add header for new boot session
      logFile.println("=== ESP32-AIO Startup Log ===");
      logFile.println("Firmware: " + String(NAME) + " v" + String(VERSION));
      logFile.println("Boot Time: " + String(millis()) + "ms since power-on");
      logFile.println("Reset Reason: " + String(esp_reset_reason()));
      logFile.println("Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
      logFile.println("Chip Model: " + String(ESP.getChipModel()));
      logFile.println("CPU Freq: " + String(ESP.getCpuFreqMHz()) + " MHz");
      logFile.println("Flash Size: " + String(ESP.getFlashChipSize()) + " bytes");
      logFile.println("MAC Address: " + WiFi.macAddress());
      logFile.println("Boot Count: " + String(espData.program.bootcount));
      logFile.println("================================");
    }
    logFile.println(String(millis()) + "ms: " + logEntry);
    logFile.close();
    Serial.println("Startup Log: " + logEntry);
  } else {
    Serial.println("ERROR: Could not write to startup log: " + logEntry);
  }
}

/**
 * @brief Log startup state information
 * @param stage The boot stage name
 * @param status The status or result
 * @param details Additional details (optional)
 */
void logStartupState(const char* stage, const char* status, const char* details = "") {
  String logEntry = String(stage) + " -> " + String(status);
  if (strlen(details) > 0) {
    logEntry += " (" + String(details) + ")";
  }
  writeStartupLog(logEntry);
}

#pragma region Setup and Loop
/**
 * @brief Initializes the system in recovery mode with AP WiFi and basic web interface
 * 
 * @details Recovery mode provides emergency access when normal boot fails:
 *          - Creates WiFi Access Point "ESP32_AIO_RECOVERY" with password "recovery123"
 *          - Sets up minimal web server on 192.168.4.1 for configuration access
 *          - Loads recovery-specific debug variables for system diagnostics
 *          - Provides file system access for configuration recovery
 *          - Enables serial command interface for remote management
 * 
 *          This mode bypasses normal hardware initialization and network connection,
 *          allowing system recovery when configuration is corrupted or unreachable.
 * 
 * @note Only essential services are started to minimize resource usage
 * @see normalboot(), setup()
 */
void recoveryBoot() {
  Serial.println("=== ENTERING RECOVERY MODE ===");
  logStartupState("Boot Mode", "Recovery Mode", "Normal boot failed");
  
  // Set all LEDs to error flash pattern to indicate recovery mode
  // I2CManager::getInstance().setAllLEDs(LEDPattern::ERROR_FLASH);
  
  // Initialize LittleFS for file operations
  Serial.println("Initializing LittleFS...");
  logStartupState("LittleFS", "Initializing", "Recovery mode");
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed - Attempting to format...");
    logStartupState("LittleFS", "Mount Failed", "Recovery mode format attempt");
    // if (LittleFS.format()) {
    //   Serial.println("LittleFS Format successful");
    //   if (LittleFS.begin(true)) {
    //     Serial.println("LittleFS Mount successful after format");
    //   } else {
    //     Serial.println("ERROR: LittleFS Mount failed even after format!");
    //   }
    // } else {
      // Serial.println("ERROR: LittleFS Format failed!");
    // }
  } else {
    Serial.println("LittleFS Mount successful");
  }
  
  // Verify file system is working
  Serial.printf("LittleFS Total: %u bytes\n", LittleFS.totalBytes());
  Serial.printf("LittleFS Used: %u bytes\n", LittleFS.usedBytes());
  
  // Setup WiFi AP for recovery access
  String recoverySSID = String(NAME) + "_RECOVERY";
  const char* recoveryPassword = "recovery123";
  
  Serial.println("Setting up Recovery WiFi AP...");
  Serial.println("SSID: " + recoverySSID);
  Serial.println("Password: " + String(recoveryPassword));
  
  WiFi.mode(WIFI_AP);
  WiFi.softAP(recoverySSID.c_str(), recoveryPassword);
  
  // Configure AP IP address
  IPAddress local_IP(192, 168, 1, 1);
  IPAddress gateway(192, 168, 1, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(local_IP, gateway, subnet);
  espData.wifi.ips[0] = 192;
  espData.wifi.ips[1] = 168;
  espData.wifi.ips[2] = 1;
  espData.wifi.ips[3] = 1;
  
  Serial.println("Recovery AP IP: " + WiFi.softAPIP().toString());
  
  // Setup recovery web server
  Serial.println("Starting Recovery Web Server...");
  
  // Serve recovery main page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = "<!DOCTYPE html><html><head><title>ESP32 Recovery Mode</title>";
    html += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
    html += "<style>body { font-family: Arial; margin: 20px; background: #f0f0f0; }";
    html += ".container { max-width: 1200px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; }";
    html += ".header { background: #d32f2f; color: white; padding: 15px; margin: -20px -20px 20px -20px; border-radius: 8px 8px 0 0; }";
    html += ".section { margin: 20px 0; padding: 15px; background: #f9f9f9; border-radius: 5px; }";
    html += ".debug-vars { font-family: monospace; white-space: pre-wrap; background: #000; color: #0f0; padding: 10px; border-radius: 5px; max-height: 400px; overflow-y: auto; }";
    html += ".button { background: #1976d2; color: white; padding: 10px 20px; border: none; border-radius: 5px; cursor: pointer; margin: 5px; }";
    html += ".button:hover { background: #1565c0; } .button.danger { background: #d32f2f; }";
    html += ".button.danger:hover { background: #c62828; } .file-input { margin: 10px 0; }</style></head>";
    html += "<body><div class='container'><div class='header'>";
    html += "<h1>🔧 ESP32 Recovery Mode</h1><p>System failed to boot normally - Recovery interface active</p></div>";
    html += "<div class='section'><h2>🔧 Recovery Actions</h2>";
    html += "<button class='button' onclick='location.href=\"/settings\"'>⚙️ Settings</button>";
    html += "<button class='button' onclick='location.href=\"/factoryReset\"'>Factory Reset</button>";
    html += "<button class='button' onclick='location.href=\"/forceNormalBoot\"'>Force Normal Boot</button>";
    html += "<button class='button' onclick='location.href=\"/reboot\"'>Reboot System</button>";
    html += "<button class='button' onclick='refreshDebug()'>Refresh Debug Info</button></div>";
    html += "<div class='section'><h2>📤 Firmware Update</h2>";
    html += "<form id='updateForm' enctype='multipart/form-data'>";
    html += "<div class='file-input'><input type='file' id='firmwareFile' accept='.bin' required>";
    html += "<button type='submit' class='button'>Upload Firmware</button></div></form></div>";
    html += "<div class='section'><h2>📁 File Management</h2><div>";
    html += "<input type='file' id='configFile' accept='.json'>";
    html += "<button class='button' onclick='uploadConfig()'>Upload Config</button>";
    html += "<button class='button' onclick='downloadConfig()'>Download Config</button></div></div>";
    html += "<div class='section'><h2>🔍 System Debug Information</h2>";
    html += "<div id='debugInfo' class='debug-vars'>Loading debug information...</div></div></div>";
    html += "<script>function refreshDebug() { fetch('/getRecoveryDebug').then(response => response.json())";
    html += ".then(data => { document.getElementById('debugInfo').textContent = data.join('\\n'); }); }";
    html += "setInterval(refreshDebug, 5000); refreshDebug();";
    html += "document.getElementById('updateForm').addEventListener('submit', function(e) {";
    html += "e.preventDefault(); const fileInput = document.getElementById('firmwareFile');";
    html += "if (fileInput.files[0]) { const formData = new FormData();";
    html += "formData.append('firmware', fileInput.files[0]);";
    html += "fetch('/update', { method: 'POST', body: formData }).then(response => {";
    html += "alert('Firmware upload ' + (response.ok ? 'successful!' : 'failed!')); }); } });";
    html += "function uploadConfig() { const fileInput = document.getElementById('configFile');";
    html += "if (fileInput.files[0]) { const formData = new FormData();";
    html += "formData.append('config', fileInput.files[0]);";
    html += "fetch('/upload', { method: 'POST', body: formData }).then(response => {";
    html += "alert('Config upload ' + (response.ok ? 'successful!' : 'failed!')); }); } }";
    html += "function downloadConfig() { window.open('/download?file=config.json', '_blank'); }";
    html += "</script></body></html>";
    
    request->send(200, "text/html", html);
  });
  
  // Recovery-specific debug endpoint
  server.on("/getRecoveryDebug", HTTP_GET, [](AsyncWebServerRequest *request) {
    updateRecoveryDebugVars();
    JsonDocument doc;
    JsonArray array = doc.to<JsonArray>();
    
    for (const auto& var : debugVars) {
      array.add(var);
    }
    
    String jsonResponse;
    serializeJson(doc, jsonResponse);
    request->send(200, "application/json", jsonResponse);
  });
  
  // Factory reset endpoint
  // server.on("/factoryReset", HTTP_GET, [](AsyncWebServerRequest *request) {
  //   Serial.println("Factory reset requested");
  //   request->send(200, "text/plain", "Factory reset initiated. Device will reboot...");
  //   delay(1000);
  //   LittleFS.format();
  //   ESP.restart();
  // });
  
  // Force normal boot endpoint
  server.on("/forceNormalBoot", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("Force normal boot requested");
    espData.program.state = 1; // Set to normal boot state
    espData.program.bootcount = 0; // Reset boot count
    espData.saveConfig();
    request->send(200, "text/plain", "Normal boot forced. Device will reboot...");
    delay(1000);
    ESP.restart();
  });
  
  // Recovery-specific debug endpoint to avoid conflicts with normal mode
  server.on("/getRecoveryDebug", HTTP_GET, [](AsyncWebServerRequest *request) {
    updateRecoveryDebugVars();
    JsonDocument doc;
    JsonArray array = doc.to<JsonArray>();
    
    for (const auto& var : debugVars) {
      array.add(var);
    }
    
    String jsonResponse;
    serializeJson(doc, jsonResponse);
    request->send(200, "application/json", jsonResponse);
  });
  
  Serial.println("Registering settings endpoints for recovery mode...");
  server.on("/settings", HTTP_GET, handleSettingsPage);
  server.on("/getSettings", HTTP_GET, handleGetSettings);
  server.on("/saveSettings", HTTP_POST, handleSaveSettings);
  Serial.println("Settings endpoints registered: /settings, /getSettings, /saveSettings");
  server.on("/getFiles", HTTP_GET, handleFileList);
  server.on("/download", HTTP_GET, handleFileDownload);
  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {}, handleFileUpload);
  server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {}, handleFirmwareUpload);
  server.on("/reboot", HTTP_GET, handleReboot);
  
  // Start the recovery server
  logStartupState("Web Server", "Starting", "Recovery mode");
  server.begin();
  logStartupState("Recovery Mode", "Complete", "Web server started");
  Serial.println("Recovery Web Server started!");
  Serial.println("Connect to WiFi: " + recoverySSID);
  Serial.println("Password: " + String(recoveryPassword));
  Serial.println("Open browser to: http://192.168.4.1");
  Serial.println("=====================================");  
}

/**
 * @brief Executes normal system boot sequence with full hardware initialization
 * 
 * @details Normal boot mode performs complete system startup including:
 *          - Serial port configuration for GPS and IMU communication
 *          - I2C bus initialization and hardware component detection
 *          - Network configuration and WiFi connection establishment
 *          - Web server startup with full feature set
 *          - GPS module initialization and NTRIP client setup
 *          - Steering system calibration and motor driver configuration
 *          - LED indicators and switch interface setup
 * 
 *          This is the primary operational mode providing full system functionality.
 *          All hardware components are initialized and operational services started.
 * 
 * @note Only called when system passes configuration validation checks
 * @see recoveryBoot(), setup(), I2Csetup()
 */
void normalboot(){
  // Normal boot sequence
  /** @brief Normal boot sequence */
  Serial.println("Normal Boot Sequence Initiated");
  logStartupState("Normal Boot", "Started", "");
  
  LittleFS.begin(); // Ensure LittleFS is mounted
  espData.setState(2);
  logStartupState("Program State", "Set to 2", "");
  
  // Start other Serial Ports
  bnoSerial.setPins(espData.pins.BNO_PIN, 10);
  bnoSerial.begin(115200);
  String bnoDetails = "Pin: " + String(espData.pins.BNO_PIN) + ", 115200 baud";
  logStartupState("BNO Serial", "Initialized", bnoDetails.c_str());
  
  gpsSerial.setPins(espData.pins.GPS_RX, espData.pins.GPS_TX);
  gpsSerial.begin(460800);
  String gpsDetails = "RX: " + String(espData.pins.GPS_RX) + ", TX: " + String(espData.pins.GPS_TX) + ", 460800 baud";
  logStartupState("GPS Serial", "Initialized", gpsDetails.c_str());
  
  // Initialize I2C Manager for centralized bus management
  I2Csetup();
  if (i2cManager.begin(&twoWire, espData.i2c.MCP_ADDRESS, espData.i2c.ADS_ADDRESS)) {
    Serial.println("I2CManager initialized successfully");
    String i2cDetails = "MCP: 0x" + String(espData.i2c.MCP_ADDRESS, HEX) + ", ADS: 0x" + String(espData.i2c.ADS_ADDRESS, HEX);
    logStartupState("I2C Manager", "Initialized Successfully", i2cDetails.c_str());
    // Set optimal ADS settings for reduced I2C load
    // i2cManager.adsSetGain(GAIN_TWOTHIRDS);  // +/- 6.144V range
  } else {
    Serial.println("I2CManager initialization failed");
    logStartupState("I2C Manager", "Failed - Restarting", "");
    ESP.restart();
  }
  
  
  // Start GPS
  // Using MCPManager singleton approach (auto-detected when no MCP pointer provided):
 
    // If everything is good, turn on power to autosteer
  Serial.println("starting wifi");
  logStartupState("WiFi", "Starting Connection", "");
    
  uint32_t wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED){
    espData.wifi.state = espWifi.connect();
    /** @brief Check for WiFi connection, if times out, create AP */
    if (millis()-wifiStart > 120000){
      Serial.println("Wifi connection timed out");
      Serial.println("Switching to AP mode");
      logStartupState("WiFi", "Connection Timeout", "Switching to AP mode after 120s");
      espData.wifi.state = espWifi.makeAP();
      break;
    }
  }
  i2cManager.setEthLED(LEDPattern::ON);
  // espConfig.wifiCfg.state = espWifi.makeAP();
  Serial.println("Wifi State: " + String(espData.wifi.state));
  String wifiDetails = "State: " + String(espData.wifi.state);
  logStartupState("WiFi", espData.wifi.state == 1 ? "Connected" : "AP Mode", wifiDetails.c_str());
  
  // ADD NETWORK READINESS CHECK
  Serial.println("=== NETWORK INITIALIZATION DEBUG ===");
  Serial.println("WiFi Status: " + String(WiFi.status()));
  Serial.println("WiFi Mode: " + String(WiFi.getMode()));
  Serial.println("IP Address: " + WiFi.localIP().toString());
  Serial.println("Gateway: " + WiFi.gatewayIP().toString());
  Serial.println("DNS: " + WiFi.dnsIP().toString());
  
  // Verify WiFi is actually connected before proceeding
  if (WiFi.status() != WL_CONNECTED || WiFi.localIP() == IPAddress(0,0,0,0)) {
    Serial.println("ERROR: WiFi not properly connected! Attempting to fix...");
    
    // Try to reconnect or switch to AP mode
    if (espData.wifi.state == 1) {
      Serial.println("Forcing AP mode due to invalid network state");
      espData.wifi.state = espWifi.makeAP();
      delay(3000); // Give AP mode time to start
    }
    
    // Check again after fix attempt
    Serial.println("After fix attempt:");
    Serial.println("WiFi Status: " + String(WiFi.status()));
    Serial.println("WiFi Mode: " + String(WiFi.getMode()));
    Serial.println("IP Address: " + WiFi.localIP().toString());
  }
  
  // Wait for network stack to stabilize
  Serial.println("Waiting for network stack to stabilize...");
  delay(3000); // Increased delay for stability
  
  // Final verification before starting UDP
  String networkDetails = "WiFi Status: " + String(WiFi.status()) + ", IP: " + WiFi.localIP().toString();
  logStartupState("Network", "Verifying", networkDetails.c_str());
  Serial.println("Final network check before UDP:");
  Serial.println("WiFi Status: " + String(WiFi.status()));
  Serial.println("IP Address: " + WiFi.localIP().toString());
  
  // START UDP SERVICES FIRST - before other components try to use them
  Serial.println("=== STARTING UDP SERVICES ===");
  logStartupState("UDP Services", "Starting", "Network ready");
  espUdp.begin(&gps);  // Initialize UDP services first
  logStartupState("UDP Services", "Complete", "Initialization successful");
  Serial.println("=== UDP SERVICES COMPLETE ===");
  
  // Now start other components that depend on UDP
  Serial.println("Starting hardware tasks...");
  logStartupState("MainPower", "Starting", "Task initialization");
  mainPower.startTask();

  Serial.println("Initializing GPS...");
  logStartupState("GPS", "Starting", "Initialization with UDP");
  gps.init(&espUdp);  // Now UDP is ready
  logStartupState("GPS", "Complete", "Initialization successful");
  
  Serial.println("Initializing steering...");
  logStartupState("Steering", "Starting", "System initialization");
  espSteer.begin(&espUdp);  // Now UDP is ready
  logStartupState("Steering", "Complete", "System ready");
  
  
  // UDP setup
  Serial.println("Network setup complete");
  // delay(5000);
  
  #pragma region Server Setup
        // Serve the main HTML page
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
          Serial.println("Getting index file");
          if (LittleFS.exists("/index.html")) {
            request->send(LittleFS, "/index.html");
          } else {
            Serial.println("ERROR: index.html not found in LittleFS");
            request->send(404, "text/plain", "index.html not found - file system may not be uploaded");
          }
        });
        // Route to get debug variables as JSON
        server.on("/getDebugVars", HTTP_GET, handleDebugVars);
        
        // Route to set debug mode (minimal vs full)
        server.on("/setDebugMode", HTTP_POST, [](AsyncWebServerRequest *request) {}, NULL, [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
          // This is the body handler for POST requests
          static String body = "";
          
          // Accumulate the body data
          if (index == 0) {
            body = "";  // Reset for new request
          }
          
          for (size_t i = 0; i < len; i++) {
            body += (char)data[i];
          }
          
          // Process only when we have received all the data
          if (index + len == total) {
            Serial.println("setDebugMode endpoint called");
            Serial.println("Request body: " + body);
            
            // Parse JSON body
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, body);
            
            if (error) {
              Serial.println("JSON parsing error: " + String(error.c_str()));
              request->send(400, "text/plain", "Invalid JSON");
              return;
            }
            
            if (doc["fullDebug"].is<bool>()) {
              bool newMode = doc["fullDebug"].as<bool>();
              fullDebugMode = newMode;
              String response = fullDebugMode ? "Full debug mode enabled" : "Minimal debug mode enabled";
              Serial.println("Debug mode changed to: " + String(fullDebugMode ? "FULL" : "MINIMAL"));
              request->send(200, "text/plain", response);
            } else {
              Serial.println("Missing fullDebug parameter in JSON");
              request->send(400, "text/plain", "Missing fullDebug parameter");
            }
          }
        });
        
        // Route to list files as JSON
        Serial.println("Registering settings endpoints for normal mode...");
        server.on("/settings", HTTP_GET, handleSettingsPage);
        server.on("/getSettings", HTTP_GET, handleGetSettings);
        server.on("/saveSettings", HTTP_POST, handleSaveSettings);
        Serial.println("Settings endpoints registered: /settings, /getSettings, /saveSettings");
        server.on("/getFiles", HTTP_GET, handleFileList);
        // Route to download files
        server.on("/download", HTTP_GET, handleFileDownload);

        // Handle file upload
        server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {}, handleFileUpload);

        server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {}, 
        handleFirmwareUpload);

        server.on("/reboot", HTTP_GET, handleReboot);
        
        // WebSocket handler for serial monitor
        // ws.onEvent(onWsEvent);
        // server.addHandler(&ws);
        
        // Serial monitor web page
        server.on("/serialmonitor", HTTP_GET, [](AsyncWebServerRequest *request){
            String html = R"(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32-AIO Serial Monitor</title>
    <meta name='viewport' content='width=device-width, initial-scale=1'>
    <style>
        body { 
            font-family: 'Courier New', monospace; 
            margin: 0; 
            padding: 20px; 
            background-color: #1e1e1e; 
            color: #ffffff; 
        }
        .container { 
            max-width: 1200px; 
            margin: 0 auto; 
        }
        .header { 
            background: #333; 
            padding: 15px; 
            border-radius: 5px; 
            margin-bottom: 10px; 
            display: flex; 
            justify-content: space-between; 
            align-items: center; 
        }
        .status { 
            color: #00ff00; 
            font-weight: bold; 
        }
        .status.disconnected { 
            color: #ff0000; 
        }
        .terminal { 
            background: #000; 
            color: #00ff00; 
            padding: 15px; 
            border-radius: 5px; 
            height: 70vh; 
            overflow-y: auto; 
            font-size: 14px; 
            white-space: pre-wrap; 
            border: 2px solid #333; 
        }
        .controls { 
            margin: 10px 0; 
            display: flex; 
            gap: 10px; 
            align-items: center; 
            flex-wrap: wrap;
        }
        .button { 
            background: #0066cc; 
            color: white; 
            border: none; 
            padding: 8px 16px; 
            border-radius: 4px; 
            cursor: pointer; 
        }
        .button:hover { 
            background: #0052a3; 
        }
        .button.clear { 
            background: #cc6600; 
        }
        .button.clear:hover { 
            background: #b85500; 
        }
        .button.danger { 
            background: #cc0000; 
        }
        .button.danger:hover { 
            background: #990000; 
        }
        input[type="text"] { 
            flex: 1; 
            padding: 8px; 
            border: 1px solid #555; 
            border-radius: 4px; 
            background: #333; 
            color: white; 
            min-width: 200px;
        }
        .timestamp { 
            color: #888; 
            font-size: 12px; 
        }
        @media (max-width: 768px) {
            .header { flex-direction: column; align-items: stretch; }
            .controls { flex-direction: column; }
            input[type="text"] { min-width: auto; margin-bottom: 10px; }
        }
    </style>
</head>
<body>
    <div class='container'>
        <div class='header'>
            <h2>📡 ESP32-AIO Serial Monitor</h2>
            <div class='status' id='status'>Connecting...</div>
        </div>
        
        <div class='controls'>
            <input type='text' id='commandInput' placeholder='Enter command (clear, restart, status)...' onkeypress='handleKeyPress(event)'>
            <button class='button' onclick='sendCommand()'>Send</button>
            <button class='button clear' onclick='clearTerminal()'>Clear</button>
            <button class='button' onclick='reconnect()'>Reconnect</button>
            <button class='button' onclick='sendPredefinedCommand("status")'>Status</button>
            <button class='button danger' onclick='sendPredefinedCommand("restart")'>Restart ESP32</button>
        </div>
        
        <div class='terminal' id='terminal'></div>
    </div>

    <script>
        let ws;
        let terminal = document.getElementById('terminal');
        let status = document.getElementById('status');
        let commandInput = document.getElementById('commandInput');
        
        function connect() {
            const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
            const wsUrl = protocol + '//' + window.location.host + '/ws';
            
            ws = new WebSocket(wsUrl);
            
            ws.onopen = function() {
                status.textContent = 'Connected';
                status.className = 'status';
                addToTerminal('[' + new Date().toLocaleTimeString() + '] WebSocket Connected\n');
            };
            
            ws.onmessage = function(event) {
                addToTerminal(event.data);
            };
            
            ws.onclose = function() {
                status.textContent = 'Disconnected';
                status.className = 'status disconnected';
                addToTerminal('[' + new Date().toLocaleTimeString() + '] WebSocket Disconnected\n');
                
                // Auto-reconnect after 3 seconds
                setTimeout(connect, 3000);
            };
            
            ws.onerror = function(error) {
                status.textContent = 'Error';
                status.className = 'status disconnected';
                addToTerminal('[' + new Date().toLocaleTimeString() + '] WebSocket Error\n');
            };
        }
        
        function addToTerminal(message) {
            terminal.textContent += message;
            terminal.scrollTop = terminal.scrollHeight;
        }
        
        function sendCommand() {
            const command = commandInput.value.trim();
            if (command && ws && ws.readyState === WebSocket.OPEN) {
                ws.send(command);
                commandInput.value = '';
            }
        }
        
        function sendPredefinedCommand(cmd) {
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send(cmd);
            }
        }
        
        function clearTerminal() {
            terminal.textContent = '';
            if (ws && ws.readyState === WebSocket.OPEN) {
                ws.send('clear');
            }
        }
        
        function reconnect() {
            if (ws) {
                ws.close();
            }
            connect();
        }
        
        function handleKeyPress(event) {
            if (event.key === 'Enter') {
                sendCommand();
            }
        }
        
        // Connect on page load
        connect();
        
        // Auto-scroll to bottom
        setInterval(() => {
            terminal.scrollTop = terminal.scrollHeight;
        }, 1000);
    </script>
</body>
</html>
            )";
            
            request->send(200, "text/html", html);
        });
        
        server.on("/toggleAPMode", HTTP_POST, handleToggleAPMode); // Add this line
        // Handle toggle state update
        server.on("/zeroWAS", HTTP_GET, handleWASzero);
        // Start server
        server.on("/Module_Disconnected", HTTP_GET, [](AsyncWebServerRequest *request){
          Serial.println("Sending Module_Disconnected.svg");
          request->send(LittleFS, "/Module_Disconnected.svg", "image/svg+xml");
        });
        server.on("/Module_Connected", HTTP_GET, [](AsyncWebServerRequest *request){
          request->send(LittleFS, "/Module_Connected.svg", "image/svg+xml");
        });
        server.on("/setGpsSource", HTTP_POST, [](AsyncWebServerRequest *request){},
        NULL,
        [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, data, len);
            if (error) {
                request->send(400, "text/plain", "Invalid JSON");
                return;
            }
            String source = doc["source"] | "";
            if (source == "um982") {

                espData.gps.externalGPS = false;
                request->send(200, "text/plain", "UM982 GPS selected");
            } else if (source == "external") {
                espData.gps.externalGPS = true;
                request->send(200, "text/plain", "External GPS selected");
            } else {
                request->send(400, "text/plain", "Unknown GPS source");
            }
        }
    );
        // server.on("/isConnected", HTTP_GET, [](AsyncWebServerRequest *request){
        //   request->send(LittleFS, "/Modu.svg", "image/svg+xml");
        // });
        logStartupState("Web Server", "Starting", "Normal mode");
        server.begin();
        logStartupState("Web Server", "Complete", "Started successfully");
      #pragma endregion
  
  logStartupState("System Boot", "Complete", "Entering normal operation");
  espData.setState(1);
  espData.setBootCount(0);
  // Add your normal boot logic here
}

/**
 * @brief Main Arduino setup function - system initialization entry point
 * 
 * @details This is the primary system initialization function that:
 *          - Initializes serial communication and LED task scheduler
 *          - Loads system configuration from NVS storage
 *          - Determines boot mode based on configuration state and boot count
 *          - Routes to either recoveryBoot() or normalboot() based on system health
 *          - Implements recovery mode fallback for corrupted configurations
 * 
 *          Boot mode selection logic:
 *          - Recovery mode: If config invalid AND boot count > 2
 *          - Normal mode: If configuration loads successfully
 * 
 * @note Called once by Arduino framework at system startup
 * @see loop(), recoveryBoot(), normalboot()
 */
void setup(){
  Serial.begin(115200);
  myLED.startTask();
  myLED.setLEDState(LEDState::SPECIAL_MODE);
  // myLED.setLEDState(LEDState::NO_ERROR);
  delay(1000); // Give time for serial to initialize
  Serial.println("\n\nStarting up...");
  
  // Initialize LittleFS first for startup logging
  if (!LittleFS.begin(true)) {
    Serial.println("LittleFS Mount Failed");
  }
  
  // Start new startup log session
  writeStartupLog("System startup initiated", true);
  logStartupState("Serial", "Initialized", "115200 baud");
  logStartupState("LED Controller", "Started", "Special mode");
  
  espData.program.confRes = espData.loadConfig();
  String confDetails = "Result: " + String(espData.program.confRes);
  logStartupState("Configuration", espData.program.confRes == 0 ? "Loaded Successfully" : "Load Failed", confDetails.c_str());
  
  // Update LED brightness from loaded configuration
  myLED.updateBrightness();
  Serial.println("LED brightness set to: " + String(espData.program.ledBrht));
  String ledDetails = "Set to " + String(espData.program.ledBrht);
  logStartupState("LED Brightness", "Updated", ledDetails.c_str());
  
  if (espData.program.state != 1 && espData.program.bootcount > 2){
    Serial.println(" Booting into Recovery Mode");
    String recoveryDetails = "State: " + String(espData.program.state) + ", Boot count: " + String(espData.program.bootcount);
    logStartupState("Boot Mode", "Recovery", recoveryDetails.c_str());
    myLED.setLEDState(LEDState::RECOVERY_MODE);
    recoveryBoot();
  } else {
    String normalDetails = "State: " + String(espData.program.state) + ", Boot count: " + String(espData.program.bootcount);
    logStartupState("Boot Mode", "Normal", normalDetails.c_str());
    normalboot();
  }
  
  // Serial command interface ready
  Serial.println("\n=== Serial Command Interface Ready ===");
  Serial.println("Type 'help' for available commands");
  Serial.print("> ");
  
  // Start Wifi AP and Webserver for diagnostics
  // espConfig.wifiCfg.state = espWifi.connect();

}

/**
 * @brief Main Arduino loop function - continuous system operation
 * 
 * @details This function runs continuously after setup() completes and provides:
 *          - Serial command processing for runtime system control
 *          - Periodic debug output for system monitoring (every 5 seconds)
 *          - System health monitoring and status updates
 * 
 *          The loop maintains system responsiveness while providing regular
 *          status feedback through the serial interface. All real-time operations
 *          and hardware interfaces are managed through interrupt handlers and
 *          background tasks.
 * 
 * @note Called continuously by Arduino framework - keep execution time minimal
 * @see setup(), handleSerialCommands(), debugPrint()
 */
void loop(){
  
  // Handle serial commands
  // handleSerialCommands();
  
  // Only debug print occasionally and check free heap
  // static unsigned long lastDebugPrint = 0;
  // unsigned long currentMillis = millis();
  
  // if (currentMillis - lastDebugPrint >= 10000) { // Every 10 seconds instead of 5
  //   if (ESP.getFreeHeap() > 50000) { // Only if we have sufficient free heap
  //     debugPrint();
  //     lastDebugPrint = currentMillis;
  //   } else {
  //     webSerial("Low memory warning: " + String(ESP.getFreeHeap()) + " bytes free\n");
  //     lastDebugPrint = currentMillis;
  //   }
  // }
  
  delay(1000); // Reduce to 1 second delay for better responsiveness
}
#pragma endregion