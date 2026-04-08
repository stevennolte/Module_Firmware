/**
 * @file Version.h
 * @brief Version information and program identification constants
 *
 * @details Version format: MAJOR.MINOR.PATCH
 *          - MAJOR: Significant architecture changes or major feature additions
 *          - MINOR: Feature additions and improvements
 *          - PATCH: Bug fixes and minor updates (auto-incremented by build script)
 */

#ifndef Version_h
#define Version_h

#include "Arduino.h"

/// @brief Current firmware version string (MAJOR.MINOR.PATCH format)
#define VERSION "1.0.0098"

/// @brief Program name identifier used for network identification and display
#define NAME "ESP32_GPS"

#endif
