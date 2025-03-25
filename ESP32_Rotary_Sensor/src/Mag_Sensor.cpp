#include "Mag_Sensor.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "Wire.h"


Mag_Sensor::Mag_Sensor(ESPconfig* espConfig, TMAG5273* sensor){
    this->_sensor = sensor;
    this->espConfig = espConfig;
}

// Task handler, runs in a separate task
void Mag_Sensor::taskHandler(void *param) {
    // Cast the param back to the ClassA object
    Mag_Sensor *instance = static_cast<Mag_Sensor *>(param);
    instance->continuousLoop();  // Call the member function
}

// Start the FreeRTOS task
void Mag_Sensor::startTask() {
    xTaskCreate(
        taskHandler,   // Task function
        "TaskA",       // Name of the task
        4096,          // Stack size (in words)
        this,          // Pass the current instance as the task parameter
        1,             // Priority of the task
        NULL           // Task handle (not needed)
    );
}

void Mag_Sensor::continuousLoop(){
    while(true){
        vTaskDelay(10);
        if(_sensor->getMagneticChannel() != 0) 
        {
            

            // Apply low-pass filter to the sensor data
            // espConfig->wasData.x = alpha * _sensor->getXData() + (1 - alpha) * espConfig->wasData.x;
            // espConfig->wasData.y = alpha * _sensor->getYData() + (1 - alpha) * espConfig->wasData.y;
            // espConfig->wasData.z = alpha * _sensor->getZData() + (1 - alpha) * espConfig->wasData.z;
            // espConfig->wasData.t = alpha * _sensor->getTemp() + (1 - alpha) * espConfig->wasData.t;
            espConfig->wasData.x = _sensor->getXData();
            espConfig->wasData.y = _sensor->getYData();
            espConfig->wasData.z = _sensor->getZData();
            espConfig->wasData.t = _sensor->getTemp();
            espConfig->wasData.sensorAngle = (alpha * (atan2(espConfig->wasData.y, espConfig->wasData.x) * 180 / PI)) + ((1 - alpha) * espConfig->wasData.sensorAngle);
            
            // espConfig->wasData.sensorAngle = alpha * _sensor->getAngleResult() + (1 - alpha) * espConfig->wasData.sensorAngle;
            
            // Serial.print("(");
            // Serial.print(espConfig->wasData.x);
            // Serial.print(", ");
            // Serial.print(espConfig->wasData.y);
            // Serial.print(", ");
            // Serial.print(espConfig->wasData.z);
            // Serial.println(") mT");
            // Serial.print(espConfig->wasData.t);
            // Serial.println(" C");
        }
    }
}



void Mag_Sensor::getMagneticFieldAndTemperature(double* x, double* y, double* z, double* t){
    // dut->getMagneticFieldAndTemperature(x, y, z, t);

}




