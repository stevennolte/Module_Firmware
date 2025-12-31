#ifndef A121SENSOR_H
#define A121SENSOR_H

#include <Arduino.h>
#include <Wire.h>

// Acconeer A121 I2C Default Address
#define A121_I2C_ADDRESS 0x52

// A121 Register addresses (based on typical I2C sensor interface)
#define A121_REG_STATUS       0x00
#define A121_REG_COMMAND      0x01
#define A121_REG_CONFIG       0x02
#define A121_REG_DISTANCE_LSB 0x10
#define A121_REG_DISTANCE_MSB 0x11
#define A121_REG_AMPLITUDE    0x12
#define A121_REG_TEMP         0x13

// A121 Commands
#define A121_CMD_RESET        0x00
#define A121_CMD_START        0x01
#define A121_CMD_STOP         0x02
#define A121_CMD_MEASURE      0x03

// A121 Status bits
#define A121_STATUS_READY     0x01
#define A121_STATUS_MEASURING 0x02
#define A121_STATUS_ERROR     0x80

class A121Sensor {
private:
    TwoWire* wire;
    uint8_t i2cAddress;
    int sdaPin;
    int sclPin;
    
    bool writeRegister(uint8_t reg, uint8_t value);
    uint8_t readRegister(uint8_t reg);
    bool readRegisters(uint8_t reg, uint8_t* buffer, uint8_t length);

public:
    A121Sensor(uint8_t address = A121_I2C_ADDRESS);
    A121Sensor(int sda, int scl, uint8_t address = A121_I2C_ADDRESS);
    
    bool begin();
    bool begin(TwoWire& wirePort);
    
    bool isConnected();
    bool reset();
    bool startMeasurement();
    bool stopMeasurement();
    
    uint8_t getStatus();
    bool isReady();
    bool isMeasuring();
    bool hasError();
    
    uint16_t getDistance();
    uint8_t getAmplitude();
    int8_t getTemperature();
    
    bool configure(uint8_t config);
};

#endif // A121SENSOR_H
