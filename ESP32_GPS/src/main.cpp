/**
 * @file main.cpp
 * @brief ESP32-S3 GPS / NTRIP / IMU Module
 *
 * @details This module runs on an ESP32-S3 Xiao and provides the GPS,
 *          NTRIP corrections, IMU, and PANDA-sentence functions that the ESP32_AIO
 *          normally handles, but as a standalone, dedicated GPS module.
 *
 *          Key capabilities:
 *          - WiFi Access Point "NOLTE_FARM" (always active) plus optional STA uplink
 *          - UM980 GNSS receiver on UART1 (RX=GPIO8, TX=GPIO7) at 460800 baud
 *          - BNO08x IMU via I2C (SDA=GPIO6, SCL=GPIO5)
 *          - NTRIP RTCM corrections received on UDP port 2233 and forwarded to GPS UART
 *          - NMEA GGA / VTG parsing → PANDA sentence generation
 *          - PANDA sentences broadcast on UDP port 9999 to all AP clients
 *          - AgIO hello / IP-update response on UDP port 8888
 *          - Web interface (LittleFS) for status monitoring and OTA updates
 *
 * @author  Steve Nolte
 * @date    2025
 * @version see include/Version.h
 */

#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <ESPmDNS.h>
#include <AsyncUDP.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Update.h>
// #include "Adafruit_NeoPixel.h"  // Disabled - no NeoPixel on Xiao S3
#include <Adafruit_BNO08x.h>
#include "SparkFun_Unicore_GNSS_Arduino_Library.h"
#include "Version.h"

// ── Pin definitions ────────────────────────────────────────────────────────

/// Built-in RGB NeoPixel on ESP32-S3-DevKitC-1 (not present on Xiao)
// #define LED_PIN       48  // Disabled - Xiao S3 has no NeoPixel

/// Power Relay control
#define POWER_RELAY_PIN  1

/// GPS UART1 – receives NMEA from UM980 TX
#define GPS_RX_PIN    7
/// GPS UART1 – transmits commands / NTRIP to UM980 RX
#define GPS_TX_PIN    8

/// IMU I2C pins
#define BNO_SDA_PIN   3
#define BNO_SCL_PIN   4
/// BNO08x I2C address
#define BNO08X_I2CADDR_DEFAULT 0x4A

// ── WiFi / network constants ───────────────────────────────────────────────

#define AP_SSID       "NOLTE_FARM"
#define AP_PASSWORD   "DontLoseMoney89"
#define AP_CHANNEL    6
#define AP_MAX_CLIENTS 8

#define STA_DEFAULT_SSID     "SSEI"
#define STA_DEFAULT_PASSWORD "Nd14il!la"

/// Static AP IP address (192.168.5.1)
#define AP_IP_1  192
#define AP_IP_2  168
#define AP_IP_3    5
#define AP_IP_4    1

/// Broadcast address for the AP subnet (.255)
#define AP_BCAST_4  255

/// AgIO main UDP port
#define PORT_AGIO   8888
/// PANDA / NMEA broadcast port
#define PORT_GPS    9999
/// NTRIP corrections input port
#define PORT_NTRIP  2233

// ── GPS / IMU data ─────────────────────────────────────────────────────────

/**
 * @brief Runtime GPS and IMU state shared between the GPS FreeRTOS task
 *        and the web/UDP handlers running on the Arduino task.
 *
 * @note  Fields read by multiple tasks are volatile-qualified or protected
 *        by the fact that char arrays are written atomically enough for
 *        simple diagnostic display.  For the PANDA builder the entire
 *        snapshot is taken inside the GPS task before transmission.
 */
struct GPSState {
    // ── GGA fields ──
    char fixTime[12]   = "";   ///< UTC time of fix (HHMMSS.SS)
    char latitude[15]  = "";   ///< Latitude in DDDMM.MMMMM
    char latNS[3]      = "";   ///< Hemisphere: N or S
    char longitude[15] = "";   ///< Longitude in DDDMM.MMMMM
    char lonEW[3]      = "";   ///< Hemisphere: E or W
    char fixQuality[3] = "";   ///< Fix quality 0–8
    char numSats[4]    = "";   ///< Satellites in use
    char HDOP[6]       = "";   ///< Horizontal dilution of precision
    char altitude[12]  = "";   ///< Altitude above MSL (metres)
    char ageDGPS[10]   = "0";  ///< Age of DGPS correction (seconds)

    // ── VTG fields ──
    char speedKnots[10] = "";  ///< Ground speed in knots
    char vtgHeading[12] = "";  ///< True course over ground (degrees)

    // ── IMU fields (values × 10, stored as integer strings) ──
    char imuHeading[8]  = "";  ///< Normalised yaw 0–3600 (× 10)
    char imuRoll[8]     = "";  ///< Roll angle × 10
    char imuPitch[8]    = "";  ///< Pitch angle × 10
    char imuYawRate[8]  = "";  ///< Yaw rate (°/s) × 10

    // ── Built PANDA sentence ──
    char nmea[160] = "";

    // ── State flags ──
    uint8_t  gpsState    = 0;   ///< 0=initialising, 1=ok, 2=failed
    uint8_t  imuState    = 0;   ///< 0=initialising, 1=ok, 2=failed
    bool     disableHeading  = false; ///< Send 0 in heading field
    bool     invertRoll      = true;  ///< Negate roll (dual-antenna default)
    bool     flipPitchRoll   = true;  ///< Swap pitch and roll axes
    volatile bool enablePandaBroadcast = true;  ///< Broadcast PANDA on UDP 9999
    volatile bool useRawNMEA = false;           ///< Send raw NMEA instead of PANDA
    volatile bool enableGpsLogging = false;     ///< Log GPS data to file

    // ── Statistics ──
    volatile uint32_t pandaCount  = 0;
    volatile uint32_t ntripCount  = 0;
    volatile uint64_t ntripBytes  = 0;
    volatile uint32_t ggaCount    = 0;
    volatile uint32_t vtgCount    = 0;
    uint32_t firstPandaMs = 0;
    uint32_t lastPandaMs  = 0;
    volatile uint32_t firstNtripMs = 0;
    volatile uint32_t lastNtripMs  = 0;

    // ── Network subnet for broadcast (updated from AgIO PGN 201) ──
    /// First 3 octets are the subnet; broadcast address is always agioSubnet.255
    uint8_t agioSubnet[3] = {AP_IP_1, AP_IP_2, AP_IP_3};

    // ── IMU watchdog ──
    uint32_t imuLastMsgMs = 0;  ///< millis() of last valid IMU frame
} gpsState;

// Yaw-rate calculation helpers (used only inside GPS task)
static float    prevYaw          = 0.0f;
static uint32_t prevYawMs        = 0;
static bool     yawInitialized   = false;

// ── Hardware objects ───────────────────────────────────────────────────────

HardwareSerial gpsSerial(1);       ///< UART1 – UM980 GPS receiver

UM980             myGNSS;          ///< SparkFun UM980 driver
Adafruit_BNO08x   bno08x(-1);      ///< Adafruit BNO08x I2C driver
sh2_SensorValue_t sensorValue;     ///< BNO08x sensor value buffer

// Adafruit_NeoPixel pixel(1, LED_PIN, NEO_GRB + NEO_KHZ800);  // Disabled - no NeoPixel on Xiao

AsyncWebServer server(80);
AsyncUDP       udpGPS;             ///< Port 9999 – PANDA broadcast
AsyncUDP       udpAIO;             ///< Port 8888 – AgIO comms
AsyncUDP       udpNtrip;           ///< Port 2233 – NTRIP forwarding

// ── Shared debug variables ─────────────────────────────────────────────────

std::vector<String> debugVars;

// ── Forward declarations ───────────────────────────────────────────────────

static void sendIMUStatus();
static void sendSubnetAnnouncement();

// ── LED helpers ────────────────────────────────────────────────────────────

static void setLED(uint8_t r, uint8_t g, uint8_t b) {
    // Disabled - Xiao S3 has no built-in NeoPixel
    // pixel.setPixelColor(0, pixel.Color(r, g, b));
    // pixel.show();
}

// ── NMEA / PANDA helpers ───────────────────────────────────────────────────

/**
 * @brief Validate GPS coordinates to prevent sending bad data.
 *
 * @details Checks fix quality, coordinate format, and valid ranges.
 * @return true if coordinates are valid and safe to broadcast
 */
