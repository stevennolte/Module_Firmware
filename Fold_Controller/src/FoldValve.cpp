#include "Arduino.h"
#include "FoldValve.h"


FoldValve::FoldValve(){}

void FoldValve::begin(uint8_t _pin1, uint8_t _pin2){
    pin1 = _pin1;
    pin2 = _pin2;
    pinMode(pin1, OUTPUT);
    pinMode(pin2, OUTPUT);
    digitalWrite(pin1, LOW);
    digitalWrite(pin2, LOW);
}

void FoldValve::loop(){
    switch (ctrlState){
        case 0:
            digitalWrite(pin1, LOW);
            digitalWrite(pin2, LOW);
            break;
        case 1:
            digitalWrite(pin1, HIGH);
            digitalWrite(pin2, HIGH);
            break;
        case 2:
            digitalWrite(pin1, LOW);
            digitalWrite(pin2, LOW);
            break;
        default:
            digitalWrite(pin1, LOW);
            digitalWrite(pin2, LOW);
            break;
    }
}