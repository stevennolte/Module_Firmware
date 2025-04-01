#ifndef IncomingUDPstructs
#define IncomingUDPstructs

#include "Arduino.h"

struct __attribute__ ((packed)) rowStatesStruct_t{
  uint8_t row1:1;
  uint8_t row2:1;
  uint8_t row3:1;
  uint8_t row4:1;
  uint8_t row5:1;
  uint8_t row6:1;
  uint8_t row7:1;
  uint8_t row8:1;
};

union rowStates_u{
  rowStatesStruct_t rowStatsStruct;
  uint8_t bytes[1];
} rowStates;


#endif