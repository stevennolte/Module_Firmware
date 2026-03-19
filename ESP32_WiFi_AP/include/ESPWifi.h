#ifndef ESPWIFI_H
#define ESPWIFI_H
#include "ESPconfig.h"

class ESPWifi {
public:
    explicit ESPWifi(ESPconfig* vars);
    void startAP();
    void connectSTA();
    void startMonitor();
    int getConnectedClients() const;

private:
    ESPconfig* espConfig;
    static void taskHandler(void* param);
    void continuousLoop();
};

#endif
