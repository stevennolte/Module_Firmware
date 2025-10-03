/**
 * @file CANBUS.h
 * @brief CAN bus communication interface for agricultural equipment integration
 * 
 * @details This header defines the CANBUS class which provides comprehensive
 *          CAN bus communication functionality for agricultural systems including:
 *          - TWAI (Two-Wire Automotive Interface) driver integration
 *          - Message transmission and reception with filtering
 *          - Agricultural equipment protocol support (ISO 11783, J1939)
 *          - Real-time data exchange with tractors and implements
 *          - Error handling and bus monitoring capabilities
 *          - Configurable bit timing and message filtering
 * 
 * @author Steve Gavel
 * @date 2024
 * @version 4.6.1
 * 
 * @see CANBUS.cpp for implementation details
 * @see ESPdata.h for configuration management
 */

#ifndef CANBUS_H
#define CANBUS_H

#include "Arduino.h"
#include "ESPdata.h"
#include "driver/twai.h"

/**
 * @brief CAN bus communication manager for agricultural equipment integration
 * 
 * @details Provides comprehensive CAN bus functionality using ESP32's TWAI interface
 *          for communication with agricultural equipment including tractors, implements,
 *          and guidance systems. Supports standard agricultural protocols and
 *          custom message handling.
 */
class CANBUS{
    public:
        /**
         * @brief Constructor for CAN bus communication manager
         * 
         * @param vars Pointer to ESPdata singleton for configuration access
         * 
         * @details Initializes CAN bus system with configuration data access
         *          for protocol settings and message filtering.
         */
        CANBUS(ESPdata* vars);
        
        /**
         * @brief Initialize CAN bus communication system
         * 
         * @details Sets up TWAI driver with configured timing parameters,
         *          message filters, and communication modes for agricultural
         *          equipment integration.
         */
        void begin();
        
        /**
         * @brief Send CAN message with specified parameters
         * 
         * @param identifier CAN message identifier (11-bit or 29-bit)
         * @param data Pointer to data payload array
         * @param data_length_code Number of data bytes (0-8)
         * 
         * @details Transmits a CAN message with the specified identifier
         *          and data payload using the TWAI interface.
         */
        void sendCAN(uint32_t identifier, uint8_t data[], uint8_t data_length_code);
        
        /**
         * @brief Receive and process incoming CAN messages
         * 
         * @details Checks for incoming CAN messages and processes them
         *          according to configured filters and protocol handlers.
         */
        void receiveCAN();
        
        /**
         * @brief Handle transmission of CAN message
         * 
         * @param message TWAI message structure to transmit
         * 
         * @details Low-level message transmission handler for TWAI interface
         *          with error checking and status monitoring.
         */
        void handle_tx_message(twai_message_t message);
        
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
        void transmit_normal_message(uint32_t identifier, uint8_t data[], uint8_t data_length_code);
        
    private:
        /// @brief Pointer to ESPdata singleton for configuration access
        ESPdata* espData;
        /// @brief TWAI receive message structure
        twai_message_t rx_message;
        /// @brief TWAI transmit message structure
        twai_message_t tx_message;
        /// @brief TWAI general configuration structure
        twai_general_config_t g_config;
        /// @brief TWAI timing configuration structure
        twai_timing_config_t t_config;
        /// @brief TWAI message filter configuration structure
        twai_filter_config_t f_config;
};

#endif