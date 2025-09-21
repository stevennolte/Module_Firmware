#ifndef myLED_h
#define myLED_h

// #include "Arduino.h"
#include "ESPdata.h"
#include <Adafruit_NeoPixel.h>

// LED Error State Definitions
enum class LEDErrorState {
    NO_ERROR = 0,           // Green - All systems normal
    CONFIG_ERROR,           // Red - Configuration load failed
    MCP_ERROR,             // Orange - MCP23017 initialization failed
    ADS_ERROR,             // Yellow - ADS1115 initialization failed
    I2C_ERROR,             // Purple - I2C communication error
    GPS_ERROR,             // Blue - GPS/IMU communication error
    WIFI_ERROR,            // Cyan - WiFi connection error
    MULTIPLE_ERRORS,       // Flashing Red - Multiple system errors
    RECOVERY_MODE,         // Flashing White - System in recovery mode
    SPECIAL_MODE           // Custom mode for special purposes
};

class MyLED{
    public:
        MyLED(ESPdata* vars);
        void showColor(uint32_t color);
        void setErrorState(LEDErrorState errorState);
        void setSpecialMode(bool enabled);  // Enable/disable special rainbow mode
        void startTask();  // Start the parallel task
        
    private:
        ESPdata* espData;
        Adafruit_NeoPixel pixel;
        LEDErrorState currentErrorState;
        bool errorOverride;
        bool specialMode;
        unsigned long lastBlinkTime;
        bool blinkState;

        static void taskHandler(void *param);  // Task handler
        void continuousLoop();  // Function to run in the background task
        void updateErrorDisplay();
        LEDErrorState detectErrorState();
        uint32_t getErrorColor(LEDErrorState state);
        void handleBlinkingStates(LEDErrorState state);
};

#endif