#ifndef FOLDCONTROLLER
#define FOLDCONTROLLER

#include <Arduino.h>
#include <FoldValve.h>
#include <DirectionalValve.h>

uint8_t foldPins1[] = {12,11,15,16,17,18,9};
uint8_t foldPins2[] = {6,7,15,16,17,10,8};
uint8_t dirPin = 13;

class FoldController{
    public:
        FoldController();
        void startTask();
        FoldValve foldValves[7];
        DirectionalValve dirValve;
        
        uint8_t i_lhOuter = 0;
        uint8_t i_lhLift = 1;
        uint8_t i_lhFold = 2;
        uint8_t i_center = 3;
        uint8_t i_rhFold = 4;
        uint8_t i_rhLift = 5;
        uint8_t i_rhOuter = 6;
        uint8_t foldCmdStates[7];

    private:
        static void taskHandler(void *param);  // Task handler
        void continuousLoop();  // Function to run in the background task

};

#endif