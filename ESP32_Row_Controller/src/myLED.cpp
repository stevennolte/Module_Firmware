#include "Arduino.h"
#include "myLED.h"

long firstPixelHue = 0;
uint32_t updateTimer = 0;

MyLED::MyLED(ESPconfig* vars) : pixel(1, 48, NEO_GRB + NEO_KHZ800) {
    espConfig = vars;
}

void MyLED::showColor(uint32_t color) {
    for (int i = 0; i < pixel.numPixels(); i++) {
        pixel.setPixelColor(i, color);
    }
    pixel.show();
}

void MyLED::taskHandler(void *param) {
    MyLED *instance = static_cast<MyLED *>(param);
    instance->continuousLoop();
}

void MyLED::startTask() {
    pixel.begin();
    pixel.setBrightness(25);
    xTaskCreate(
        taskHandler,
        "LEDTask",
        4096,
        this,
        1,
        NULL
    );
}

void MyLED::continuousLoop() {
    while (true) {
        vTaskDelay(500 / portTICK_PERIOD_MS);
        switch (espConfig->progData.state) {
            case 0:
                // Off - initialising
                pixel.setPixelColor(0, pixel.Color(0, 0, 0));
                pixel.show();
                break;
            case 1:
                // Green - running normally, toolbar down
                pixel.setPixelColor(0, pixel.Color(0, 255, 0));
                pixel.show();
                break;
            case 2:
                // Rainbow - booting / connecting to WiFi
                pixel.setPixelColor(0, pixel.gamma32(pixel.ColorHSV(firstPixelHue)));
                pixel.show();
                firstPixelHue += 256;
                if (firstPixelHue > 5 * 65536) {
                    firstPixelHue = 0;
                }
                break;
            case 3:
                // Blue/white blink - toolbar up (sections disabled)
                if (millis() - updateTimer < 500) {
                    pixel.setPixelColor(0, 0x00ffffff);
                    pixel.show();
                } else if ((millis() - updateTimer > 500) && (millis() - updateTimer < 1000)) {
                    pixel.setPixelColor(0, 0x000000ff);
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
