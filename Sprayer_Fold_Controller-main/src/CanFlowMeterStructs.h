#ifndef CanFlowMeterStructs
#define CanFlowMeterStructs

#include "Arduino.h"

struct __attribute__ ((packed)) canIDStruct_t{
  uint32_t canID : 32;
};

union canID_u{
canIDStruct_t canIDStruct;
uint8_t bytes[sizeof(canIDStruct_t)];
} canID;

struct __attribute__ ((packed)) incommingCANflowStruct_t{
  uint32_t flowMeasure : 32;
  uint32_t volumeCnt : 32;
};
union incommingCANflow{
incommingCANflowStruct_t incommingCANflowStruct;
uint8_t bytes[sizeof(incommingCANflowStruct_t)];
} incommingCANflow;

struct __attribute__ ((packed)) incommingCANstatusStruct_t{
  uint8_t magneticIssue : 1;
  uint8_t rvsd1 : 1;
  uint8_t powerVoltageAlarm : 1;
  uint8_t overtemp : 1;
  uint8_t noValidCalibration : 1;
  uint8_t memoryAccessError : 1;
  uint8_t rvsd2 : 1;
  uint8_t outAstatus : 2;
  uint8_t outBstatus : 2;
  uint8_t rvsd3 : 4;
  uint8_t measureActive : 1;
  uint8_t measurePaused : 1;
  uint8_t flowing : 1;
  uint8_t empty : 1;
  uint8_t rvsd4 : 1;
  uint8_t cleanActive : 1;
  uint8_t virtualRateEnable : 1;
  uint8_t rvsd5 : 1;
  uint8_t unstableMeasure : 1;
  uint8_t flowmeterService : 1;
  uint64_t placeholder : 39;
};

union incommingCANstatus{
incommingCANstatusStruct_t incommingCANstatusStruct;
uint8_t bytes[sizeof(incommingCANstatusStruct_t)];
} incommingCANstatus;

struct __attribute__ ((packed)) flowmeterCANstartupStruct_t{
  uint8_t serialNumberDigit1 : 4;
  uint8_t serialNumberDigit2 : 4;
  uint8_t serialNumberDigit3 : 4;
  uint8_t flowmeterSubtype : 4;
  uint8_t flowmeterType : 4;
  uint8_t placeholder : 4;
};

union flowmeterCANstartup{
flowmeterCANstartupStruct_t flowmeterCANstartupStruct;
uint8_t bytes[sizeof(flowmeterCANstartupStruct_t)];
} flowmeterCANstartup;

#endif