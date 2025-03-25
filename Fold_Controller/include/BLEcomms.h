#ifndef BLEIPRECEIVER_H
#define BLEIPRECEIVER_H

#include <NimBLEDevice.h>

class BLEIPReceiver {
public:
    BLEIPReceiver(const char* serviceUUID, const char* ipCharacteristicUUID, const char* apStateCharacteristicUUID, const char* enabledStateCharacteristicUUID);
    void begin(const char* deviceName);
    std::string getReceivedIP();
    void setCurrentIP(const std::string& ip);
    void setApState(int state);
    void setEnabledState(int state);
    int getApState();
    int getEnabledState();
    bool isValidIPAddress(const char* ip);

private:
    class IPCharacteristicCallbacks : public NimBLECharacteristicCallbacks {
    public:
        IPCharacteristicCallbacks(BLEIPReceiver* parent);
        void onWrite(NimBLECharacteristic* pCharacteristic);
        void onRead(NimBLECharacteristic* pCharacteristic);

    private:
        BLEIPReceiver* parent;
    };

    NimBLEServer* pServer;
    NimBLEService* pService;
    NimBLECharacteristic* pIPCharacteristic;
    NimBLECharacteristic* pApStateCharacteristic;
    NimBLECharacteristic* pEnabledStateCharacteristic;

    std::string receivedIP;
    std::string currentIP;
    int apState;
    int enabledState;
    const char* deviceName;
    const char* serviceUUID;
    const char* ipCharacteristicUUID;
    const char* apStateCharacteristicUUID;
    const char* enabledStateCharacteristicUUID;
    
    friend class IPCharacteristicCallbacks;
};

#endif // BLEIPRECEIVER_H
