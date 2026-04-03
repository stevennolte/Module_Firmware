/**
 * @file Version.h
 * @brief Version information and program identification constants
 * 
 * @details This header defines the program version and identification constants
 *          used throughout the ESP32-AIO agricultural controller system.
 *          
 *          Version format: MAJOR.MINOR.PATCH
 *          - MAJOR: Significant architecture changes or major feature additions
 *          - MINOR: Feature additions and improvements
 *          - PATCH: Bug fixes and minor updates
 * 
 * @author Steve Gavel
 * @date 2024
 * @version 1.1.005
 * 
 * @see main.cpp for version display
 * @see ESPdata.h for version storage
 */

#ifndef Version_h
#define Version_h

#include "Arduino.h"

/// @brief Current firmware version string (MAJOR.MINOR.PATCH format)
#define VERSION "1.1.0049"

/// @brief Program name identifier used for network identification and display
#define NAME "ESP32_AIO"

#endif