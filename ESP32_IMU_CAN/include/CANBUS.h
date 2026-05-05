#ifndef CANBUS_H
#define CANBUS_H

#include "Arduino.h"
#include "ESPconfig.h"
#include "driver/twai.h"

// ── J1939 PGN constants ──────────────────────────────────────────────────────
// PGN 60928 (0xEE00) – Address Claimed / Cannot Claim Address
#define J1939_PGN_ADDRESS_CLAIMED   0xEE00u
// PGN 59392 (0xEA00) – Request PGN
#define J1939_PGN_REQUEST           0xEA00u
// Proprietary B broadcast PGN for custom IMU data (0xFF04)
#define J1939_PGN_IMU_DATA          0xFF04u

// Build a 29-bit J1939 CAN ID (priority 6, PDU format / PDU specific / SA)
// For Proprietary B (PF >= 0xF0): PDU_specific = Group Extension (part of PGN)
// For Address Claimed (PF = 0xEE < 0xF0): PDU_specific = destination address
#define J1939_ID(pf, ps, sa)  ((0x18000000u) | ((uint32_t)(pf) << 16) | ((uint32_t)(ps) << 8) | (uint8_t)(sa))

// Broadcast CAN ID for ADDRESS_CLAIMED (destination = 0xFF = global)
#define J1939_ADDR_CLAIMED_ID(sa)   J1939_ID(0xEE, 0xFF, sa)
// Broadcast CAN ID for IMU data (Proprietary B, GE = 0x04)
#define J1939_IMU_DATA_ID(sa)       J1939_ID(0xFF, 0x04, sa)

class CANBUS {
    public:
        CANBUS(ESPconfig* vars);

        // Initialise TWAI driver and claim a J1939 address (blocking, max ~1 s)
        bool begin();

        // Send the 8-byte IMU data frame using the claimed SA
        void sendIMUData(float roll, float pitch, float headingTrue, uint8_t accuracy);

        // Low-level helpers
        void transmit(uint32_t identifier, uint8_t data[], uint8_t dlc, bool extended = true);

    private:
        ESPconfig* espConfig;

        // Build the 8-byte J1939 NAME into buf[]
        void buildNAME(uint8_t buf[8]);

        // Send ADDRESS_CLAIMED or CANNOT_CLAIM_ADDRESS
        void sendAddressClaimed(uint8_t sa);
        void sendCannotClaimAddress();

        // J1939 address claiming procedure (runs once in begin())
        bool claimAddress();

        // FreeRTOS receive task – monitors for address-claim conflicts
        static void rxTaskHandler(void *param);
        void rxLoop();

        void handle_tx(twai_message_t& message);
};

#endif
