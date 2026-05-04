#ifndef CANBUS_H
#define CANBUS_H

#include "Arduino.h"
#include "ESPconfig.h"
#include "driver/twai.h"

class CANBUS {
    public:
        CANBUS(ESPconfig* vars);
        bool begin();
        void sendIMUData(float roll, float pitch, float yaw, uint8_t accuracy);
        void transmit(uint32_t identifier, uint8_t data[], uint8_t dlc, bool extended);

    private:
        ESPconfig* espConfig;
        void handle_tx(twai_message_t& message);
};

#endif
