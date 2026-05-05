#include "myLED.h"

static long firstPixelHue = 0;
static uint32_t ledUpdateTimer = 0;

MyLED::MyLED(ESPconfig* vars) : pixel(1, 48, NEO_GRB + NEO_KHZ800) {
    espConfig = vars;
}

void MyLED::startTask() {
    pixel.begin();
    pixel.setBrightness(espConfig->progCfg.ledBrht);
    xTaskCreate(taskHandler, "LEDTask", 4096, this, 1, NULL);
}

void MyLED::taskHandler(void *param) {
    MyLED* instance = static_cast<MyLED*>(param);
    instance->continuousLoop();
}

void MyLED::continuousLoop() {
    while (true) {
        vTaskDelay(50 / portTICK_PERIOD_MS);
        switch (espConfig->progData.state) {
            case 0:
                pixel.setPixelColor(0, pixel.Color(0, 0, 0));
                pixel.show();
                break;
            case 1:  // Running OK
                pixel.setPixelColor(0, pixel.Color(0, 255, 0));
                pixel.show();
                break;
            case 2:  // Starting up / rainbow
                pixel.setPixelColor(0, pixel.gamma32(pixel.ColorHSV(firstPixelHue)));
                pixel.show();
                firstPixelHue += 256;
                if (firstPixelHue > 5 * 65536) firstPixelHue = 0;
                break;
            case 3:  // Error – fast blue/white blink
                if (millis() - ledUpdateTimer < 500) {
                    pixel.setPixelColor(0, 0x00ffffff);
                    pixel.show();
                } else if (millis() - ledUpdateTimer < 1000) {
                    pixel.setPixelColor(0, 0x000000ff);
                    pixel.show();
                } else {
                    ledUpdateTimer = millis();
                }
                break;
            default:
                break;
        }
    }
}
