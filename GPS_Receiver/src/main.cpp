#include <Arduino.h>
#include <myWifi.h>
#include <NMEAParser.h>
#include "ESP32OTAPull.h"
#include "Version.h"
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include "AsyncUDP.h"
#include "messageStructs.h"
#include "myLED.h"
#include "configLoad.h"
#include "vector"
#include "BNO08x_AOG.h"
#include "BluetoothComms.h"
#include "driver/temp_sensor.h"

#define SDA_0 5
#define SCL_0 4


Heartbeat_u heartbeat;
Heartbeat_t * hbData = &heartbeat.heartbeat_t;
Shutdown_u shutdownMsg;
TwoWire twoWire = TwoWire(0);
std::vector<String> debugVars;
ProgramData_t progData;
MyLED myLED(progData, 48, 254);
ConfigLoad config(progData);
MyWifi myWifi;
AsyncWebServer server(80);
ESP32OTAPull ota;
NMEAParser<2> parser;
BNO080 bno08x;
BluetoothRemote bleRemote;

class UDPMethods{
  private:
    
    uint32_t udpTimeout = 1000;
    
    int heartbeatTimePrevious=0;
    int heartbeatTimeTrip=250;
    int flowDataTimePrevious=0;
    int flowDataTimeTrip=1000;
    
  public:
    uint32_t ntripTimestamp;
    uint32_t gpsTimestamp;
    AsyncUDP udp;
    AsyncUDP udpNtrip;
    // Hello_u hello;
    UDPMethods(){
    }

    uint8_t calcChecksum(uint8_t* data, size_t size){
      // Serial.print("Calculating Checksum: ");
      // Serial.println(size);  
      uint8_t checksum = 0;
      // for (int i = 0; i < size; i++){
      //   Serial.print(data[i]);
      //   Serial.print(" ");
      // }
      // Serial.println();
        for (int i = 2; i < data[4] + 5; i++){
            checksum += data[i];
        }
        return checksum;
    }
    void begin(){
      
      udpNtrip.listen(2233);
      udp.listen(8888);
      Serial.println("UDP Listening");
      
      char version[64];
      strcpy(version, VERSION);
      char *token = strtok(version, ".");
      int i = 0;
      while (token != NULL) {
        int intValue = atoi(token);
        switch (i){
          case 0:
            hbData->version1 = intValue;
            break;
          case 1:
            hbData->version2 = intValue;
            break;
          case 2:
            hbData->version3 = intValue;
            break;
        }
        i++;
        token = strtok(NULL, ".");
      }
      Serial.println("testbreak");
      
      hbData->aogByte1 = 0x80;
      hbData->aogByte2 = 0x81;
      hbData->sourceAddress = 51;
      hbData->PGN = MY_PGN;
      hbData->length = 10;
      hbData->myPGN = HEARBEAT_PGN;
      
      udpNtrip.onPacket([this](AsyncUDPPacket packet) {
        ntripTimestamp = millis();
        char packetBuffer[255];
        size_t packetLength = packet.length();
        uint8_t *_data = packet.data();
        
        Serial2.write(_data, packetLength);
        Serial.printf("Received UDP packet of length: %u\n", packetLength);
        Serial.println("Sent Ntrip");
      });
      udp.onPacket([](AsyncUDPPacket packet) {
        if (packet.data()[0]==0x80 & packet.data()[1]==0x81){
          progData.udpTimer = millis();
          uint8_t checksum = 0;
          Hello_u hello;
          switch (packet.data()[3]){
            case 149:
              switch (packet.data()[5])
                {
                case 5:
                  progData.shutdown = true;
                  break;
                case 6:
                  Serial.print("Update Cmd from Server: ");
                  Serial.println(packet.remoteIP()[3]);
                  Serial.println(packet.data()[6]);
                  Serial.println((packet.data()[8] << 8) + packet.data()[7]);
                  progData.serverAddress=packet.remoteIP()[3];
                  progData.serverPort = (packet.data()[8] << 8) + packet.data()[7];
                  
                  config.updateConfig();
                  ESP.restart();
                  break;
                
                default:
                  break;
                }
            case 200: // Hello from AIO
                
                hello.hello_t.aogByte1 = 0x80;
                hello.hello_t.aogByte2 = 0x81;
                hello.hello_t.sourceAddress = 120;
                hello.hello_t.PGN = 120;
                hello.hello_t.length = 5;
                
                for (int i = 2; i < hello.bytes[4] + 5; i++){
                    checksum += hello.bytes[i];
                }
                hello.hello_t.checksum = checksum;
                Serial.println("Hello from AIO");
                Serial.print("\t");
                for (int i = 0; i < sizeof(Hello_t); i++){
                  Serial.print(hello.bytes[i]);
                  Serial.print(" ");
                }
                Serial.println();
              break;
            case 201:
              progData.ips[0] = packet.data()[7];
              progData.ips[1] = packet.data()[8];
              progData.ips[2] = packet.data()[9];
              config.updateConfig();
              ESP.restart();
              break;
            
                break;
              }
              
        
        }
      });
    }

