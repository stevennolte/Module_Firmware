#include <Arduino.h>
#include "ESPconfig.h"
#include "ESPWifi.h"
#include "myLED.h"
#include "Wire.h"
#include "ESPudp.h"
#include "Adafruit_MCP23X17.h"
#include "MCPManager.h"
#include <Adafruit_ADS1X15.h>
// #include "Indicators.h"
#include "MainPower.h"
#include "GPS.h"
#include "HardwareSerial.h"
#include "ESP32OTAPull.h"
#include "ESPsteer.h"
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include "WebServer.h"

//TODO: add wifi connect timer to ap mode

TwoWire twoWire = TwoWire(0);
TwoWire twoWire1 = TwoWire(1);
HardwareSerial bnoSerial(2);
HardwareSerial gpsSerial(1);

Adafruit_MCP23X17 mcp;
Adafruit_ADS1115 ads;

// Using singleton pattern - single access point for configuration
ESPconfig& espConfig = ESPconfig::getInstance();

// Get MCPManager singleton instance (alternative approach)
MCPManager& mcpManager = MCPManager::getInstance();

// Components using singleton instance
GPS gps(&espConfig, &gpsSerial, &bnoSerial, &mcp);
MyLED myLED(&espConfig);
MainPower mainPower(&espConfig, &mcp, &ads);
ESPWifi espWifi(&espConfig);
ESPudp espUdp(&espConfig);
ESP32OTAPull ota;
AsyncWebServer server(80);
WebServerManager webServerManager(&server, &espConfig);

ESPsteer espSteer(&espConfig, &ads, &mcp);

// Reference shortcuts using singleton
auto& progData = espConfig.progData;
auto& progCfg = espConfig.progCfg;
auto& progState = espConfig.progData.state;
auto& wifiCfg = espConfig.wifiCfg;

bool I2Csetup(){
  if(!twoWire.setPins(espConfig.gpioDefs.SDA_PIN, espConfig.gpioDefs.SCL_PIN)){
    Serial.println("Wire failed to set pins");
    return false;
  }
  // if(!twoWire.setClock(1000000)){
  //   Serial.println("Wire failed to set clock");
  //   return false;
  // }
  if(!twoWire.begin()){
    Serial.println("Wire failed to begin");
    return false;
  }
  byte error, address;
  int nDevices;

  Serial.println("Scanning...");

  twoWire.beginTransmission(0x20);
  error = twoWire.endTransmission();
  if (error == 0)
  {
    Serial.println("MCP23017 found at address 0x20  !");
    progData.mcpState = 1;
  }
  else 
  {
    Serial.println("Unknown error at address 0x20");
    progData.mcpState = 2;
  }
  
  twoWire.beginTransmission(0x48);
  error = twoWire.endTransmission();
  if (error == 0)
  {
    Serial.println("ADS1115 found at address 0x48  !");
    progData.adsState = 1;
  }
  else 
  {
    Serial.println("Unknown error at address 0x48");
    progData.adsState = 2;
  }
  if (progData.mcpState == 2 || progData.adsState == 2){
    return false;
  } else {
    return true;
  }
  
}

#pragma region OTA
const char *errtext(int code)
{
	switch(code)
	{
		case ESP32OTAPull::UPDATE_AVAILABLE:
			return "An update is available but wasn't installed";
		case ESP32OTAPull::NO_UPDATE_PROFILE_FOUND:
			return "No profile matches";
		case ESP32OTAPull::NO_UPDATE_AVAILABLE:
			return "Profile matched, but update not applicable";
		case ESP32OTAPull::UPDATE_OK:
			return "An update was done, but no reboot";
		case ESP32OTAPull::HTTP_FAILED:
			return "HTTP GET failure";
		case ESP32OTAPull::WRITE_ERROR:
			return "Write error";
		case ESP32OTAPull::JSON_PROBLEM:
			return "Invalid JSON";
		case ESP32OTAPull::OTA_UPDATE_FAIL:
			return "Update fail (no OTA partition?)";
		default:
			if (code > 0)
				return "Unexpected HTTP response code";
			break;
	}
	return "Unknown error";
}

void OtaPullCallback(int offset, int totallength)
{
	Serial.printf("Updating %d of %d (%02d%%)...\r", offset, totallength, 100 * offset / totallength);
}

void softwareUpdate(){
  char basePath[] = "/%s/Releases/OTA_Config.json";
  char CONFIG_URL[150];
  sprintf(CONFIG_URL, basePath, NAME);
  Serial.println(CONFIG_URL);
  char SERVER[150];
  sprintf(SERVER, "http://%d.%d.%d.%d:%d",espConfig.wifiCfg.ips[0],espConfig.wifiCfg.ips[1],espConfig.wifiCfg.ips[2],espConfig.otaCfg.ipAddr,espConfig.otaCfg.port);
  Serial.print("CONFIG_URL: ");
  Serial.println(CONFIG_URL);
  Serial.print("SERVER: ");
  Serial.println(SERVER);
  
  ota.SetConfig(NAME);
  ota.SetCallback(OtaPullCallback);
  
  Serial.printf("We are running version %s of the sketch, Board='%s', Device='%s', IP='%s \n", VERSION, ARDUINO_BOARD, WiFi.macAddress().c_str(),(String)(WiFi.localIP()[3]));
  Serial.println();
  // Serial.printf("Checking %s to see if an update is available...\n", CONFIG_URL);
  Serial.println();
  int ret = ota.CheckForOTAUpdate(SERVER, CONFIG_URL, VERSION);
  Serial.printf("CheckForOTAUpdate returned %d (%s)\n\n", ret, errtext(ret));
}

