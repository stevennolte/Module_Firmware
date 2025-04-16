#include "FoldControl.h"


FoldControl::FoldControl(ESPconfig* vars){
    espConfig = vars;
    for (int i = 0; i < 7; i++){
        pinMode(espConfig->gpioDefs.foldPins1[i],OUTPUT);
        pinMode(espConfig->gpioDefs.foldPins2[i],OUTPUT);
    }
    pinMode(espConfig->gpioDefs.directionalValvePin,OUTPUT);
}

void FoldControl::taskHandler(void *param){
    FoldControl* instance = (FoldControl*)param;
    instance->continuousLoop();
}
void FoldControl::continuousLoop(){
    while (true){
        if (millis()-espConfig->foldData.lastMsgRecieved > 2000){
            for (int i = 0; i < 7; i++){
                espConfig->foldData.foldStates[i] = 0;
            }
        }
        uint8_t isLift = 0;
        for (int i = 0; i < 7; i++){    
            if(espConfig->foldData.foldStates[i] == 1){
                isLift++;
            }
        }
        if (isLift > 0){
            // Set pins not matching lift to off
            for (int i = 0; i < 7; i++){
                if(espConfig->foldData.foldStates[i] == 0 || espConfig->foldData.foldStates[i] == 2){
                    digitalWrite(espConfig->gpioDefs.foldPins1[i], LOW); // Set the first set of pins to LOW
                    digitalWrite(espConfig->gpioDefs.foldPins2[i], LOW); // Set the second set of pins to LOW
                }
            }
            digitalWrite(espConfig->gpioDefs.directionalValvePin, HIGH); // Set the directional valve pin to HIGH
            for (int i = 0; i < 7; i++){ 
                if(espConfig->foldData.foldStates[i] == 1){
                    digitalWrite(espConfig->gpioDefs.foldPins1[i], HIGH); // Set the first set of pins to HIGH
                    digitalWrite(espConfig->gpioDefs.foldPins2[i], HIGH); // Set the second set of pins to LOW
                }    
            }
        } else {
            for (int i = 0; i < 7; i++){
                if(espConfig->foldData.foldStates[i] == 0 || espConfig->foldData.foldStates[i] == 1){
                    digitalWrite(espConfig->gpioDefs.foldPins1[i], LOW); // Set the first set of pins to LOW
                    digitalWrite(espConfig->gpioDefs.foldPins2[i], LOW); // Set the second set of pins to LOW
                }
            }
            digitalWrite(espConfig->gpioDefs.directionalValvePin, LOW); // Set the directional valve pin to LOW
            for (int i = 0; i < 7; i++){ 
                if(espConfig->foldData.foldStates[i] == 2){
                    digitalWrite(espConfig->gpioDefs.foldPins1[i], HIGH); // Set the first set of pins to HIGH
                    digitalWrite(espConfig->gpioDefs.foldPins2[i], HIGH); // Set the second set of pins to LOW
                }                       
            }
        }
        for (int i = 0; i < 7; i++){
            espConfig->gpioStates.foldPins1[i] = digitalRead(espConfig->gpioDefs.foldPins1[i]);
            espConfig->gpioStates.foldPins2[i] = digitalRead(espConfig->gpioDefs.foldPins2[i]);
        }
        
        vTaskDelay(50 / portTICK_PERIOD_MS); 
    }
}

void FoldControl::begin(){
    xTaskCreate(
        taskHandler,   // Task function
        "FoldControl", // Name of the task
        4096,          // Stack size (in words)
        this,          // Pass the current instance as the task parameter
        1,             // Priority of the task
        NULL           // Task handle (not needed)
    );
}