    void udpCheck(){    // 📌  udpCheck 📝 🗑️
      if (millis() - progData.udpTimer < udpTimeout){
        progData.AOGconnected == 1;
      } else
      {
        progData.AOGconnected == 2;
      }
      
    }

    

    void sendHeartbeat(){
      // TODO: add heartbeat
      if (millis()-heartbeatTimePrevious > heartbeatTimeTrip){
        heartbeatTimePrevious = millis();
        
        hbData->isConnectedAOG = progData.AOGconnected;
        hbData->isConnectedCAN1 = progData.can1connected;
        hbData->isConnectedCAN2 = progData.can2connected;
        hbData->isConnectedINA219 = progData.ina219Connected;
        
        
        // int cksum=0;
        // for (int i=2;i<=sizeof(Heartbeat_t)-1;i++)
        // {
        //   cksum += heartbeat.bytes[i];
        //   // Serial.print(heartbeat.bytes[i]);
        //   // Serial.print(" ");
        // }
        udp.writeTo(heartbeat.bytes,sizeof(Heartbeat_t),IPAddress(progData.ips[0],progData.ips[1],progData.ips[2],255),9999);
        
      }
    }
 
    void sendShutdown(){
      shutdownMsg.shutdown_t.aogByte1 = 0x80;
      shutdownMsg.shutdown_t.aogByte2 = 0x81;
      shutdownMsg.shutdown_t.PGN = 149;
      shutdownMsg.shutdown_t.length = 3;
      shutdownMsg.shutdown_t.myPGN = 3;
      shutdownMsg.shutdown_t.confirm = 1;
      // udp.writeTo(shutdown.bytes,sizeof(Shutdown_t),IPAddress(Config.ips[0],Config.ips[1],Config.ips[2],255),9999);
    }
};
UDPMethods udpMethods = UDPMethods();

#pragma region GPS
#define RAD_TO_DEG_X_10 572.95779513082320876798154814105
#define REPORT_INTERVAL 20
uint32_t print_timer = 0;
uint32_t READ_BNO_TIME = 0; 
bool GGA_Available = false;    //Do we have GGA on correct port?
const char* asciiHex = "0123456789ABCDEF";
bool passThroughGPS = false;
bool passThroughGPS2 = false;
uint32_t gpsReadyTime = 0;        //Used for GGA timeout

float roll = 0;
float pitch = 0;
float yaw = 0;
//Fusing BNO with Dual
double rollDelta;
double rollDeltaSmooth;
double correctionHeading;
double gyroDelta;
double imuGPS_Offset;
double gpsHeading;
double imuCorrected;
#define twoPI 6.28318530717958647692
#define PIBy2 1.57079632679489661923
// the new PANDA sentence buffer
char nmea[100];

