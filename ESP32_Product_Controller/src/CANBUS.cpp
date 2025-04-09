#include "CANBUS.h"


CANBUS::CANBUS(ESPconfig* vars) {
    espConfig = vars;
}

void CANBUS::taskHandler(void *param) {
    // Cast the param back to the ClassA object
    CANBUS *instance = static_cast<CANBUS *>(param);
    instance->continuousLoop();  // Call the member function
}

void CANBUS::continuousLoop() {
    while (true) {
        
        receiveCAN();
        vTaskDelay(10/portTICK_PERIOD_MS);
    }
}

void CANBUS::handle_tx_message(twai_message_t message)
    {
      esp_err_t result = twai_transmit(&message, pdMS_TO_TICKS(100));
      if (result == ESP_OK){
      }
      else {
        Serial.printf("\n%s: Failed to queue the message for transmission.\n", esp_err_to_name(result));
      }
    }

void CANBUS::sendCAN(uint32_t identifier, uint8_t data[], uint8_t data_length_code = TWAI_FRAME_MAX_DLC)
    {
    // configure message to transmit
    twai_message_t message = {
        .flags = TWAI_MSG_FLAG_EXTD,
        .identifier = identifier,
        .data_length_code = data_length_code,
    };
    memcpy(message.data, data, data_length_code);

    //Transmit messages using self reception request
    handle_tx_message(message);
    }
    
uint8_t CANBUS::begin(){
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)espConfig->gpioDefs.CAN_TX, (gpio_num_t)espConfig->gpioDefs.CAN_RX, TWAI_MODE_NO_ACK);  // TWAI_MODE_NORMAL, TWAI_MODE_NO_ACK or TWAI_MODE_LISTEN_ONLY
      twai_timing_config_t t_config  = TWAI_TIMING_CONFIG_250KBITS();
      twai_filter_config_t f_config  = TWAI_FILTER_CONFIG_ACCEPT_ALL();
      twai_driver_install(&g_config, &t_config, &f_config);
      
      if (twai_start() == ESP_OK) {
        printf("Driver started\n");
    } else {
        printf("Failed to start driver\n");
        return 2;
    }
      
      twai_status_info_t status;
      twai_get_status_info(&status);
      Serial.print("TWAI state ");
      Serial.println(status.state);
     
      while (espConfig->regData.regID.regID_Struct.id == 0){    
          uint8_t ack[] = {1,255,0,0,0,0,0,0};
          sendCAN(0x18EC0001,ack,8);
          receiveCAN();
          delay(50);
          if (millis() > 15000){
            Serial.println("No response from the valve, please check the wiring and power supply.");
            return 2;
          }
       }
      delay(1000);
      sendRegCmd(100, 80);
      xTaskCreate(
        taskHandler,   // Task function
        "TaskA",       // Name of the task
        4096,          // Stack size (in words)
        this,          // Pass the current instance as the task parameter
        1,             // Priority of the task
        NULL           // Task handle (not needed)
      );
      return 1;
      // transmit_normal_message(0x06FF3A01, ack);

}

void CANBUS::sendRegCmd(uint16_t _target, uint8_t _speed) {
  espConfig->regData.regCommand.regCommandStruct.byte_1 = 0x22;
  espConfig->regData.regCommand.regCommandStruct.dic_index_1 = 0x07;
  espConfig->regData.regCommand.regCommandStruct.dic_index_2 = 0x20;
  espConfig->regData.regCommand.regCommandStruct.sub_index = 0x00;
  espConfig->regData.regCommand.regCommandStruct.byte_5 = 0x04;
  espConfig->regData.regCommand.regCommandStruct.movement_speed = _speed;
  espConfig->regData.regCommand.regCommandStruct.target = _target;
  sendCAN(espConfig->regData.cmdID, espConfig->regData.regCommand.bytes, 8);
}

void CANBUS::receiveCAN()
{
    twai_message_t message;
    if (twai_receive(&message, pdMS_TO_TICKS(10)) == ESP_OK) {
      espConfig->regData.regID.regID_Struct.id = message.identifier;
      if (espConfig->regData.regID.bytes[1]==56 & espConfig->regData.regID.bytes[2] == 255){
        
        for (int i = 0; i<sizeof(espConfig->regData.regReport);i++){
          espConfig->regData.regReport.bytes[i]=message.data[i];
        }
        espConfig->regData.lastMsgRecieved = millis();
      }
    }

}

    

    