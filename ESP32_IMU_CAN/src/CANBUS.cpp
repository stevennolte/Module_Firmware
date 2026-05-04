#include "CANBUS.h"

CANBUS::CANBUS(ESPconfig* vars) {
    espConfig = vars;
}

bool CANBUS::begin() {
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(
        (gpio_num_t)espConfig->gpioDefs.CAN_TX,
        (gpio_num_t)espConfig->gpioDefs.CAN_RX,
        TWAI_MODE_NORMAL);
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    esp_err_t install_res = twai_driver_install(&g_config, &t_config, &f_config);
    if (install_res != ESP_OK) {
        Serial.printf("TWAI driver install failed: %s\n", esp_err_to_name(install_res));
        espConfig->progData.canState = 2;
        return false;
    }

    if (twai_start() != ESP_OK) {
        Serial.println("TWAI start failed");
        espConfig->progData.canState = 2;
        return false;
    }

    twai_status_info_t status;
    twai_get_status_info(&status);
    Serial.printf("TWAI started, state=%d\n", status.state);
    espConfig->progData.canState = 1;
    return true;
}

void CANBUS::handle_tx(twai_message_t& message) {
    esp_err_t result = twai_transmit(&message, pdMS_TO_TICKS(100));
    if (result != ESP_OK) {
        Serial.printf("CAN TX failed: %s\n", esp_err_to_name(result));
    }
}

void CANBUS::transmit(uint32_t identifier, uint8_t data[], uint8_t dlc, bool extended) {
    twai_message_t message = {};
    message.identifier       = identifier;
    message.data_length_code = dlc;
    message.flags            = extended ? TWAI_MSG_FLAG_EXTD : 0;
    memcpy(message.data, data, dlc);
    handle_tx(message);
}

void CANBUS::sendIMUData(float roll, float pitch, float yaw, uint8_t accuracy) {
    // Pack roll, pitch, yaw as int16 (degrees x 10) into an 8-byte CAN frame.
    // Byte layout:
    //   [0:1] roll  int16 little-endian (deg * 10)
    //   [2:3] pitch int16 little-endian (deg * 10)
    //   [4:5] yaw   int16 little-endian (deg * 10)
    //   [6]   accuracy (0-3)
    //   [7]   simple XOR checksum of bytes 0-6
    int16_t rollI  = (int16_t)(roll  * 10.0f);
    int16_t pitchI = (int16_t)(pitch * 10.0f);
    int16_t yawI   = (int16_t)(yaw   * 10.0f);

    uint8_t data[8];
    data[0] = (uint8_t)(rollI  & 0xFF);
    data[1] = (uint8_t)((rollI  >> 8) & 0xFF);
    data[2] = (uint8_t)(pitchI & 0xFF);
    data[3] = (uint8_t)((pitchI >> 8) & 0xFF);
    data[4] = (uint8_t)(yawI   & 0xFF);
    data[5] = (uint8_t)((yawI   >> 8) & 0xFF);
    data[6] = accuracy;
    data[7] = data[0] ^ data[1] ^ data[2] ^ data[3] ^ data[4] ^ data[5] ^ data[6];

    transmit(espConfig->canCfg.txID, data, 8, espConfig->canCfg.extFrame);
}
