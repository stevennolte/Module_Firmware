#include "Arduino.h"
#include "myLED.h"

long firstPixelHue = 0;
uint32_t updateTimer = 0;

MyLED::MyLED(ProgramData_t &data, uint8_t pin, uint8_t intensity) : data(data), pixel(1, pin, NEO_GRB + NEO_KHZ800)  {
  pixel.begin();
  pixel.setBrightness(intensity);
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
    // Example logic: cycle through colors continuously
    // if (data.programState == 2){
    //   for (int i = 0; i < pixel.numPixels(); i++) {
    //     pixel.setPixelColor(i, pixel.Color(random(255), random(255), random(255)));
    //   } 
    // }
    // else 
    // {
    //   pixel.setPixelColor(0,0,0,0);
    // }
    // pixel.show();
    // vTaskDelay(10 / portTICK_PERIOD_MS);  // Delay for 500ms to control the speed of the loop
    switch (data.programState){
      case 1:
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
      case 2:
        pixel.setPixelColor(0,pixel.Color(0,255,0));
        pixel.show();
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
