/**
 * @file main.cpp
 * @brief ESP32-S3 GPS / NTRIP / IMU Module
 *
 * @details This module runs on an ESP32-S3 DevKitC-1 N8R8 and provides the GPS,
 *          NTRIP corrections, IMU, and PANDA-sentence functions that the ESP32_AIO
 *          normally handles, but as a standalone, dedicated GPS module.
 *
 *          Key capabilities:
 *          - WiFi Access Point "NOLTE_FARM" (always active) plus optional STA uplink
 *          - UM980 GNSS receiver on UART1 (RX=GPIO13, TX=GPIO14) at 460800 baud
 *          - BNO08x IMU in RVC mode on UART2 (RX=GPIO12) at 115200 baud
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
#include <AsyncUDP.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <Update.h>
#include "Adafruit_NeoPixel.h"
#include "Adafruit_BNO08x_RVC.h"
#include "SparkFun_Unicore_GNSS_Arduino_Library.h"
#include "Version.h"

// ── Pin definitions ────────────────────────────────────────────────────────

/// Built-in RGB NeoPixel on ESP32-S3-DevKitC-1
#define LED_PIN       48

/// GPS UART1 – receives NMEA from UM980 TX
#define GPS_RX_PIN    13
/// GPS UART1 – transmits commands / NTRIP to UM980 RX
#define GPS_TX_PIN    14

/// IMU UART2 – receives RVC frames from BNO08x TX
#define BNO_RX_PIN    12
/// IMU UART2 – TX not used but required by HardwareSerial.begin()
#define BNO_TX_PIN    17

// ── WiFi / network constants ───────────────────────────────────────────────

#define AP_SSID       "NOLTE_FARM"
#define AP_PASSWORD   "DontLoseMoney89"
#define AP_CHANNEL    6
#define AP_MAX_CLIENTS 8

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

    // ── Statistics ──
    volatile uint32_t pandaCount  = 0;
    volatile uint32_t ntripCount  = 0;
    volatile uint64_t ntripBytes  = 0;
    volatile uint32_t ggaCount    = 0;
    volatile uint32_t vtgCount    = 0;
    uint32_t firstPandaMs = 0;
    uint32_t lastPandaMs  = 0;

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
HardwareSerial bnoSerial(2);       ///< UART2 – BNO08x IMU (RVC mode)

UM980             myGNSS;          ///< SparkFun UM980 driver
Adafruit_BNO08x_RVC rvc;          ///< Adafruit BNO08x RVC driver

Adafruit_NeoPixel pixel(1, LED_PIN, NEO_GRB + NEO_KHZ800);

AsyncWebServer server(80);
AsyncUDP       udpGPS;             ///< Port 9999 – PANDA broadcast
AsyncUDP       udpAIO;             ///< Port 8888 – AgIO comms
AsyncUDP       udpNtrip;           ///< Port 2233 – NTRIP forwarding

// ── Shared debug variables ─────────────────────────────────────────────────

std::vector<String> debugVars;

// ── LED helpers ────────────────────────────────────────────────────────────

static void setLED(uint8_t r, uint8_t g, uint8_t b) {
    pixel.setPixelColor(0, pixel.Color(r, g, b));
    pixel.show();
}

// ── NMEA / PANDA helpers ───────────────────────────────────────────────────

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
 *                 speed,heading,roll,pitch,yawRate,*XX\\r\\n
 */
