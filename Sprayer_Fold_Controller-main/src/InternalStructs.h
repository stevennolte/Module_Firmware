#ifndef InternalStructs
#define InternalStructs

#include "Arduino.h"

struct programStates_t{
  bool wifiConnected;
  bool pwmDriverConnected;
  bool adsConnected;
  bool udpConnected;
  bool currentSensor1;
  bool currentSensor2;
  bool currentSensor3;
  uint32_t udpTimer;
  bool freqLoopback;
  bool BNOconnected;
} programStates;

struct timestampStruct_t{
  uint32_t timestamp;
};

union timestamp_u{
timestampStruct_t timestampStruct;
uint8_t bytes[sizeof(timestampStruct_t)];
} timestampUnion;

struct sectionDataStruct_t {
  uint8_t sectionNum;
  uint16_t dutyCycleCMD;
  uint16_t frequencyCMD;
  uint16_t frequencyRPT;
  uint16_t current_ma;
  uint8_t CanFlowmeterSA;
  uint16_t CanFlowmeterFlowrate;
};

#endif