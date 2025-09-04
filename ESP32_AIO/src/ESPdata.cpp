#include "ESPdata.h"

ESPdata* ESPdata::instance = nullptr;

ESPdata::ESPdata() : state(0), lastSteerAngle(0.0), uptime(0) {
    preferences.begin("espdata", false); // Namespace "espdata", read-write
}

ESPdata& ESPdata::getInstance() {
    if (instance == nullptr) {
        instance = new ESPdata();
    }
    return *instance;
}

void ESPdata::destroyInstance() {
    if (instance != nullptr) {
        instance->preferences.end();
        delete instance;
        instance = nullptr;
    }
}

void ESPdata::loadData() {
    state = preferences.getUChar("state", 0);
    lastSteerAngle = preferences.getFloat("lastSteerAngle", 0.0);
    uptime = preferences.getUInt("uptime", 0);
    // Add more loads as needed
}

void ESPdata::saveData() {
    preferences.putUChar("state", state);
    preferences.putFloat("lastSteerAngle", lastSteerAngle);
    preferences.putUInt("uptime", uptime);
    // Add more saves as needed
}
