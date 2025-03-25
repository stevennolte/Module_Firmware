#include "BLEcomms.h"

// Constructor
BLEIPReceiver::BLEIPReceiver(const char* serviceUUID, const char* ipCharacteristicUUID, const char* apStateCharacteristicUUID, const char* enabledStateCharacteristicUUID)
    : deviceName(deviceName), serviceUUID(serviceUUID), ipCharacteristicUUID(ipCharacteristicUUID), apStateCharacteristicUUID(apStateCharacteristicUUID), enabledStateCharacteristicUUID(enabledStateCharacteristicUUID),
      currentIP("0.0.0.0"), apState(0), enabledState(0) {}

void BLEIPReceiver::begin(const char* deviceName) {
    NimBLEDevice::init(deviceName);
    pServer = NimBLEDevice::createServer();
    pService = pServer->createService(serviceUUID);

    // IP characteristic with read and write properties
    pIPCharacteristic = pService->createCharacteristic(
                            ipCharacteristicUUID,
                            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
                        );
    pIPCharacteristic->setCallbacks(new IPCharacteristicCallbacks(this));
    pIPCharacteristic->setValue(currentIP);

    // AP State characteristic with read and write properties
    pApStateCharacteristic = pService->createCharacteristic(
                                apStateCharacteristicUUID,
                                NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
                             );
    pApStateCharacteristic->setValue(std::to_string(apState));

    // Enabled State characteristic with read and write properties
    pEnabledStateCharacteristic = pService->createCharacteristic(
                                    enabledStateCharacteristicUUID,
                                    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE
                                 );
    pEnabledStateCharacteristic->setValue(std::to_string(enabledState));

    pService->start();

    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(serviceUUID);
    pAdvertising->start();
}

// Get the received IP address
std::string BLEIPReceiver::getReceivedIP() {
    return receivedIP;
}

// Set the current IP address
void BLEIPReceiver::setCurrentIP(const std::string& ip) {
    if (isValidIPAddress(ip.c_str())) {
        currentIP = ip;
        pIPCharacteristic->setValue(currentIP);
    }
}

// Set the AP state
void BLEIPReceiver::setApState(int state) {
    apState = state;
    pApStateCharacteristic->setValue(std::to_string(apState));
}

// Get the AP state
int BLEIPReceiver::getApState() {
    return apState;
}

// Set the enabled state
void BLEIPReceiver::setEnabledState(int state) {
    enabledState = state;
    pEnabledStateCharacteristic->setValue(std::to_string(enabledState));
}

// Get the enabled state
int BLEIPReceiver::getEnabledState() {
    return enabledState;
}

// IP validation function
bool BLEIPReceiver::isValidIPAddress(const char* ip) {
    int segments = 0;
    int chCnt = 0;

    while (*ip) {
        if (*ip == '.') {
            if (chCnt == 0 || chCnt > 3) return false;
            segments++;
            chCnt = 0;
        } else if (*ip >= '0' && *ip <= '9') {
            chCnt++;
        } else {
            return false;
        }
        ip++;
    }
    return segments == 3 && chCnt > 0 && chCnt <= 3;
}

// Callback constructor
BLEIPReceiver::IPCharacteristicCallbacks::IPCharacteristicCallbacks(BLEIPReceiver* parent) {
    this->parent = parent;
}

// Write callback for IP address
void BLEIPReceiver::IPCharacteristicCallbacks::onWrite(NimBLECharacteristic* pCharacteristic) {
    std::string value = pCharacteristic->getValue();

    if (parent->isValidIPAddress(value.c_str())) {
        parent->receivedIP = value;
        Serial.print("Received valid IP: ");
        Serial.println(parent->receivedIP.c_str());
    } else {
        Serial.println("Invalid IP address format received.");
    }
}

// Optional: Read callback
void BLEIPReceiver::IPCharacteristicCallbacks::onRead(NimBLECharacteristic* pCharacteristic) {
    if (pCharacteristic == parent->pIPCharacteristic) {
        pCharacteristic->setValue(parent->currentIP);
    }
}
