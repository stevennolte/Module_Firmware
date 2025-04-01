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
        vTaskDelay(1000 / portTICK_PERIOD_MS); // Delay for 1 second
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

