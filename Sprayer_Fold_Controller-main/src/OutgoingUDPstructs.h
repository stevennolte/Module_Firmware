#ifndef OutgoingUDPstructs
#define OutgoingUDPstructs
#include "Arduino.h"


struct __attribute__ ((packed)) heartbeatStruct_t{
  uint8_t aogByte1 : 8;
  uint8_t aogByte2 : 8;
  uint8_t aogByte3 : 8;
  uint8_t aogHeartBeatPGN : 8;
  uint8_t length : 8;
  uint8_t machineID : 8;
  uint8_t udpState : 8;
  uint8_t checksum : 8;
};

union heartbeat{
heartbeatStruct_t heartbeatStruct;
uint8_t bytes[sizeof(heartbeatStruct_t)];
} heartbeat;


struct __attribute__ ((packed)) rowDataStruct_t{
  uint8_t aogByte0 : 8;
  uint8_t aogByte1 : 8;
  uint8_t src : 8;
  uint8_t aogFlowDataPGN : 8;
  uint8_t length : 8;
  uint8_t sectionNum : 8;
  uint16_t freqFlowRate : 16;
  uint16_t canFlowRate : 16;
  uint16_t avgCurrentDraw : 16;
  uint16_t maxCurrentDraw : 16;
  uint16_t Voltage : 16;
};

union rowData_u{
rowDataStruct_t rowDataStruct;
uint8_t bytes[sizeof(rowDataStruct_t)];
} rowData;


struct __attribute__ ((packed)) flowDataStruct_t{
  uint8_t aogByte0 : 8;
  uint8_t aogByte1 : 8;
  uint8_t src : 8;
  uint8_t aogFlowDataPGN : 8;
  uint8_t length : 8;
  uint8_t section0Num : 8;
  uint8_t section1Num : 8;
  uint8_t section2Num : 8;
  uint8_t section3Num : 8;
  uint16_t section0flow : 16;
  uint16_t section1flow : 16;
  uint16_t section2flow : 16;
  uint16_t section3flow : 16;
  uint16_t voltage1 : 16;
  uint16_t voltage2 : 16;
  uint16_t voltage3 : 16;
  uint16_t voltage4 : 16;
  uint8_t checksum : 8;
};

union flowData_u{
flowDataStruct_t flowDataStruct;
uint8_t bytes[sizeof(flowDataStruct_t)];
} flowData;


#endif