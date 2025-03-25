#ifndef BLUETOOTHCOMMS_H
#define BLUETOOTHCOMMS_H

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

class BluetoothRemote {
public:
    BluetoothRemote();  // Constructor
    void initBLE(const char* deviceName);  // Initialize BLE
    void sendIPData(const std::string& ipValue);  // Send sensor data as a BLE notification

private:
    BLEServer* pServer;
    BLECharacteristic* ipCharacteristic;
    
};


#endif