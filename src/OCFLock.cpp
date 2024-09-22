#include <OCFLock.h>

void OCFLock::init(){
    lockedIn = false;
    lockedOut = false;
    armedIn = false;
    armedOut = false;
    enableServo(OCFDirection::BOTH);
    lock(OCFDirection::BOTH);
    disableServo(OCFDirection::BOTH);
}

void OCFLock::moveServoIn(int angle){
    log_d("Moving servo in to %d", angle);
    servoIn.write(angle);
}
void OCFLock::moveServoOut(int angle){
    log_d("Moving servo out to %d", angle);
    servoOut.write(angle);
}
void OCFLock::enableServo(OCFDirection direction){
    if (direction == OCFDirection::IN)
    {
        log_d("Enable servo in and lock it");
        servoIn.attach(OCF_SERVO_IN_PIN);
        moveServoIn(OCF_SERVO_IN_LOCKED);
        lockedIn = true;
        armedIn = true;
    }else if (direction == OCFDirection::OUT)
    {
        log_d("Enable servo out and lock it");
        servoOut.attach(OCF_SERVO_OUT_PIN);
        moveServoOut(OCF_SERVO_OUT_LOCKED);
        lockedOut = true;
        armedOut = true;
    }else if (direction == OCFDirection::BOTH)
    {
        enableServo(OCFDirection::IN);
        enableServo(OCFDirection::OUT);
    }    
}
void OCFLock::disableServo(OCFDirection direction){
    if (direction == OCFDirection::IN)
    {
        log_d("Disable servo in and lock it");
        moveServoIn(OCF_SERVO_IN_LOCKED);
        servoIn.detach();
        lockedIn = true;
        armedIn = false;
    }else if (direction == OCFDirection::OUT)
    {
        log_d("Disable servo out and lock it");
        moveServoOut(OCF_SERVO_OUT_LOCKED);
        servoOut.detach();
        lockedOut = true;
        armedOut = false;
    }else if (direction == OCFDirection::BOTH)
    {
        disableServo(OCFDirection::IN);
        disableServo(OCFDirection::OUT);
    }   
}
void OCFLock::lock(OCFDirection direction){
    if (direction == OCFDirection::IN)
    {
        log_d("Lock in");
        moveServoIn(OCF_SERVO_IN_LOCKED);
        lockedIn = true;
    }else if (direction == OCFDirection::OUT)
    {
        log_d("Lock out");
        moveServoOut(OCF_SERVO_OUT_LOCKED);
        lockedOut = true;
    }else if (direction == OCFDirection::BOTH)
    {
        lock(OCFDirection::IN);
        lock(OCFDirection::OUT);
    }   
}
void OCFLock::unlock(OCFDirection direction){
    if (direction == OCFDirection::IN)
    {
        log_d("Unlock in");
        moveServoIn(OCF_SERVO_IN_UNLOCKED);
        lockedIn = false;
    }else if (direction == OCFDirection::OUT)
    {
        log_d("Unlock out");
        moveServoOut(OCF_SERVO_OUT_UNLOCKED);
        lockedOut = false;
    }else if (direction == OCFDirection::BOTH)
    {
        lock(OCFDirection::IN);
        lock(OCFDirection::OUT);
    }   
}

bool OCFLock::isLockedOut(){ return lockedOut;}
bool OCFLock::isLockedIn(){ return lockedIn;}
bool OCFLock::isArmedIn(){ return armedIn;}
bool OCFLock::isArmedOut(){ return armedOut;}

