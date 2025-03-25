

#include "BluetoothComms.h"



BluetoothRemote::BluetoothRemote() {
    pServer = nullptr;
    ipCharacteristic = nullptr;
    
}

void BluetoothRemote::initBLE(const char* deviceName) {
    BLEDevice::init(deviceName);
    pServer = BLEDevice::createServer();

    BLEService* remoteService = pServer->createService("00001234-0000-1000-8000-00805f9b34fb");

    
    // Sensor characteristic (for reading sensor data)
    ipCharacteristic = remoteService->createCharacteristic(
        "00005680-0000-1000-8000-00805f9b34fb",
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );

    remoteService->start();
    BLEAdvertising* pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(remoteService->getUUID());
    pAdvertising->start();
}



void BluetoothRemote::sendIPData(const std::string& ipValue) {
    if (ipCharacteristic) {
        ipCharacteristic->setValue(ipValue);
        ipCharacteristic->notify();  // Send as BLE notification
    }
}