static bool isGpsPositionValid() {
    // Must have at least a GPS fix
    int fixQuality = atoi(gpsState.fixQuality);
    if (fixQuality < 1) return false;
    
    // Must have coordinate strings with minimum length
    if (strlen(gpsState.latitude) < 4 || strlen(gpsState.longitude) < 4) return false;
    if (strlen(gpsState.latNS) < 1 || strlen(gpsState.lonEW) < 1) return false;
    
    // Validate direction indicators
    if (gpsState.latNS[0] != 'N' && gpsState.latNS[0] != 'S') return false;
    if (gpsState.lonEW[0] != 'E' && gpsState.lonEW[0] != 'W') return false;
    
    // Validate lat/lon contain ONLY digits and decimal points (no letters)
    for (size_t i = 0; i < strlen(gpsState.latitude); i++) {
        if (!isdigit(gpsState.latitude[i]) && gpsState.latitude[i] != '.') {
            Serial.printf("GPS: Invalid character '%c' in latitude\n", gpsState.latitude[i]);
            return false;
        }
    }
    for (size_t i = 0; i < strlen(gpsState.longitude); i++) {
        if (!isdigit(gpsState.longitude[i]) && gpsState.longitude[i] != '.') {
            Serial.printf("GPS: Invalid character '%c' in longitude\n", gpsState.longitude[i]);
            return false;
        }
    }
    
    // Parse and validate latitude
    double lat = atof(gpsState.latitude);
    int deg = (int)(lat / 100);
    double min = lat - (deg * 100);
    double decLat = deg + (min / 60.0);
    if (gpsState.latNS[0] == 'S') decLat = -decLat;
    
    // Parse and validate longitude
    double lon = atof(gpsState.longitude);
    deg = (int)(lon / 100);
    min = lon - (deg * 100);
    double decLon = deg + (min / 60.0);
    if (gpsState.lonEW[0] == 'W') decLon = -decLon;
    
    // Check ranges and valid numbers
    if (decLat < -90.0 || decLat > 90.0) return false;
    if (decLon < -180.0 || decLon > 180.0) return false;
    if (isnan(decLat) || isnan(decLon)) return false;
    if (isinf(decLat) || isinf(decLon)) return false;
    
    return true;
}

/**
 * @brief Log GPS position data to file.
 *
 * @details Appends CSV-formatted GPS data to /gpslog.csv with timestamp,
 *          coordinates in decimal degrees, fix quality, and other metrics.
 *          Creates local copies of strings to avoid race conditions.
 */
static void logGpsData() {
    if (!gpsState.enableGpsLogging) return;
    if (!isGpsPositionValid()) return;
    
    // Create local copies to avoid race conditions with NMEA parser
    char lat[16], lon[16], latNS[2], lonEW[2];
    char alt[12], fixQ[4], sats[4], hdop[8], speed[12], heading[8];
    
    strncpy(lat, gpsState.latitude, sizeof(lat) - 1);
    strncpy(lon, gpsState.longitude, sizeof(lon) - 1);
    strncpy(latNS, gpsState.latNS, sizeof(latNS) - 1);
    strncpy(lonEW, gpsState.lonEW, sizeof(lonEW) - 1);
    strncpy(alt, gpsState.altitude, sizeof(alt) - 1);
    strncpy(fixQ, gpsState.fixQuality, sizeof(fixQ) - 1);
    strncpy(sats, gpsState.numSats, sizeof(sats) - 1);
    strncpy(hdop, gpsState.HDOP, sizeof(hdop) - 1);
    strncpy(speed, gpsState.speedKnots, sizeof(speed) - 1);
    strncpy(heading, gpsState.imuHeading, sizeof(heading) - 1);
    
    lat[sizeof(lat)-1] = '\0';
    lon[sizeof(lon)-1] = '\0';
    latNS[sizeof(latNS)-1] = '\0';
    lonEW[sizeof(lonEW)-1] = '\0';
    alt[sizeof(alt)-1] = '\0';
    fixQ[sizeof(fixQ)-1] = '\0';
    sats[sizeof(sats)-1] = '\0';
    hdop[sizeof(hdop)-1] = '\0';
    speed[sizeof(speed)-1] = '\0';
    heading[sizeof(heading)-1] = '\0';
    
    // Validate NMEA format: latitude should be ddmm.mmmm (min 7 chars), longitude dddmm.mmmm (min 8 chars)
    if (strlen(lat) < 7 || strlen(lon) < 8) return;
    if (latNS[0] != 'N' && latNS[0] != 'S') return;
    if (lonEW[0] != 'E' && lonEW[0] != 'W') return;
    
    // Validate all characters in lat/lon are digits or decimal point
    for (size_t i = 0; i < strlen(lat); i++) {
        if (!isdigit(lat[i]) && lat[i] != '.') return;
    }
    for (size_t i = 0; i < strlen(lon); i++) {
        if (!isdigit(lon[i]) && lon[i] != '.') return;
    }
    
    // Parse coordinates to decimal degrees using local copies
    double latVal = atof(lat);
    double lonVal = atof(lon);
    
    // Validate NMEA ranges: lat degrees 0-90, lon degrees 0-180
    int latDeg = (int)(latVal / 100);
    double latMin = latVal - (latDeg * 100);
    int lonDeg = (int)(lonVal / 100);
    double lonMin = lonVal - (lonDeg * 100);
    
    if (latDeg < 0 || latDeg > 90) return;
    if (lonDeg < 0 || lonDeg > 180) return;
    if (latMin < 0 || latMin >= 60) return;
    if (lonMin < 0 || lonMin >= 60) return;
    
    double decLat = latDeg + (latMin / 60.0);
    if (latNS[0] == 'S') decLat = -decLat;
    
    double decLon = lonDeg + (lonMin / 60.0);
    if (lonEW[0] == 'W') decLon = -decLon;
    
    // Final validation of decimal degrees
    if (decLat < -90.0 || decLat > 90.0 || decLon < -180.0 || decLon > 180.0) {
        return; // Skip corrupted data
    }
    
    // Open file in append mode
    File logFile = LittleFS.open("/gpslog.csv", "a");
    if (!logFile) {
        Serial.println("Failed to open GPS log file");
        return;
    }
    
    // Write CSV line: timestamp,lat,lon,alt,fixQuality,sats,hdop,speed,heading
    char line[256];
    snprintf(line, sizeof(line), "%lu,%.7f,%.7f,%s,%s,%s,%s,%s,%s\n",
             millis(),
             decLat,
             decLon,
             alt,
             fixQ,
             sats,
             hdop,
             speed,
             heading);
    
    logFile.print(line);
    logFile.close();
}

/**
 * @brief Remove trailing CR, LF, and control characters from a C-string.
 */
static void cleanField(char* s) {
    if (!s) return;
    size_t n = strlen(s);
    for (size_t i = 0; i < n; i++) {
        if ((uint8_t)s[i] < 32 || s[i] == '\r' || s[i] == '\n') {
            s[i] = '\0';
            break;
        }
    }
    // Remove trailing spaces
    int end = (int)strlen(s) - 1;
    while (end >= 0 && s[end] == ' ') { s[end--] = '\0'; }
}

/**
 * @brief Parse a GGA sentence and populate gpsState GGA fields.
 * @param sentence  Null-terminated GGA sentence including leading '$'.
 */
