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
    return;
}

void WAS::loop(){
    #ifndef WAS_DEBUG

    if(espData->steer.wirelessWAS){
        vTaskDelay(10);
    } else {
        // TODO: Migrate to use adsManager.getRawReading(0)
        if (ads != nullptr) {
            espData->steer.rawADS = ads->readADC_SingleEnded(0);
            // espData->steer.actSteerAngle = ads->computeVolts(espData->steer.rawADS);
        } else {
            // Temporary placeholder until ADS Manager migration
            espData->steer.rawADS = 0;
        }
    }
        
    
    vTaskDelay(200);  // Reduced from 100ms to 200ms to reduce I2C bus load
    #else
    updateRampValue();
    vTaskDelay(10);
    #endif


}

void WAS::zeroSteerAngle() {
    espData->steer.wasZeroAngle = espData->steer.actSteerAngle;
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
