#include "Product_Ctrl.h"


Product_Ctrl::Product_Ctrl(ESPconfig* vars, CANBUS* canbus, Adafruit_ADS1015* ads) {
    espConfig = vars;
    this->ads = ads;
}

void Product_Ctrl::begin(){
    Serial.println("Starting Product Controller");
    // meter.begin(espConfig->gpioDefs.FLOW_PIN, espConfig->flowCfg.flowCalNumber);  
    // meter.setTresholds(espConfig->flowCfg.maxFlow, espConfig->flowCfg.maxFlow);
    for (uint8_t i = 1; i<6; i++){
        pinMode(espConfig->gpioDefs.sectionPins[i], OUTPUT);
    }
    espConfig->rateData.adsReading = ads->readADC_SingleEnded(0);
    if (espConfig->rateData.adsReading > 300){  ///TODO: check threshold
        espConfig->rateData.pressState = 1; // psi sensor is connected
    } else {
        espConfig->rateData.pressState = 2; // psi sensor is not connected
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
        #pragma region Section Control
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
        #pragma endregion

        #pragma region Rate Calculation
        espConfig->rateData.targetRowFlowRate = (espConfig->rateData.targetRate * espConfig->rateData.speed*20.0)/5940.0;
        espConfig->rateData.targetPressure = 275.5083893 * pow(espConfig->rateData.targetRowFlowRate, 2) - 23.20941433 * espConfig->rateData.targetRowFlowRate + 4.769518499;
        uint8_t _rowsActive = 0;
        for (uint8_t i = 1; i<65; i++){
            if (espConfig->rateData.sectionStates[i] == 1){
                _rowsActive++;
            }
        }
        espConfig->rateData.targetFlowRate = espConfig->rateData.targetRowFlowRate * float(_rowsActive);
        #pragma endregion

        #pragma region Flow Meter
        if(espConfig->rateData.prevPulseCount != espConfig->rateData.pulseCount){
            
            uint64_t timedelta = esp_timer_get_time() - espConfig->rateData.pulseTime;
            espConfig->rateData.frequency = double(espConfig->rateData.pulseCount)/(double(timedelta)/1000000.0);
            espConfig->rateData.pulseTime = esp_timer_get_time();
            espConfig->rateData.pulseCount = 0;
        } else if(esp_timer_get_time() - espConfig->rateData.pulseTime > 1000000){
            espConfig->rateData.frequency = 0;
            espConfig->rateData.pulseCount = 0;
            
        }
        espConfig->rateData.prevPulseCount = espConfig->rateData.pulseCount;
        #pragma endregion

        #pragma region Pressure Sensor
        //TODO: check reading to psi conversion
        espConfig->rateData.adsReading = ads->readADC_SingleEnded(0);
        espConfig->rateData.adsMVreading = float(espConfig->rateData.adsReading) * 0.003;
        espConfig->rateData.actualPressure = (espConfig->rateData.adsMVreading * 0.1875)/1000.0; // convert to psi
        #pragma endregion

        #pragma region Regulator Control
        //TODO:  regulator logic
        //TODO:  set manual control mode
        #pragma endregion

       

        vTaskDelay(500/portTICK_PERIOD_MS);
    }
}

/*
psi equation per row
    red	psi = 275.5083893 * x^2 - 23.20941433 * x +	4.769518499
    purple psi =	581.9491364 * x^2 +	26.30578448 * x	- 2.981712418
How to correlate psi to reg target?
    could use curve and then error offset
    
TODO: How to handle no sensor connected? 
*/
