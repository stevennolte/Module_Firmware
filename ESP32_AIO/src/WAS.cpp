#include "WAS.h"


WAS::WAS(ESPconfig* vars, Adafruit_ADS1115* ads) {
    espConfig = vars;
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
    if (millis()-espConfig->steerData.lastWAStime < 1000){
        espConfig->steerCfg.wirelessWAS = true;
    } else {
        espConfig->steerCfg.wirelessWAS = false;
    }
    if (espConfig->progData.adsState == 1 && espConfig->steerCfg.wirelessWAS == false){ 
        espConfig->steerData.actSteerAngle = ads->readADC_SingleEnded(0);
    }
    vTaskDelay(10);
    #else
    updateRampValue();
    vTaskDelay(10);
    #endif


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

        espConfig->steerData.actSteerAngle = rampValue;
        // Serial.println(rampValue);
    }
}

#endif