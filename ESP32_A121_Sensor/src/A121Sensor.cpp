#include "A121Sensor.h"

A121Sensor::A121Sensor(uint8_t address) 
    : wire(&Wire), i2cAddress(address), sdaPin(-1), sclPin(-1) {
}

A121Sensor::A121Sensor(int sda, int scl, uint8_t address)
    : wire(&Wire), i2cAddress(address), sdaPin(sda), sclPin(scl) {
}

bool A121Sensor::begin() {
    return begin(Wire);
}

bool A121Sensor::begin(TwoWire& wirePort) {
    wire = &wirePort;
    
    // Initialize I2C with custom pins if specified
    if (sdaPin != -1 && sclPin != -1) {
        wire->begin(sdaPin, sclPin);
    } else {
        wire->begin();
    }
    
    // Set I2C clock speed to 400kHz (fast mode)
    wire->setClock(400000);
    
    delay(100); // Allow sensor to stabilize
    
    // Check if sensor is connected
    if (!isConnected()) {
        return false;
    }
    
    // Reset sensor to known state
    if (!reset()) {
        return false;
    }
    
    delay(50); // Wait for reset to complete
    
    return isReady();
}

bool A121Sensor::isConnected() {
    wire->beginTransmission(i2cAddress);
    return (wire->endTransmission() == 0);
}

bool A121Sensor::reset() {
    return writeRegister(A121_REG_COMMAND, A121_CMD_RESET);
}

bool A121Sensor::startMeasurement() {
    return writeRegister(A121_REG_COMMAND, A121_CMD_START);
}

bool A121Sensor::stopMeasurement() {
    return writeRegister(A121_REG_COMMAND, A121_CMD_STOP);
}

uint8_t A121Sensor::getStatus() {
    return readRegister(A121_REG_STATUS);
}

bool A121Sensor::isReady() {
    return (getStatus() & A121_STATUS_READY) != 0;
}

bool A121Sensor::isMeasuring() {
    return (getStatus() & A121_STATUS_MEASURING) != 0;
}

bool A121Sensor::hasError() {
    return (getStatus() & A121_STATUS_ERROR) != 0;
}

uint16_t A121Sensor::getDistance() {
    uint8_t buffer[2];
    if (readRegisters(A121_REG_DISTANCE_LSB, buffer, 2)) {
        return (buffer[1] << 8) | buffer[0];
    }
    return 0;
}

uint8_t A121Sensor::getAmplitude() {
    return readRegister(A121_REG_AMPLITUDE);
}

int8_t A121Sensor::getTemperature() {
    return (int8_t)readRegister(A121_REG_TEMP);
}

bool A121Sensor::configure(uint8_t config) {
    return writeRegister(A121_REG_CONFIG, config);
}

bool A121Sensor::writeRegister(uint8_t reg, uint8_t value) {
    wire->beginTransmission(i2cAddress);
    wire->write(reg);
    wire->write(value);
    return (wire->endTransmission() == 0);
}

uint8_t A121Sensor::readRegister(uint8_t reg) {
    wire->beginTransmission(i2cAddress);
    wire->write(reg);
    if (wire->endTransmission() != 0) {
        return 0;
    }
    
    wire->requestFrom(i2cAddress, (uint8_t)1);
    if (wire->available()) {
        return wire->read();
    }
    return 0;
}

bool A121Sensor::readRegisters(uint8_t reg, uint8_t* buffer, uint8_t length) {
    wire->beginTransmission(i2cAddress);
    wire->write(reg);
    if (wire->endTransmission() != 0) {
        return false;
    }
    
    wire->requestFrom(i2cAddress, length);
    for (uint8_t i = 0; i < length; i++) {
        if (wire->available()) {
            buffer[i] = wire->read();
        } else {
            return false;
        }
    }
    return true;
}
