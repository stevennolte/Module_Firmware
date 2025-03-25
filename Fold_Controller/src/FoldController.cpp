#include "Arduino.h"
#include "FoldController.h"


FoldController::FoldController(){}

// Task handler, runs in a separate task
void FoldController::taskHandler(void *param) {
    // Cast the param back to the ClassA object
    FoldController *instance = static_cast<FoldController *>(param);
    instance->continuousLoop();  // Call the member function
}

// Start the FreeRTOS task
void FoldController::startTask() {
    for (int i=0; i<7;i++){
        foldValves[i].begin(foldPins1[i], foldPins2[i]);
    }
    dirValve.begin(dirPin);

    xTaskCreate(
        taskHandler,   // Task function
        "TaskA",       // Name of the task
        4096,          // Stack size (in words)
        this,          // Pass the current instance as the task parameter
        1,             // Priority of the task
        NULL           // Task handle (not needed)
    );
}

// Function to run in parallel
void FoldController::continuousLoop() {
  while (true) {
    // check to see if any state is commanding lift
    bool liftCmd = false;
    for (int i=0; i<7; i++){
        if (foldCmdStates[i] == 1){
            liftCmd = true;
        }
    }
    // if cmd is to lift any of the functions, block any lowering and only command the lifting functions
    if (liftCmd){
        for (int i=0; i<7; i++){
            if (foldCmdStates[i] == 1){
                foldValves->ctrlState = 1;
            } else {
                foldValves->ctrlState = 0;
            }
        }
        dirValve.ctrlState = 1;
    } 
    // if cmd is not lift, let the commands go through
    else 
    {  
        for (int i=0; i<7; i++){
            foldValves->ctrlState = foldCmdStates[i];
        }
        dirValve.ctrlState = 0;
    }
    // set the outputs
    dirValve.loop();
    for (int i=0; i<7;i++){
        foldValves[i].loop();
    }
    
    // Example logic: cycle through colors continuously
    vTaskDelay(10 / portTICK_PERIOD_MS);  // Delay for 500ms to control the speed of the loop
  }
}