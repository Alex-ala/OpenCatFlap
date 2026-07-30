#include <OCFStateMachine.h>
#include <OCFConfig.h>
#include <OCFMQTT.h>
#include <OCFRFID.h>

// TODO: Enable WIFI & MQTT to debug with powersupply
OCFStateMachine::OCFStateMachine(OCFDirection direction) : direction(direction)
{
    // Servos are attached and parked in the locked position by OCFLock::init().
    servoAttached = false;
    lastMotion = 0;
    lastRFID = 0;
    rfidTag = 0;
    lastRead = 0;
    rfidReading = false;
    directionName = writeDirection(direction);
    if (direction == OCFDirection::IN)
    {
        pin_motion = OCF_MOTION_OUTSIDE_PIN;
    }
    else if (direction == OCFDirection::OUT)
    {
        pin_motion = OCF_MOTION_INSIDE_PIN;
    }
    pinMode(pin_motion, INPUT_PULLUP);
    currentState = OCFMachineStates::IDLE;
    log_d("%s StateMachine initialized", directionName);
}

void OCFStateMachine::update()
{
    if (currentState == OCFMachineStates::IDLE)
        updateIdle();
    else if (currentState == OCFMachineStates::READING)
        updateReading();
    else if (currentState == OCFMachineStates::UNLOCKED)
        updateUnlocked();
}

void OCFStateMachine::updateIdle()
{
    bool motion = digitalRead(pin_motion) == LOW;
    if (!motion)
        return;
    lastMotion = millis();
    transitionIdleToReading();
}

void OCFStateMachine::updateReading()
{
    bool motion = digitalRead(pin_motion) == LOW;
    if (motion) {
        lastMotion = millis();
    }
    // Return to Idle after 10s of no motion
    if (lastMotion <= millis() - 10000) {
        transitionReadingToIdle();
    }else if (lastRFID != 0 && lastRFID >= millis() - 2000) {
        if (checkTag(rfidTag)) {
            transitionReadingToUnlocked();
        }
    }
}

void OCFStateMachine::updateUnlocked()
{
    bool motion = digitalRead(pin_motion) == LOW;
    if (motion) {
        lastMotion = millis();
    }
    if (lastMotion <= millis() - 10000) {
        log_d("%s Lock due to inactivity", directionName);

        transitionUnlockedToReading();
        return;
    }
    if (lastRFID != 0 && lastRFID >= millis() - 2000) {
        if (!checkTag(rfidTag)) {
            log_d("%s Lock due to invalid tag", directionName);
            transitionUnlockedToReading();
            return;
        }
    }
    // Return to Reading after 5s of no valid RFID
    if (lastRFID == 0 && lastRFID <= millis() - 5000)
        log_d("%s Lock due to no tag", directionName);
        transitionUnlockedToReading();
}

void OCFStateMachine::transitionIdleToReading()
{
    OCFLock::enableServo(direction);
    servoAttached = true;
    rfidReading = false;
    lastRFID = 0;
    OCFRFID::enable(direction);
    currentState = OCFMachineStates::READING;
    log_d("%s: Reading", directionName);
    OCFMQTT::sendMessage(directionName, "Reading");
}

void OCFStateMachine::transitionReadingToIdle()
{
    OCFLock::disableServo(direction);
    servoAttached = false;
    rfidReading = false;
    lastMotion = 0;
    lastRFID = 0;
    lastRead = 0;
    rfidTag = 0;
    OCFRFID::disable(direction);
    currentState = OCFMachineStates::IDLE;
    log_d("%s: Idle", directionName);
    OCFMQTT::sendMessage(directionName, "Idle");
}

void OCFStateMachine::transitionReadingToUnlocked()
{
    OCFLock::unlock(direction);
    currentState = OCFMachineStates::UNLOCKED;
    log_d("%s: Unlocked", directionName);
    OCFMQTT::sendMessage(directionName, "Unlocked");
}

void OCFStateMachine::transitionUnlockedToReading()
{
    OCFLock::lock(direction);
    currentState = OCFMachineStates::READING;
    log_d("%s: Reading", directionName);
    OCFMQTT::sendMessage(directionName, "Reading");
}

OCFMachineStates OCFStateMachine::getState(){
    return currentState;
}

bool OCFStateMachine::checkTag(unsigned long long id){
    // TODO: prevent execution and state changes so often. Keep lastRFID intact for transition/update logic
    std::map<unsigned long long, OCFCat>::iterator it = OCFState::cats.find(id);
    if (it->first != id) {
        log_d("Unknown rfid read: %llu", id);
        char message[30];
        sprintf(message, "Unknown cat looks %s: %llu", writeDirection(direction), id);
        OCFMQTT::sendMessage("activity", message);
        return false;
    }
    OCFCat cat = it->second;
    cat.last_seen = OCFWifi::getEpochTime();
    OCFState::cats.insert_or_assign(cat.rfid, cat);
    log_d("%s detected.", cat.name);
    char message[30];
    sprintf(message, "Cat looks %s: %s", writeDirection(direction), cat.name);
    OCFMQTT::sendMessage("activity", message);
    if (direction == OCFDirection::IN){
        log_d("Cat entering has state: %d (cat), %d (global)",cat.allow_in, OCFState::config.allow_in);
        if (cat.allow_in == true && OCFState::config.allow_in == true) return true;
    }else{
        log_d("Cat leaving has state: %d (cat), %d (global)",cat.allow_out, OCFState::config.allow_out);
        if (cat.allow_out == true && OCFState::config.allow_out == true) return true;
    }
    return false;
}

void OCFStateMachine::readRFID(unsigned long long tag){
    rfidTag = tag;
    lastRFID = millis();
}