#pragma endregion

#pragma region Buttons
void IRAM_ATTR handleSteerSwitch() {
  unsigned long currentTime = millis();
  if (currentTime - espConfig.switchData.steerSwitchLastTime > 100) {
      espConfig.switchData.steerSwitch = true; // Set the flag
      espConfig.switchData.steerSwitchLastTime = currentTime; // Update the debounce timestamp
  }
}

void IRAM_ATTR handleWorkSwitch() {
  unsigned long currentTime = millis();
  if (currentTime - espConfig.switchData.workSwitchLastTime > 100) {
      espConfig.switchData.workSwitch = true; // Set the flag
      espConfig.switchData.workSwitchLastTime = currentTime; // Update the debounce timestamp
  }
}

void buttonSetup(){
  // Set up the GPIO pins for the buttons
  pinMode(espConfig.gpioDefs.STEER_SWITCH_PIN, INPUT_PULLUP);
  pinMode(espConfig.gpioDefs.WORK_SWITCH_PIN, INPUT_PULLUP);
  
  // Attach interrupts to the buttons
  attachInterrupt(digitalPinToInterrupt(espConfig.gpioDefs.STEER_SWITCH_PIN), handleSteerSwitch, FALLING);
  attachInterrupt(digitalPinToInterrupt(espConfig.gpioDefs.WORK_SWITCH_PIN), handleWorkSwitch, FALLING);
}
#pragma endregion

void setup(){
  progData.state = 0;
  myLED.startTask();
  progData.state = 2;
  
  // Start USB Serial Port
  Serial.begin(115200);
  delay(5000);   // Wait for the usb to connect so you can see the outputs at startup
  Serial.println("Starting up...");
  espConfig.progCfg.confRes = espConfig.loadConfig();
  // Start Wifi AP and Webserver for diagnostics
  // espConfig.wifiCfg.state = espWifi.connect();
  
  while (wifiCfg.state != 1){
    wifiCfg.state = espWifi.connect();
    if (millis() > 120000){
      Serial.println("Wifi connection timed out");
      wifiCfg.state = espWifi.makeAP();
      break;
    }
  }
  // espConfig.wifiCfg.state = espWifi.makeAP();
  Serial.println("Wifi State: " + String(espConfig.wifiCfg.state));
  
  // Start webserver
  webServerManager.begin();

  // Start other Serial Ports
  bnoSerial.setPins(espConfig.gpioDefs.BNO_PIN, 10);
  bnoSerial.begin(115200);
  gpsSerial.setPins(espConfig.gpioDefs.GPS_RX, espConfig.gpioDefs.GPS_TX);
  gpsSerial.begin(115200);
  
  // Grab the config
  

  // Start I2C and check for hardware
  
  if(!I2Csetup()){
    Serial.println("I2C setup failed");
    progData.state = 3;
    while(1){
      delay(1000);
    }
  }
  if (progData.mcpState == 1){
    mcp.begin_I2C(0x20, &twoWire);
    
    // Also initialize the MCPManager singleton (alternative approach)
    mcpManager.begin(0x20, &twoWire);
    
    // mcp.pinMode(espConfig.gpioDefs.rtkFix, OUTPUT);
    // mcp.digitalWrite(espConfig.gpioDefs.rtkFix, HIGH);
    // delay(1000);
    // mcp.digitalWrite(espConfig.gpioDefs.rtkFix, LOW);
  }
  if (progData.adsState == 1){
    ads.begin(0x48, &twoWire);
  }
  
  
  // Start GPS
  // Traditional approach using MCP pointer injection:
  gps.init(&espUdp);
  
  // Alternative approach using MCPManager singleton:
  // gps.initWithSingleton(&espUdp);

  // If everything is good, turn on power to autosteer
  mainPower.startTask();
  espSteer.begin(&espUdp);
  
  // Scan for Wifi networks
  // espWifi.connect();
  
  // UDP setup
  espUdp.begin(&gps);
  Serial.println("Network setup complete");
  // delay(5000);
  progData.state = 1;
  
}

void debugPrint(){
  Serial.printf("Timestamp since boot [ms]: %lu", millis());
  Serial.printf(" progName: %s", espConfig.progCfg.name);
  Serial.printf(" progState: %lu", progState);
  Serial.printf(" confRes: %lu", espConfig.progCfg.confRes);
  Serial.printf(" wifiRes: %lu", espConfig.wifiCfg.state);
  Serial.printf(" gpsFix: %lu", espConfig.gpsData.fixQualityInt);
  Serial.printf(" ip[0]: %d", espConfig.wifiCfg.ips[0]);
  Serial.printf(" ip[1]: %d", espConfig.wifiCfg.ips[1]);
  Serial.printf(" ip[2]: %d", espConfig.wifiCfg.ips[2]);
  Serial.printf(" ip[3]: %d", espConfig.wifiCfg.ips[3]);
  
  
  
  // Serial.printf(" gpsAge: %lu", espConfig.gpsData.);
  Serial.println();
  // Serial.println(twoWire.requestFrom(0x22, 0x01));
  // Serial.printf("Mag x: %.2f mT, y: %.2f mT, z: %.2f mT, Temp: %.2f °C\n", espConfig.magData.x, espConfig.magData.y, espConfig.magData.z, (espConfig.magData.t*1.8)+32);
  // Serial.println();
  // Serial.println(espConfig.progCfg.name);
  // Serial.println();
}

void loop(){
  
  //Read in NMEA from the UM980
  debugPrint();
  
  delay(5000);
}