#include "Arduino.h"
#include "myLED.h"
// #include "gpio_definitions.h"

long firstPixelHue = 0;
uint32_t updateTimer = 0;

MyLED::MyLED(ESPdata* vars) : pixel(1, 48, NEO_GRB + NEO_KHZ800) {
  espData = vars;
  currentErrorState = LEDState::NO_ERROR;
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

void MyLED::setLEDState(LEDState errorState) {
    currentErrorState = errorState;
    errorOverride = true;
}

void MyLED::setSpecialMode(bool enabled) {
    specialMode = enabled;
    if (enabled) {
        currentErrorState = LEDState::SPECIAL_MODE;
        errorOverride = true;
    } else {
        errorOverride = false; // Return to automatic error detection
    }
}

void MyLED::updateBrightness() {
    pixel.setBrightness(espData->program.ledBrht);
    // Note: The brightness change will take effect on the next pixel.show() call
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
  pixel.setBrightness(espData->program.ledBrht);
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
LEDState MyLED::detectErrorState() {
    int errorCount = 0;
    LEDState primaryError = LEDState::NO_ERROR;
    
    // Check configuration load result
    if (espData->program.confRes != 1) {
        errorCount++;
        if (primaryError == LEDState::NO_ERROR) primaryError = LEDState::CONFIG_ERROR;
    }
    
    // Check MCP23017 state
    if (espData->program.mcpState != 1) {
        errorCount++;
        if (primaryError == LEDState::NO_ERROR) primaryError = LEDState::MCP_ERROR;
    }
    
    // Check ADS1115 state
    if (espData->program.adsState != 1) {
        errorCount++;
        if (primaryError == LEDState::NO_ERROR) primaryError = LEDState::ADS_ERROR;
    }
    
    // Check I2C communication state
    if (espData->program.twoWireState != 1) {
        errorCount++;
        if (primaryError == LEDState::NO_ERROR) primaryError = LEDState::I2C_ERROR;
    }
    
    // Check GPS/IMU state
    if (espData->gps.imuState != 1) {
        errorCount++;
        if (primaryError == LEDState::NO_ERROR) primaryError = LEDState::GPS_ERROR;
    }
    
    // Check WiFi state (only if WiFi is expected to be connected)
    if (espData->wifi.state == 0) {
        errorCount++;
        if (primaryError == LEDState::NO_ERROR) primaryError = LEDState::WIFI_ERROR;
    }
    
    // Check if in recovery mode (program state != 1) - this indicates system-level issue
    if (espData->program.state == 0 || espData->program.confRes == 0) {
        return LEDState::RECOVERY_MODE;
    }
    
    // Return multiple errors if more than one issue detected
    if (errorCount > 1) {
        return LEDState::MULTIPLE_ERRORS;
    }
    
    return primaryError;
}

// Update LED display based on current error state
void MyLED::updateErrorDisplay() {
    switch (currentErrorState) {
        case LEDState::NO_ERROR:
            // Always show green when no errors are detected
            pixel.setPixelColor(0, pixel.Color(0, 255, 0)); // Green - All systems normal
            break;
            
        case LEDState::SPECIAL_MODE:
            // Rainbow effect for special mode
            pixel.setPixelColor(0, pixel.gamma32(pixel.ColorHSV(firstPixelHue)));
            firstPixelHue = firstPixelHue + 256;
            if (firstPixelHue > 5*65536) {
                firstPixelHue = 0;
            }
            break;
            
        case LEDState::MULTIPLE_ERRORS:
        case LEDState::RECOVERY_MODE:
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
uint32_t MyLED::getErrorColor(LEDState state) {
    switch (state) {
        case LEDState::NO_ERROR:
            return pixel.Color(0, 255, 0);      // Green
        case LEDState::CONFIG_ERROR:
            return pixel.Color(255, 0, 0);      // Red
        case LEDState::MCP_ERROR:
            return pixel.Color(255, 165, 0);    // Orange
        case LEDState::ADS_ERROR:
            return pixel.Color(255, 255, 0);    // Yellow
        case LEDState::I2C_ERROR:
            return pixel.Color(128, 0, 128);    // Purple
        case LEDState::GPS_ERROR:
            return pixel.Color(0, 0, 255);      // Blue
        case LEDState::WIFI_ERROR:
            return pixel.Color(0, 255, 255);    // Cyan
        case LEDState::MULTIPLE_ERRORS:
            return pixel.Color(255, 0, 0);      // Red (for blinking)
        case LEDState::RECOVERY_MODE:
            return pixel.Color(255, 255, 255);  // White (for blinking)
        case LEDState::SPECIAL_MODE:
            return pixel.Color(128, 128, 255);  // Light blue (fallback, usually uses rainbow)
        default:
            return pixel.Color(255, 0, 255);    // Magenta - Unknown error
    }
}

// Handle blinking states for errors that need attention
void MyLED::handleBlinkingStates(LEDState state) {
    unsigned long currentTime = millis();
    unsigned long blinkInterval;
    
    if (state == LEDState::MULTIPLE_ERRORS) {
        blinkInterval = 200; // Fast blink for multiple errors (5 Hz)
    } else if (state == LEDState::RECOVERY_MODE) {
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
