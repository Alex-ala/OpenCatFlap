#ifndef __OCFSTATEMACHINE_H__
#define __OCFSTATEMACHINE_H__

#include <Arduino.h>
#include <definitions.h>
#include <ArduinoJson.h>
#include <OCFFilesystem.h>
#include <OCFHelper.h>
#include <OCFLock.h>
#include <OCFWifi.h>
#include <ArxContainer.h>
#include <OCFCat.h>
#include <OCFRFID.h>
#include <vector>

/*
There will be two of these statemachines. One for inward transits (outside PIR, outside RFID, inside lock), one for outward transits (inside PIR, inside RFID, outside lock).
A machine is wihtin a state and relying on the update() function to be called regularely. This then executes the correct update method for its current state.

The machine has 3 states.
Idle:
    This state means that the flap is locked and no cat is nearby.
    In this state the flap is watching for movement via PIR
    If a movement is detected, transition to Reading.
    Transitioning to reading includes activating the RFID sensor and enabling servos
Reading:
    This state means that a cat might approach and movement is detected.
    In this state the flap will try to read RFID periodically
    If an RFID is read, transition to Unlocked.
    Transitioning to Unlocked includes unlocking the lock
    If no movement is detected for 10s transition to Idle
    Transitioning to Idle includes disabling the RFID sensor
Unlocked:
    This state means that a cat was detected and the flap was unlocked.
    In this state:
    1. RFID is read periodically.
        a. If an unallowed tag is read, transition to Reading
        b. If no tag is read for 5s, transition to Reading
    Transitioning to Reading includes locking the flap
*/

enum OCFMachineStates
{
    IDLE,
    READING,
    UNLOCKED
};
class OCFRFID;
class OCFStateMachine
{
private:
    OCFMachineStates currentState;
    const OCFDirection direction;
    bool servoAttached;
    uint32_t lastMotion;
    uint32_t lastRFID;
    uint32_t lastRead;
    unsigned long long rfidTag;
    bool rfidReading;
    bool tagPending;
    std::vector<unsigned long long> reportedTags;
    const char * directionName;

    int pin_motion;
    int pin_rfid_power;
    int pin_rfid_clk;
    int pin_rfid_dmod;
    bool checkTag(unsigned long long id);
    bool takePendingTag();
    bool motionDetected();

    void updateIdle();
    void updateReading();
    void updateUnlocked();
    void transitionIdleToReading();
    void transitionReadingToIdle();
    void transitionReadingToUnlocked();
    void transitionUnlockedToReading();

public:
    void update();
    OCFStateMachine(OCFDirection direction);
    OCFMachineStates getState();
    void readRFID(unsigned long long tag);

};

#endif
