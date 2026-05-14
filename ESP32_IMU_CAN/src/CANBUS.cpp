#include "CANBUS.h"
#include "esp_efuse.h"
#include "esp_efuse_table.h"

CANBUS::CANBUS(ESPconfig* vars) {
    espConfig = vars;
}

// ─── Low-level TX ─────────────────────────────────────────────────────────────

void CANBUS::handle_tx(twai_message_t& message) {
    esp_err_t result = twai_transmit(&message, pdMS_TO_TICKS(100));
    if (result != ESP_OK) {
        //Serial.printf("CAN TX failed: %s\n", esp_err_to_name(result));
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

// ─── J1939 NAME ───────────────────────────────────────────────────────────────
// The 64-bit J1939 NAME is packed into 8 data bytes (little-endian, bit 0 = LSB).
//
//  bits 20..0   Identity Number      (21 bits)
//  bits 31..21  Manufacturer Code    (11 bits)
//  bits 34..32  ECU Instance         ( 3 bits)
//  bits 39..35  Function Instance    ( 5 bits)
//  bits 47..40  Function             ( 8 bits)
//  bit  48      Reserved (0)
//  bits 55..49  Vehicle System       ( 7 bits)
//  bits 59..56  Vehicle System Inst  ( 4 bits)
//  bits 62..60  Industry Group       ( 3 bits)  2 = Agriculture
//  bit  63      Arbitrary Addr Cap   ( 1 bit )

void CANBUS::buildNAME(uint8_t buf[8]) {
    // Seed identity number from lower 21 bits of MAC if not yet set
    if (espConfig->canCfg.identityNumber == 0) {
        uint8_t mac[6];
        esp_efuse_read_field_blob(ESP_EFUSE_MAC_FACTORY, mac, 48);
        uint32_t id = ((uint32_t)mac[3] << 16) | ((uint32_t)mac[4] << 8) | mac[5];
        espConfig->canCfg.identityNumber = id & 0x1FFFFF;
    }

    uint32_t ident = espConfig->canCfg.identityNumber & 0x1FFFFFu;
    uint16_t mfr   = espConfig->canCfg.manufacturerCode & 0x7FFu;
    uint8_t  ecuI  = espConfig->canCfg.ecuInstance      & 0x07u;
    uint8_t  fnI   = espConfig->canCfg.functionInstance & 0x1Fu;
    uint8_t  fn    = espConfig->canCfg.function;
    uint8_t  vs    = espConfig->canCfg.vehicleSystem    & 0x7Fu;
    uint8_t  ig    = espConfig->canCfg.industryGroup    & 0x07u;
    uint8_t  aac   = 1u; // always capable of arbitrary addressing

    buf[0] =  (uint8_t)(ident & 0xFF);
    buf[1] =  (uint8_t)((ident >> 8) & 0xFF);
    buf[2] =  (uint8_t)(((ident >> 16) & 0x1F) | ((mfr & 0x07u) << 5));
    buf[3] =  (uint8_t)((mfr >> 3) & 0xFF);
    buf[4] =  (uint8_t)(ecuI | (fnI << 3));
    buf[5] =  fn;
    buf[6] =  (uint8_t)(vs << 1);          // bit 0 = reserved
    buf[7] =  (uint8_t)((ig << 4) | (aac << 7));
}

// ─── J1939 Address Claiming ───────────────────────────────────────────────────

void CANBUS::sendAddressClaimed(uint8_t sa) {
    uint8_t name[8];
    buildNAME(name);
    uint32_t id = J1939_ADDR_CLAIMED_ID(sa);
    transmit(id, name, 8, true);
    Serial.printf("J1939: ADDRESS_CLAIMED sent, SA=0x%02X, CAN_ID=0x%08X\n", sa, id);
}

void CANBUS::sendCannotClaimAddress() {
    uint8_t name[8];
    buildNAME(name);
    // CANNOT_CLAIM uses SA = 0xFE (null address)
    uint32_t id = J1939_ADDR_CLAIMED_ID(0xFE);
    transmit(id, name, 8, true);
    Serial.println("J1939: CANNOT_CLAIM_ADDRESS sent");
}

// Attempt to claim a J1939 address.
// Per SAE J1939-81:
//   1. Send ADDRESS_CLAIMED with desired SA.
//   2. Wait 250 ms for any contender.
//   3. If another device sends ADDRESS_CLAIMED for same SA with lower (higher-priority) NAME,
//      increment SA and repeat.
//   4. After successful 250-ms quiet period, address is owned.
bool CANBUS::claimAddress() {
    uint8_t name[8];
    buildNAME(name);
    // Compare NAMEs as 64-bit little-endian values; lower value = higher J1939 priority
    uint64_t ourName = 0;
    for (int i = 7; i >= 0; i--) ourName = (ourName << 8) | name[7 - i];

    uint8_t sa = espConfig->canCfg.j1939SA;

    for (int attempt = 0; attempt < 10; attempt++) {
        sendAddressClaimed(sa);

        uint32_t deadline = millis() + 250;
        bool conflict = false;
        while (millis() < deadline) {
            twai_message_t msg;
            if (twai_receive(&msg, pdMS_TO_TICKS(10)) == ESP_OK) {
                if (!(msg.flags & TWAI_MSG_FLAG_EXTD)) continue;
                uint8_t rxSA  = (uint8_t)(msg.identifier & 0xFF);
                uint8_t rxPF  = (uint8_t)((msg.identifier >> 16) & 0xFF);
                // ADDRESS_CLAIMED: PF = 0xEE
                if (rxPF == 0xEE && rxSA == sa && msg.data_length_code == 8) {
                    // Another device claims the same SA
                    uint64_t theirName = 0;
                    for (int i = 7; i >= 0; i--) theirName = (theirName << 8) | msg.data[7 - i];
                    if (theirName < ourName) {
                        // They have higher priority; we must yield
                        Serial.printf("J1939: SA 0x%02X conflict, yielding\n", sa);
                        conflict = true;
                        break;
                    } else if (theirName == ourName) {
                        // Same NAME – should not happen (identity numbers must differ)
                        conflict = true;
                        break;
                    }
                    // Our NAME has higher priority; re-assert our claim
                    sendAddressClaimed(sa);
                    deadline = millis() + 250;
                }
            }
        }

        if (!conflict) {
            espConfig->canCfg.j1939SA       = sa;
            espConfig->canCfg.addressClaimed = true;
            Serial.printf("J1939: Address claimed! SA=0x%02X\n", sa);
            return true;
        }
        // Try next address (skip reserved range 0xFD-0xFF)
        sa++;
        if (sa >= 0xFD) {
            sendCannotClaimAddress();
            espConfig->canCfg.addressClaimed = false;
            return false;
        }
    }
    sendCannotClaimAddress();
    espConfig->canCfg.addressClaimed = false;
    return false;
}

// ─── CAN receive task ─────────────────────────────────────────────────────────
// Runs forever after address is claimed.
// Handles:
//   – ADDRESS_CLAIMED conflicts from other nodes
//   – REQUEST for ADDRESS_CLAIMED (PGN 0xEA00 with group-extension 0xEE)

void CANBUS::rxTaskHandler(void *param) {
    CANBUS* self = static_cast<CANBUS*>(param);
    self->rxLoop();
}

void CANBUS::rxLoop() {
    uint8_t myName[8];
    buildNAME(myName);
    uint64_t ourName = 0;
    for (int i = 7; i >= 0; i--) ourName = (ourName << 8) | myName[7 - i];

    while (true) {
        twai_message_t msg;
        if (twai_receive(&msg, pdMS_TO_TICKS(50)) == ESP_OK) {
            if (!(msg.flags & TWAI_MSG_FLAG_EXTD)) continue;

            uint8_t rxSA  = (uint8_t)(msg.identifier & 0xFF);
            uint8_t rxPF  = (uint8_t)((msg.identifier >> 16) & 0xFF);
            uint8_t rxPS  = (uint8_t)((msg.identifier >> 8) & 0xFF);

            // ADDRESS_CLAIMED conflict check
            if (rxPF == 0xEE && rxSA == espConfig->canCfg.j1939SA && msg.data_length_code == 8) {
                uint64_t theirName = 0;
                for (int i = 7; i >= 0; i--) theirName = (theirName << 8) | msg.data[7 - i];
                if (theirName < ourName) {
                    // We must yield; attempt to claim the next available address
                    Serial.printf("J1939: Post-claim conflict on SA 0x%02X, re-claiming\n",
                                  espConfig->canCfg.j1939SA);
                    espConfig->canCfg.addressClaimed = false;
                    claimAddress();
                } else {
                    // We win; re-assert
                    sendAddressClaimed(espConfig->canCfg.j1939SA);
                }
            }

            // REQUEST for ADDRESS_CLAIMED (PGN 0xEA00, data = {0x00, 0xEE, 0x00} = PGN 0xEE00 LE)
            // PF = 0xEA, destination = our SA or 0xFF (global), data = {0x00, 0xEE, 0x00}
            if (rxPF == 0xEA &&
                (rxPS == espConfig->canCfg.j1939SA || rxPS == 0xFF) &&
                msg.data_length_code == 3 &&
                msg.data[0] == 0x00 && msg.data[1] == 0xEE && msg.data[2] == 0x00) {
                // Someone is requesting Address Claimed; respond
                sendAddressClaimed(espConfig->canCfg.j1939SA);
            }
        }
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}

// ─── Initialise ───────────────────────────────────────────────────────────────

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

    // J1939 address claiming
    if (!claimAddress()) {
        Serial.println("J1939: Address claiming failed");
        espConfig->progData.canState = 2;
        return false;
    }

    espConfig->progData.canState = 1;

    // Start background receive / conflict-monitor task
    xTaskCreate(rxTaskHandler, "CANrx", 4096, this, 3, NULL);

    return true;
}

// ─── IMU data TX ─────────────────────────────────────────────────────────────
// J1939 Proprietary B broadcast, PGN 0xFF04
// CAN 29-bit ID: priority=6, PF=0xFF, GE=0x04, SA=our address
//
// Data layout (8 bytes, little-endian int16, degrees × 100):
//   [0:1]  Roll          (int16)  e.g. +45.00° → 4500
//   [2:3]  Pitch         (int16)  e.g. -10.50° → -1050
//   [4:5]  Heading True  (int16)  0–35999  (0.00°–359.99°)
//   [6]    Sensor accuracy (0–3 from BNO08x)
//   [7]    Status flags  bit0=IMU_OK  bit1=HEADING_VALID

void CANBUS::sendIMUData(float roll, float pitch, float headingTrue, uint8_t accuracy) {
    if (!espConfig->canCfg.addressClaimed) return;

    int16_t rollI    = (int16_t)(roll  * 100.0f);
    int16_t pitchI   = (int16_t)(pitch * 100.0f);
    // Normalise heading to 0–359.99°
    while (headingTrue < 0.0f)   headingTrue += 360.0f;
    while (headingTrue >= 360.0f) headingTrue -= 360.0f;
    int16_t headingI = (int16_t)(headingTrue * 100.0f);

    uint8_t status = 0;
    if (espConfig->progData.imuState == 1) status |= 0x01; // IMU_OK
    if (accuracy >= 2)                     status |= 0x02; // HEADING_VALID (accuracy ≥ 2 = medium calibrated)

    uint8_t data[8];
    data[0] = (uint8_t)(rollI    & 0xFF);
    data[1] = (uint8_t)((rollI    >> 8) & 0xFF);
    data[2] = (uint8_t)(pitchI   & 0xFF);
    data[3] = (uint8_t)((pitchI   >> 8) & 0xFF);
    data[4] = (uint8_t)(headingI & 0xFF);
    data[5] = (uint8_t)((headingI >> 8) & 0xFF);
    data[6] = accuracy;
    data[7] = status;

    uint32_t id = J1939_IMU_DATA_ID(espConfig->canCfg.j1939SA);
    transmit(id, data, 8, true);
}
