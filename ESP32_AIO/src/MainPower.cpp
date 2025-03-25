#include "MainPower.h"


MainPower::MainPower(ESPconfig* config, Adafruit_MCP23X17* mcp, Adafruit_ADS1115* ads)
{
    _config = config;
    _mcp = mcp;
    _ads = ads;
    _powerOn = _config->gpioDefs.mainPowerInd;
    _mainPowerPin = _config->gpioDefs.mainPowerPin;
}

void MainPower::startTask()
{
    _mcp->pinMode(_powerOn, OUTPUT);
    pinMode(_config->gpioDefs.mainPowerPin, OUTPUT);
    digitalWrite(_config->gpioDefs.mainPowerPin, HIGH);
    _mcp->digitalWrite(_powerOn, HIGH);
    xTaskCreatePinnedToCore(
        taskHandler,   /* Task function. */
        "MainPower",     /* name of task. */
        10000,       /* Stack size of task */
        this,        /* parameter of the task */
        1,           /* priority of the task */
        NULL,        /* Task handle to keep track of created task */
        0);          /* pin task to core 0 */
}

void MainPower::taskHandler(void *param)
{
    MainPower* _this = (MainPower*)param; // Get the object pointer
    _this->continuousLoop();
}

void MainPower::continuousLoop()
{
    while (1)
    {
        // getCurrent();
        vTaskDelay(1000);
    }
}

void MainPower::getCurrent(){
    int16_t adc3;
    adc3 = _ads->readADC_SingleEnded(3);
    Serial.print("AIN0: "); Serial.println(adc3);
}