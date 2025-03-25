#ifndef CANBUS_H
#define CANBUS_H

#include "Arduino.h"
#include "ESPconfig.h"
#include "driver/twai.h"
#include "Regulator.h"

class CANBUS{
    public:
        CANBUS(ESPconfig* vars);
        uint8_t begin();
        void sendCAN(uint32_t identifier, uint8_t data[], uint8_t data_length_code);
        void receiveCAN();
        void handle_tx_message(twai_message_t message);
        void transmit_normal_message(uint32_t identifier, uint8_t data[], uint8_t data_length_code);
    private:
        ESPconfig* espConfig;
        twai_message_t rx_message;
        twai_message_t tx_message;
        twai_general_config_t g_config;
        twai_timing_config_t t_config;
        twai_filter_config_t f_config;
        
        static void taskHandler(void *param);  // Task handler
        void continuousLoop();  // Function to run in the background task
        
};

#endif