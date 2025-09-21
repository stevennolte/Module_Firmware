#include "Arduino.h"
#include "myLED.h"
// #include "gpio_definitions.h"

long firstPixelHue = 0;
uint32_t updateTimer = 0;

MyLED::MyLED(ESPdata* vars) : pixel(1, 48, NEO_GRB + NEO_KHZ800) {
  espData = vars;
  currentErrorState = LEDErrorState::NO_ERROR;
  errorOverride = false;
  specialMode = false;
  lastBlinkTime = 0;
  blinkState = false;
}


void MyLED::showColor(uint32_t color) {
    for (int i = 0; i < pixel.numPixels(); i++) {
        pixel.setPixelColor(i, color);  // Set each pixel to the same color
    }
    pixel.show();  // Send the color data to the strip
}

void MyLED::setErrorState(LEDErrorState errorState) {
    currentErrorState = errorState;
    errorOverride = true;
}

void MyLED::setSpecialMode(bool enabled) {
    specialMode = enabled;
    if (enabled) {
        currentErrorState = LEDErrorState::SPECIAL_MODE;
        errorOverride = true;
    } else {
        errorOverride = false; // Return to automatic error detection
    }
}

// Task handler, runs in a separate task
void MyLED::taskHandler(void *param) {
    // Cast the param back to the ClassA object
    MyLED *instance = static_cast<MyLED *>(param);
    instance->continuousLoop();  // Call the member function
}

// Start the FreeRTOS task
void MyLED::startTask() {
  
  pixel.begin();
  pixel.setBrightness(25);
  xTaskCreate(
        taskHandler,   // Task function
        "TaskA",       // Name of the task
        4096,          // Stack size (in words)
        this,          // Pass the current instance as the task parameter
        1,             // Priority of the task
        NULL           // Task handle (not needed)
    );
}

// Function to run in parallel
void MyLED::continuousLoop() {
 while (true) {
    vTaskDelay(50/portTICK_PERIOD_MS);  // Update every 50ms for smooth error blinking
    
    // If no error override is set, automatically detect error states
    if (!errorOverride) {
        currentErrorState = detectErrorState();
    }
    
    // Update the LED display based on current error state
    updateErrorDisplay();
    
    // Reset error override after some time to allow auto-detection
    if (errorOverride && (millis() - lastBlinkTime) > 10000) { // 10 seconds
        errorOverride = false;
    }
  }
}

// Detect current system error state based on ESPdata
LEDErrorState MyLED::detectErrorState() {
    int errorCount = 0;
    LEDErrorState primaryError = LEDErrorState::NO_ERROR;
    
    // Check configuration load result
    if (espData->program.confRes != 1) {
        errorCount++;
        if (primaryError == LEDErrorState::NO_ERROR) primaryError = LEDErrorState::CONFIG_ERROR;
    }
    
    // Check MCP23017 state
    if (espData->program.mcpState != 1) {
        errorCount++;
        if (primaryError == LEDErrorState::NO_ERROR) primaryError = LEDErrorState::MCP_ERROR;
    }
    
    // Check ADS1115 state
    if (espData->program.adsState != 1) {
        errorCount++;
        if (primaryError == LEDErrorState::NO_ERROR) primaryError = LEDErrorState::ADS_ERROR;
    }
    
    // Check I2C communication state
    if (espData->program.twoWireState != 1) {
        errorCount++;
        if (primaryError == LEDErrorState::NO_ERROR) primaryError = LEDErrorState::I2C_ERROR;
    }
    
    // Check GPS/IMU state
    if (espData->gps.imuState != 1) {
        errorCount++;
        if (primaryError == LEDErrorState::NO_ERROR) primaryError = LEDErrorState::GPS_ERROR;
    }
    
    // Check WiFi state (only if WiFi is expected to be connected)
    if (espData->wifi.state == 0) {
        errorCount++;
        if (primaryError == LEDErrorState::NO_ERROR) primaryError = LEDErrorState::WIFI_ERROR;
    }
    
    // Check if in recovery mode (program state != 1) - this indicates system-level issue
    if (espData->program.state == 0 || espData->program.confRes == 0) {
        return LEDErrorState::RECOVERY_MODE;
    }
    
    // Return multiple errors if more than one issue detected
    if (errorCount > 1) {
        return LEDErrorState::MULTIPLE_ERRORS;
    }
    
    return primaryError;
}

