#include "WAS.h"


WAS::WAS(ESPdata* vars) : i2cManager(I2CManager::getInstance()) {
    espData = vars;
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
        espData->steer.rawADS = i2cManager.getRawReading(0);

        espData->steer.sensorVoltage = i2cManager.getVoltage(0);
        uint16_t zeroPoint = 6800;
        espData->steer.actSteerAngle = (float)((int)espData->steer.rawADS/2 - (int)zeroPoint+espData->steer.steerOffset/2) / (float)(espData->steer.countsPerDeg);
        // TODO: Migrate to use i2cManager.getRawReading(0)
        // espData->steer.rawADS = i2cManager.getRawReading(0);
        // espData->steer.actSteerAngle = ads->computeVolts(espData->steer.rawADS);
        
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
