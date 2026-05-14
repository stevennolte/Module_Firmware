# Module Pinout Reference

This document describes the GPIO pin assignments for every firmware module in this repository.  All modules target the **ESP32-S3** unless noted otherwise.

---

## Table of Contents

1. [ESP32_AIO (All-In-One)](#esp32_aio)
2. [ESP32_GPS](#esp32_gps)
3. [ESP32_IMU_CAN](#esp32_imu_can)
4. [ESP32_Product_Controller](#esp32_product_controller)
5. [ESP32_Fold_Controller](#esp32_fold_controller)
6. [ESP32_Rotary_Sensor](#esp32_rotary_sensor)
7. [ESP32_Row_Controller](#esp32_row_controller)
8. [ESP32_WiFi_AP](#esp32_wifi_ap)
9. [Section_Controller](#section_controller)
10. [Power_Monitor](#power_monitor)
11. [Fold_Controller](#fold_controller)
12. [GPS_Receiver](#gps_receiver)
13. [Joystick](#joystick)
14. [Sprayer_Fold_Controller](#sprayer_fold_controller)

---

## ESP32_AIO

**All-In-One agricultural controller** – GPS, IMU, steering, and power management on a single ESP32-S3 board.

### ESP32-S3 GPIO

| GPIO | Signal | Direction | Description |
|------|--------|-----------|-------------|
| 4  | WORK_SWITCH | Input | Work / implement control switch |
| 5  | STEER_SWITCH | Input | Steering engage / disengage switch |
| 6  | STEER_TEST | Input | Steering system test mode |
| 7  | MOTOR_A (INA) | Output | Motor driver direction pin A |
| 8  | MOTOR_B (INB) | Output | Motor driver direction pin B |
| 9  | MOTOR_PWM | Output | Motor driver PWM speed control |
| 10 | GPS_FIX_IND | Output | GPS fix status indicator |
| 11 | RTK_FIX_IND | Output | RTK fix status indicator |
| 12 | BNO_RX | Input | BNO08x IMU serial RX (UART2) |
| 13 | GPS_RX | Input | GPS receiver NMEA RX (UART1) |
| 14 | GPS_TX / ENA | Output | GPS TX (UART1) / Motor enable A |
| 15 | ENB | Output | Motor driver enable B |
| 39 | MAIN_POWER | Output | Main power control relay |
| 40 | MAIN_POWER_DEN | Output | Main power enable |
| 41 | SDA | I/O | I²C data – Wire0 |
| 42 | SCL | Output | I²C clock – Wire0 |
| 48 | LED | Output | Built-in NeoPixel status LED |

### I²C Devices (Wire0 – SDA=41, SCL=42)

| Address | Device | Role |
|---------|--------|------|
| 0x20 | MCP23017 | 16-bit I/O expander (inputs & status LEDs) |
| 0x48 | ADS1115 | 16-bit ADC (WAS + current sensing) |

#### MCP23017 Pin Map (address 0x20)

| MCP Pin | Signal | Direction | Description |
|---------|--------|-----------|-------------|
| GPA0 (0) | WORK_SWITCH | Input | Work switch |
| GPA1 (1) | REMOTE_SWITCH | Input | Remote control switch |
| GPA2 (2) | STEER_SWITCH | Input | Steering engage switch |
| GPB0 (8) | POWER_ON_IND | Output | System power-on indicator LED |
| GPB1 (9) | ETH_GOOD | Output | Ethernet / WiFi status LED |
| GPB2 (10) | GPS_FIX | Output | GPS fix indicator LED |
| GPB3 (11) | RTK_FIX | Output | RTK fix indicator LED |
| GPB4 (12) | STEER_STANDBY | Output | Steering standby indicator LED |
| GPB5 (13) | STEER_ACTIVE | Output | Steering active indicator LED |
| GPB6 (14) | MOTOR_ENB | Output | Motor driver enable B |
| GPB7 (15) | MOTOR_ENA | Output | Motor driver enable A |

#### ADS1115 Channel Map (address 0x48)

| Channel | Signal | Description |
|---------|--------|-------------|
| AIN0 | WAS_HIGH | Wheel angle sensor – high side |
| AIN1 | WAS_LOW | Wheel angle sensor – low side |
| AIN2 | MOTOR_IS | Steering motor current sense |
| AIN3 | MAIN_POWER_IS | Main power current sense |

---

## ESP32_GPS

**Standalone GPS / NTRIP / IMU module** running on an **ESP32-S3 Xiao**.

Provides UM980 GNSS data, NTRIP corrections, BNO08x IMU, and PANDA sentence generation over WiFi UDP.

### ESP32-S3 Xiao GPIO

| GPIO | Signal | Direction | Description |
|------|--------|-----------|-------------|
| 1  | POWER_RELAY | Output | Power relay control |
| 3  | BNO_SDA | I/O | BNO08x IMU I²C SDA |
| 4  | BNO_SCL | Output | BNO08x IMU I²C SCL |
| 7  | GPS_RX | Input | UM980 NMEA data RX (UART1) |
| 8  | GPS_TX | Output | Commands / NTRIP to UM980 (UART1) |

### I²C Devices

| Address | Device | Role |
|---------|--------|------|
| 0x4A | BNO08x | IMU – heading, pitch, roll |

### UART

| Port | Baud | Pins | Description |
|------|------|------|-------------|
| UART1 | 460800 | RX=7, TX=8 | UM980 GNSS receiver |

---

## ESP32_IMU_CAN

**IMU-to-CAN bridge** – reads BNO085 attitude data and transmits it over J1939 CAN bus.

### ESP32-S3 GPIO

| GPIO | Signal | Direction | Description |
|------|--------|-----------|-------------|
| 1  | CAN_TX | Output | TWAI / J1939 CAN transmit |
| 2  | CAN_RX | Input | TWAI / J1939 CAN receive |
| 41 | SDA | I/O | I²C data – BNO085 |
| 42 | SCL | Output | I²C clock – BNO085 |
| 48 | LED | Output | Built-in NeoPixel status LED |

### I²C Devices (SDA=41, SCL=42)

| Address | Device | Role |
|---------|--------|------|
| 0x4B | BNO08x | IMU – roll, pitch, heading |

### CAN Bus

| Parameter | Value |
|-----------|-------|
| Protocol | J1939 (29-bit extended IDs) |
| TX PGN | 0xFF04 (Proprietary B IMU data) |
| Default SA | 0x80 |
| Default TX rate | 50 ms |

---

## ESP32_Product_Controller

**Sprayer / liquid product rate controller** with flow meter, pressure sensing, section outputs, and CAN bus.

### ESP32-S3 GPIO

| GPIO | Signal | Direction | Description |
|------|--------|-----------|-------------|
| 1  | CAN_TX | Output | TWAI CAN transmit |
| 2  | CAN_RX | Input | TWAI CAN receive |
| 5  | SDA_H | I/O | I²C data – Wire1 (secondary bus) |
| 6  | SCL_H | Output | I²C clock – Wire1 (secondary bus) |
| 9  | SECTION_5 | Output | Section valve output 5 |
| 10 | SECTION_4 | Output | Section valve output 4 |
| 11 | SECTION_3 | Output | Section valve output 3 |
| 12 | SECTION_2 | Output | Section valve output 2 |
| 13 | SECTION_1 | Output | Section valve output 1 |
| 14 | FLOW_PIN | Input | Flow meter pulse input |
| 41 | SDA | I/O | I²C data – Wire0 (primary bus) |
| 42 | SCL | Output | I²C clock – Wire0 (primary bus) |
| 48 | LED | Output | Built-in NeoPixel status LED |

### I²C Devices

| Bus | Address | Device | Role |
|-----|---------|--------|------|
| Wire0 (41/42) | 0x48 | ADS1015 | 12-bit ADC (pressure / flow) |

---

## ESP32_Fold_Controller

**Hydraulic fold controller** – drives up to 7 double-acting fold valves plus a directional valve.

### ESP32-S3 GPIO

| GPIO | Signal | Direction | Description |
|------|--------|-----------|-------------|
| 9  | FOLD_7A | Output | Fold valve 7 (RH Outer Wing) – coil A (`foldPins1[6]`) |
| 11 | FOLD_2A | Output | Fold valve 2 (LH Wing Rotate) – coil A (`foldPins1[1]`) |
| 12 | FOLD_1A | Output | Fold valve 1 (LH Outer Wing) – coil A (`foldPins1[0]`) |
| 13 | DIR_VALVE | Output | Directional valve control |
| 14 | POWER_PIN | Output | Power relay control |
| 15 | FOLD_3 | Output | Fold valve 3 (LH Wing Lift) shared coil pin |
| 16 | FOLD_4 | Output | Fold valve 4 (Center Lift) shared coil pin |
| 17 | FOLD_5 | Output | Fold valve 5 (RH Wing Lift) shared coil pin |
| 18 | FOLD_6A | Output | Fold valve 6 (RH Wing Rotate) – coil A (`foldPins1[5]`) |
| 41 | SDA | I/O | I²C data |
| 42 | SCL | Output | I²C clock |
| 48 | LED | Output | Built-in NeoPixel status LED |

**Valve pin mapping:**

| Valve Index | Function | Pin A (`foldPins1`) | Pin B (`foldPins2`) |
|-------------|----------|---------------------|---------------------|
| 0 | LH Outer Wing | 12 | 6 |
| 1 | LH Wing Rotate | 11 | 7 |
| 2 | LH Wing Lift | 15 | 15 |
| 3 | Center Lift | 16 | 16 |
| 4 | RH Wing Lift | 17 | 17 |
| 5 | RH Wing Rotate | 18 | 10 |
| 6 | RH Outer Wing | 9 | 8 |

---

## ESP32_Rotary_Sensor

**Magnetic rotary angle sensor** – reads TMAG5273 position and outputs an analog voltage via MCP4725 DAC.

### ESP32-S3 GPIO

| GPIO | Signal | Direction | Description |
|------|--------|-----------|-------------|
| 5  | SDA_H | I/O | I²C data – Wire1 (MCP4725 DAC) |
| 6  | SCL_H | Output | I²C clock – Wire1 |
| 41 | SDA | I/O | I²C data – Wire0 (TMAG5273 sensor) |
| 42 | SCL | Output | I²C clock – Wire0 |
| 48 | LED | Output | Built-in NeoPixel status LED |

### I²C Devices

| Bus | Address | Device | Role |
|-----|---------|--------|------|
| Wire0 (41/42) | 0x22 | TMAG5273 | Magnetic angle sensor |
| Wire1 (5/6) | 0x60 | MCP4725 | 12-bit DAC – analog angle output |

---

## ESP32_Row_Controller

**Planter row-unit controller** – independently controls up to 12 row MOSFET outputs and reads toolbar-position sensors.

### ESP32-S3 GPIO

| GPIO | Signal | Direction | Description |
|------|--------|-----------|-------------|
| 1  | TOOLBAR_1 | Input | Toolbar sensor 1 (HIGH = raised) |
| 2  | TOOLBAR_2 | Input | Toolbar sensor 2 (HIGH = raised) |
| 4  | ROW_12 | Output | Row unit 12 MOSFET |
| 5  | ROW_11 | Output | Row unit 11 MOSFET |
| 6  | ROW_10 | Output | Row unit 10 MOSFET |
| 7  | ROW_9 | Output | Row unit 9 MOSFET |
| 8  | ROW_4 | Output | Row unit 4 MOSFET |
| 9  | ROW_3 | Output | Row unit 3 MOSFET |
| 10 | ROW_2 | Output | Row unit 2 MOSFET |
| 11 | ROW_1 | Output | Row unit 1 MOSFET |
| 12 | POWER_RELAY | Output | Main power relay (turns on after boot) |
| 15 | ROW_8 | Output | Row unit 8 MOSFET |
| 16 | ROW_7 | Output | Row unit 7 MOSFET |
| 17 | ROW_6 | Output | Row unit 6 MOSFET |
| 18 | ROW_5 | Output | Row unit 5 MOSFET |
| 48 | LED | Output | Built-in NeoPixel status LED |

Row pin order in firmware: `{11, 10, 9, 8, 18, 17, 16, 15, 7, 6, 5, 4}` (row 1 → row 12).

---

## ESP32_WiFi_AP

**WiFi Access Point / bridge module** – bridges a STA upstream network to an AP for field devices.  Runs on an **ESP32 DevKitV1** (not S3).

### ESP32 DevKitV1 GPIO

| GPIO | Signal | Direction | Description |
|------|--------|-----------|-------------|
| 2  | LED | Output | Built-in LED status indicator |

> This module has no additional GPIO peripherals; all function is network-only.

---

## Section_Controller

**Section / rate controller** – manages spray sections, flow sensing, regulator, and CAN communication.  Identical GPIO layout to `ESP32_Product_Controller`.

### ESP32-S3 GPIO

| GPIO | Signal | Direction | Description |
|------|--------|-----------|-------------|
| 1  | CAN_TX | Output | TWAI CAN transmit |
| 2  | CAN_RX | Input | TWAI CAN receive |
| 5  | SDA_H | I/O | I²C data – Wire1 |
| 6  | SCL_H | Output | I²C clock – Wire1 |
| 9  | SECTION_5 | Output | Section valve output 5 |
| 10 | SECTION_4 | Output | Section valve output 4 |
| 11 | SECTION_3 | Output | Section valve output 3 |
| 12 | SECTION_2 | Output | Section valve output 2 |
| 13 | SECTION_1 | Output | Section valve output 1 |
| 14 | FLOW_PIN | Input | Flow meter pulse input |
| 41 | SDA | I/O | I²C data – Wire0 |
| 42 | SCL | Output | I²C clock – Wire0 |
| 48 | LED | Output | Built-in NeoPixel status LED |

### I²C Devices

| Bus | Address | Device | Role |
|-----|---------|--------|------|
| Wire0 (41/42) | 0x48 | ADS1115 | 16-bit ADC (pressure sensing) |

---

## Power_Monitor

**System power and CAN bus monitor** – reads key/battery power states via INA219 and bridges CAN traffic over UDP.

> **Note:** This module uses an older ESP32 (not S3). I²C is on GPIO 5/4 instead of 41/42.

### ESP32 GPIO

| GPIO | Signal | Direction | Description |
|------|--------|-----------|-------------|
| 4  | SCL_0 | Output | I²C clock – INA219 |
| 5  | SDA_0 | I/O | I²C data – INA219 |
| 12 | KEY_POWER | Input | Key/ignition power sense input |
| 14 | BAT_POWER | Output | Battery power relay control |
| 41 | CAN_TX (CAN2 mode) | Output | TWAI CAN transmit |
| 42 | CAN_RX (CAN2 mode) | Input | TWAI CAN receive |

> **CAN pin selection:** The firmware supports two configurations via compile-time define.  
> - `CAN1`: RX=2, TX=1  
> - `CAN2` (default): RX=41, TX=42

### I²C Devices (SDA=5, SCL=4)

| Address | Device | Role |
|---------|--------|------|
| auto-detect | INA219 | Bus voltage and current monitor |

---

## Fold_Controller

**Legacy hydraulic fold controller** – drives the same fold-valve hardware as `ESP32_Fold_Controller` but communicates over BLE.

### ESP32-S3 GPIO

| GPIO | Signal | Direction | Description |
|------|--------|-----------|-------------|
| 8  | FOLD_7B | Output | Fold valve 7 (RH Outer Wing) – coil B (`foldPins2[6]`) |
| 9  | FOLD_7A | Output | Fold valve 7 (RH Outer Wing) – coil A (`foldPins1[6]`) |
| 10 | FOLD_6B | Output | Fold valve 6 (RH Wing Rotate) – coil B (`foldPins2[5]`) |
| 11 | FOLD_2A | Output | Fold valve 2 (LH Wing Rotate) – coil A (`foldPins1[1]`) |
| 12 | FOLD_1A | Output | Fold valve 1 (LH Outer Wing) – coil A (`foldPins1[0]`) |
| 13 | DIR_VALVE | Output | Directional valve control |
| 15 | FOLD_3 | Output | Fold valve 3 (LH Wing Lift) shared coil pin |
| 16 | FOLD_4 | Output | Fold valve 4 (Center Lift) shared coil pin |
| 17 | FOLD_5 | Output | Fold valve 5 (RH Wing Lift) shared coil pin |
| 18 | FOLD_6A | Output | Fold valve 6 (RH Wing Rotate) – coil A (`foldPins1[5]`) |
| 48 | LED | Output | Built-in NeoPixel status LED |

**Valve pin mapping** (same as ESP32_Fold_Controller):

| Valve Index | Function | Pin A (`foldPins1`) | Pin B (`foldPins2`) |
|-------------|----------|---------------------|---------------------|
| 0 | LH Outer Wing | 12 | 6 |
| 1 | LH Wing Rotate | 11 | 7 |
| 2 | LH Wing Lift | 15 | 15 |
| 3 | Center Lift | 16 | 16 |
| 4 | RH Wing Lift | 17 | 17 |
| 5 | RH Wing Rotate | 18 | 10 |
| 6 | RH Outer Wing | 9 | 8 |

---

## GPS_Receiver

**Legacy GPS / IMU receiver** with BNO08x over I²C and NMEA parsing.  Bluetooth LE remote support included.

### ESP32-S3 GPIO

| GPIO | Signal | Direction | Description |
|------|--------|-----------|-------------|
| 4  | SCL_0 | Output | I²C clock – BNO08x |
| 5  | SDA_0 | I/O | I²C data – BNO08x |
| 48 | LED | Output | Built-in NeoPixel status LED |

### UART

| Port | Baud | Description |
|------|------|-------------|
| Serial2 (default pins) | 460800 | GPS NMEA data input |

### I²C Devices (SDA=5, SCL=4)

| Address | Device | Role |
|---------|--------|------|
| 0x4A / 0x4B (auto-scan) | BNO08x | IMU – heading, pitch, roll |

---

## Joystick

**Field joystick controller** – reads physical button inputs and transmits fold / section / steering commands over UDP.

### ESP32-S3 GPIO

| GPIO | Signal | Direction | Description |
|------|--------|-----------|-------------|
| 1  | INPUT_1 | Input | Joystick button / switch input |
| 2  | INPUT_2 / CENTER_LOWER | Input | Center lower command |
| 3  | INPUT_3 / CENTER_LIFT | Input | Center lift command |
| 4  | INPUT_4 / RH_LIFT | Input | Right-hand lift command |
| 5  | INPUT_5 / LH_LIFT | Input | Left-hand lift command |
| 6  | INPUT_6 / LH_LOWER | Input | Left-hand lower command |
| 7  | INPUT_7 / RH_LOWER | Input | Right-hand lower command |
| 8  | INPUT_8 / RH_BTN | Input | Right-hand button |
| 9  | INPUT_9 | Input | General input |
| 19 | SPI_CS | Output | SPI chip select |
| 20 | SPI_MOSI | Output | SPI MOSI |
| 21 | SPI_CLK | Output | SPI clock |
| 41 | SDA | I/O | I²C data |
| 42 | SCL | Output | I²C clock |
| 43 | INPUT_10 / AUTO_STEER | Input | Auto-steer enable switch |
| 47 | SPI_MISO | Input | SPI MISO |
| 48 | LED | Output | Built-in NeoPixel status LED |

### I²C Devices (SDA=41, SCL=42)

| Address | Device | Role |
|---------|--------|------|
| 0x20 | MCP23017 | Possible I/O expander |
| 0x22 | TLE493D | Possible magnetic sensor |

---

## Sprayer_Fold_Controller

**Combined sprayer / fold controller** – manages hydraulic fold valves, directional valve, power relay, and SD card logging.

### ESP32-S3 GPIO

| GPIO | Signal | Direction | Description |
|------|--------|-----------|-------------|
| 9  | ADDRESS_PIN / FOLD_7A | Input / Output | Address-select input at boot (read once); used as fold valve 7 coil A output during operation |
| 11 | FOLD_2A | Output | Fold valve 2 (LH Wing Rotate) – coil A (`foldPins1[1]`) |
| 12 | FOLD_1A | Output | Fold valve 1 (LH Outer Wing) – coil A (`foldPins1[0]`) |
| 13 | DIR_VALVE | Output | Directional valve control |
| 14 | POWER_RELAY | Output | Power relay control |
| 15 | FOLD_3 | Output | Fold valve 3 (LH Wing Lift) shared coil pin |
| 16 | FOLD_4 | Output | Fold valve 4 (Center Lift) shared coil pin |
| 17 | FOLD_5 | Output | Fold valve 5 (RH Wing Lift) shared coil pin |
| 18 | FOLD_6A | Output | Fold valve 6 (RH Wing Rotate) – coil A (`foldPins1[5]`) |
| 19 | SPI_CS | Output | SD card SPI chip select (HSPI) |
| 20 | SPI_MOSI | Output | SD card SPI MOSI (HSPI) |
| 21 | SPI_CLK | Output | SD card SPI clock (HSPI) |
| 47 | SPI_MISO | Input | SD card SPI MISO (HSPI) |

> **GPIO 9 dual-use:** `ADDRESS_PIN` is read as an input during WiFi setup to determine the module's IP address offset; thereafter the pin is reconfigured as an output driving fold valve 7 coil A.

**Valve pin mapping:**

| Valve Index | Function | Pin A (`foldPins1`) | Pin B (`foldPins2`) |
|-------------|----------|---------------------|---------------------|
| 0 | LH Outer Wing | 12 | 6 |
| 1 | LH Wing Rotate | 11 | 7 |
| 2 | LH Wing Lift | 15 | 15 |
| 3 | Center Lift | 16 | 16 |
| 4 | RH Wing Lift | 17 | 17 |
| 5 | RH Wing Rotate | 18 | 10 |
| 6 | RH Outer Wing | 9 | 8 |
