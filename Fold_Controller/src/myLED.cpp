#include "Arduino.h"
#include "myLED.h"



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
    if (data.programState == 2){
      for (int i = 0; i < pixel.numPixels(); i++) {
        pixel.setPixelColor(i, pixel.Color(random(255), random(255), random(255)));
      } 
    }
    else 
    {
      pixel.setPixelColor(0,0,0,0);
    }
    pixel.show();
    vTaskDelay(10 / portTICK_PERIOD_MS);  // Delay for 500ms to control the speed of the loop
  }
}