// Update LED display based on current error state
void MyLED::updateErrorDisplay() {
    switch (currentErrorState) {
        case LEDErrorState::NO_ERROR:
            // Always show green when no errors are detected
            pixel.setPixelColor(0, pixel.Color(0, 255, 0)); // Green - All systems normal
            break;
            
        case LEDErrorState::SPECIAL_MODE:
            // Rainbow effect for special mode
            pixel.setPixelColor(0, pixel.gamma32(pixel.ColorHSV(firstPixelHue)));
            firstPixelHue = firstPixelHue + 256;
            if (firstPixelHue > 5*65536) {
                firstPixelHue = 0;
            }
            break;
            
        case LEDErrorState::MULTIPLE_ERRORS:
        case LEDErrorState::RECOVERY_MODE:
            handleBlinkingStates(currentErrorState);
            return; // Skip pixel.show() as it's handled in handleBlinkingStates
            
        default:
            // Solid color for single error states
            pixel.setPixelColor(0, getErrorColor(currentErrorState));
            break;
    }
    
    pixel.show();
}

// Get color for specific error state
uint32_t MyLED::getErrorColor(LEDErrorState state) {
    switch (state) {
        case LEDErrorState::NO_ERROR:
            return pixel.Color(0, 255, 0);      // Green
        case LEDErrorState::CONFIG_ERROR:
            return pixel.Color(255, 0, 0);      // Red
        case LEDErrorState::MCP_ERROR:
            return pixel.Color(255, 165, 0);    // Orange
        case LEDErrorState::ADS_ERROR:
            return pixel.Color(255, 255, 0);    // Yellow
        case LEDErrorState::I2C_ERROR:
            return pixel.Color(128, 0, 128);    // Purple
        case LEDErrorState::GPS_ERROR:
            return pixel.Color(0, 0, 255);      // Blue
        case LEDErrorState::WIFI_ERROR:
            return pixel.Color(0, 255, 255);    // Cyan
        case LEDErrorState::MULTIPLE_ERRORS:
            return pixel.Color(255, 0, 0);      // Red (for blinking)
        case LEDErrorState::RECOVERY_MODE:
            return pixel.Color(255, 255, 255);  // White (for blinking)
        case LEDErrorState::SPECIAL_MODE:
            return pixel.Color(128, 128, 255);  // Light blue (fallback, usually uses rainbow)
        default:
            return pixel.Color(255, 0, 255);    // Magenta - Unknown error
    }
}

// Handle blinking states for errors that need attention
void MyLED::handleBlinkingStates(LEDErrorState state) {
    unsigned long currentTime = millis();
    unsigned long blinkInterval;
    
    if (state == LEDErrorState::MULTIPLE_ERRORS) {
        blinkInterval = 200; // Fast blink for multiple errors (5 Hz)
    } else if (state == LEDErrorState::RECOVERY_MODE) {
        blinkInterval = 500; // Medium blink for recovery mode (1 Hz)
    } else {
        blinkInterval = 300; // Default blink rate for other blinking states
    }
    
    // Handle error state blinking
    if (currentTime - lastBlinkTime >= blinkInterval) {
        blinkState = !blinkState;
        lastBlinkTime = currentTime;
        
        if (blinkState) {
            pixel.setPixelColor(0, getErrorColor(state));
        } else {
            pixel.setPixelColor(0, pixel.Color(0, 0, 0)); // Off
        }
        pixel.show();
    }
}
