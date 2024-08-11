#ifndef __OCFSTATEMACHINE_H__
#define __OCFSTATEMACHINE_H__

#include <Arduino.h>
#include <Servo.h>
#include <definitions.h>
#include <ArduinoJson.h>
#include <OCFFilesystem.h>
#include <OCFHelper.h>
#include <OCFLock.h>

enum OCFMachineStates { IDLE, READING, UNLOCKED };
class OCFStateMachine {
    private:
        class OCFMachineStateIdle: OCFMachineState {
            void update();
        };
        class OCFMachineStateReading: OCFMachineState {
            void update();
        };
        class OCFMachineStateUnlocked: OCFMachineState {
            void update();
        };

        OCFMachineState currentState;
    public:
        void update();
};


#endif