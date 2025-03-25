#ifndef definitions
#define definitions

#include "Arduino.h"

#define VERSION "1.0.00"
// #define CONFIG_URL   "http://192.168.0.151:5500/OTA_Config.json" 
#define SPRAYER = true
#define SKETCH_CONFIG "Joystick"
#define HEARBEAT_PGN 155
// const char* filename = "/CONFIG.txt";
// const char* updateLogFile = "/Update_Log.txt"; 
// const char* dataLogFilePath = "/Data_Logs";



#define SPI_CS 19
#define SPI_MOSI 20
#define SPI_CLK 21
#define SPI_MISO 47

String cmds[] = {"Sec_Enable","Steer_Eanble","Left_Lift","Left_Lower","Right_Lift","Right_Lower","Center_Lift","Center_Lower"};


// uint8_t inputPins[] = {1,2,3,4,5,6,9,8};
uint8_t inputPins[] = {43,8,5,6,4,7,3,2};


#endif