#ifndef MAG_SENSOR_H
#define MAG_SENSOR_H

#include "Arduino.h"
#include "SparkFun_TMAG5273_Arduino_Library.h"
#include "Wire.h"
#include "ESPconfig.h"
#include "math.h"



class Mag_Sensor{
    public:
        Mag_Sensor(ESPconfig* vars, TMAG5273* sensor);
        void startTask();  // Start the parallel task
        void getMagneticFieldAndTemperature(double* x, double* y, double* z, double* t);
        
        void printRegisters();
    private:
        TMAG5273* _sensor;
        ESPconfig* espConfig;
        static void taskHandler(void *param);  // Task handler
        void continuousLoop();  // Function to run in the background task
        float alpha = 0.05;
    };

#endif

