/**
 * @file myLED.h
 * @brief Smart LED status indicator system for system health monitoring
 * 
 * @details This header defines the MyLED class which provides comprehensive
 *          status indication using a NeoPixel RGB LED including:
 *          - Color-coded system status indication for error identification
 *          - Multi-threaded LED animation processing
 *          - Configurable brightness control
 *          - Special animation modes for testing and demonstration
 *          - Real-time system health monitoring and visual feedback
 * 
 * @author Steve Gavel
 * @date 2024
 * @version 4.6.1
 * 
 * @see myLED.cpp for implementation details
 * @see ESPdata.h for configuration management
 */

#ifndef myLED_h
#define myLED_h

#include "ESPdata.h"
#include <Adafruit_NeoPixel.h>

/**
 * @brief LED status indicator enumeration for system health communication
 * 
 * @details Defines color-coded states for comprehensive system status indication:
 *          - Normal operation indicators (green)
 *          - Component-specific error indicators (various colors)
 *          - Special modes for recovery and testing
 *          - Animation patterns for complex status communication
 */
enum class LEDState {
    NO_ERROR = 0,           ///< @brief Green - All systems normal, ready for operation
    CONFIG_ERROR,           ///< @brief Red - Configuration load failed, check NVS
    MCP_ERROR,              ///< @brief Orange - MCP23017 I/O expander initialization failed
    ADS_ERROR,              ///< @brief Yellow - ADS1115 ADC initialization failed
    I2C_ERROR,              ///< @brief Purple - I2C communication bus error
    GPS_ERROR,              ///< @brief Blue - GPS/IMU communication error
    WIFI_ERROR,             ///< @brief Cyan - WiFi connection error
    MULTIPLE_ERRORS,        ///< @brief Flashing Red - Multiple system errors detected
    RECOVERY_MODE,          ///< @brief Flashing White - System in recovery mode
    SPECIAL_MODE            ///< @brief Custom rainbow mode for testing and demonstration
};

/**
 * @brief Smart LED status indicator management class
 * 
 * @details Provides comprehensive LED-based status indication with multi-threaded
 *          animation processing. Manages NeoPixel RGB LED for visual system health
 *          monitoring and error identification in agricultural applications.
 */
class MyLED{
    public:
        /**
         * @brief Constructor for LED status indicator system
         * 
         * @param vars Pointer to ESPdata singleton for configuration access
         * 
         * @details Initializes LED system with configuration parameters including
         *          pin assignment, brightness settings, and animation parameters.
         */
        MyLED(ESPdata* vars);
        
        /**
         * @brief Displays a specific color on the LED
         * 
         * @param color 32-bit RGB color value (0xRRGGBB format)
         * 
         * @details Sets the LED to a solid color with current brightness setting.
         *          Used for immediate status indication and testing.
         */
        void showColor(uint32_t color);
        
        /**
         * @brief Sets the LED to indicate a specific system state
         * 
         * @param errorState System state to be indicated (see LEDState enum)
         * 
         * @details Updates the LED status to reflect current system health.
         *          Automatically selects appropriate color and animation pattern.
         */
        void setLEDState(LEDState errorState);
        
        /**
         * @brief Enables or disables special rainbow animation mode
         * 
         * @param enabled True to enable rainbow mode, false for normal operation
         * 
         * @details Special mode for testing and demonstration purposes.
         *          Cycles through rainbow colors for visual appeal and testing.
         */
        void setSpecialMode(bool enabled);
        
        /**
         * @brief Updates LED brightness from configuration settings
         * 
         * @details Applies current brightness setting from ESPdata configuration
         *          to the NeoPixel LED. Called when brightness is changed via web interface.
         */
        void updateBrightness();
        
        /**
         * @brief Starts the LED animation task in parallel thread
         * 
         * @details Creates a FreeRTOS task for LED animation processing.
         *          Enables non-blocking LED animations while main system operates normally.
         */
        void startTask();
        
    private:
        ESPdata* espData;
        Adafruit_NeoPixel pixel;
        LEDState currentErrorState;
        bool errorOverride;
        bool specialMode;
        unsigned long lastBlinkTime;
        bool blinkState;

        static void taskHandler(void *param);  // Task handler
        void continuousLoop();  // Function to run in the background task
        void updateErrorDisplay();
        LEDState detectErrorState();
        uint32_t getErrorColor(LEDState state);
        void handleBlinkingStates(LEDState state);
};

#endif