static void buildPandaSentence() {
    strcpy(gpsState.nmea, "$PANDA,");

    auto append = [](const char* s) {
        strncat(gpsState.nmea, s, sizeof(gpsState.nmea) - strlen(gpsState.nmea) - 1);
    };

    if (*gpsState.fixTime)    append(gpsState.fixTime);    append(",");
    if (*gpsState.latitude)   append(gpsState.latitude);   append(",");
    if (*gpsState.latNS)      append(gpsState.latNS);      append(",");
    if (*gpsState.longitude)  append(gpsState.longitude);  append(",");
    if (*gpsState.lonEW)      append(gpsState.lonEW);      append(",");
    if (*gpsState.fixQuality) append(gpsState.fixQuality); append(",");
    if (*gpsState.numSats)    append(gpsState.numSats);    append(",");
    if (*gpsState.HDOP)       append(gpsState.HDOP);       append(",");
    if (*gpsState.altitude)   append(gpsState.altitude);   append(",");

    append(gpsState.ageDGPS);  append(",");

    if (*gpsState.speedKnots) append(gpsState.speedKnots); append(",");

    // Heading
    if (gpsState.disableHeading) {
        append("0");
    } else if (*gpsState.imuHeading) {
        append(gpsState.imuHeading);
    }
    append(",");

    if (*gpsState.imuRoll)    append(gpsState.imuRoll);    append(",");
    if (*gpsState.imuPitch)   append(gpsState.imuPitch);   append(",");
    if (*gpsState.imuYawRate) append(gpsState.imuYawRate); append(",");

    append("*");

    // Guard against buffer overflow before checksum digits
    if (strlen(gpsState.nmea) > sizeof(gpsState.nmea) - 5) {
        // Truncate and re-add '*'
        gpsState.nmea[sizeof(gpsState.nmea) - 5] = '*';
        gpsState.nmea[sizeof(gpsState.nmea) - 4] = '\0';
    }

    calculateChecksum();
    strncat(gpsState.nmea, "\r\n", sizeof(gpsState.nmea) - strlen(gpsState.nmea) - 1);
}

// ── IMU handler ────────────────────────────────────────────────────────────

/**
 * @brief Read the latest BNO08x RVC frame and update gpsState IMU fields.
 *
 * @details Values are multiplied by 10 and stored as integer strings to match
 *          the AgOpenGPS PANDA specification.  Yaw rate is calculated from the
 *          difference between successive heading readings.
 *
 *          Called from the GPS task on every loop iteration so it drains the
 *          BNO08x serial buffer promptly.
 */
static void readIMU() {
    if (gpsState.imuState == 2) return;  // permanently failed

    BNO08x_RVC_Data data;
    if (!rvc.read(&data)) {
        // No new frame available; check watchdog
        if (gpsState.imuLastMsgMs > 0 &&
            millis() - gpsState.imuLastMsgMs > 2000) {
            gpsState.imuState = 2;
            Serial.println("IMU watchdog timeout – marking failed");
        }
        return;
    }

    gpsState.imuState     = 1;
    gpsState.imuLastMsgMs = millis();

    // Apply orientation corrections
    float pitch   = data.pitch;
    float roll    = data.roll;
    float heading = data.yaw;

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

                    // On GGA sentences: read latest IMU then build + send PANDA
                    if (strstr(buf, "GGA") != nullptr) {
                        readIMU();  // get the freshest IMU frame before PANDA
                        buildPandaSentence();

                        // Broadcast on the AP subnet
                        IPAddress bcast(gpsState.agioSubnet[0], gpsState.agioSubnet[1],
                                        gpsState.agioSubnet[2], 255);
                        udpGPS.writeTo((const uint8_t*)gpsState.nmea,
                                       strlen(gpsState.nmea), bcast, PORT_GPS);
                        taskYIELD();

                        gpsState.pandaCount++;
                        uint32_t now = millis();
                        if (gpsState.pandaCount == 1) gpsState.firstPandaMs = now;
                        gpsState.lastPandaMs = now;
                    }
                }

                idx = 0;
                memset(buf, 0, sizeof(buf));
            }
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

        vTaskDelay(pdMS_TO_TICKS(5));
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

        gpsSerial.write(data, len);

        gpsState.ntripCount++;
        gpsState.ntripBytes += len;

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

                // If IMU is active, also send IMU-module hello (PGN 121)
                if (gpsState.imuState == 1) {
                    reply[2] = 79;
                    reply[3] = 121;
                    ck = 0;
                    for (int i = 2; i < (int)reply[4] + 5; i++) ck += reply[i];
                    reply[10] = ck;
                    udpAIO.writeTo(reply, sizeof(reply), bcast, PORT_AGIO);
                }
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
        // Standard AgOpenGPS UM980 configuration
        myGNSS.sendCommand("CONFIG RTK RELIABILITY 3 1");   delay(300);
        myGNSS.sendCommand("CONFIG SMOOTH RTKHEIGHT 0");    delay(300);
        myGNSS.sendCommand("CONFIG HEADING RELIABILITY 3"); delay(300);
        myGNSS.sendCommand("CONFIG HEADING VARIABLELENGTH");delay(300);
        myGNSS.sendCommand("CONFIG SMOOTH HEADING 0");      delay(300);
        myGNSS.sendCommand("GNGGA, .1");                    delay(300);
        myGNSS.sendCommand("GPVTG, .1");                    delay(300);
        Serial.println("UM980 configured");
    }
}