// GGA
char fixTime[12];
char latitude[15];
char latNS[3];
char longitude[15];
char lonEW[3];
char fixQuality[2];
char numSats[4];
char HDOP[5];
char altitude[12];
char ageDGPS[10];

// VTG
char vtgHeading[12] = { };
char speedKnots[10] = { };

// IMU
char imuHeading[6];
char imuRoll[6];
char imuPitch[6];
char imuYawRate[6];
const bool invertRoll= true;  //Used for IMU with dual antenna
//Variables for settings - 0 is false
struct Setup {
  uint8_t InvertWAS = 0;
  uint8_t IsRelayActiveHigh = 0;    // if zero, active low (default)
  uint8_t MotorDriveDirection = 0;
  uint8_t SingleInputWAS = 1;
  uint8_t CytronDriver = 1;
  uint8_t SteerSwitch = 0;          // 1 if switch selected
  uint8_t SteerButton = 0;          // 1 if button selected
  uint8_t ShaftEncoder = 0;
  uint8_t PressureSensor = 0;
  uint8_t CurrentSensor = 0;
  uint8_t PulseCountMax = 5;
  uint8_t IsDanfoss = 0;
  uint8_t IsUseY_Axis = 0;     //Set to 0 to use X Axis, 1 to use Y avis
}; Setup steerConfig;               // 9 bytes
bool useBNO08x = false;
const uint8_t bno08xAddresses[] = { 0x4A, 0x4B };
const int16_t nrBNO08xAdresses = sizeof(bno08xAddresses) / sizeof(bno08xAddresses[0]);
uint8_t bno08xAddress;

void CalculateChecksum(void)
{
  int16_t sum = 0;
  int16_t inx = 0;
  char tmp;

  // The checksum calc starts after '$' and ends before '*'
  for (inx = 1; inx < 200; inx++)
  {
    tmp = nmea[inx];

    // * Indicates end of data and start of checksum
    if (tmp == '*')
    {
      break;
    }

    sum ^= tmp;    // Build checksum
  }

  byte chk = (sum >> 4);
  char hex[2] = { asciiHex[chk], 0 };
  strcat(nmea, hex);

  chk = (sum % 16);
  char hex2[2] = { asciiHex[chk], 0 };
  strcat(nmea, hex2);
}

// If odd characters showed up.
void errorHandler()
{
  //nothing at the moment
}

void readBNO()
{ 
          // Serial.println("readBNO");
          if (bno08x.dataAvailable() == true)
        {
            // Serial.println("readBNO conditional");
            float dqx, dqy, dqz, dqw, dacr;
            uint8_t dac;

            //get quaternion
            bno08x.getQuat(dqx, dqy, dqz, dqw, dacr, dac);
/*            
            while (bno08x.dataAvailable() == true)
            {
                //get quaternion
                bno08x.getQuat(dqx, dqy, dqz, dqw, dacr, dac);
                //Serial.println("Whiling");
                //Serial.print(dqx, 4);
                //Serial.print(F(","));
                //Serial.print(dqy, 4);
                //Serial.print(F(","));
                //Serial.print(dqz, 4);
                //Serial.print(F(","));
                //Serial.println(dqw, 4);
            }
            //Serial.println("End of while");
*/            
            float norm = sqrt(dqw * dqw + dqx * dqx + dqy * dqy + dqz * dqz);
            dqw = dqw / norm;
            dqx = dqx / norm;
            dqy = dqy / norm;
            dqz = dqz / norm;

            float ysqr = dqy * dqy;

            // yaw (z-axis rotation)
            float t3 = +2.0 * (dqw * dqz + dqx * dqy);
            float t4 = +1.0 - 2.0 * (ysqr + dqz * dqz);
            yaw = atan2(t3, t4);

            // Convert yaw to degrees x10
            correctionHeading = -yaw;
            yaw = (int16_t)((yaw * -RAD_TO_DEG_X_10));
            if (yaw < 0) yaw += 3600;

            // pitch (y-axis rotation)
            float t2 = +2.0 * (dqw * dqy - dqz * dqx);
            t2 = t2 > 1.0 ? 1.0 : t2;
            t2 = t2 < -1.0 ? -1.0 : t2;
//            pitch = asin(t2) * RAD_TO_DEG_X_10;

            // roll (x-axis rotation)
            float t0 = +2.0 * (dqw * dqx + dqy * dqz);
            float t1 = +1.0 - 2.0 * (dqx * dqx + ysqr);
//            roll = atan2(t0, t1) * RAD_TO_DEG_X_10;

            if(steerConfig.IsUseY_Axis)
            {
              roll = asin(t2) * RAD_TO_DEG_X_10;
              pitch = atan2(t0, t1) * RAD_TO_DEG_X_10;
            }
            else
            {
              pitch = asin(t2) * RAD_TO_DEG_X_10;
              roll = atan2(t0, t1) * RAD_TO_DEG_X_10;
            }
            
            if(invertRoll)
            {
              roll *= -1;
            }
            // Serial.println(roll);
        }
}

