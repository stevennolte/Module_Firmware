#include "CANBUS.h"


CANBUS::CANBUS(ESPconfig* vars) {
    espConfig = vars;
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
    
void CANBUS::begin(){
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)espConfig->canCfg.txPin, (gpio_num_t)espConfig->canCfg.rxPin, TWAI_MODE_NO_ACK);  // TWAI_MODE_NORMAL, TWAI_MODE_NO_ACK or TWAI_MODE_LISTEN_ONLY
      twai_timing_config_t t_config  = TWAI_TIMING_CONFIG_250KBITS();
      twai_filter_config_t f_config  = TWAI_FILTER_CONFIG_ACCEPT_ALL();
      twai_driver_install(&g_config, &t_config, &f_config);
      
      if (twai_start() == ESP_OK) {
        printf("Driver started\n");
    } else {
        printf("Failed to start driver\n");
        return;
    }
      
      twai_status_info_t status;
      twai_get_status_info(&status);
      Serial.print("TWAI state ");
      Serial.println(status.state);
      uint8_t ack[] = {1,255,0,0,0,0,0,0};
      for (int i=0; i<25;i++){
        sendCAN(0x18EC0001,ack,8);
        delay(100);
      }
      // transmit_normal_message(0x06FF3A01, ack);

}



    void CANBUS::receiveCAN()
    {
        twai_message_t message;
        if (twai_receive(&message, pdMS_TO_TICKS(10)) == ESP_OK) {
        }
  
    }

    