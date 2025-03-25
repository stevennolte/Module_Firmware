// #ifndef Regulator_h
// #define Regulator_h

// #include "Arduino.h"

// uint32_t regLastMsgRecieved = 0;

// struct __attribute__ ((packed)) regCommand_Struct_t{
//     uint64_t byte_1 : 8;
//     uint64_t dic_index_1 : 8;
//     uint64_t dic_index_2 : 8;
//     uint64_t sub_index : 8;
//     uint64_t byte_5 : 8;
     
//     // uint64_t movement_direction : 2;
//     // uint64_t movement_mode : 1;
//     // uint64_t movement_type : 1;
//     // uint64_t reserved_1 : 4;
//     // uint64_t placeholder : 8;
//     uint64_t movement_speed : 8;
    
//     uint64_t target : 16;
// };

// union regCommand_u{
// regCommand_Struct_t regCommandStruct;
// uint8_t bytes[sizeof(regCommand_Struct_t)];
// } regCommand;


// struct __attribute__ ((packed)) regReport_Struct_t{
//     uint64_t position : 16;
//     uint8_t valve_status : 8;
//     uint8_t valve_alarms_1 : 8;
//     uint8_t valve_alarms_2 : 8;
// };

// union regReport_u{
//     regReport_Struct_t regReport_Struct;
//     uint8_t bytes[sizeof(regReport_Struct_t)];
// } regReport;

// struct __attribute__ ((packed)) regID_Struct_t{
//     uint32_t id : 32;
// };

// union regID_u{
//     regID_Struct_t regID_Struct;
//     uint8_t bytes[sizeof(regID_Struct_t)];
// } regID;

// #endif


