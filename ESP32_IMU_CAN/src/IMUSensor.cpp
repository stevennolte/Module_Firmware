#include "IMUSensor.h"

#define RAD_TO_DEG 57.2957795130823f

IMUSensor::IMUSensor(ESPconfig* vars) {
    espConfig = vars;
}

bool IMUSensor::begin() {
    Wire.setPins(espConfig->gpioDefs.SDA_PIN, espConfig->gpioDefs.SCL_PIN);
    Wire.setClock(400000);
    Wire.begin();

    // Try configured address first, then the alternate
    uint8_t addresses[2] = { espConfig->i2cDefs.BNO_ADDRESS,
                              (uint8_t)(espConfig->i2cDefs.BNO_ADDRESS ^ 0x01) };
    for (int ai = 0; ai < 2; ai++) {
        uint8_t addr = addresses[ai];
        if (bno.begin(addr, Wire)) {
            espConfig->i2cDefs.BNO_ADDRESS = addr;
            Serial.printf("BNO08x found at 0x%02X\n", addr);
            bno.enableRotationVector(espConfig->imuData.reportInterval);
            espConfig->progData.imuState = 1;
            return true;
        }
    }
    Serial.println("BNO08x not found – check wiring and I2C address");
    espConfig->progData.imuState = 2;
    return false;
}

void IMUSensor::startTask() {
    xTaskCreate(taskHandler, "IMUTask", 8192, this, 2, NULL);
}

void IMUSensor::taskHandler(void *param) {
    IMUSensor* instance = static_cast<IMUSensor*>(param);
    instance->continuousLoop();
}

void IMUSensor::continuousLoop() {
    while (true) {
        if (bno.dataAvailable()) {
            float qi, qj, qk, qr, radAcc;
            uint8_t acc;
            bno.getQuat(qi, qj, qk, qr, radAcc, acc);

            // Normalize quaternion
            float norm = sqrtf(qr * qr + qi * qi + qj * qj + qk * qk);
            if (norm > 0.0f) {
                qr /= norm; qi /= norm; qj /= norm; qk /= norm;
            }

            float roll, pitch, yaw;
            quaternionToEuler(qi, qj, qk, qr, roll, pitch, yaw);

            // Apply magnetic declination to obtain true-north heading
            // Rotation Vector already fuses magnetometer → magnetic heading
            // True heading = magnetic heading + declination (+ east, - west)
            float headingTrue = yaw + espConfig->imuData.magDeclination;
            while (headingTrue < 0.0f)    headingTrue += 360.0f;
            while (headingTrue >= 360.0f) headingTrue -= 360.0f;

            espConfig->imuData.roll         = roll;
            espConfig->imuData.pitch        = pitch;
            espConfig->imuData.yaw          = yaw;
            espConfig->imuData.headingTrue  = headingTrue;
            espConfig->imuData.accuracy     = acc;
            espConfig->imuData.lastUpdate   = millis();
        }
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}

void IMUSensor::quaternionToEuler(float qi, float qj, float qk, float qr,
                                   float& roll, float& pitch, float& yaw) {
    float ysqr = qj * qj;

    // Roll (x-axis rotation)
    float t0 = +2.0f * (qr * qi + qj * qk);
    float t1 = +1.0f - 2.0f * (qi * qi + ysqr);
    roll = atan2f(t0, t1) * RAD_TO_DEG;

    // Pitch (y-axis rotation)
    float t2 = +2.0f * (qr * qj - qk * qi);
    t2 = t2 > 1.0f ? 1.0f : (t2 < -1.0f ? -1.0f : t2);
    pitch = asinf(t2) * RAD_TO_DEG;

    // Yaw (z-axis rotation)
    float t3 = +2.0f * (qr * qk + qi * qj);
    float t4 = +1.0f - 2.0f * (ysqr + qk * qk);
    yaw = atan2f(t3, t4) * RAD_TO_DEG;
    if (yaw < 0.0f) yaw += 360.0f;
}
