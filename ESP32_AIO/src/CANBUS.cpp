/**
 * @file CANBUS.cpp
 * @brief Implementation of CAN bus communication for agricultural equipment integration
 * 
 * @details This file implements the CANBUS class functionality for managing
 *          CAN bus communication using ESP32's TWAI interface. Provides
 *          message transmission, reception, and protocol handling for
 *          agricultural equipment integration.
 * 
 * @author Steve Gavel
 * @date 2024
 * @version 4.6.1
 * 
 * @see CANBUS.h for class interface definition
 * @see ESPdata.h for configuration management
 */

#include "CANBUS.h"

/**
 * @brief Constructor for CAN bus communication manager
 * 
 * @param vars Pointer to ESPdata singleton for configuration access
 * 
 * @details Initializes CAN bus system with configuration data access
 *          for protocol settings and communication parameters.
 */
CANBUS::CANBUS(ESPdata* vars) {
    espData = vars;
}

/**
 * @brief Handle transmission of a CAN message via TWAI interface
 * 
 * @param message TWAI message structure containing data to transmit
 * 
 * @details Queues message for transmission using TWAI driver with
 *          timeout protection and error handling for failed transmissions.
 */
void CANBUS::handle_tx_message(twai_message_t message)
{
      esp_err_t result = twai_transmit(&message, pdMS_TO_TICKS(100));
      if (result == ESP_OK){
      }
      else {
        Serial.printf("\n%s: Failed to queue the message for transmission.\n", esp_err_to_name(result));
      }
}

/**
 * @brief Send CAN message with extended identifier format
 * 
 * @param identifier 29-bit extended CAN identifier
 * @param data Pointer to data payload array (up to 8 bytes)
 * @param data_length_code Number of data bytes to send (default: 8)
 * 
 * @details Constructs and transmits a CAN message using extended frame
 *          format suitable for agricultural protocols like J1939 and ISO 11783.
 */
void CANBUS::sendCAN(uint32_t identifier, uint8_t data[], uint8_t data_length_code)
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
    
/**
 * @brief Initialize CAN bus communication system
 * 
 * @details Sets up TWAI driver with 250kbps timing, configures GPIO pins,
 *          and starts the CAN bus interface. Sends initial acknowledgment
 *          messages for system handshake with connected equipment.
 */
void CANBUS::begin(){
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)espData->can.txPin, (gpio_num_t)espData->can.rxPin, TWAI_MODE_NO_ACK);  // TWAI_MODE_NORMAL, TWAI_MODE_NO_ACK or TWAI_MODE_LISTEN_ONLY
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

/**
 * @brief Receive and process incoming CAN messages
 * 
 * @details Checks for incoming CAN messages with a 10ms timeout and
 *          processes them according to configured message filters.
 *          Non-blocking operation suitable for continuous monitoring.
 */
void CANBUS::receiveCAN()
{
    twai_message_t message;
    if (twai_receive(&message, pdMS_TO_TICKS(10)) == ESP_OK) {
    }

}

/**
 * @brief Transmit normal (non-extended) CAN message
 * 
 * @param identifier Standard 11-bit CAN identifier
 * @param data Pointer to data payload array
 * @param data_length_code Number of data bytes (0-8)
 * 
 * @details Transmits a standard format CAN message with 11-bit
 *          identifier for basic agricultural equipment communication.
 */
void CANBUS::transmit_normal_message(uint32_t identifier, uint8_t data[], uint8_t data_length_code)
{
    // Configure message to transmit with standard format
    twai_message_t message = {
        .flags = 0,  // Standard frame (no extended flag)
        .identifier = identifier,
        .data_length_code = data_length_code,
    };

    memcpy(message.data, data, data_length_code);

    // Transmit the message
    handle_tx_message(message);
}