/**
 * @brief Initialise the BNO08x IMU in RVC mode.
 *
 * @details Starts UART2 at 115200 baud with a 3-second timeout task so a
 *          missing IMU does not block the boot sequence.
 */
static void initIMU() {
    Serial.printf("Starting IMU UART2  RX=%d TX=%d  115200 baud\n",
                  BNO_RX_PIN, BNO_TX_PIN);
    bnoSerial.begin(115200, SERIAL_8N1, BNO_RX_PIN, BNO_TX_PIN);

    volatile bool done   = false;
    volatile bool result = false;
    struct Params { Adafruit_BNO08x_RVC* r; HardwareSerial* s;
                    volatile bool* d; volatile bool* ok; };
    Params p = { &rvc, &bnoSerial, &done, &result };

    xTaskCreate([](void* arg) {
        auto* p = (Params*)arg;
        *p->ok = p->r->begin(p->s);
        *p->d  = true;
        vTaskDelete(nullptr);
    }, "IMUInit", 4096, &p, 1, nullptr);

    uint32_t t0 = millis();
    while (!done && millis() - t0 < 3000) { delay(100); Serial.print('.'); }
    Serial.println();

    if (done && result) {
        gpsState.imuState    = 1;
        gpsState.imuLastMsgMs = millis();
        Serial.println("BNO08x IMU started");
    } else {
        gpsState.imuState = 2;
        Serial.println("BNO08x not detected – IMU disabled");
    }
}

// ── WiFi ───────────────────────────────────────────────────────────────────

/**
 * @brief Start the "NOLTE_FARM" WiFi access point.
 *
 * @details Uses WIFI_AP mode (AP-only) so the module is always reachable
 *          at a predictable IP regardless of upstream network availability.
 *          The AP IP is 192.168.5.1 / 255.255.255.0.
 */
static void startWiFiAP() {
    IPAddress ip(AP_IP_1, AP_IP_2, AP_IP_3, AP_IP_4);
    IPAddress gw(AP_IP_1, AP_IP_2, AP_IP_3, AP_IP_4);
    IPAddress subnet(255, 255, 255, 0);

    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(ip, gw, subnet);
    WiFi.softAP(AP_SSID, AP_PASSWORD, AP_CHANNEL, 0, AP_MAX_CLIENTS);

    Serial.printf("WiFi AP started  SSID=%s  IP=%s\n",
                  AP_SSID, WiFi.softAPIP().toString().c_str());

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
    debugVars.push_back("NTRIP packets: " + String((unsigned long)gpsState.ntripCount));
    debugVars.push_back("NTRIP bytes: "   + String((uint32_t)gpsState.ntripBytes));
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
    delay(1000);
    Serial.printf("\n\n=== %s v%s booting ===\n", NAME, VERSION);

    // ── LED ───────────────────────────────────────────────────────────────
    pixel.begin();
    setLED(0, 0, 50);   // dim blue = booting

    // ── File system ───────────────────────────────────────────────────────
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed");
    } else {
        Serial.println("LittleFS mounted");
    }

    // ── WiFi AP ───────────────────────────────────────────────────────────
    startWiFiAP();

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
    server.on("/getDebugVars", HTTP_GET, handleDebugVars);
    server.on("/getFiles",     HTTP_GET, handleFileList);
    server.on("/reboot",       HTTP_GET, handleReboot);

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
    Serial.println("Web server started on http://" + WiFi.softAPIP().toString());

    // ── GPS FreeRTOS task (Core 1) ────────────────────────────────────────
    xTaskCreatePinnedToCore(gpsTask, "GPS_Task", 16384, nullptr, 3, nullptr, 1);

    // ── Status LED: ready ─────────────────────────────────────────────────
    setLED(0, 20, 0);   // dim green = running

    Serial.printf("Setup complete – AP: %s  IP: %s\n",
                  AP_SSID, WiFi.softAPIP().toString().c_str());
}

// ── loop() ────────────────────────────────────────────────────────────────

void loop() {
    // Update LED based on GPS fix quality and client count
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
