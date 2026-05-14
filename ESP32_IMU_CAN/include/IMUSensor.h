#ifndef IMUSENSOR_H
#define IMUSENSOR_H

#include "Arduino.h"
#include "Wire.h"
#include "ESPconfig.h"
#include "BNO08x_AOG.h"

class IMUSensor {
    public:
        IMUSensor(ESPconfig* vars);
        bool begin();
        void startTask();

    private:
        ESPconfig* espConfig;
        BNO080 bno;
        uint32_t lastCalibrationSave = 0;
        uint8_t lastSavedAccuracy = 0;

        static void taskHandler(void *param);
        void continuousLoop();

        void quaternionToEuler(float qi, float qj, float qk, float qr,
                               float& roll, float& pitch, float& yaw);
};

#endif