static void parseGGA(const char* sentence) {
    char buf[160];
    strncpy(buf, sentence, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char* fields[16] = {};
    int   n = 0;
    char* tok = strtok(buf, ",*");
    while (tok && n < 16) { fields[n++] = tok; tok = strtok(nullptr, ",*"); }

    if (n < 10) return;

    // Clear all GGA fields before populating
    gpsState.fixTime[0] = gpsState.latitude[0] = gpsState.latNS[0]   = '\0';
    gpsState.longitude[0] = gpsState.lonEW[0]  = gpsState.fixQuality[0] = '\0';
    gpsState.numSats[0]   = gpsState.HDOP[0]   = gpsState.altitude[0]   = '\0';
    gpsState.ageDGPS[0]   = '\0';

    auto copy = [&](char* dst, size_t dstSz, int idx) {
        if (idx < n && fields[idx] && *fields[idx]) {
            strncpy(dst, fields[idx], dstSz - 1);
            dst[dstSz - 1] = '\0';
            cleanField(dst);
        }
    };

    copy(gpsState.fixTime,    sizeof(gpsState.fixTime),    1);
    copy(gpsState.latitude,   sizeof(gpsState.latitude),   2);
    copy(gpsState.latNS,      sizeof(gpsState.latNS),      3);
    copy(gpsState.longitude,  sizeof(gpsState.longitude),  4);
    copy(gpsState.lonEW,      sizeof(gpsState.lonEW),      5);
    copy(gpsState.fixQuality, sizeof(gpsState.fixQuality), 6);
    copy(gpsState.numSats,    sizeof(gpsState.numSats),    7);
    copy(gpsState.HDOP,       sizeof(gpsState.HDOP),       8);
    copy(gpsState.altitude,   sizeof(gpsState.altitude),   9);
    // Field 13 in GGA is age of DGPS corrections
    if (n > 13) {
        copy(gpsState.ageDGPS, sizeof(gpsState.ageDGPS), 13);
    }
    if (!*gpsState.ageDGPS) strcpy(gpsState.ageDGPS, "0");

    gpsState.ggaCount++;
}

/**
 * @brief Parse a VTG sentence and populate gpsState VTG fields.
 * @param sentence  Null-terminated VTG sentence including leading '$'.
 */
static void parseVTG(const char* sentence) {
    char buf[100];
    strncpy(buf, sentence, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char* fields[10] = {};
    int   n = 0;
    char* tok = strtok(buf, ",*");
    while (tok && n < 10) { fields[n++] = tok; tok = strtok(nullptr, ",*"); }

    if (n < 6) return;

    gpsState.speedKnots[0] = gpsState.vtgHeading[0] = '\0';

    // Field 1 – true course over ground
    if (fields[1] && *fields[1]) {
        strncpy(gpsState.vtgHeading, fields[1], sizeof(gpsState.vtgHeading) - 1);
        gpsState.vtgHeading[sizeof(gpsState.vtgHeading) - 1] = '\0';
        cleanField(gpsState.vtgHeading);
    }
    // Field 5 – speed in knots
    if (fields[5] && *fields[5]) {
        strncpy(gpsState.speedKnots, fields[5], sizeof(gpsState.speedKnots) - 1);
        gpsState.speedKnots[sizeof(gpsState.speedKnots) - 1] = '\0';
        cleanField(gpsState.speedKnots);
    }

    gpsState.vtgCount++;
}

/**
 * @brief Dispatch an NMEA sentence to the appropriate parser.
 */
static void parseNMEA(const char* sentence) {
    if (!sentence || sentence[0] != '$' || strlen(sentence) < 7) return;
    if      (strstr(sentence, "GGA") != nullptr) parseGGA(sentence);
    else if (strstr(sentence, "VTG") != nullptr) parseVTG(sentence);
}

/**
 * @brief Calculate and append the NMEA XOR checksum to gpsState.nmea.
 *        Expects the sentence to already contain a trailing '*'.
 */
static void calculateChecksum() {
    int  sum  = 0;
    int  len  = strlen(gpsState.nmea);
    for (int i = 1; i < len; i++) {
        if (gpsState.nmea[i] == '*') break;
        sum ^= (uint8_t)gpsState.nmea[i];
    }
    char hex[3];
    snprintf(hex, sizeof(hex), "%02X", (uint8_t)sum);
    strncat(gpsState.nmea, hex, sizeof(gpsState.nmea) - strlen(gpsState.nmea) - 1);
}

/**
 * @brief Assemble the $PANDA sentence from current GPS / IMU state.
 *
 * @details Field order matches the AgOpenGPS PANDA specification:
 *          $PANDA,time,lat,N/S,lon,E/W,quality,sats,hdop,alt,dgpsAge,
 *                 speed,heading,pitch,roll,yawRate,*XX\r\n
 */
static void buildPandaSentence() {
    strcpy(gpsState.nmea, "$PANDA,");

    strcat(gpsState.nmea, gpsState.fixTime);
    strcat(gpsState.nmea, ",");

    strcat(gpsState.nmea, gpsState.latitude);
    strcat(gpsState.nmea, ",");

    strcat(gpsState.nmea, gpsState.latNS);
    strcat(gpsState.nmea, ",");

    strcat(gpsState.nmea, gpsState.longitude);
    strcat(gpsState.nmea, ",");

    strcat(gpsState.nmea, gpsState.lonEW);
    strcat(gpsState.nmea, ",");

    strcat(gpsState.nmea, gpsState.fixQuality);
    strcat(gpsState.nmea, ",");

    strcat(gpsState.nmea, gpsState.numSats);
    strcat(gpsState.nmea, ",");

    strcat(gpsState.nmea, gpsState.HDOP);
    strcat(gpsState.nmea, ",");

    strcat(gpsState.nmea, gpsState.altitude);
    strcat(gpsState.nmea, ",");

    strcat(gpsState.nmea, gpsState.ageDGPS);
    strcat(gpsState.nmea, ",");

    strcat(gpsState.nmea, gpsState.speedKnots);
    strcat(gpsState.nmea, ",");

    // Heading field
    if (gpsState.disableHeading) {
        strcat(gpsState.nmea, "0");
    } else {
        strcat(gpsState.nmea, gpsState.imuHeading);
    }
    strcat(gpsState.nmea, ",");

    // IMPORTANT: Order is pitch, roll (not roll, pitch!)
    strcat(gpsState.nmea, gpsState.imuPitch);
    strcat(gpsState.nmea, ",");

    strcat(gpsState.nmea, gpsState.imuRoll);
    strcat(gpsState.nmea, ",");

    strcat(gpsState.nmea, gpsState.imuYawRate);

    strcat(gpsState.nmea, "*");

    calculateChecksum();
    strcat(gpsState.nmea, "\r\n");
}

// ── IMU handler ────────────────────────────────────────────────────────────

/**
 * @brief Read the latest BNO08x rotation vector and update gpsState IMU fields.
 *
 * @details Values are multiplied by 10 and stored as integer strings to match
 *          the AgOpenGPS PANDA specification.  Yaw rate is calculated from the
 *          difference between successive heading readings.
 *
 *          Called from the GPS task on every loop iteration to poll for new data.
 */
static void readIMU() {
    if (gpsState.imuState == 2) return;  // permanently failed

    // Poll for new sensor events
    if (!bno08x.getSensorEvent(&sensorValue)) {
        // No new data available; check watchdog
        if (gpsState.imuLastMsgMs > 0 &&
            millis() - gpsState.imuLastMsgMs > 2000) {
            gpsState.imuState = 2;
            Serial.println("IMU watchdog timeout – marking failed");
        }
        return;
    }

    // We only process rotation vector reports
    if (sensorValue.sensorId != SH2_ROTATION_VECTOR) {
        return;
    }

    gpsState.imuState     = 1;
    gpsState.imuLastMsgMs = millis();

    // Convert quaternion to Euler angles
    float qr = sensorValue.un.rotationVector.real;
    float qi = sensorValue.un.rotationVector.i;
    float qj = sensorValue.un.rotationVector.j;
    float qk = sensorValue.un.rotationVector.k;

    // Yaw (heading)
    float heading = atan2(2.0f * (qr * qk + qi * qj), 1.0f - 2.0f * (qj * qj + qk * qk)) * 57.2958f;
    // Pitch
    float pitch = asin(2.0f * (qr * qj - qk * qi)) * 57.2958f;
    // Roll
    float roll = atan2(2.0f * (qr * qi + qj * qk), 1.0f - 2.0f * (qi * qi + qj * qj)) * 57.2958f;

    if (gpsState.flipPitchRoll) {
        float tmp = pitch; pitch = roll; roll = tmp;
    }
    if (gpsState.invertRoll) {
        roll = -roll;
    }

    // Normalise heading to 0 – 360
    while (heading <    0.0f) heading += 360.0f;
    while (heading >= 360.0f) heading -= 360.0f;

    // Yaw rate (°/s)
    float    yawRate = 0.0f;
    uint32_t now     = millis();
    if (yawInitialized && prevYawMs > 0) {
        float dt    = (float)(now - prevYawMs) / 1000.0f;
        float delta = heading - prevYaw;
        if (delta >  180.0f) delta -= 360.0f;
        if (delta < -180.0f) delta += 360.0f;
        if (dt > 0.001f) {
            yawRate = delta / dt;
            if (yawRate >  500.0f) yawRate =  500.0f;
            if (yawRate < -500.0f) yawRate = -500.0f;
        }
    }
    prevYaw       = heading;
    prevYawMs     = now;
    yawInitialized = true;

    // Convert to integer × 10 and write strings atomically
    char tmpH[8], tmpP[8], tmpR[8], tmpY[8];
    snprintf(tmpH, sizeof(tmpH), "%d", (int16_t)(heading  * 10.0f));
    snprintf(tmpP, sizeof(tmpP), "%d", (int16_t)(pitch    * 10.0f));
    snprintf(tmpR, sizeof(tmpR), "%d", (int16_t)(roll     * 10.0f));
    snprintf(tmpY, sizeof(tmpY), "%d", (int16_t)(yawRate  * 10.0f));

    memcpy(gpsState.imuHeading,  tmpH, sizeof(tmpH));
    memcpy(gpsState.imuPitch,    tmpP, sizeof(tmpP));
    memcpy(gpsState.imuRoll,     tmpR, sizeof(tmpR));
    memcpy(gpsState.imuYawRate,  tmpY, sizeof(tmpY));
}

// ── GPS FreeRTOS task ──────────────────────────────────────────────────────

/**
 * @brief Main GPS processing task (pinned to Core 1).
 *
 * @details Reads raw bytes from the GPS UART, assembles complete NMEA
 *          sentences, calls the parsers, reads the IMU, builds the PANDA
 *          sentence, and broadcasts it over UDP once per GGA sentence.
 *
 * @param param  Unused (nullptr).
 */
static void gpsTask(void* param) {
    static char  buf[200];
    static int   idx      = 0;
    static uint32_t lastStatusMs = 0;
    static uint32_t lastSubnetMs = 0;

    Serial.println("GPS task started on core " + String(xPortGetCoreID()));

    while (true) {
        // ── Read IMU every iteration to keep buffer drained ──────────────
        readIMU();

        // ── Read GPS UART ─────────────────────────────────────────────────
        while (gpsSerial.available()) {
            char c = gpsSerial.read();

            if (idx >= (int)sizeof(buf) - 1) {
                // Buffer overflow – reset
                idx = 0;
                memset(buf, 0, sizeof(buf));
            }

            buf[idx++] = c;

            if (c == '\n') {
                buf[idx] = '\0';

                if (idx > 6) {
                    parseNMEA(buf);

                    // On GGA sentences: read latest IMU then build + send PANDA (or forward raw NMEA)
                    if (strstr(buf, "GGA") != nullptr) {
                        uint32_t now = millis();
                        readIMU();  // get the freshest IMU frame before PANDA
                        
                        // Only build and send if GPS position is valid
                        if (isGpsPositionValid()) {
                            const char* messageToSend = nullptr;
                            
                            if (gpsState.useRawNMEA) {
                                // Send raw NMEA sentence
                                messageToSend = buf;
                            } else {
                                // Build and send PANDA sentence
                                buildPandaSentence();
                                messageToSend = gpsState.nmea;
                            }

                            // Broadcast on the AP subnet (if enabled)
                            if (gpsState.enablePandaBroadcast && messageToSend) {
                                IPAddress bcast(gpsState.agioSubnet[0], gpsState.agioSubnet[1],
                                                gpsState.agioSubnet[2], 255);
                                udpGPS.writeTo((const uint8_t*)messageToSend,
                                               strlen(messageToSend), bcast, PORT_GPS);
                                taskYIELD();
                            }

                            gpsState.pandaCount++;
                            if (gpsState.pandaCount == 1) gpsState.firstPandaMs = now;
                            gpsState.lastPandaMs = now;

                            // Send IMU status to AgIO (if IMU is active)
                            sendIMUStatus();
                            
                            // Log GPS data if logging is enabled
                            logGpsData();
                        }
                    }
                }

                idx = 0;
                memset(buf, 0, sizeof(buf));
            }
        }

        // ── Periodic subnet announcement ──────────────────────────────────
        if (millis() - lastSubnetMs >= 1000) {
            lastSubnetMs = millis();
            sendSubnetAnnouncement();
        }

        // ── Periodic status print ─────────────────────────────────────────
        if (millis() - lastStatusMs >= 10000) {
            lastStatusMs = millis();
            Serial.printf("[GPS] GGA=%lu  VTG=%lu  PANDA=%lu  NTRIP=%lu  IMU=%s\n",
                          (unsigned long)gpsState.ggaCount,
                          (unsigned long)gpsState.vtgCount,
                          (unsigned long)gpsState.pandaCount,
                          (unsigned long)gpsState.ntripCount,
                          gpsState.imuState == 1 ? "ok" : (gpsState.imuState == 2 ? "fail" : "init"));
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

// ── UDP setup ──────────────────────────────────────────────────────────────

/**
 * @brief Start the GPS PANDA broadcast socket (port 9999).
 *        The socket is used for outgoing writes; no packet handler needed.
 */
static void startUDPgps() {
    udpGPS.listen(PORT_GPS);
    Serial.printf("UDP GPS broadcast socket ready on port %d\n", PORT_GPS);
}

/**
 * @brief Start the NTRIP forwarding listener on port 2233.
 *
 * @details Every incoming packet is written byte-for-byte to the GPS UART
 *          so the UM980 can apply RTCM corrections for RTK.
 */
static void startUDPntrip() {
    udpNtrip.listen(PORT_NTRIP);
    udpNtrip.onPacket([](AsyncUDPPacket pkt) {
        size_t   len  = pkt.length();
        uint8_t* data = pkt.data();

        // Write immediately to GPS UART for minimal latency
        size_t written = gpsSerial.write(data, len);
        if (written == len) {
            uint32_t now = millis();
            gpsState.ntripCount++;
            gpsState.ntripBytes += len;
            if (gpsState.ntripCount == 1) gpsState.firstNtripMs = now;
            gpsState.lastNtripMs = now;
        }

        static uint32_t lastPrint = 0;
        if (millis() - lastPrint > 5000) {
            lastPrint = millis();
            Serial.printf("[NTRIP] Forwarded packet #%lu (%zu B total)\n",
                          (unsigned long)gpsState.ntripCount,
                          (size_t)gpsState.ntripBytes);
        }
    });
    Serial.printf("NTRIP forwarder listening on UDP port %d\n", PORT_NTRIP);
}

/**
 * @brief Start the AgIO listener on port 8888.
 *
 * @details Handles:
 *          - PGN 200  Hello – reply with module presence + IMU presence
 *          - PGN 201  Set IP  – update broadcast subnet and restart
 */
static void startUDPaio() {
    udpAIO.listen(PORT_AGIO);
    udpAIO.onPacket([](AsyncUDPPacket pkt) {
        if (pkt.length() < 5) return;
        uint8_t* d = pkt.data();
        if (d[0] != 0x80 || d[1] != 0x81) return;

        switch (d[3]) {
            case 200: {
                // Hello from AgIO – send GPS module presence reply
                uint8_t reply[11] = {};
                reply[0] = 0x80;
                reply[1] = 0x81;
                reply[2] = AP_IP_4;
                reply[3] = AP_IP_4;
                reply[4] = 5;   // payload length
                reply[5] = 0;   // no WAS angle
                reply[6] = 0;
                reply[7] = 0;
                reply[8] = 0;
                reply[9] = 0;
                // Checksum
                uint8_t ck = 0;
                for (int i = 2; i < (int)reply[4] + 5; i++) ck += reply[i];
                reply[10] = ck;

                IPAddress bcast(gpsState.agioSubnet[0], gpsState.agioSubnet[1],
                                gpsState.agioSubnet[2], 255);
                udpAIO.writeTo(reply, sizeof(reply), bcast, PORT_AGIO);

                // Always send IMU-module hello (PGN 121) so AgIO knows module exists
                // IMU active state is conveyed via PGN 211 data packets
                reply[2] = 79;
                reply[3] = 121;
                ck = 0;
                for (int i = 2; i < (int)reply[4] + 5; i++) ck += reply[i];
                reply[10] = ck;
                udpAIO.writeTo(reply, sizeof(reply), bcast, PORT_AGIO);
                break;
            }
            case 201:
                // AgIO tells us its subnet – store and restart
                if (pkt.length() >= 10) {
                    gpsState.agioSubnet[0] = d[7];
                    gpsState.agioSubnet[1] = d[8];
                    gpsState.agioSubnet[2] = d[9];
                    // Our own module IP (last octet = AP_IP_4) is unchanged
                    Serial.printf("[AgIO] IP update: %d.%d.%d.x – rebooting\n",
                                  d[7], d[8], d[9]);
                    delay(200);
                    ESP.restart();
                }
                break;

            default:
                break;
        }
    });
    Serial.printf("AgIO listener ready on UDP port %d\n", PORT_AGIO);
}

/**
 * @brief Send IMU subnet announcement to AgIO.
 *
 * @details Sends PGN 203 (0xCB) packet with IMU module's IP address and subnet.
 *          Called periodically (~1 Hz) to maintain module presence in AgIO.
 */
static void sendSubnetAnnouncement() {
    // Build subnet announcement packet: PGN 203 (0xCB), 7 bytes payload
    // Packet format: [0x80][0x81][Src=79][PGN=203][Len=7][IP1][IP2][IP3][IP4][Sub1][Sub2][Sub3][CK]
    uint8_t subnetPacket[13] = {};
    subnetPacket[0] = 0x80;
    subnetPacket[1] = 0x81;
    subnetPacket[2] = 79;   // Source: IMU module ID
    subnetPacket[3] = 203;  // PGN 203 (0xCB): Subnet announcement
    subnetPacket[4] = 7;    // Payload length

    // IP address (4 bytes)
    subnetPacket[5] = gpsState.agioSubnet[0];
    subnetPacket[6] = gpsState.agioSubnet[1];
    subnetPacket[7] = gpsState.agioSubnet[2];
    subnetPacket[8] = AP_IP_4;  // Our IMU module IP last octet

    // Subnet (3 bytes)
    subnetPacket[9] = gpsState.agioSubnet[0];
    subnetPacket[10] = gpsState.agioSubnet[1];
    subnetPacket[11] = gpsState.agioSubnet[2];

    // Checksum
    uint8_t ck = 0;
    for (int i = 2; i < (int)subnetPacket[4] + 5; i++) ck += subnetPacket[i];
    subnetPacket[12] = ck;

    // Broadcast to AgIO
    IPAddress bcast(gpsState.agioSubnet[0], gpsState.agioSubnet[1],
                    gpsState.agioSubnet[2], 255);
    udpAIO.writeTo(subnetPacket, sizeof(subnetPacket), bcast, PORT_AGIO);
}

/**
 * @brief Send IMU data packet to AgIO.
 *
 * @details Sends PGN 211 (0xD3) packet with roll, pitch, heading, and yaw rate.
 *          Values are sent as int16 (degrees × 10 for angles, deg/s × 10 for rate).
 *          Called at 10Hz when GPS broadcasts (works with both PANDA and raw NMEA modes).
 *          In raw NMEA mode, this is the only way IMU data reaches AgOpenGPS.
 */
static void sendIMUStatus() {
    if (gpsState.imuState != 1) return;  // Only send if IMU is working

    // Parse current IMU values (stored as strings × 10)
    int16_t roll    = atoi(gpsState.imuRoll);
    int16_t pitch   = atoi(gpsState.imuPitch);
    int16_t heading = atoi(gpsState.imuHeading);
    int16_t yawRate = atoi(gpsState.imuYawRate);

    // Build IMU data packet: PGN 211 (0xD3), 8 bytes payload
    // Packet format: [0x80][0x81][Src=79][PGN=211][Len=8][Heading][Roll][Pitch][YawRate][CK]
    uint8_t imuPacket[14] = {};
    imuPacket[0] = 0x80;
    imuPacket[1] = 0x81;
    imuPacket[2] = 79;   // Source: IMU module ID
    imuPacket[3] = 211;  // PGN 211 (0xD3): IMU data
    imuPacket[4] = 8;    // Payload length

    // Pack int16 values as little-endian: heading, roll, pitch, yaw rate
    imuPacket[5] = heading & 0xFF;
    imuPacket[6] = (heading >> 8) & 0xFF;
    imuPacket[7] = roll & 0xFF;
    imuPacket[8] = (roll >> 8) & 0xFF;
    imuPacket[9] = pitch & 0xFF;
    imuPacket[10] = (pitch >> 8) & 0xFF;
    imuPacket[11] = yawRate & 0xFF;
    imuPacket[12] = (yawRate >> 8) & 0xFF;

    // Checksum
    uint8_t ck = 0;
    for (int i = 2; i < (int)imuPacket[4] + 5; i++) ck += imuPacket[i];
    imuPacket[13] = ck;

    // Broadcast to AgIO
    IPAddress bcast(gpsState.agioSubnet[0], gpsState.agioSubnet[1],
                    gpsState.agioSubnet[2], 255);
    udpAIO.writeTo(imuPacket, sizeof(imuPacket), bcast, PORT_AGIO);
}

// ── GPS hardware init ──────────────────────────────────────────────────────

/**
 * @brief Initialise the UM980 GNSS receiver.
 *
 * @details Starts UART1, attempts to connect to the UM980 with a 3-second
 *          timeout (two retries), then sends the standard AgOpenGPS
 *          configuration commands.  Falls back gracefully so NMEA is still
 *          parsed even if the UM980 library fails to handshake.
 */
static void initGPS() {
    Serial.printf("Starting GPS UART1  RX=%d TX=%d  460800 baud\n",
                  GPS_RX_PIN, GPS_TX_PIN);
    gpsSerial.begin(460800, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

    bool connected = false;
    for (int attempt = 0; attempt < 2 && !connected; attempt++) {
        Serial.printf("UM980 connection attempt %d/2 ...", attempt + 1);

        volatile bool done   = false;
        volatile bool result = false;
        struct Params { UM980* g; HardwareSerial* s; volatile bool* d; volatile bool* r; };
        Params p = { &myGNSS, &gpsSerial, &done, &result };

        xTaskCreate([](void* arg) {
            auto* p = (Params*)arg;
            *p->r = p->g->begin(*p->s, "UM980");
            *p->d = true;
            vTaskDelete(nullptr);
        }, "GPSInit", 4096, &p, 1, nullptr);

        uint32_t t0 = millis();
        while (!done && millis() - t0 < 3000) { delay(100); Serial.print('.'); }
        Serial.println();

        if (done && result) {
            connected = true;
            gpsState.gpsState = 1;
            Serial.println("UM980 connected!");
        } else {
            Serial.println("UM980 timeout");
        }
    }

    if (!connected) {
        gpsState.gpsState = 2;
        Serial.println("UM980 failed – raw NMEA forwarding only");
    } else {
        // Enable all satellite systems (GPS, GLONASS, BeiDou, Galileo, QZSS)
        myGNSS.sendCommand("CONFIG SIGNALGROUP 1");         delay(300);  // All constellations
        
        // Standard AgOpenGPS UM980 configuration
        myGNSS.sendCommand("CONFIG RTK RELIABILITY 3 1");   delay(300);
        myGNSS.sendCommand("CONFIG SMOOTH RTKHEIGHT 0");    delay(300);
        myGNSS.sendCommand("CONFIG HEADING RELIABILITY 3"); delay(300);
        myGNSS.sendCommand("CONFIG HEADING VARIABLELENGTH");delay(300);
        myGNSS.sendCommand("CONFIG SMOOTH HEADING 0");      delay(300);
        myGNSS.sendCommand("GNGGA 0.1");                    delay(300);  // GN = all GNSS systems
        myGNSS.sendCommand("GPVTG 0.1");                    delay(300);
        Serial.println("UM980 configured: All satellites enabled, 10Hz output");
    }
}

/**
 * @brief Scan I2C bus and print all detected devices
 */
static void scanI2C() {
    Serial.println("Scanning I2C bus...");
    uint8_t count = 0;
    
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        uint8_t error = Wire.endTransmission();
        
        if (error == 0) {
            Serial.printf("  I2C device found at address 0x%02X\n", addr);
            count++;
        }
    }
    
    if (count == 0) {
        Serial.println("  No I2C devices found");
    } else {
        Serial.printf("  Found %d I2C device(s)\n", count);
    }
}

/**
 * @brief Initialise the BNO08x IMU via I2C.
 *
 * @details Starts I2C on custom pins and enables rotation vector reports
 *          at 100 Hz. Uses a 3-second timeout task so a missing IMU does
 *          not block the boot sequence.
 */
static void initIMU() {
    Serial.printf("Starting IMU I2C  SDA=%d SCL=%d\n",
                  BNO_SDA_PIN, BNO_SCL_PIN);
    Wire.begin(BNO_SDA_PIN, BNO_SCL_PIN);
    
    // Scan I2C bus to help diagnose connection issues
    scanI2C();

    volatile bool done   = false;
    volatile bool result = false;
    struct Params { Adafruit_BNO08x* b; volatile bool* d; volatile bool* ok; };
    Params p = { &bno08x, &done, &result };

    xTaskCreate([](void* arg) {
        auto* p = (Params*)arg;
        *p->ok = p->b->begin_I2C(BNO08X_I2CADDR_DEFAULT);
        if (*p->ok) {
            // Enable rotation vector reports at ~100 Hz (10000 µs)
            *p->ok = p->b->enableReport(SH2_ROTATION_VECTOR, 10000);
        }
        *p->d  = true;
        vTaskDelete(nullptr);
    }, "IMUInit", 4096, &p, 1, nullptr);

    uint32_t t0 = millis();
    while (!done && millis() - t0 < 3000) { delay(100); Serial.print('.'); }
    Serial.println();

    if (done && result) {
        gpsState.imuState    = 1;
        gpsState.imuLastMsgMs = millis();
        Serial.println("BNO08x IMU started (I2C)");
    } else {
        gpsState.imuState = 2;
        Serial.println("BNO08x not detected – IMU disabled");
    }
}

// ── WiFi ───────────────────────────────────────────────────────────────────

/**
 * @brief Try to connect to a WiFi network as STA. Returns true if successful.
 */
bool connectWiFiSTA(const char* ssid, const char* password, uint32_t timeoutMs = 10000) {
    Serial.printf("Connecting to WiFi network: %s ...\n", ssid);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(200);
    WiFi.begin(ssid, password);
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < timeoutMs) {
        delay(250);
        Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
        IPAddress localIP = WiFi.localIP();
        Serial.printf("\nWiFi STA connected! IP: %s\n", localIP.toString().c_str());
        Serial.printf("STA MAC: %s\n", WiFi.macAddress().c_str());
        
        // Update broadcast subnet to match the connected network
        gpsState.agioSubnet[0] = localIP[0];
        gpsState.agioSubnet[1] = localIP[1];
        gpsState.agioSubnet[2] = localIP[2];
        Serial.printf("Broadcast subnet updated to: %d.%d.%d.255\n", 
                      gpsState.agioSubnet[0], gpsState.agioSubnet[1], gpsState.agioSubnet[2]);
        
        return true;
    } else {
        Serial.println("\nWiFi STA connect failed");
        return false;
    }
}

/**
 * @brief Start the "NOLTE_FARM" WiFi access point.
 *
 * @details Uses WIFI_AP mode (AP-only) so the module is always reachable
 *          at a predictable IP regardless of upstream network availability.
 *          The AP IP is 192.168.5.1 / 255.255.255.0.
 */
static void startWiFiAP() {
    Serial.println("Starting WiFi AP...");
    
    // Ensure WiFi is fully reset
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(500);
    
    // Set WiFi to persistent mode (helps with some S3 modules)
    WiFi.persistent(true);
    
    IPAddress ip(AP_IP_1, AP_IP_2, AP_IP_3, AP_IP_4);
    IPAddress gw(AP_IP_1, AP_IP_2, AP_IP_3, AP_IP_4);
    IPAddress subnet(255, 255, 255, 0);

    // Set mode to AP
    WiFi.mode(WIFI_AP);
    delay(200);
    
    // Configure IP before starting AP
    if (!WiFi.softAPConfig(ip, gw, subnet)) {
        Serial.println("ERROR: AP config failed!");
    }
    
    // Start AP - try with hidden=false (explicitly visible)
    Serial.printf("Starting AP: %s on channel %d\n", AP_SSID, AP_CHANNEL);
    bool apStarted = WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, false, AP_MAX_CLIENTS);
    
    if (!apStarted) {
        Serial.println("ERROR: Failed to start AP!");
        // Try again on channel 1 as fallback
        Serial.println("Retrying on channel 1...");
        apStarted = WiFi.softAP(AP_SSID, AP_PASSWORD, 1, false, AP_MAX_CLIENTS);
    }
    
    delay(500);
    
    // Disable power saving for better reliability
    WiFi.setSleep(false);
    
    // Set maximum TX power
    WiFi.setTxPower(WIFI_POWER_19_5dBm);
    
    // Verify AP is running
    delay(1000);
    wifi_mode_t mode = WiFi.getMode();
    Serial.printf("Current WiFi mode: %d (2=AP, 3=STA+AP)\n", mode);
    
    if (WiFi.softAPgetStationNum() >= 0) {  // Check if AP functions are working
        Serial.printf("WiFi AP ACTIVE - SSID=%s  IP=%s\n",
                      AP_SSID, WiFi.softAPIP().toString().c_str());
        Serial.printf("AP MAC: %s\n", WiFi.softAPmacAddress().c_str());
        Serial.printf("AP is broadcasting and should be visible\n");
    } else {
        Serial.println("WARNING: AP may not be fully active!");
    }

    // Initialise broadcast subnet to the AP subnet
    gpsState.agioSubnet[0] = AP_IP_1;
    gpsState.agioSubnet[1] = AP_IP_2;
    gpsState.agioSubnet[2] = AP_IP_3;
}

// ── Web server helpers ─────────────────────────────────────────────────────

static void updateDebugVars() {
    debugVars.clear();
    debugVars.push_back("Program: "  + String(NAME));
    debugVars.push_back("Version: "  + String(VERSION));
    debugVars.push_back("Uptime [s]: " + String(millis() / 1000.0f, 1));
    debugVars.push_back("Free Heap: " + String(ESP.getFreeHeap()) + " B");
    debugVars.push_back("AP SSID: "  + String(AP_SSID));
    debugVars.push_back("AP IP: "    + WiFi.softAPIP().toString());
    debugVars.push_back("AP Clients: " + String(WiFi.softAPgetStationNum()));
    debugVars.push_back("--- GPS ---");
    debugVars.push_back("GPS State: " + String(gpsState.gpsState == 1 ? "OK" :
                                               (gpsState.gpsState == 2 ? "FAILED" : "INIT")));
    debugVars.push_back("Fix Quality: " + String(gpsState.fixQuality));
    debugVars.push_back("Satellites: "  + String(gpsState.numSats));
    debugVars.push_back("Latitude: "    + String(gpsState.latitude) + " " + String(gpsState.latNS));
    debugVars.push_back("Longitude: "   + String(gpsState.longitude) + " " + String(gpsState.lonEW));
    debugVars.push_back("Altitude: "    + String(gpsState.altitude) + " m");
    debugVars.push_back("Speed: "       + String(gpsState.speedKnots) + " kn");
    debugVars.push_back("HDOP: "        + String(gpsState.HDOP));
    debugVars.push_back("GGA count: "   + String((unsigned long)gpsState.ggaCount));
    debugVars.push_back("VTG count: "   + String((unsigned long)gpsState.vtgCount));
    debugVars.push_back("PANDA sent: "  + String((unsigned long)gpsState.pandaCount));
    if (gpsState.pandaCount > 1 && gpsState.firstPandaMs > 0) {
        float elapsed = (millis() - gpsState.firstPandaMs) / 1000.0f;
        float hz = elapsed > 0 ? (gpsState.pandaCount - 1) / elapsed : 0;
        debugVars.push_back("PANDA rate: " + String(hz, 2) + " Hz");
    }
    debugVars.push_back("Last PANDA: "  + String(gpsState.nmea));
    debugVars.push_back("--- IMU ---");
    debugVars.push_back("IMU State: "   + String(gpsState.imuState == 1 ? "OK" :
                                                  (gpsState.imuState == 2 ? "FAILED" : "INIT")));
    debugVars.push_back("Heading (×10): " + String(gpsState.imuHeading));
    debugVars.push_back("Roll (×10): "    + String(gpsState.imuRoll));
    debugVars.push_back("Pitch (×10): "   + String(gpsState.imuPitch));
    debugVars.push_back("Yaw rate (×10): "+ String(gpsState.imuYawRate));
    debugVars.push_back("--- NTRIP ---");
    debugVars.push_back("PANDA broadcast: " + String(gpsState.enablePandaBroadcast ? "ON" : "OFF"));
    debugVars.push_back("NTRIP packets: " + String((unsigned long)gpsState.ntripCount));
    debugVars.push_back("NTRIP bytes: "   + String((uint32_t)gpsState.ntripBytes));
    
    if (gpsState.ntripCount > 0) {
        // Average packet size
        float avgSize = (float)gpsState.ntripBytes / (float)gpsState.ntripCount;
        debugVars.push_back("Avg packet size: " + String(avgSize, 1) + " B");
        
        // Time since last packet
        if (gpsState.lastNtripMs > 0) {
            float secondsAgo = (millis() - gpsState.lastNtripMs) / 1000.0f;
            debugVars.push_back("Last packet: " + String(secondsAgo, 1) + " s ago");
        }
        
        // Data rate
        if (gpsState.ntripCount > 1 && gpsState.firstNtripMs > 0) {
            float elapsed = (millis() - gpsState.firstNtripMs) / 1000.0f;
            if (elapsed > 0) {
                float pktRate = (gpsState.ntripCount - 1) / elapsed;
                float byteRate = gpsState.ntripBytes / elapsed;
                debugVars.push_back("Packet rate: " + String(pktRate, 2) + " pkt/s");
                
                // Format byte rate nicely
                if (byteRate >= 1024) {
                    debugVars.push_back("Data rate: " + String(byteRate / 1024.0f, 2) + " KB/s");
                } else {
                    debugVars.push_back("Data rate: " + String(byteRate, 1) + " B/s");
                }
            }
        }
    } else {
        debugVars.push_back("No NTRIP data received");
    }
    
    debugVars.push_back("Broadcast subnet: " + String(gpsState.agioSubnet[0]) + "." + 
                        String(gpsState.agioSubnet[1]) + "." + String(gpsState.agioSubnet[2]) + ".255");
}

static void handleDebugVars(AsyncWebServerRequest* req) {
    updateDebugVars();
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    for (const auto& v : debugVars) arr.add(v);
    String json;
    serializeJson(doc, json);
    req->send(200, "application/json", json);
}

/**
 * @brief Serve GPS position data as JSON for map display.
 *
 * @details Converts NMEA lat/lon format (ddmm.mmmm) to decimal degrees.
 *          Validates coordinates to prevent invalid positions on map.
 */
static void handleGpsPos(AsyncWebServerRequest* req) {
    JsonDocument doc;
    
    int fixQuality = atoi(gpsState.fixQuality);
    bool valid = false;
    double decLat = 0.0;
    double decLon = 0.0;
    
    // Only process if we have at least a GPS fix (quality >= 1)
    if (fixQuality >= 1 && strlen(gpsState.latitude) > 4 && strlen(gpsState.longitude) > 4 &&
        strlen(gpsState.latNS) > 0 && strlen(gpsState.lonEW) > 0) {
        
        // Parse latitude from NMEA format (ddmm.mmmm) to decimal degrees
        double lat = atof(gpsState.latitude);
        int deg = (int)(lat / 100);
        double min = lat - (deg * 100);
        decLat = deg + (min / 60.0);
        if (gpsState.latNS[0] == 'S') decLat = -decLat;
        
        // Parse longitude from NMEA format (dddmm.mmmm) to decimal degrees
        double lon = atof(gpsState.longitude);
        deg = (int)(lon / 100);
        min = lon - (deg * 100);
        decLon = deg + (min / 60.0);
        if (gpsState.lonEW[0] == 'W') decLon = -decLon;
        
        // Validate coordinates are within valid ranges
        if (decLat >= -90.0 && decLat <= 90.0 && 
            decLon >= -180.0 && decLon <= 180.0 &&
            !isnan(decLat) && !isnan(decLon) &&
            !isinf(decLat) && !isinf(decLon)) {
            valid = true;
        }
    }
    
    doc["valid"] = valid;
    doc["lat"] = valid ? decLat : 0.0;
    doc["lon"] = valid ? decLon : 0.0;
    doc["alt"] = atof(gpsState.altitude);
    doc["fixQuality"] = fixQuality;
    doc["sats"] = atoi(gpsState.numSats);
    doc["hdop"] = atof(gpsState.HDOP);
    doc["heading"] = strlen(gpsState.imuHeading) > 0 ? atoi(gpsState.imuHeading) / 10.0 : 0.0;
    doc["speed"] = atof(gpsState.speedKnots);
    
    String json;
    serializeJson(doc, json);
    req->send(200, "application/json", json);
}

static void handleReboot(AsyncWebServerRequest* req) {
    req->send(200, "text/plain", "Rebooting...");
    delay(100);
    ESP.restart();
}

static void handleFileList(AsyncWebServerRequest* req) {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    File root = LittleFS.open("/");
    File f    = root.openNextFile();
    while (f) {
        JsonObject obj = arr.add<JsonObject>();
        obj["name"] = String(f.name());
        obj["size"] = f.size();
        f = root.openNextFile();
    }
    String json;
    serializeJson(doc, json);
    req->send(200, "application/json", json);
}

static bool fwUpdateSkip = false;

static void handleFirmwareUpload(AsyncWebServerRequest* req, String filename,
                                  size_t index, uint8_t* data, size_t len, bool final) {
    if (!index) {
        fwUpdateSkip = false;
        Serial.printf("OTA Start: %s\n", filename.c_str());
        if (!filename.startsWith(NAME)) {
            Serial.printf("Firmware rejected: '%s' ≠ '%s'\n", filename.c_str(), NAME);
            fwUpdateSkip = true;
        } else if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    }
    if (fwUpdateSkip) {
        if (final) req->send(400, "text/plain",
                             "Wrong firmware file – expected prefix: " + String(NAME));
        return;
    }
    if (Update.write(data, len) != len) Update.printError(Serial);
    if (final) {
        if (Update.end(true)) {
            Serial.printf("OTA OK: %u B\n", index + len);
            req->send(200, "text/html", "Update complete! Rebooting...");
            delay(1000);
            ESP.restart();
        } else {
            Update.printError(Serial);
            req->send(500, "text/html", "Update failed.");
        }
    }
}

static void handleFilesystemUpload(AsyncWebServerRequest* req, String filename,
                                    size_t index, uint8_t* data, size_t len, bool final) {
    if (!index) {
        Serial.printf("FS OTA Start: %s\n", filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN, U_SPIFFS)) {
            Update.printError(Serial);
            req->send(500, "text/plain", "Filesystem update failed to start.");
            return;
        }
    }
    if (Update.write(data, len) != len) Update.printError(Serial);
    if (final) {
        if (Update.end(true)) {
            Serial.printf("FS OTA OK: %u B\n", index + len);
            req->send(200, "text/html", "Filesystem update complete! Rebooting...");
            delay(1000);
            ESP.restart();
        } else {
            Update.printError(Serial);
            req->send(500, "text/html", "Filesystem update failed.");
        }
    }
}

static void handleFileUpload(AsyncWebServerRequest* req, String filename,
                              size_t index, uint8_t* data, size_t len, bool final) {
    if (!index) {
        Serial.printf("Upload start: %s\n", filename.c_str());
        req->_tempFile = LittleFS.open("/" + filename, "w");
    }
    if (len) req->_tempFile.write(data, len);
    if (final) {
        req->_tempFile.close();
        Serial.printf("Upload end: %s  %u B\n", filename.c_str(), index + len);
        req->send(200, "text/plain", "File Uploaded");
    }
}

// ── setup() ───────────────────────────────────────────────────────────────

void setup() {
    // ── Serial (USB-CDC) ──────────────────────────────────────────────────
    Serial.begin(115200);
    delay(3000);
    Serial.printf("\n\n=== %s v%s booting ===\n", NAME, VERSION);

    // ── Power Relay ───────────────────────────────────────────────────────
    pinMode(POWER_RELAY_PIN, OUTPUT);
    digitalWrite(POWER_RELAY_PIN, LOW);
    Serial.println("Power relay LOW - waiting 1s...");
    delay(1000);
    digitalWrite(POWER_RELAY_PIN, HIGH);
    Serial.println("Power relay HIGH");
    delay(1000);
    // ── LED ───────────────────────────────────────────────────────────────
    // pixel.begin();  // Disabled - no NeoPixel on Xiao
    // setLED(0, 0, 50);   // dim blue = booting

    // ── File system ───────────────────────────────────────────────────────
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed");
    } else {
        Serial.println("LittleFS mounted");
    }

    // ── WiFi STA/Client ───────────────────────────────────────────────────
    // Try connecting to WiFi network first (60 second timeout)
    bool staConnected = connectWiFiSTA(STA_DEFAULT_SSID, STA_DEFAULT_PASSWORD, 60000);
    
    if (!staConnected) {
        Serial.println("STA connection failed - starting AP mode");
        startWiFiAP();
    } else {
        Serial.println("WiFi connected - AP mode not needed");
    }

    // ── mDNS ──────────────────────────────────────────────────────────────
    if (MDNS.begin("esp32_gps")) {
        Serial.println("mDNS responder started: esp32_gps.local");
        MDNS.addService("http", "tcp", 80);
        MDNS.addServiceTxt("http", "tcp", "version", VERSION);
        MDNS.addServiceTxt("http", "tcp", "name", NAME);
    } else {
        Serial.println("Error starting mDNS");
    }

    // ── GPS ───────────────────────────────────────────────────────────────
    initGPS();

    // ── IMU ───────────────────────────────────────────────────────────────
    Serial.println("Initialising IMU...");
    initIMU();

    // ── UDP ───────────────────────────────────────────────────────────────
    startUDPgps();
    startUDPntrip();
    startUDPaio();

    // ── Web server ────────────────────────────────────────────────────────
    server.on("/", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(LittleFS, "/index.html", "text/html");
    });
    server.on("/index.html", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(LittleFS, "/index.html", "text/html");
    });
    server.on("/map.html", HTTP_GET, [](AsyncWebServerRequest* r) {
        r->send(LittleFS, "/map.html", "text/html");
    });
    server.on("/getDebugVars", HTTP_GET, handleDebugVars);
    server.on("/getGpsPos",    HTTP_GET, handleGpsPos);
    server.on("/getFiles",     HTTP_GET, handleFileList);
    server.on("/reboot",       HTTP_GET, handleReboot);

    // PANDA broadcast control
    server.on("/getGpsForwarding", HTTP_GET, [](AsyncWebServerRequest* r) {
        JsonDocument doc;
        doc["enabled"] = gpsState.enablePandaBroadcast;
        String json;
        serializeJson(doc, json);
        r->send(200, "application/json", json);
    });
    server.on("/setGpsForwarding", HTTP_GET, [](AsyncWebServerRequest* r) {
        if (r->hasParam("enable")) {
            gpsState.enablePandaBroadcast = r->getParam("enable")->value() == "1" || 
                                            r->getParam("enable")->value() == "true";
            Serial.printf("PANDA broadcast %s\n", gpsState.enablePandaBroadcast ? "enabled" : "disabled");
        }
        JsonDocument doc;
        doc["enabled"] = gpsState.enablePandaBroadcast;
        String json;
        serializeJson(doc, json);
        r->send(200, "application/json", json);
    });

    // Message format control (PANDA vs raw NMEA)
    server.on("/getMessageFormat", HTTP_GET, [](AsyncWebServerRequest* r) {
        JsonDocument doc;
        doc["useRawNMEA"] = gpsState.useRawNMEA;
        String json;
        serializeJson(doc, json);
        r->send(200, "application/json", json);
    });
    server.on("/setMessageFormat", HTTP_GET, [](AsyncWebServerRequest* r) {
        if (r->hasParam("raw")) {
            gpsState.useRawNMEA = r->getParam("raw")->value() == "1" || 
                                  r->getParam("raw")->value() == "true";
            Serial.printf("Message format: %s\n", gpsState.useRawNMEA ? "Raw NMEA" : "PANDA");
        }
        JsonDocument doc;
        doc["useRawNMEA"] = gpsState.useRawNMEA;
        String json;
        serializeJson(doc, json);
        r->send(200, "application/json", json);
    });

    // GPS logging control
    server.on("/getGpsLogging", HTTP_GET, [](AsyncWebServerRequest* r) {
        JsonDocument doc;
        doc["enabled"] = gpsState.enableGpsLogging;
        // Get log file size if it exists
        if (LittleFS.exists("/gpslog.csv")) {
            File f = LittleFS.open("/gpslog.csv", "r");
            doc["size"] = f.size();
            doc["lines"] = 0; // Could count but expensive
            f.close();
        } else {
            doc["size"] = 0;
            doc["lines"] = 0;
        }
        String json;
        serializeJson(doc, json);
        r->send(200, "application/json", json);
    });
    
    server.on("/setGpsLogging", HTTP_GET, [](AsyncWebServerRequest* r) {
        if (r->hasParam("enable")) {
            bool newState = r->getParam("enable")->value() == "1" || 
                           r->getParam("enable")->value() == "true";
            
            // If enabling for first time, write CSV header
            if (newState && !gpsState.enableGpsLogging && !LittleFS.exists("/gpslog.csv")) {
                File logFile = LittleFS.open("/gpslog.csv", "w");
                if (logFile) {
                    logFile.println("timestamp_ms,latitude,longitude,altitude_m,fix_quality,satellites,hdop,speed_kn,heading_deg");
                    logFile.close();
                    Serial.println("GPS log file created with header");
                }
            }
            
            gpsState.enableGpsLogging = newState;
            Serial.printf("GPS logging %s\n", gpsState.enableGpsLogging ? "enabled" : "disabled");
        }
        JsonDocument doc;
        doc["enabled"] = gpsState.enableGpsLogging;
        String json;
        serializeJson(doc, json);
        r->send(200, "application/json", json);
    });
    
    server.on("/downloadGpsLog", HTTP_GET, [](AsyncWebServerRequest* r) {
        if (LittleFS.exists("/gpslog.csv")) {
            r->send(LittleFS, "/gpslog.csv", "text/csv", true); // true = download
        } else {
            r->send(404, "text/plain", "Log file not found");
        }
    });
    
    server.on("/clearGpsLog", HTTP_GET, [](AsyncWebServerRequest* r) {
        if (LittleFS.exists("/gpslog.csv")) {
            LittleFS.remove("/gpslog.csv");
            Serial.println("GPS log file deleted");
        }
        r->send(200, "text/plain", "Log cleared");
    });

    server.on("/update", HTTP_POST,
              [](AsyncWebServerRequest* r) {},
              handleFirmwareUpload);
    server.on("/updatefs", HTTP_POST,
              [](AsyncWebServerRequest* r) {},
              handleFilesystemUpload);
    server.on("/upload", HTTP_POST,
              [](AsyncWebServerRequest* r) {},
              handleFileUpload);

    // Module identification endpoint used by pc_server
    server.on("/version", HTTP_GET, [](AsyncWebServerRequest* r) {
        JsonDocument doc;
        doc["name"]    = NAME;
        doc["version"] = VERSION;
        String json;
        serializeJson(doc, json);
        r->send(200, "application/json", json);
    });

    server.begin();
    
    // Print the correct IP based on WiFi mode
    if (WiFi.getMode() == WIFI_STA && WiFi.status() == WL_CONNECTED) {
        Serial.println("Web server started on http://" + WiFi.localIP().toString());
    } else {
        Serial.println("Web server started on http://" + WiFi.softAPIP().toString());
    }

    // ── GPS FreeRTOS task (Core 1) ────────────────────────────────────────
    xTaskCreatePinnedToCore(gpsTask, "GPS_Task", 16384, nullptr, 3, nullptr, 1);

    // ── Status LED: ready ─────────────────────────────────────────────────
    // setLED(0, 20, 0);   // dim green = running

    // Print setup complete with correct mode and IP
    if (WiFi.getMode() == WIFI_STA && WiFi.status() == WL_CONNECTED) {
        Serial.printf("Setup complete – STA Mode  IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.printf("Setup complete – AP: %s  IP: %s\n",
                      AP_SSID, WiFi.softAPIP().toString().c_str());
    }
}

