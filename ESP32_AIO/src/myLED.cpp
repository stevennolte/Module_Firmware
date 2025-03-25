#include "Arduino.h"
#include "myLED.h"
// #include "gpio_definitions.h"

long firstPixelHue = 0;
uint32_t updateTimer = 0;

MyLED::MyLED(ESPconfig* vars) : pixel(1, 48, NEO_GRB + NEO_KHZ800) {
  espConfig = vars;
  
  // Adafruit_NeoPixel pixel(1,espConfig->progCfg.ledPin,NEO_GRB + NEO_KHZ800);
  
  // pixel.show();
}


void MyLED::showColor(uint32_t color) {
    for (int i = 0; i < pixel.numPixels(); i++) {
        pixel.setPixelColor(i, color);  // Set each pixel to the same color
    }
    pixel.show();  // Send the color data to the strip
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
    vTaskDelay(500/portTICK_PERIOD_MS);
    // vTaskDelay(500);
    switch (espConfig->progData.state){
      case 0:
        pixel.setPixelColor(0,pixel.Color(0,0,0));
        pixel.show();
        break;
      case 1:
        pixel.setPixelColor(0,pixel.Color(0,255,0));
        pixel.show();
        break;
      case 2:
      // int pixelHue = firstPixelHue;
        pixel.setPixelColor(0, pixel.gamma32(pixel.ColorHSV(firstPixelHue)));
        pixel.show();
        firstPixelHue = firstPixelHue + 256;
        if (firstPixelHue > 5*65536){
          firstPixelHue = 0;
        }
        delay(5);
        // // rainbow(20);
        
        break;
      case 3:
        if (millis()-updateTimer < 500){
          pixel.setPixelColor(0,0x00ffffff);
          pixel.show();
        } else if ((millis()-updateTimer > 500) && (millis()-updateTimer < 1000))
        {
          pixel.setPixelColor(0,0x000000ff);
          pixel.show();
        } else {
          updateTimer = millis();
        }
        

        break;
      default:
        break;
    }
  }
}
