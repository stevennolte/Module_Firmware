#ifndef ImuStructs
#define ImuStructs

#include "Arduino.h"

struct imuStruct_t{
  
} imuStruct;

struct euler_t {
  float yaw;
  float pitch;
  float roll;
} ypr;

#endif