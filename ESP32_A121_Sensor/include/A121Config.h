#ifndef A121_CONFIG_H
#define A121_CONFIG_H

// Default I2C configuration
#define A121_DEFAULT_SDA_PIN 8
#define A121_DEFAULT_SCL_PIN 9
#define A121_DEFAULT_I2C_SPEED 400000  // 400 kHz

// Measurement configuration
#define A121_DEFAULT_MEASUREMENT_INTERVAL_MS 1000
#define A121_MEASUREMENT_TIMEOUT_MS 5000

// Configuration presets
#define A121_CONFIG_SHORT_RANGE   0x00  // Optimized for 0-50cm
#define A121_CONFIG_MEDIUM_RANGE  0x01  // Optimized for 0-150cm
#define A121_CONFIG_LONG_RANGE    0x02  // Optimized for 0-500cm
#define A121_CONFIG_LOW_POWER     0x03  // Low power consumption mode
#define A121_CONFIG_HIGH_ACCURACY 0x04  // High accuracy mode

// Sensor limits and ranges
#define A121_MIN_DISTANCE_MM 0
#define A121_MAX_DISTANCE_MM 5000
#define A121_INVALID_DISTANCE 0xFFFF

// Error codes
#define A121_ERROR_NONE           0x00
#define A121_ERROR_I2C_COMM       0x01
#define A121_ERROR_NOT_READY      0x02
#define A121_ERROR_TIMEOUT        0x03
#define A121_ERROR_INVALID_CONFIG 0x04
#define A121_ERROR_SENSOR_FAULT   0x80

#endif // A121_CONFIG_H
