/*
 *****************************************************************************
 * Copyright (C) 2019 Infineon Technologies AG. All rights reserved.
 *
 * Infineon Technologies AG (INFINEON) is supplying this file for use
 * exclusively with Infineon's products. This file can be freely
 * distributed within development tools and software supporting such microcontroller
 * products.
 *
 * THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 * OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 * INFINEON SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR DIRECT, INDIRECT, INCIDENTAL,
 * ASPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 *
 ******************************************************************************
 */
 
 
 
 #ifndef _TLV493D__H_
 #define _TLV493D__H_
 
 #include <stdbool.h>
 #include <stdint.h>
 
 typedef struct {
     int16_t x;
     int16_t y;
     int16_t z;
     int16_t temp;
 } TLx493D_data_frame_t;
 
 enum {
     TLx493D_OK = 0,
     TLx493D_INVALID_ARGUMENT = -1,
     TLx493D_INVALID_FRAME = -2,
     TLx493D_NOT_IMPLEMENETED = -3,
     TLx493D_INVALID_SENSOR_STATE = -4,
     TLx493D_WU_ENABLE_FAIL  = -5,
 };
 
 
 typedef enum {
     TLx493D_TYPE_UNKNOWN,
     TLx493D_TYPE_TLV_A1B6,
     TLx493D_TYPE_TLE_A2B6,
     TLx493D_TYPE_TLE_W2B6,
     TLx493D_TYPE_TLI_W2BW
 } TLV493D_sensor_type_t;
 
 typedef enum {
     TLx493D_OP_MODE_NOT_INITIALIZED,
     TLx493D_OP_MODE_POWER_DOWN,
     TLx493D_OP_MODE_MCM,
     TLx493D_OP_MODE_FAST,
     TLx493D_OP_MODE_LOW_POWER,
     TLx493D_OP_MODE_ULTRA_LOW_POWER,
 } TLV493D_op_mode_t;
 
 
 
 int32_t TLx493D_init(void);
 
 TLV493D_sensor_type_t TLx493D_get_sensor_type(void);
 
 int32_t TLx493D_set_operation_mode(TLV493D_op_mode_t mode);
 
 TLV493D_op_mode_t TLx493D_get_operation_mode();
 
 int32_t TLx493D_read_frame(TLx493D_data_frame_t *frame);
 
 
 uint8_t MISC_get_parity(uint8_t data);
 
 #endif //_TLV493D__H_