void imuHandler()
{
    int16_t temp = 0;
    // Serial.println(useBNO08x);
      if (useBNO08x)
      {
          // Serial.println("insert data");
          //BNO is reading in its own timer    
          // Fill rest of Panda Sentence - Heading
          temp = yaw;
          itoa(temp, imuHeading, 10);

          // the pitch x10
          temp = (int16_t)pitch;
          itoa(temp, imuPitch, 10);

          // the roll x10
          temp = (int16_t)roll;
          itoa(temp, imuRoll, 10);

          // YawRate - 0 for now
          itoa(0, imuYawRate, 10);
      }
    

}

void BuildNmea(void)
{
    strcpy(nmea, "");

    
    strcat(nmea, "$PANDA,");

    strcat(nmea, fixTime);
    strcat(nmea, ",");

    strcat(nmea, latitude);
    strcat(nmea, ",");

    strcat(nmea, latNS);
    strcat(nmea, ",");

    strcat(nmea, longitude);
    strcat(nmea, ",");

    strcat(nmea, lonEW);
    strcat(nmea, ",");

    // 6
    strcat(nmea, fixQuality);
    strcat(nmea, ",");

    strcat(nmea, numSats);
    strcat(nmea, ",");

    strcat(nmea, HDOP);
    strcat(nmea, ",");

    strcat(nmea, altitude);
    strcat(nmea, ",");

    //10
    strcat(nmea, ageDGPS);
    strcat(nmea, ",");

    //11
    strcat(nmea, speedKnots);
    strcat(nmea, ",");

    //12
    strcat(nmea, imuHeading);
    strcat(nmea, ",");

    //13
    strcat(nmea, imuRoll);
    strcat(nmea, ",");

    //14
    strcat(nmea, imuPitch);
    strcat(nmea, ",");

    //15
    strcat(nmea, imuYawRate);

    strcat(nmea, "*");

    CalculateChecksum();

    strcat(nmea, "\r\n");

    if (!passThroughGPS && !passThroughGPS2)
    {
        if (millis() - print_timer > 100){
          print_timer = millis();
          Serial.println(nmea); 
        }
         //Always send USB GPS data
    }

    if (progData.wifiConnected == 1)   //If ethernet running send the GPS there
    {
        int len = strlen(nmea);
        // udpMethods.udp.writeTo(nmea,len,IPAddress(progData.ips[0],progData.ips[1],progData.ips[2],255),9999);
        udpMethods.udp.broadcastTo(nmea,9999);
        // Eth_udpPAOGI.beginPacket(Eth_ipDestination, portDestination);
        // Eth_udpPAOGI.write(nmea, len);
        // Eth_udpPAOGI.endPacket();
    }
}



