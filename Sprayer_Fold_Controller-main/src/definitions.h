#ifndef definitions
#define definitions

#include "Arduino.h"


#define VERSION "1.0.0098"
#define LOOPBACK false
#define CANFLOWMETERS false
#define SEND_UDP true
#define CURRENT_SENSE true
#define CAN_FLOW_INPUT true
#define EEPROM_SIZE 2

const char* filename = "/CONFIG.txt";
const char* updateLogFile = "/Update_Log.txt"; 
const char* dataLogFilePath = "/Data_Logs";

#define STARTUP_DELAY 1000
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_CHANNEL1            LEDC_CHANNEL_0
#define LEDC_CHANNEL2            LEDC_CHANNEL_1
#define LEDC_CHANNEL3            LEDC_CHANNEL_2
#define LEDC_CHANNEL4            LEDC_CHANNEL_3
#define LH_OUTER_FLIP_CAP 11
#define LH_OUTER_FLIP_ROD 1
#define LH_WING_ROTATE_OUT 2
#define LH_WING_ROTATE_IN 3
#define LH_WING_RAISE 4
#define RH_WING_RAISE 5
#define CENTER_RAISE 6

#define RH_WING_ROTATE_IN 7
#define RH_WING_ROTATE_OUT 8
#define RH_OUTER_FLIP_CAP 9
#define RH_OUTER_FLIP_ROD 10
#define DIRECTIONAL_VALVE 0
// uint8_t foldPins[] = {13,12,11,10,9,8,18,17,16,15,7,6};
uint8_t foldPins1[] = {12,11,15,16,17,18,9};
uint8_t foldPins2[] = {6,7,15,16,17,10,8};
uint8_t directionalValvePin = 13;

#define lhOuterWingRotate 0
#define lhWingRotate 1
#define lhWingLift 2
#define centerLift 3
#define rhWingLift 4
#define rhWingRotate 5
#define rhOuterWingRotate 6
uint8_t udpFoldCmds[7];
uint8_t foldCmds[7];
uint8_t joystickCmds[8];

#define SPI_CS 19
#define SPI_MOSI 20
#define SPI_CLK 21
#define SPI_MISO 47


#define ADDRESS_PIN 9
#define POWER_RELAY_PIN 14
#define DEBUG_PRINT_WINDOW 250


char CONFIG_URL[64];
uint64_t debugPrintTimestamp = 0;
uint16_t logCnt;
uint32_t timeStamp;

#endif