#include "Product_Ctrl.h"


Product_Ctrl::Product_Ctrl(ESPconfig* vars, CANBUS* canbus) : meter() {
    espConfig = vars;
}

void Product_Ctrl::begin(){
    meter.begin(espConfig->gpioDefs.FLOW_PIN, espConfig->flowCfg.flowCalNumber);  
    meter.setTresholds(espConfig->flowCfg.maxFlow, espConfig->flowCfg.maxFlow);
    for (uint8_t i = 1; i<6; i++){
        pinMode(espConfig->gpioDefs.sectionPins[i], OUTPUT);
    }
    xTaskCreate(
        taskHandler,   // Task function
        "TaskC",       // Name of the task
        4096,          // Stack size (in words)
        this,          // Pass the current instance as the task parameter
        1,             // Priority of the task
        NULL           // Task handle (not needed)
    );
}

void Product_Ctrl::taskHandler(void *param){
    Product_Ctrl* instance = (Product_Ctrl*)param;
    instance->continuousLoop();
}

void Product_Ctrl::continuousLoop(){
    while (true){
        
        if (millis()-espConfig->rateData.lastSectionMsg > 2000){
            for (uint8_t i = 1; i<65; i++){
                espConfig->rateData.sectionStates[i] = 0;
            }
        }
        for (uint8_t i = 1; i<6; i++){
            if (espConfig->rateData.sectionStates[i] == 1){
                digitalWrite(espConfig->gpioDefs.sectionPins[i], HIGH);

            } else {
                digitalWrite(espConfig->gpioDefs.sectionPins[i], LOW);

            }
        }
        espConfig->rateData.targetRowFlowRate = (espConfig->rateData.targetRate * espConfig->rateData.speed*20.0)/5940.0;
        espConfig->rateData.targetPressure = 275.5083893 * pow(espConfig->rateData.targetRowFlowRate, 2) - 23.20941433 * espConfig->rateData.targetRowFlowRate + 4.769518499;
        uint8_t _rowsActive = 0;
        for (uint8_t i = 1; i<65; i++){
            if (espConfig->rateData.sectionStates[i] == 1){
                _rowsActive++;
            }
        }
        espConfig->rateData.targetFlowRate = espConfig->rateData.targetRowFlowRate * float(_rowsActive);
        espConfig->rateData.actualFlowRate = meter.getFlowRateLiterMinute() * lpmConversion;
        
        vTaskDelay(500/portTICK_PERIOD_MS);
    }
}

// psi equation per row
// 
// red	psi = 275.5083893 * x^2 - 23.20941433 * x +	4.769518499
// purple psi =	581.9491364 * x^2 +	26.30578448 * x	- 2.981712418