/*
  $PANDA
  (1) Time of fix

  position
  (2,3) 4807.038,N Latitude 48 deg 07.038' N
  (4,5) 01131.000,E Longitude 11 deg 31.000' E

  (6) 1 Fix quality:
    0 = invalid
    1 = GPS fix(SPS)
    2 = DGPS fix
    3 = PPS fix
    4 = Real Time Kinematic
    5 = Float RTK
    6 = estimated(dead reckoning)(2.3 feature)
    7 = Manual input mode
    8 = Simulation mode
  (7) Number of satellites being tracked
  (8) 0.9 Horizontal dilution of position
  (9) 545.4 Altitude (ALWAYS in Meters, above mean sea level)
  (10) 1.2 time in seconds since last DGPS update
  (11) Speed in knots

  FROM IMU:
  (12) Heading in degrees
  (13) Roll angle in degrees(positive roll = right leaning - right down, left up)

  (14) Pitch angle in degrees(Positive pitch = nose up)
  (15) Yaw Rate in Degrees / second

  CHKSUM
*/

/*
  $GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M ,  ,*47
   0     1      2      3    4      5 6  7  8   9    10 11  12 13  14
        Time      Lat       Lon     FixSatsOP Alt
  Where:
     GGA          Global Positioning System Fix Data
     123519       Fix taken at 12:35:19 UTC
     4807.038,N   Latitude 48 deg 07.038' N
     01131.000,E  Longitude 11 deg 31.000' E
     1            Fix quality: 0 = invalid
                               1 = GPS fix (SPS)
                               2 = DGPS fix
                               3 = PPS fix
                               4 = Real Time Kinematic
                               5 = Float RTK
                               6 = estimated (dead reckoning) (2.3 feature)
                               7 = Manual input mode
                               8 = Simulation mode
     08           Number of satellites being tracked
     0.9          Horizontal dilution of position
     545.4,M      Altitude, Meters, above mean sea level
     46.9,M       Height of geoid (mean sea level) above WGS84
                      ellipsoid
     (empty field) time in seconds since last DGPS update
     (empty field) DGPS station ID number
      47          the checksum data, always begins with


  $GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A
  0      1    2   3      4    5      6   7     8     9     10   11
        Time      Lat        Lon       knots  Ang   Date  MagV

  Where:
     RMC          Recommended Minimum sentence C
     123519       Fix taken at 12:35:19 UTC
     A            Status A=active or V=Void.
     4807.038,N   Latitude 48 deg 07.038' N
     01131.000,E  Longitude 11 deg 31.000' E
     022.4        Speed over the ground in knots
     084.4        Track angle in degrees True
     230394       Date - 23rd of March 1994
     003.1,W      Magnetic Variation
      6A          The checksum data, always begins with

  $GPVTG,054.7,T,034.4,M,005.5,N,010.2,K*48

    VTG          Track made good and ground speed
    054.7,T      True track made good (degrees)
    034.4,M      Magnetic track made good
    005.5,N      Ground speed, knots
    010.2,K      Ground speed, Kilometers per hour
     48          Checksum
*/

void VTG_Handler()
{
  // vtg heading
  parser.getArg(0, vtgHeading);

  // vtg Speed knots
  parser.getArg(4, speedKnots);


}

void defaulthandler(){
  Serial.println("here we are");
}

