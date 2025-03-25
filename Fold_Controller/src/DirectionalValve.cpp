#include "Arduino.h"
#include "DirectionalValve.h"


DirectionalValve::DirectionalValve(){}

void DirectionalValve::begin(uint8_t _pin){
    pin = _pin;
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
}

void DirectionalValve::loop(){
    switch (ctrlState){
        case 0:
            digitalWrite(pin, LOW);
            break;
        case 1:
            digitalWrite(pin, HIGH);
            break;
        default:
            digitalWrite(pin, LOW);
            break;
    }
}