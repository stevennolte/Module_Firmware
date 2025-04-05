#include "Signal_Gen.h"


Signal_Gen::Signal_Gen(ESPconfig* vars, MCP4725* _sensor) {
    espConfig = vars;
    this->_sensor = _sensor;
    
}

void Signal_Gen::taskHandler(void *param) {
    // Cast the param back to the ClassA object
    Signal_Gen *instance = static_cast<Signal_Gen *>(param);
    instance->continuousLoop();  // Call the member function
}

void Signal_Gen::continuousLoop() {
    while (true) {
        vTaskDelay(100/portTICK_PERIOD_MS);
        espConfig->wasData.sigReading = _sensor->getValue();
        // Serial.println(_sensor->getValue());
        // vTaskDelay(500);
    }
}
            

void Signal_Gen::startTask() {
    xTaskCreate(
        taskHandler,   // Task function
        "TaskB",       // Name of the task
        4096,          // Stack size (in words)
        this,          // Pass the current instance as the task parameter
        1,             // Priority of the task
        NULL           // Task handle (not needed)
    );
}