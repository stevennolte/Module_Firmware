#ifndef FOLDCONTROL_H
#define FOLDCONTROL_H

#include <Arduino.h>
#include "ESPconfig.h"

class FoldControl
{
    public:
        FoldControl(ESPconfig* vars);
        void begin();
        void foldWings();
        void unfoldWings();
        void raiseWings();
        void lowerWings();
        void foldOuterWings();
        void unfoldOuterWings();
        void foldCenterWings();
        void unfoldCenterWings();
        void foldLeftWing();
        void unfoldLeftWing();
        void foldRightWing();
        void unfoldRightWing();

    private:
        ESPconfig* espConfig;
        static void taskHandler(void *param);  // Task handler
        void continuousLoop();  // Function to run in the background task
};


#endif