void GGA_Handler() //Rec'd GGA
{
    // fix time
    parser.getArg(0, fixTime);

    // latitude
    parser.getArg(1, latitude);
    parser.getArg(2, latNS);

    // longitude
    parser.getArg(3, longitude);
    // Serial.println(longitude);
    parser.getArg(4, lonEW);

    // fix quality
    parser.getArg(5, fixQuality);

    // satellite #
    parser.getArg(6, numSats);

    // HDOP
    parser.getArg(7, HDOP);

    // altitude
    parser.getArg(8, altitude);

    // time of last DGPS update
    parser.getArg(12, ageDGPS);

    
    GGA_Available = true;
    Serial.println("parsed");
    
    if (useBNO08x)
    {
       imuHandler();          //Get IMU data ready
       BuildNmea();           //Build & send data GPS data to AgIO (Both Dual & Single)
       
    }
    else if (!useBNO08x) 
    {
        
        itoa(65535, imuHeading, 10);       //65535 is max value to stop AgOpen using IMU in Panda
        BuildNmea();
    }
    
    gpsReadyTime = millis();    //Used for GGA timeout (LED's ETC) 
}


#pragma endregion

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
// #if defined(LED_BUILTIN) // flicker LED on update
// 	static int status = LOW;
// 	status = status == LOW && offset < totallength ? HIGH : LOW;
// 	digitalWrite(LED_BUILTIN, status);
// #endif
}


void softwareUpdate(){
  char basePath[] = "/%s/Releases/OTA_Config.json";
  char CONFIG_URL[150];
  sprintf(CONFIG_URL, basePath, progData.sketchConfig);
  Serial.println(CONFIG_URL);
  

  char SERVER[150];
  sprintf(SERVER, "http://%d.%d.%d.%d:%d",progData.ips[0],progData.ips[1],progData.ips[2],progData.serverAddress,progData.serverPort);

  Serial.print("CONFIG_URL: ");
  Serial.println(CONFIG_URL);
  Serial.print("SERVER: ");
  Serial.println(SERVER);
  
  ota.SetConfig(progData.sketchConfig);
  ota.SetCallback(OtaPullCallback);
  
  Serial.printf("We are running version %s of the sketch, Board='%s', Device='%s', IP='%s \n", VERSION, ARDUINO_BOARD, WiFi.macAddress().c_str(),(String)(WiFi.localIP()[3]));
  Serial.println();
  // Serial.printf("Checking %s to see if an update is available...\n", CONFIG_URL);
  Serial.println();
  int ret = ota.CheckForOTAUpdate(SERVER, CONFIG_URL, VERSION);
  Serial.printf("CheckForOTAUpdate returned %d (%s)\n\n", ret, errtext(ret));
}

#pragma endregion
#pragma region Webserver

void updateDebugVars() {
  debugVars.clear(); // Clear the list to update it dynamically
  debugVars.push_back("Timestamp since boot [ms]: " + String(millis()));
  debugVars.push_back("Free Heap: " + String(ESP.getFreeHeap()) + " bytes");
  float tempReading;
  temp_sensor_read_celsius(&tempReading);
  debugVars.push_back("Temp: " + String(tempReading));
  debugVars.push_back("Version: " + String(VERSION));
  debugVars.push_back("Wifi IP: " + String(WiFi.SSID()));
  debugVars.push_back("Wifi State: " + String(progData.wifiConnected));
  debugVars.push_back("IP Address: " + String(progData.ips[0])+"."+String(progData.ips[1])+"."+String(progData.ips[2])+"."+String(progData.ips[3]));
  debugVars.push_back("NMEA: " + String(nmea));
  debugVars.push_back("Ntrip Timestamp: " + String(udpMethods.ntripTimestamp));
  debugVars.push_back("Program State: " + String(progData.programState));
  std::string ipValue = "Sensor: " + std::to_string(WiFi.localIP());
  // bleRemote.sendIPData(ipValue);d
}


// Function to serve the debug variables as JSON
void handleDebugVars(AsyncWebServerRequest *request) {
  updateDebugVars();  // Update the debug variables just before sending
  DynamicJsonDocument doc(1024);
  JsonArray array = doc.to<JsonArray>();
  
  for (const auto& var : debugVars) {
    array.add(var);
  }
  
  String jsonResponse;
  serializeJson(doc, jsonResponse);
  request->send(200, "application/json", jsonResponse);
}


