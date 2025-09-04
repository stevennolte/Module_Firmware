#ifndef ESPDATA_H
#define ESPDATA_H

#include <Arduino.h>
#include <Preferences.h>

class ESPdata {
private:
    static ESPdata* instance;
    Preferences preferences;

    // Example program data members
    uint8_t state;
    float lastSteerAngle;
    uint32_t uptime;
    // Add more program data members as needed

    ESPdata();

public:
    static ESPdata& getInstance();
    static void destroyInstance();

    // Load all program data from Preferences
    void loadData();
    // Save all program data to Preferences
    void saveData();

    // Getters and setters for program data
    uint8_t getState() const { return state; }
    void setState(uint8_t s) { state = s; }

    float getLastSteerAngle() const { return lastSteerAngle; }
    void setLastSteerAngle(float angle) { lastSteerAngle = angle; }

    uint32_t getUptime() const { return uptime; }
    void setUptime(uint32_t t) { uptime = t; }
};

#endif // ESPDATA_H
