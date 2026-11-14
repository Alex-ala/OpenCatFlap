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
    tagPending = false;
    directionName = writeDirection(direction);
    if (direction == OCFDirection::INWARD)
    {
        pin_motion = OCF_MOTION_OUTSIDE_PIN;
    }
    else if (direction == OCFDirection::OUTWARD)
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

bool OCFStateMachine::motionDetected()
{
    return digitalRead(pin_motion) == LOW;
}

bool OCFStateMachine::takePendingTag()
{
    bool pending = tagPending;
    tagPending = false;
    return pending;
}

void OCFStateMachine::updateIdle()
{
    if (!motionDetected())
        return;
    lastMotion = millis();
    transitionIdleToReading();
}

void OCFStateMachine::updateReading()
{
    if (motionDetected()) {
        lastMotion = millis();
    }
    if (millis() - lastMotion >= OCF_MOTION_TIMEOUT_MS) {
        transitionReadingToIdle();
        return;
    }
    if (takePendingTag() && checkTag(rfidTag)) {
        transitionReadingToUnlocked();
    }
}

void OCFStateMachine::updateUnlocked()
{
    if (motionDetected()) {
        lastMotion = millis();
    }
    if (millis() - lastMotion >= OCF_MOTION_TIMEOUT_MS) {
        log_d("%s Lock due to inactivity", directionName);
        transitionUnlockedToReading();
        return;
    }
    if (takePendingTag() && !checkTag(rfidTag)) {
        log_d("%s Lock due to invalid tag", directionName);
        transitionUnlockedToReading();
        return;
    }
    if (millis() - lastRFID >= OCF_UNLOCK_TAG_TIMEOUT_MS) {
        log_d("%s Lock due to no tag", directionName);
        transitionUnlockedToReading();
    }
}

void OCFStateMachine::transitionIdleToReading()
{
    OCFLock::enableServo(direction);
    servoAttached = true;
    rfidReading = false;
    lastRFID = 0;
    tagPending = false;
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
    tagPending = false;
    reportedTags.clear();
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
    bool known = false;
    bool allowed = false;
    char name[sizeof(OCFCat::name)] = "";
    {
        OCFState::Guard guard;
        std::map<unsigned long long, OCFCat>::iterator it = OCFState::cats.find(id);
        if (it != OCFState::cats.end()) {
            OCFCat& cat = it->second;
            known = true;
            cat.last_seen = OCFWifi::getEpochTime();
            copyString(name, sizeof(name), cat.name);
            if (direction == OCFDirection::INWARD) {
                allowed = cat.allow_in && OCFState::config.allow_in;
            } else {
                allowed = cat.allow_out && OCFState::config.allow_out;
            }
        }
    }
    bool first_sighting = std::find(reportedTags.begin(), reportedTags.end(), id) == reportedTags.end();
    if (first_sighting) {
        reportedTags.push_back(id);
        char message[64];
        if (known) {
            log_d("%s detected, allowed %s: %d", name, directionName, allowed);
            snprintf(message, sizeof(message), "Cat looks %s: %s", directionName, name);
        } else {
            log_d("Unknown rfid read: %llu", id);
            snprintf(message, sizeof(message), "Unknown cat looks %s: %llu", directionName, id);
        }
        OCFMQTT::sendMessage("activity", message);
    }
    return allowed;
}

void OCFStateMachine::readRFID(unsigned long long tag){
    rfidTag = tag;
    lastRFID = millis();
    tagPending = true;
}