// ── loop() ────────────────────────────────────────────────────────────────

void loop() {
    // Update LED based on GPS fix quality and client count (disabled - no NeoPixel on Xiao)
    /*
    static uint32_t lastLedUpdate = 0;
    if (millis() - lastLedUpdate >= 500) {
        lastLedUpdate = millis();
        int quality  = atoi(gpsState.fixQuality);
        int clients  = WiFi.softAPgetStationNum();
        if (quality >= 4) {
            setLED(0, 50, 0);           // bright green: RTK fix
        } else if (quality >= 1) {
            setLED(0, 15, 15);          // cyan: GPS fix (no RTK)
        } else if (clients > 0) {
            setLED(0, 0, 20);           // blue: client connected, no fix
        } else {
            setLED(5, 5, 0);            // dim yellow: no clients, no fix
        }
    }
    */

    static uint32_t lastDebug = 0;
    if (millis() - lastDebug >= 15000) {
        lastDebug = millis();
        Serial.printf("[Loop] Clients=%d  PANDA=%lu  NTRIP=%lu  Heap=%u\n",
                      WiFi.softAPgetStationNum(),
                      (unsigned long)gpsState.pandaCount,
                      (unsigned long)gpsState.ntripCount,
                      ESP.getFreeHeap());
    }

    delay(100);
}
