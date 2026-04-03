#include "WAS.h"


WAS::WAS(ESPdata* vars) : i2cManager(I2CManager::getInstance()) {
    espData = vars;
    filteredRawADS = 0.0;
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
        espData->steer.actSteerAngle = 0;
    } else {
        // Get raw ADC reading
        uint16_t rawReading = i2cManager.getRawReading(0);
        
        // Apply exponential moving average filter using configurable alpha value
        if (filteredRawADS == 0.0) {
            // Initialize filter with first reading
            filteredRawADS = (float)rawReading;
        } else {
            // Apply low-pass filter for noise reduction
            float alpha = espData->steer.wasFilterValue;
            filteredRawADS = alpha * (float)rawReading + (1.0 - alpha) * filteredRawADS;
        }
        
        // Store filtered raw ADC value
        espData->steer.rawADS = (uint16_t)filteredRawADS;
        
        // Calculate voltage from filtered raw value
        espData->steer.sensorVoltage = i2cManager.getVoltage(0);
        
        // Calculate steering angle using AgOpenGPS AIO v4 reference implementation
        // Zero point at 6805 (half of 13610, which is 5V at 16-bit resolution)
        int16_t steeringPosition = (int16_t)(espData->steer.rawADS / 2);
        
        if (espData->steer.invertWAS) {
            // Inverted mode: subtract offset, negate counts per degree
            steeringPosition = steeringPosition - 6805 - (int16_t)espData->steer.steerOffset;
            espData->steer.actSteerAngle = (float)steeringPosition / -(float)espData->steer.countsPerDeg;
        } else {
            // Normal mode: add offset, positive counts per degree
            steeringPosition = steeringPosition - 6805 + (int16_t)espData->steer.steerOffset;
            espData->steer.actSteerAngle = (float)steeringPosition / (float)espData->steer.countsPerDeg;
        }
        
    }
        
    
    vTaskDelay(50);  // 50ms delay for responsive steering control
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
