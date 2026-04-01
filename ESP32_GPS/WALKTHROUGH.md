# ESP32_GPS Firmware – Code Walkthrough

This document walks through every meaningful layer of the ESP32_GPS firmware (`src/main.cpp`), from hardware initialization through data-flow, networking, and the web interface.

---

## Table of Contents

1. [Purpose and Architecture](#1-purpose-and-architecture)
2. [Project Files](#2-project-files)
3. [Build Configuration (`platformio.ini`)](#3-build-configuration-platformioini)
4. [Pin Assignments and Network Constants](#4-pin-assignments-and-network-constants)
5. [Central State Object – `GPSState`](#5-central-state-object--gpsstate)
6. [Hardware Objects](#6-hardware-objects)
7. [Boot Sequence – `setup()`](#7-boot-sequence--setup)
8. [GPS Processing Pipeline – `gpsTask()`](#8-gps-processing-pipeline--gpstask)
   - 8a. [UART Character Assembly](#8a-uart-character-assembly)
   - 8b. [NMEA Parsing – `parseNMEA()`, `parseGGA()`, `parseVTG()`](#8b-nmea-parsing--parsenmea-parsegga-parsevtg)
   - 8c. [Coordinate Validation – `isGpsPositionValid()`](#8c-coordinate-validation--isgpspositionvalid)
   - 8d. [PANDA Sentence Construction](#8d-panda-sentence-construction)
   - 8e. [UDP Broadcast Decision Logic](#8e-udp-broadcast-decision-logic)
   - 8f. [Periodic Diagnostics](#8f-periodic-diagnostics)
9. [IMU Integration – `readIMU()`](#9-imu-integration--readimu)
   - 9a. [Quaternion → Euler Conversion](#9a-quaternion--euler-conversion)
   - 9b. [Axis Remapping – `flipPitchRoll` and `invertRoll`](#9b-axis-remapping--flippitchroll-and-invertroll)
   - 9c. [Yaw Rate Calculation](#9c-yaw-rate-calculation)
   - 9d. [IMU Watchdog](#9d-imu-watchdog)
10. [UDP Communications](#10-udp-communications)
    - 10a. [Port 9999 – PANDA / NMEA Broadcast (`udpGPS`)](#10a-port-9999--panda--nmea-broadcast-udpgps)
    - 10b. [Port 2233 – NTRIP Forwarding (`udpNtrip`)](#10b-port-2233--ntrip-forwarding-udpntrip)
    - 10c. [Port 8888 – AgIO Protocol (`udpAIO`)](#10c-port-8888--agio-protocol-udpaio)
    - 10d. [Subnet Announcement – PGN 203](#10d-subnet-announcement--pgn-203)
    - 10e. [IMU Data Packet – PGN 211](#10e-imu-data-packet--pgn-211)
11. [WiFi Modes](#11-wifi-modes)
12. [GPS Hardware Initialization – `initGPS()`](#12-gps-hardware-initialization--initgps)
13. [IMU Hardware Initialization – `initIMU()`](#13-imu-hardware-initialization--initimu)
14. [Web Server and REST API](#14-web-server-and-rest-api)
    - 14a. [Static Pages](#14a-static-pages)
    - 14b. [Diagnostic Endpoints](#14b-diagnostic-endpoints)
    - 14c. [Control Endpoints](#14c-control-endpoints)
    - 14d. [GPS Logging Endpoints](#14d-gps-logging-endpoints)
    - 14e. [OTA Update Endpoints](#14e-ota-update-endpoints)
15. [GPS Data Logging – `logGpsData()`](#15-gps-data-logging--loggpsdata)
16. [Web Interface – HTML / JavaScript](#16-web-interface--html--javascript)
17. [Arduino `loop()`](#17-arduino-loop)
18. [Data-Flow Diagram](#18-data-flow-diagram)

---

## 1. Purpose and Architecture

The ESP32_GPS module is a **dedicated GPS / NTRIP / IMU node** designed for the AgOpenGPS precision-farming software ecosystem. It is intended as a standalone replacement for the GPS and IMU functions that would otherwise be embedded inside an all-in-one control board (ESP32_AIO).

Key responsibilities:

| Responsibility | Transport |
|---|---|
| Receive NMEA data from a Unicore UM980 GNSS receiver | UART1 |
| Apply RTCM NTRIP corrections from an external caster | UDP → UART1 |
| Read orientation from a BNO08x IMU | I²C |
| Build AgOpenGPS **PANDA** sentences and broadcast them | UDP port 9999 |
| Respond to AgIO module-discovery and IP-assignment packets | UDP port 8888 |
| Send dedicated IMU data packets to AgIO | UDP port 8888 |
| Serve a web dashboard and accept OTA firmware updates | HTTP port 80 |
| Log GPS position to a CSV file on flash | LittleFS |

The firmware runs on a **Seeed XIAO ESP32-S3**. All heavy I/O (GPS UART reading, IMU polling, PANDA building, UDP broadcasting) is placed in a **FreeRTOS task pinned to Core 1**, leaving Core 0 free for the WiFi/TCP/IP stack and the web server.

---

## 2. Project Files

```
ESP32_GPS/
├── src/
│   └── main.cpp          ← entire firmware (this walkthrough's subject)
├── include/
│   └── Version.h         ← VERSION and NAME constants
├── data/
│   ├── index.html        ← web dashboard (served from LittleFS)
│   └── map.html          ← Leaflet live-map page
├── platformio.ini        ← build environment definition
└── build_post_script.py  ← auto-increments PATCH number in Version.h
```

`Version.h` only defines two constants:

```cpp
#define VERSION "1.0.0043"
#define NAME    "ESP32_GPS"
```

`NAME` is used as a filename prefix guard for OTA uploads, ensuring the wrong module's firmware cannot be flashed accidentally.

---

## 3. Build Configuration (`platformio.ini`)

```ini
[env:seeed_xiao_esp32s3]
platform  = espressif32
board     = seeed_xiao_esp32s3
framework = arduino
board_build.filesystem = littlefs
monitor_speed = 115200
extra_scripts = build_post_script.py
lib_deps =
    bblanchon/ArduinoJson@^7.1.0
    ESP32Async/ESPAsyncWebServer @ 3.6.0
    adafruit/Adafruit BNO08x
    sparkfun/SparkFun UM980 Triband RTK GNSS Arduino Library@^2.0.0
build_flags =
    -DARDUINO_USB_MODE=1
    -DARDUINO_USB_CDC_ON_BOOT=1
```

Notable decisions:

- **LittleFS** is the filesystem for serving the web UI and storing GPS logs.
- `ARDUINO_USB_CDC_ON_BOOT=1` routes `Serial` over the native USB-CDC port so there is no need for a separate USB-to-UART adapter during development.
- `build_post_script.py` increments the `PATCH` digit in `Version.h` on every successful firmware build, producing a monotonically increasing version string.

---

## 4. Pin Assignments and Network Constants

### Hardware pins

| Constant | Value | Purpose |
|---|---|---|
| `POWER_RELAY_PIN` | 1 | Power relay output (driven HIGH after boot) |
| `GPS_RX_PIN` | 7 | UART1 RX – receives NMEA from UM980 TX |
| `GPS_TX_PIN` | 8 | UART1 TX – sends commands and NTRIP RTCM to UM980 RX |
| `BNO_SDA_PIN` | 3 | I²C SDA for BNO08x |
| `BNO_SCL_PIN` | 4 | I²C SCL for BNO08x |
| `BNO08X_I2CADDR_DEFAULT` | `0x4A` | I²C address of BNO08x |

### WiFi constants

| Constant | Value | Meaning |
|---|---|---|
| `AP_SSID` | `"NOLTE_FARM"` | Access-point SSID |
| `AP_PASSWORD` | `"DontLoseMoney89"` | AP pre-shared key |
| `AP_CHANNEL` | 6 | 2.4 GHz channel |
| `AP_MAX_CLIENTS` | 8 | Maximum simultaneous AP clients |
| `AP_IP_*` | 192.168.5.1 | Static AP IP (subnet 192.168.5.0/24) |

The module first tries to join the `SSEI` network as a STA client (60-second timeout). If that fails it falls back to AP-only mode at 192.168.5.1.

### UDP ports

| Constant | Port | Purpose |
|---|---|---|
| `PORT_AGIO` | 8888 | AgIO module discovery and IP management |
| `PORT_GPS` | 9999 | PANDA / NMEA broadcast to AgOpenGPS |
| `PORT_NTRIP` | 2233 | RTCM corrections input |

---

## 5. Central State Object – `GPSState`

```cpp
struct GPSState { ... } gpsState;
```

A single global instance `gpsState` carries the entire runtime state of the module. It is accessed from both the GPS FreeRTOS task (Core 1) and the web-server callbacks (Core 0). Concurrent writes are kept safe in practice because:

- Fields that are read by both cores are declared `volatile`.
- The GPS task snapshots all required string fields into a local `buf` before parsing, avoiding partial reads during transmission.
- Boolean control flags (`enablePandaBroadcast`, `useRawNMEA`, `enableGpsLogging`) are `volatile bool` and written atomically.

### GGA fields (from `$GxGGA` sentences)

| Field | Size | Content |
|---|---|---|
| `fixTime[12]` | 12 | UTC time `HHMMSS.SS` |
| `latitude[15]` | 15 | `DDMM.MMMM` |
| `latNS[3]` | 3 | `"N"` or `"S"` |
| `longitude[15]` | 15 | `DDDMM.MMMM` |
| `lonEW[3]` | 3 | `"E"` or `"W"` |
| `fixQuality[3]` | 3 | 0=none, 1=GPS, 2=DGPS, 4=RTK fixed, 5=RTK float |
| `numSats[4]` | 4 | Satellites used |
| `HDOP[6]` | 6 | Horizontal dilution of precision |
| `altitude[12]` | 12 | Altitude above MSL in metres |
| `ageDGPS[10]` | 10 | Age of DGPS corrections in seconds |

### VTG fields (from `$GxVTG` sentences)

| Field | Content |
|---|---|
| `speedKnots[10]` | Ground speed in knots |
| `vtgHeading[12]` | True course over ground (degrees) |

### IMU fields

All four IMU values are stored as **integer strings of the actual value × 10** to match the PANDA specification (avoids floating-point formatting in time-critical path).

| Field | Content |
|---|---|
| `imuHeading[8]` | Normalised yaw 0–3600 (i.e. 0.0°–360.0°) |
| `imuRoll[8]` | Roll × 10 |
| `imuPitch[8]` | Pitch × 10 |
| `imuYawRate[8]` | Yaw rate (°/s) × 10 |

### Control flags

| Flag | Default | Effect |
|---|---|---|
| `disableHeading` | `false` | When true, heading field in PANDA is forced to `"0"` |
| `invertRoll` | `true` | Negates roll (appropriate for dual-antenna mount) |
| `flipPitchRoll` | `true` | Swaps the pitch and roll axes |
| `enablePandaBroadcast` | `true` | Gate on UDP transmit |
| `useRawNMEA` | `false` | Send raw `$GxGGA` instead of `$PANDA` |
| `enableGpsLogging` | `false` | Write positions to `/gpslog.csv` |

### Broadcast subnet

```cpp
uint8_t agioSubnet[3] = {192, 168, 5};
```

All UDP broadcasts are sent to `agioSubnet[0].agioSubnet[1].agioSubnet[2].255`. The subnet is updated when a PGN 201 packet arrives from AgIO, or automatically from the DHCP address when STA mode connects successfully.

---

## 6. Hardware Objects

```cpp
HardwareSerial    gpsSerial(1);    // UART1
UM980             myGNSS;          // SparkFun UM980 driver
Adafruit_BNO08x   bno08x(-1);      // BNO08x (no reset pin)
sh2_SensorValue_t sensorValue;     // BNO08x event buffer

AsyncWebServer server(80);
AsyncUDP       udpGPS;             // port 9999
AsyncUDP       udpAIO;             // port 8888
AsyncUDP       udpNtrip;           // port 2233
```

Three independent `AsyncUDP` objects are used—one per port—so each can have its own packet handler without multiplexing.

---

## 7. Boot Sequence – `setup()`

```
Serial.begin(115200)
  │
  ▼
Power relay: LOW → 1s delay → HIGH    (ensures downstream hardware gets clean power)
  │
  ▼
LittleFS.begin(true)                   (true = format if mount fails)
  │
  ▼
connectWiFiSTA("SSEI", ..., 60 000 ms)
  │   success → agioSubnet = DHCP subnet
  │   fail    ──────────────────────────────► startWiFiAP()
  │                                             agioSubnet = 192.168.5.x
  ▼
MDNS.begin("esp32_gps")               → esp32_gps.local
  │
  ▼
initGPS()                             → configure UM980, UART1 460800 baud
  │
  ▼
initIMU()                             → BNO08x I²C, rotation vector @ 100 Hz
  │
  ▼
startUDPgps()   (port 9999)
startUDPntrip() (port 2233)
startUDPaio()   (port 8888)
  │
  ▼
server.on(...)  (register all HTTP routes)
server.begin()
  │
  ▼
xTaskCreatePinnedToCore(gpsTask, ..., core=1)
  │
  ▼
setup() returns → loop() runs on Core 0
```

The power relay is driven HIGH after a 1-second pause so that external GPS/IMU hardware is guaranteed to have stabilised power before the firmware attempts communication.

---

## 8. GPS Processing Pipeline – `gpsTask()`

`gpsTask` runs on **Core 1** at FreeRTOS priority 3. It does three things in each iteration:

1. Poll the IMU.
2. Drain the GPS UART into a line buffer.
3. On every complete `\n`-terminated sentence, call `parseNMEA`.

### 8a. UART Character Assembly

```cpp
static char buf[200];
static int  idx = 0;
...
while (gpsSerial.available()) {
    char c = gpsSerial.read();
    if (idx >= sizeof(buf) - 1) { idx = 0; memset(buf, 0, sizeof(buf)); }
    buf[idx++] = c;
    if (c == '\n') {
        buf[idx] = '\0';
        if (idx > 6) parseNMEA(buf);
        idx = 0;
        memset(buf, 0, sizeof(buf));
    }
}
```

- A 200-byte buffer is enough for any standard NMEA sentence (max is ~82 characters in spec, though UM980 GGA lines can be a little longer).
- Buffer overflow protection resets the index rather than allowing memory corruption.
- After a newline, the buffer is cleared so the next sentence starts fresh.
- The minimum-length check (`idx > 6`) rejects noise and empty lines.

### 8b. NMEA Parsing – `parseNMEA()`, `parseGGA()`, `parseVTG()`

`parseNMEA` dispatches by looking for the substring `"GGA"` or `"VTG"` anywhere in the sentence (handles both talker prefixes `$GP`, `$GN`, `$GL`, etc.):

```cpp
if      (strstr(sentence, "GGA")) parseGGA(sentence);
else if (strstr(sentence, "VTG")) parseVTG(sentence);
```

Both parsers use `strtok` with `","` and `"*"` as delimiters to split the sentence into an array of `char*` field pointers, then copy each field into the appropriate `gpsState` member via a lambda helper:

```cpp
auto copy = [&](char* dst, size_t dstSz, int idx) {
    if (idx < n && fields[idx] && *fields[idx]) {
        strncpy(dst, fields[idx], dstSz - 1);
        dst[dstSz - 1] = '\0';
        cleanField(dst);
    }
};
```

`cleanField()` strips trailing `\r`, `\n`, control characters, and spaces so that stored strings are always printable and null-terminated.

**GGA field map** (0-indexed after splitting on `,*`):

| Index | Field | Stored in |
|---|---|---|
| 1 | UTC time | `gpsState.fixTime` |
| 2 | Latitude | `gpsState.latitude` |
| 3 | N/S | `gpsState.latNS` |
| 4 | Longitude | `gpsState.longitude` |
| 5 | E/W | `gpsState.lonEW` |
| 6 | Fix quality | `gpsState.fixQuality` |
| 7 | Satellites | `gpsState.numSats` |
| 8 | HDOP | `gpsState.HDOP` |
| 9 | Altitude | `gpsState.altitude` |
| 13 | DGPS age | `gpsState.ageDGPS` |

**VTG field map:**

| Index | Field | Stored in |
|---|---|---|
| 1 | True course (°) | `gpsState.vtgHeading` |
| 5 | Speed (knots) | `gpsState.speedKnots` |

### 8c. Coordinate Validation – `isGpsPositionValid()`

Before a PANDA sentence is built or logged, the coordinates pass through a layered validator:

1. **Fix quality ≥ 1** – no fix → skip.
2. **String length** – latitude ≥ 4 chars, longitude ≥ 4 chars, hemisphere chars present.
3. **Hemisphere sanity** – `latNS` must be `'N'` or `'S'`; `lonEW` must be `'E'` or `'W'`.
4. **Character-by-character digit check** – every character in both coordinate strings must be a digit or `.` (catches NMEA framing errors that leave letters in the field).
5. **Range check** – decimal-degree conversion must yield |lat| ≤ 90 and |lon| ≤ 180.
6. **NaN / Inf guard** – `isnan()` and `isinf()` on the converted values.

Only after passing all six layers is a broadcast or log write allowed.

### 8d. PANDA Sentence Construction

`buildPandaSentence()` assembles the `$PANDA` sentence by concatenating strings from `gpsState` in a fixed order:

```
$PANDA,<time>,<lat>,<N/S>,<lon>,<E/W>,<quality>,<sats>,<hdop>,<alt>,<dgpsAge>,
       <speed_kn>,<heading×10>,<pitch×10>,<roll×10>,<yawRate×10>*<CHECKSUM>\r\n
```

The heading field is conditionally replaced with `"0"` when `gpsState.disableHeading` is true (useful when running without an IMU).

`calculateChecksum()` performs the standard NMEA XOR: it XORs every byte between `$` (exclusive) and `*` (exclusive), then formats the result as a two-hex-digit suffix.

### 8e. UDP Broadcast Decision Logic

After every valid GGA sentence:

```cpp
readIMU();   // grab freshest IMU frame

if (isGpsPositionValid()) {
    const char* msg = gpsState.useRawNMEA ? buf : (buildPandaSentence(), gpsState.nmea);

    if (gpsState.enablePandaBroadcast && msg) {
        IPAddress bcast(subnet[0], subnet[1], subnet[2], 255);
        udpGPS.writeTo((uint8_t*)msg, strlen(msg), bcast, PORT_GPS);
        taskYIELD();   // give WiFi stack a chance to send
    }
    pandaCount++;
    sendIMUStatus();   // PGN 211 to AgIO
    logGpsData();      // CSV append if enabled
}
```

`taskYIELD()` is called immediately after writing to the UDP socket to let the WiFi task on Core 0 transmit the datagram before the next UART byte arrives.

### 8f. Periodic Diagnostics

Two timers inside `gpsTask`:

- **Every 1 second** – calls `sendSubnetAnnouncement()` (PGN 203 broadcast so AgIO can locate the module).
- **Every 10 seconds** – prints a one-line status to `Serial` with GGA/VTG/PANDA/NTRIP counts and IMU state.

---

## 9. IMU Integration – `readIMU()`

`readIMU()` is called at the top of every `gpsTask` loop iteration AND a second time immediately before building a PANDA sentence (to capture the most recent IMU frame before transmission).

### 9a. Quaternion → Euler Conversion

The BNO08x SHTP protocol delivers orientation as a unit quaternion `(qr, qi, qj, qk)` (real part first). The firmware converts this to Euler angles using the standard ZYX (aerospace) convention:

```cpp
float heading = atan2(2*(qr*qk + qi*qj), 1 - 2*(qj*qj + qk*qk)) * 57.2958f;  // yaw
float pitch   = asin (2*(qr*qj - qk*qi))                          * 57.2958f;  // pitch
float roll    = atan2(2*(qr*qi + qj*qk), 1 - 2*(qi*qi + qj*qj))  * 57.2958f;  // roll
```

`57.2958` = 180/π, converting radians to degrees.

### 9b. Axis Remapping – `flipPitchRoll` and `invertRoll`

The BNO08x axes may not align with the vehicle axes depending on how the board is physically mounted. Two flags correct this without requiring recompilation:

- `flipPitchRoll = true` (default): swaps the computed `pitch` and `roll` values. Use when the sensor is mounted with its X axis pointing up instead of forward.
- `invertRoll = true` (default): negates the roll value. Use when a starboard lean reads as negative in the sensor frame but should read as positive in the vehicle frame (or vice versa).

These flags are currently hardcoded defaults. Future versions could make them web-configurable.

### 9c. Yaw Rate Calculation

```cpp
float dt    = (now - prevYawMs) / 1000.0f;         // seconds
float delta = heading - prevYaw;
if (delta >  180.0f) delta -= 360.0f;              // handle 359→1 wrap
if (delta < -180.0f) delta += 360.0f;
yawRate = delta / dt;
yawRate = clamp(yawRate, -500.0f, 500.0f);         // cap unrealistic spikes
```

The wrap-around correction ensures that a heading crossing 0°/360° produces a sensible rate rather than a ±360°/s spike.

### 9d. IMU Watchdog

If `bno08x.getSensorEvent()` returns false for more than 2 consecutive seconds after the first valid message, `imuState` is set to `2` (failed) and `readIMU()` becomes a no-op. This prevents stale IMU values from being broadcast silently.

---

## 10. UDP Communications

### 10a. Port 9999 – PANDA / NMEA Broadcast (`udpGPS`)

This socket is used **outbound only**. `startUDPgps()` calls `udpGPS.listen(9999)` purely to open the socket; no incoming packet handler is registered.

Every GGA-triggered PANDA (or raw GGA) sentence is sent to the broadcast address of the active subnet. Any AgOpenGPS instance on the same network segment automatically receives the GPS stream without any pairing step.

### 10b. Port 2233 – NTRIP Forwarding (`udpNtrip`)

An external NTRIP proxy (running on the PC or another node) forwards RTCM3 correction bytes as UDP datagrams to port 2233. The firmware's handler writes those bytes directly to the GPS UART with minimal latency:

```cpp
udpNtrip.onPacket([](AsyncUDPPacket pkt) {
    gpsSerial.write(pkt.data(), pkt.length());
    gpsState.ntripCount++;
    gpsState.ntripBytes += pkt.length();
    ...
});
```

The UM980 receives the RTCM stream on its command port and uses it to compute RTK corrections, improving horizontal accuracy from ~1 m (standalone GPS) to ~2 cm (RTK fixed, fix quality 4).

### 10c. Port 8888 – AgIO Protocol (`udpAIO`)

All AgIO binary packets start with the two-byte header `[0x80, 0x81]`. The handler verifies this header, then dispatches by PGN (byte index 3):

#### PGN 200 – Hello / Module Discovery

AgIO broadcasts PGN 200 periodically to discover modules. On receipt, two replies are sent to the broadcast address:

**GPS module reply** (source byte = `AP_IP_4` = 1):
```
[0x80][0x81][1][1][5][0][0][0][0][0][CK]
```

**IMU module reply** (source byte = 79, PGN = 121):
```
[0x80][0x81][79][121][5][0][0][0][0][0][CK]
```

The IMU hello is always sent regardless of `imuState`—AgIO needs to know the module exists even if the IMU is currently initialising. Whether live IMU data is available is communicated through the presence (or absence) of PGN 211 data packets.

The checksum is a simple byte-sum of bytes from index 2 through `payloadLen + 4` (inclusive).

#### PGN 201 – IP / Subnet Update

When AgIO wants the module to switch to a different network subnet:

```cpp
gpsState.agioSubnet[0] = d[7];  // e.g. 192
gpsState.agioSubnet[1] = d[8];  // e.g. 168
gpsState.agioSubnet[2] = d[9];  // e.g. 1
ESP.restart();
```

After storing the new subnet the module reboots so that it re-configures its WiFi on the updated subnet.

### 10d. Subnet Announcement – PGN 203

Sent by `sendSubnetAnnouncement()` once per second from `gpsTask`. Packet layout (13 bytes):

```
[0x80][0x81][79][203][7][IP1][IP2][IP3][IP4][Sub1][Sub2][Sub3][CK]
```

- Source = 79 (IMU module ID).
- IP1..IP4 = the module's current IP address (`agioSubnet[0..2]` + `AP_IP_4`).
- Sub1..Sub3 = the subnet prefix (same as IP1..IP3).

This packet lets AgIO discover the module's location on the network and display it in the device list.

### 10e. IMU Data Packet – PGN 211

Sent by `sendIMUStatus()` on every GGA reception (≈10 Hz). Packet layout (14 bytes):

```
[0x80][0x81][79][211][8][HeadL][HeadH][RollL][RollH][PitchL][PitchH][YawRL][YawRH][CK]
```

All four values are `int16_t` in **little-endian** byte order, units of degrees × 10. This matches the AgOpenGPS IMU module protocol.

---

## 11. WiFi Modes

The module prefers **STA mode** (joining an existing network) because it gives the NTRIP proxy on the PC direct network access to the module. It falls back to **AP mode** if the STA connection times out.

### STA mode (`connectWiFiSTA`)

- Uses `WIFI_STA` mode.
- Waits up to `timeoutMs` (default 10 s, 60 s on first boot attempt).
- On success, reads the DHCP-assigned IP and updates `agioSubnet` to match, so broadcasts go to the right subnet automatically.

### AP mode (`startWiFiAP`)

- Starts `WIFI_AP` (AP-only; not AP+STA).
- Static IP 192.168.5.1 / 255.255.255.0.
- Channel 6. Falls back to channel 1 if `softAP()` fails.
- Power save disabled (`WiFi.setSleep(false)`) and TX power maximised (`19.5 dBm`) for outdoor use.
- Up to 8 simultaneous clients.

In both modes mDNS is started with hostname `esp32_gps`, so the module is reachable at `http://esp32_gps.local` from any device with mDNS support.

---

## 12. GPS Hardware Initialization – `initGPS()`

```
gpsSerial.begin(460800, SERIAL_8N1, GPS_RX_PIN=7, GPS_TX_PIN=8)
  │
  ▼
For attempt in [1, 2]:
    spawn temp FreeRTOS task → myGNSS.begin(gpsSerial, "UM980")
    wait up to 3 000 ms for task to complete
    if ok → connected = true; break
  │
  │ if never connected:
  │   gpsState.gpsState = 2  ("failed – raw NMEA forwarding only")
  │
  ▼ if connected:
    gpsState.gpsState = 1
    sendCommand("CONFIG SIGNALGROUP 1")          // all constellations
    sendCommand("CONFIG RTK RELIABILITY 3 1")
    sendCommand("CONFIG SMOOTH RTKHEIGHT 0")
    sendCommand("CONFIG HEADING RELIABILITY 3")
    sendCommand("CONFIG HEADING VARIABLELENGTH")
    sendCommand("CONFIG SMOOTH HEADING 0")
    sendCommand("GNGGA 0.1")                     // GGA at 10 Hz (GN = all GNSS)
    sendCommand("GPVTG 0.1")                     // VTG at 10 Hz
```

The UM980 connection attempt is wrapped in a short-lived FreeRTOS task so the 3-second handshake timeout does not block `setup()`. If both attempts fail the firmware continues with `gpsState.gpsState = 2`: the UART is still open and any NMEA lines the UM980 sends out of its default configuration will be parsed normally.

---

## 13. IMU Hardware Initialization – `initIMU()`

```
Wire.begin(SDA=3, SCL=4)
scanI2C()                              // prints all detected addresses to Serial
  │
  ▼
spawn temp FreeRTOS task:
    bno08x.begin_I2C(0x4A)
    bno08x.enableReport(SH2_ROTATION_VECTOR, 10000 µs)   // 100 Hz
  │
wait up to 3 000 ms
  │
  ├─ success: gpsState.imuState = 1
  └─ timeout: gpsState.imuState = 2
```

`scanI2C()` iterates all 127 I²C addresses and prints any that acknowledge, helping diagnose wiring issues without a logic analyser.

The `SH2_ROTATION_VECTOR` report type fuses the accelerometer, gyroscope, and magnetometer into a calibrated quaternion. The 10 000 µs (= 100 Hz) report interval gives a low-latency heading and tilt that is re-read just before every 10 Hz PANDA transmission.

---

## 14. Web Server and REST API

The firmware uses **ESPAsyncWebServer** so all HTTP responses are non-blocking. The following routes are registered:

### 14a. Static Pages

| Route | File served |
|---|---|
| `GET /` | `/index.html` from LittleFS |
| `GET /index.html` | `/index.html` from LittleFS |
| `GET /map.html` | `/map.html` from LittleFS |

### 14b. Diagnostic Endpoints

| Route | Response |
|---|---|
| `GET /getDebugVars` | JSON array of human-readable status strings |
| `GET /getGpsPos` | JSON `{valid, lat, lon, alt, fixQuality, sats, hdop, heading, speed}` |
| `GET /getFiles` | JSON array of `{name, size}` for files on LittleFS |
| `GET /version` | JSON `{name, version}` – used by the `pc_server` auto-updater |
| `GET /reboot` | Triggers `ESP.restart()` after 100 ms |

`/getDebugVars` calls `updateDebugVars()` which assembles a `std::vector<String>` covering system info, GPS fields, IMU fields, NTRIP statistics, and broadcast subnet. The web UI polls this at 2 Hz.

`/getGpsPos` converts the stored NMEA-format latitude/longitude (DDMM.MMMM) to decimal degrees and returns them as `double` values suitable for Leaflet's `L.marker([lat, lon])`.

### 14c. Control Endpoints

| Route | Parameter | Effect |
|---|---|---|
| `GET /setGpsForwarding` | `enable=0\|1` | Toggle `gpsState.enablePandaBroadcast` |
| `GET /getGpsForwarding` | – | Returns `{enabled: bool}` |
| `GET /setMessageFormat` | `raw=0\|1` | Toggle `gpsState.useRawNMEA` |
| `GET /getMessageFormat` | – | Returns `{useRawNMEA: bool}` |

### 14d. GPS Logging Endpoints

| Route | Effect |
|---|---|
| `GET /setGpsLogging?enable=1` | Creates `/gpslog.csv` with header row (if not exists), sets `enableGpsLogging = true` |
| `GET /setGpsLogging?enable=0` | Sets `enableGpsLogging = false` |
| `GET /getGpsLogging` | Returns `{enabled, size, lines}` |
| `GET /downloadGpsLog` | Streams `/gpslog.csv` as a download attachment |
| `GET /clearGpsLog` | Deletes `/gpslog.csv` |

### 14e. OTA Update Endpoints

| Route | Handler | Purpose |
|---|---|---|
| `POST /update` | `handleFirmwareUpload` | Flash new firmware `.bin` (filename must start with `ESP32_GPS`) |
| `POST /updatefs` | `handleFilesystemUpload` | Flash new LittleFS image |
| `POST /upload` | `handleFileUpload` | Upload arbitrary file to LittleFS |

`handleFirmwareUpload` guards against flashing the wrong module: if the uploaded filename does not begin with `NAME` (`"ESP32_GPS"`), the upload is rejected with HTTP 400. This prevents accidentally flashing an `ESP32_AIO` build onto this module.

Both OTA handlers call `Update.end(true)` which sets the boot partition and then `ESP.restart()` after a 1-second delay so the response can be sent.

---

## 15. GPS Data Logging – `logGpsData()`

Called after every valid PANDA broadcast when `enableGpsLogging` is true.

Steps:
1. Check `isGpsPositionValid()` (same guard as PANDA broadcast).
2. Copy all required string fields to local buffers to avoid race conditions with the NMEA parser running on the same task.
3. Validate the local copies again (character-by-character digit checks + NMEA range checks).
4. Convert NMEA DDMM.MMMM to decimal degrees.
5. Append a CSV line to `/gpslog.csv`:

```
timestamp_ms,latitude,longitude,altitude_m,fix_quality,satellites,hdop,speed_kn,heading_deg
1234567,43.1234567,-93.4567890,280.3,4,14,0.8,2.3,1845
```

The `timestamp_ms` column is `millis()` (milliseconds since boot), not absolute UTC, which keeps the code simple and avoids time-sync complexity.

The log file is limited only by LittleFS capacity (~200 KB of usable flash on the Xiao S3). The web interface provides download and clear endpoints for management.

---

## 16. Web Interface – HTML / JavaScript

Both HTML pages are served from LittleFS and require no external frameworks beyond Leaflet (loaded from unpkg CDN on `map.html`).

### `index.html` – Status Dashboard

- Green-themed card grid layout.
- Polls `GET /getDebugVars` every 500 ms. The response is a flat JSON array of `"Key: Value"` strings; the JavaScript splits on `": "` and renders each entry as a `<div class="stat-row">`.
- Separate control cards for:
  - PANDA broadcast enable/disable (`/setGpsForwarding`)
  - Message format switch (`/setMessageFormat`)
  - GPS log start/stop and download (`/setGpsLogging`, `/downloadGpsLog`, `/clearGpsLog`)
- OTA form (hidden by default, revealed by clicking "OTA Update" in the nav bar).
- Status badges use CSS classes `badge-ok` (green), `badge-warn` (yellow), `badge-err` (red), `badge-off` (grey).

### `map.html` – Live Position Map

- Uses the **Leaflet** mapping library with an OpenStreetMap tile layer.
- Polls `GET /getGpsPos` every 1 second.
- Displays a coloured marker at the current position (green = RTK fixed, yellow = float/DGPS, red = basic GPS, grey = no fix).
- Shows a breadcrumb track (polyline of recent positions).
- Control bar at the top shows lat/lon, fix quality badge, satellites, heading, and speed.
- "Follow me" mode auto-pans the map to keep the current position centred.

---

## 17. Arduino `loop()`

`loop()` runs on Core 0 (the WiFi/network core). It does almost nothing:

```cpp
void loop() {
    static uint32_t lastDebug = 0;
    if (millis() - lastDebug >= 15000) {
        lastDebug = millis();
        Serial.printf("[Loop] Clients=%d  PANDA=%lu  NTRIP=%lu  Heap=%u\n", ...);
    }
    delay(100);
}
```

All real work is in `gpsTask` on Core 1. `loop()` only prints a 15-second heartbeat line to Serial and yields via `delay(100)` so the WiFi background tasks and web-server callbacks get CPU time. There is no LED update code in the shipping build (the NeoPixel section is commented out because the Xiao S3 does not have a built-in NeoPixel).

---

## 18. Data-Flow Diagram

```
  ┌─────────────────────────────────────────────────────────────────────┐
  │                         ESP32-S3 (XIAO)                             │
  │                                                                     │
  │  Core 1 – gpsTask (priority 3)                                      │
  │  ┌─────────────────────────────────────────────────────────────┐   │
  │  │                                                             │   │
  │  │  UM980 GNSS ──UART1──► char assembly ──► parseNMEA()       │   │
  │  │     460800 baud          buf[200]         parseGGA()        │   │
  │  │                                           parseVTG()        │   │
  │  │  BNO08x IMU ───I²C──► readIMU() ──► gpsState IMU fields    │   │
  │  │     100 Hz             quaternion        ×10 integer        │   │
  │  │                        → Euler           strings            │   │
  │  │                                                             │   │
  │  │  On every valid GGA:                                        │   │
  │  │    isGpsPositionValid()? ──NO──► skip                       │   │
  │  │           │ YES                                             │   │
  │  │           ▼                                                 │   │
  │  │    useRawNMEA?                                              │   │
  │  │      NO ──► buildPandaSentence() ──► gpsState.nmea         │   │
  │  │      YES──► use raw buf                                     │   │
  │  │           │                                                 │   │
  │  │           ▼                                                 │   │
  │  │    enablePandaBroadcast?                                    │   │
  │  │      YES──► udpGPS.writeTo(bcast:9999)                      │   │
  │  │                                                             │   │
  │  │    sendIMUStatus() ──► udpAIO.writeTo(bcast:8888, PGN211)  │   │
  │  │    logGpsData()    ──► LittleFS /gpslog.csv (if enabled)   │   │
  │  │                                                             │   │
  │  │  Every 1 s:  sendSubnetAnnouncement() → PGN203:8888        │   │
  │  │  Every 10 s: Serial diagnostic print                        │   │
  │  └─────────────────────────────────────────────────────────────┘   │
  │                                                                     │
  │  Core 0 – WiFi stack + Arduino loop()                               │
  │  ┌─────────────────────────────────────────────────────────────┐   │
  │  │                                                             │   │
  │  │  UDP:2233 ──► NTRIP handler ──► gpsSerial.write() → UM980  │   │
  │  │                                                             │   │
  │  │  UDP:8888 ──► AgIO handler                                  │   │
  │  │                PGN200 Hello  ──► reply PGN200 + PGN121      │   │
  │  │                PGN201 SetIP  ──► update subnet + reboot     │   │
  │  │                                                             │   │
  │  │  HTTP:80  ──► AsyncWebServer                                │   │
  │  │                /getDebugVars, /getGpsPos, /getFiles         │   │
  │  │                /setGpsForwarding, /setMessageFormat         │   │
  │  │                /setGpsLogging, /downloadGpsLog              │   │
  │  │                /update (OTA), /updatefs, /upload            │   │
  │  │                / (index.html), /map.html                    │   │
  │  │                                                             │   │
  │  │  loop(): 15 s heartbeat to Serial, delay(100)               │   │
  │  └─────────────────────────────────────────────────────────────┘   │
  └─────────────────────────────────────────────────────────────────────┘

  External connections:
    UM980 GNSS receiver  ←──UART1──── GPS corrections (RTCM via UDP:2233)
                         ────UART1──► NMEA GGA + VTG sentences
    BNO08x IMU           ←──I²C─────► rotation vector @ 100 Hz
    AgOpenGPS (PC)       ←──UDP:9999─ PANDA or raw NMEA
                         ←──UDP:8888─ module hello replies, PGN211 IMU data
                         ────UDP:8888─ PGN200 hello, PGN201 IP update
    NTRIP proxy (PC)     ────UDP:2233─ RTCM3 correction stream
    Web browser          ←──HTTP:80── dashboard, map, OTA
```
