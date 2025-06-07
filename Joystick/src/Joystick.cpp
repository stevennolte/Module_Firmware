// Joystick.cpp
// Implementation file for the Joystick module

#include "Joystick.h"

// Constructor
Joystick::Joystick(ESPconfig* vars) {
    espConfig = vars;
    // Initialize joystick state
    
}



// Update joystick state
void Joystick::initialize() {
    // Initialize joystick state
    // Set up GPIO pins, etc.
    for (int i = 0; i < sizeof(espConfig->gpioDefs.inputPins); i++){
        pinMode(espConfig->gpioDefs.inputPins[i], INPUT_PULLUP);
       }
    xTaskCreate(
        taskHandler,   // Task function
        "Joystick",       // Name of the task
        4096,          // Stack size (in words)
        this,          // Pass the current instance as the task parameter
        1,             // Priority of the task
        NULL           // Task handle (not needed)
    );
}

void Joystick::taskHandler(void *param){
    Joystick* instance = (Joystick*)param;
    instance->continuousLoop();
}

void Joystick::continuousLoop() {
    bool prevAutoSteerState = digitalRead(espConfig->gpioDefs.inputPins[9]); // Track previous state of the input pin
    bool prevSectionState = digitalRead(espConfig->gpioDefs.inputPins[7]);
    uint8_t reading = 0;
    while (true) {
        reading = !digitalRead(espConfig->gpioDefs.inputPins[4]);
        if (reading != espConfig->joyCmds.leftLift) {
            espConfig->joyCmds.changeInCmd = true;
        }
        espConfig->joyCmds.leftLift = reading;
        reading = !digitalRead(espConfig->gpioDefs.inputPins[5]);
        if (reading != espConfig->joyCmds.leftLower) {
            espConfig->joyCmds.changeInCmd = true;
        }
        espConfig->joyCmds.leftLower = reading;
        reading = !digitalRead(espConfig->gpioDefs.inputPins[3]);
        if (reading != espConfig->joyCmds.rightLift) {
            espConfig->joyCmds.changeInCmd = true;
        }
        espConfig->joyCmds.rightLift = reading;
        reading = !digitalRead(espConfig->gpioDefs.inputPins[6]);
        if (reading != espConfig->joyCmds.rightLower) {
            espConfig->joyCmds.changeInCmd = true;
        }
        espConfig->joyCmds.rightLower = reading;
        reading = !digitalRead(espConfig->gpioDefs.inputPins[2]);
        if (reading != espConfig->joyCmds.centerLift) {
            espConfig->joyCmds.changeInCmd = true;
        }
        espConfig->joyCmds.centerLift = reading;
        reading = !digitalRead(espConfig->gpioDefs.inputPins[1]);
        if (reading != espConfig->joyCmds.centerLower) {
            espConfig->joyCmds.changeInCmd = true;
        }
        espConfig->joyCmds.centerLower = reading;

        espConfig->joyCmds.leftLift = !digitalRead(espConfig->gpioDefs.inputPins[4]);
        espConfig->joyCmds.leftLower = !digitalRead(espConfig->gpioDefs.inputPins[5]);
        espConfig->joyCmds.rightLift = !digitalRead(espConfig->gpioDefs.inputPins[3]);
        espConfig->joyCmds.rightLower = !digitalRead(espConfig->gpioDefs.inputPins[6]);
        espConfig->joyCmds.centerLift = !digitalRead(espConfig->gpioDefs.inputPins[2]);
        espConfig->joyCmds.centerLower = !digitalRead(espConfig->gpioDefs.inputPins[1]);

        // Read the current state of the autoSteer input pin
        bool currentAutoSteerState = digitalRead(espConfig->gpioDefs.inputPins[9]);
        bool currentSectionState = digitalRead(espConfig->gpioDefs.inputPins[7]);
        // Check for a rising edge (button press)
        if (prevAutoSteerState == HIGH && currentAutoSteerState == LOW) {
            // Toggle autoSteer between 0 and 1
            espConfig->joyCmds.changeInCmd = true;
            espConfig->joyCmds.autoSteer = !espConfig->joyCmds.autoSteer;
        }
        if (prevSectionState == HIGH && currentSectionState == LOW) {
            espConfig->joyCmds.changeInCmd = true;
            espConfig->joyCmds.sectionControl = !espConfig->joyCmds.sectionControl;
        }

        // Update the previous state
        prevAutoSteerState = currentAutoSteerState;
        prevSectionState = currentSectionState;

        // espConfig->joyCmds.sectionControl = !digitalRead(espConfig->gpioDefs.inputPins[7]);

        vTaskDelay(10 / portTICK_PERIOD_MS); // Delay for 10ms
    }
}
