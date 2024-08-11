#ifndef __OCFSTATEMACHINE_H__
#define __OCFSTATEMACHINE_H__

#include <Arduino.h>
#include <definitions.h>
#include <ArduinoJson.h>
#include <OCFFilesystem.h>
#include <OCFHelper.h>
#include <OCFLock.h>

enum OCFMachineStates { IDLE, READING, UNLOCKED };
class OCFStateMachine {
    private:
        OCFMachineStates currentState;
        bool servoAttached;
        uint64_t lastActivity;
        uint64_t lastRFIDActivity;
        char rfidTag[29];
        bool rfidReading;

        void updateIdle();
        void updateReading();
        void updateUnlocked();
        void transitionIdleToReading();
        void transitionReadingToIdle();
        void transitionReadingToUnlocked();
        void transitionUnlockedToReading();
        void transitionUnlockedToIdle();
    public:
        void update();
        void OCFStatemachine();
};


#endif