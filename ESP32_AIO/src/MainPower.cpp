#include "MainPower.h"


MainPower::MainPower(ESPdata* config, Adafruit_ADS1115* ads) : _mcpManager(MCPManager::getInstance())
{
    _data = config;
    _ads = ads;
    _powerOn = _data->pins.mainPowerInd;
    _mainPowerPin = _data->pins.mainPowerPin;
}

void MainPower::startTask()
{
    if (_mcpManager.isInitialized()) {
        _mcpManager.setupPowerPin();     // Uses ESPdata pin definitions automatically
        _mcpManager.setPowerState(true); // Uses ESPdata pin definitions automatically
        Serial.println("MainPower: Power indicator configured using MCPManager");
    } else {
        Serial.println("MainPower: MCPManager not initialized, power indicator not configured");
    }
    
    pinMode(_mainPowerPin, OUTPUT);
    pinMode(_data->pins.mainPowerDen, OUTPUT);
    digitalWrite(_data->pins.mainPowerDen, LOW);

    digitalWrite(_mainPowerPin, HIGH);
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
        getCurrent();
        vTaskDelay(200 / portTICK_PERIOD_MS); // Reduced from 100ms to 200ms to reduce I2C bus load
    }
}

void MainPower::getCurrent(){
    // Read main power current using ADS1115 (temporarily disabled until ADS migration)
    // TODO: Migrate to use adsManager.getRawReading(_data->adsConfig.mainPowerISpin)
    if (_ads != nullptr) {
        // _data->power.mainCurrent = _ads->readADC_SingleEnded(_data->adsConfig.mainPowerISpin) * 3; // Example conversion factor, adjust as needed
        _data->power.mainCurrentRaw = _ads->readADC_SingleEnded(_data->adsConfig.mainPowerISpin);
    } else {
        // Temporary placeholder until ADS Manager migration
        _data->power.mainCurrentRaw = 0;
    }
}
