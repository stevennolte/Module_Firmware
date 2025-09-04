#include "WAS.h"


WAS::WAS(ESPdata* vars, Adafruit_ADS1115* ads) {
    espData = vars;
    this->ads = ads;
    #ifdef WAS_DEBUG
    rampValue = -20.0;
    rampIncrement = 40.0 / (10 * 1000 / 10); // 40 units over 10 seconds, with 10ms delay
    rampDirection = 1; // 1 for up, -1 for down
    lastUpdateTime = millis();
    #endif
    // 
}

void WAS::init() {
    Serial.println("\t\tInitializing WAS");
    
}

void WAS::loop(){
    #ifndef WAS_DEBUG
    if (millis()-espData->steerData.lastWAStime < 1000){
        espData->steerCfg.wirelessWAS = true;
    } else {
        espData->steerCfg.wirelessWAS = false;
        if (espData->progData.adsState == 1 && espData->steerCfg.wirelessWAS == false && espData->steerCfg.useADS == 1){ 
            espData->steerData.actSteerAngle = ads->computeVolts(ads->readADC_SingleEnded(0));
        } else {
            espData->steerData.actSteerAngle = 0.0;
        }
    }
    vTaskDelay(10);
    #else
    updateRampValue();
    vTaskDelay(10);
    #endif


}

void WAS::zeroSteerAngle() {
    espData->steerData.wasZeroAngle = espData->steerData.actSteerAngle;
}

#ifdef WAS_DEBUG
void WAS::updateRampValue() {
    unsigned long currentTime = millis();
    if (currentTime - lastUpdateTime >= 10) { // Update every 10 milliseconds
        lastUpdateTime = currentTime;
        rampValue += rampIncrement * rampDirection;
        if (rampValue >= 20.0) {
            rampValue = 20.0;
            rampDirection = -1; // Change direction to down
            
        } else if (rampValue <= -20.0) {
            rampValue = -20.0;
            rampDirection = 1; // Change direction to up
        }

        espData->steerData.actSteerAngle = rampValue;
        // Serial.println(rampValue);
    }
}

#endif
