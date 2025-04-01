#include <StatusLed.h>
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

Adafruit_NeoPixel pixels(1, 48, NEO_GRB + NEO_KHZ800);
StatusLED::StatusLED(){
    
}


void StatusLED::setup(){
    pixels.begin();
    pixels.setPixelColor(0,pixels.Color(150,0,0));
    pixels.setBrightness(100);
    pixels.show();
}
void StatusLED::connectionsGood(){
    pixels.setPixelColor(0,pixels.Color(0,150,0));
    pixels.show();
}

void StatusLED::connectedToWifi(){
    pixels.setPixelColor(0,pixels.Color(0,0,150));   
    pixels.show();
}