// Function to serve the file list as JSON
void handleFileList(AsyncWebServerRequest *request) {
  DynamicJsonDocument doc(1024);
  JsonArray array = doc.to<JsonArray>();

  // Open LittleFS root directory and list files
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

// Reboot handler
void handleReboot(AsyncWebServerRequest *request) {
  request->send(200, "text/plain", "Rebooting...");
  delay(100); // Give some time for the response to be sent
  ESP.restart();
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


#pragma endregion

void setup(){
  Serial.begin(115200);
  
  config.begin();
  myLED.startTask();
  progData.programState = 1;
  bleRemote.initBLE("GPS_Receiver");
  while (true){
    progData.wifiConnected = myWifi.connect(progData.ips, progData.sketchConfig);
    if (progData.wifiConnected == 1){
      
      progData.programState = 3;
      softwareUpdate();
      progData.programState = 1;
      udpMethods.begin();
      #pragma region Server Setup
        // Serve the main HTML page
        server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
          request->send(LittleFS, "/index.html");
        });
        // Route to get debug variables as JSON
        server.on("/getDebugVars", HTTP_GET, handleDebugVars);
        // Route to list files as JSON
        server.on("/listFiles", HTTP_GET, handleFileList);
        // Route to download files
        server.on("/download", HTTP_GET, handleFileDownload);

        // Handle file upload
        server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request) {}, handleFileUpload);
        server.on("/update", HTTP_POST, [](AsyncWebServerRequest *request) {}, 
        handleFirmwareUpload);
        server.on("/reboot", HTTP_GET, handleReboot);
        // Start server
        server.begin();
      #pragma endregion
      break;
    }
  }
  twoWire.setPins(SDA_0,SCL_0);
  twoWire.begin();
  uint8_t error;
  for (int16_t i = 0; i < nrBNO08xAdresses; i++)
      {
          bno08xAddress = bno08xAddresses[i];

          //Serial.print("\r\nChecking for BNO08X on ");
          //Serial.println(bno08xAddress, HEX);
          twoWire.beginTransmission(bno08xAddress);
          error = twoWire.endTransmission();

          if (error == 0)
          {
              //Serial.println("Error = 0");
              Serial.print("0x");
              Serial.print(bno08xAddress, HEX);
              Serial.println(" BNO08X Ok.");

              // Initialize BNO080 lib
              if (bno08x.begin(bno08xAddress, twoWire)) //??? Passing NULL to non pointer argument, remove maybe ???
              {
                  //Increase I2C data rate to 400kHz
                  twoWire.setClock(400000); 

                  delay(300);

                  // Use gameRotationVector and set REPORT_INTERVAL
                  bno08x.enableGameRotationVector(REPORT_INTERVAL);
                  useBNO08x = true;
              }
              else
              {
                  Serial.println("BNO080 not detected at given I2C address.");
              }
          }
          else
          {
              //Serial.println("Error = 4");
              Serial.print("0x");
              Serial.print(bno08xAddress, HEX);
              Serial.println(" BNO08X not Connected or Found");
          }
          if (useBNO08x) break;
      }

  parser.setErrorHandler(errorHandler);
  parser.addHandler("G-GGA", GGA_Handler);
  parser.addHandler("G-VTG", VTG_Handler);
  // parser.setDefaultHandler(defaulthandler);
  Serial2.setPins(19,20);
  Serial2.begin(460800);
  Serial.println("Setup Complete");
  Serial.println(useBNO08x);
  progData.programState = 1;
}


void loop(){
  if (Serial2.available())
  
  // Serial.println(Serial2.readStringUntil('\n'));
  {
      parser << Serial2.read();
      // Serial.println("read serial2");
      // BuildNmea();
      
  }
  if((millis() - READ_BNO_TIME) > REPORT_INTERVAL && useBNO08x)
  {
    READ_BNO_TIME = millis();
    readBNO();
    // Serial.println("reading BNO");
    // BuildNmea();
  }
}