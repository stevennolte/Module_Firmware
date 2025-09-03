#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Update.h>
#include "ESPconfig.h"

// Define HTTP method constants if not already defined
#ifndef HTTP_GET
#define HTTP_GET 1
#endif
#ifndef HTTP_POST  
#define HTTP_POST 3
#endif

class WebServerManager {
private:
    AsyncWebServer* server;
    ESPconfig* espConfig;
    std::vector<String> debugVars;
    
    // Handler functions
    void handleFileList(AsyncWebServerRequest *request);
    void handleFileDownload(AsyncWebServerRequest *request);
    void handleWASzero(AsyncWebServerRequest *request);
    void handleFirmwareUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
    void handleDebugVars(AsyncWebServerRequest *request);
    void handleReboot(AsyncWebServerRequest *request);
    void handleFileUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
    void handleToggleAPMode(AsyncWebServerRequest *request);
    
    // Helper functions
    void updateDebugVars();
    void setupRoutes();

public:
    WebServerManager(AsyncWebServer* serverPtr, ESPconfig* configPtr);
    void begin();
    void handleClient();
};

#endif // WEBSERVER_H
