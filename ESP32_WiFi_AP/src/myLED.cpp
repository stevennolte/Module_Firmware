#include "myLED.h"

MyLED::MyLED(ESPconfig* vars)
    : espConfig(vars), pixel(1, vars->gpioDefs.LED_PIN, NEO_GRB + NEO_KHZ800) {}

void MyLED::startTask() {
    pixel.begin();
    pixel.setBrightness(25);
    pixel.show();
    xTaskCreate(taskHandler, "LEDTask", 2048, this, 1, NULL);
}

void MyLED::taskHandler(void* param) {
    ((MyLED*)param)->continuousLoop();
}

void MyLED::continuousLoop() {
    while (true) {
        switch (espConfig->progData.state) {
            case 0: // Initializing
                pixel.setPixelColor(0, pixel.Color(0, 0, 0));
                break;
            case 1: // Running – AP active, solid green
                pixel.setPixelColor(0, pixel.Color(0, 50, 0));
                break;
            case 2: // Booting / connecting – rainbow
            {
                uint8_t pos = (uint8_t)(rainbowStep & 0xFF);
                uint8_t r = 0, g = 0, b = 0;
                if (pos < 85)       { r = pos * 3;       g = 255 - pos * 3; }
                else if (pos < 170) { pos -= 85;  g = pos * 3; b = 255 - pos * 3; }
                else                { pos -= 170; b = pos * 3; r = 255 - pos * 3; }
                pixel.setPixelColor(0, pixel.Color(r, g, b));
                rainbowStep += 5;
                break;
            }
            case 3: // Clients connected – bright blue pulse
            {
                static bool on = true;
                pixel.setPixelColor(0, on ? pixel.Color(0, 0, 60) : pixel.Color(0, 0, 0));
                on = !on;
                break;
            }
            default:
                pixel.setPixelColor(0, pixel.Color(40, 40, 0)); // yellow = unknown
                break;
        }
        pixel.show();
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}
