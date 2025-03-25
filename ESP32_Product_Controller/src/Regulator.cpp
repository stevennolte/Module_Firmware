

// class RegulatorController{
//     private:
//       uint8_t rampSpeed = 0;
//       uint32_t previousMsgSentTime = 0;
//       uint32_t cmdID = 0x18EFB480;
//     public:
//       RegulatorController(){
//         regCommand.regCommandStruct.byte_1=0x22;
//         regCommand.regCommandStruct.dic_index_1=0x07;
//         regCommand.regCommandStruct.dic_index_2=0x20;
//         regCommand.regCommandStruct.sub_index = 0x00;
//         // regCommand.regCommandStruct.movement_direction = 0;
//         // regCommand.regCommandStruct.movement_mode = 1;
//         // regCommand.regCommandStruct.movement_type = 0;
//         regCommand.regCommandStruct.byte_5 = 0x04;
//         regCommand.regCommandStruct.movement_speed = 50;
//         regCommand.regCommandStruct.target = 100;
//         // regCommand.regCommandStruct.placeholder = 4;
//         // regCommand.regCommandStruct.reserved_1 = 0x04;
//         // regCommand.regCommandStruct.reserved_2 = 0;
//       }
//       uint8_t controlState = 0;    // 0=off  1=target position  2=target pressure
//       uint16_t targetPosition = 0;
//       uint16_t targetPositionPress = 0;
//       uint16_t targetPressure = 0;
//       uint16_t regulatorPosition = 0;
//       uint8_t regulatorStatus = 0;  // 0=disconnected  1=connected  2=error
//       uint8_t regulatorSpeed = 0x64;
  
//       void loop(){
//         if(millis()-regLastMsgRecieved>REGULATOR_CAN_TIMEOUT){
//           hbData->regState = 0;
//           regulatorStatus=0;
//         } else if(millis()-regLastMsgRecieved<REGULATOR_CAN_TIMEOUT){
//           regulatorStatus=1;
          
//         }
//         // if(millis()-previousMsgSentTime>REGULATOR_CAN_SEND_TIME && controlState>0){
//         if(millis()-previousMsgSentTime>REGULATOR_CAN_SEND_TIME){
//           previousMsgSentTime=millis();
//           sendCmd();
//         }
//       }
  
//       void sendCmd(){
//         controlState = programCmds.systemState;
//         // switch(controlState){
//         //   case 0:
//         //     regCommand.regCommandStruct.target=100;
//         //   case 1:
//         //     regCommand.regCommandStruct.target=targetPosition;
//         //     break;
//         //   case 2:
//         //     regCommand.regCommandStruct.target=targetPositionPress;
//         //     break;
//         // }
        
//         regCommand.regCommandStruct.byte_1=0x22;
//         regCommand.regCommandStruct.dic_index_1=0x07;
//         regCommand.regCommandStruct.dic_index_2=0x20;
//         regCommand.regCommandStruct.sub_index = 0x00;
//         // regCommand.regCommandStruct.movement_direction = 0;
//         // regCommand.regCommandStruct.movement_mode = 1;
//         // regCommand.regCommandStruct.movement_type = 0;
//         regCommand.regCommandStruct.byte_5 = 0x04;
//         // regCommand.regCommandStruct.movement_speed = 50;
        
//         regCommand.regCommandStruct.movement_speed=regulatorSpeed;
//         uint8_t data_out[8];
//         for(int i=0;i<8;i++){
//           data_out[i]=regCommand.bytes[i];
//         }
//         // uint8_t data_out[]={0x22,0x07,0x20,0x00,0x04,0x3C,0x88,0x13};
//         canHandler.transmit_normal_message(0x18EFB480,data_out,8);
//       }
  
  
//   };
//   RegulatorController regControl = RegulatorController();