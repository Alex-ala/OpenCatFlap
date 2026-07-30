#include <OCFLock.h>

// ServoESP32's automatic channel allocator only recycles a channel when it is the most
// recently allocated one (Servo.h: `if (_channel == channel_next_free - 1)`). The two
// servos are attached and detached in interleaved order, so auto-allocation leaks one
// channel per lock cycle: after SOC_LEDC_CHANNEL_NUM cycles attach() returns false, never
// calls ledcSetup/ledcAttachPin, and both servos are dead until reboot.
//
// Passing an explicit channel bypasses the allocator entirely - channel_next_free is
// never touched, so it cannot ratchet up. Each servo owns its channel for the lifetime of
// the process; disabling only stops the pulse train (park), it never gives a channel back.

void OCFLock::init(){
    if (initialized) return;
    if (!servoIn.attach(OCF_SERVO_IN_PIN, OCF_SERVO_IN_CHANNEL))
        log_e("Failed to attach servo in on pin %d", OCF_SERVO_IN_PIN);
    if (!servoOut.attach(OCF_SERVO_OUT_PIN, OCF_SERVO_OUT_CHANNEL))
        log_e("Failed to attach servo out on pin %d", OCF_SERVO_OUT_PIN);
    initialized = true;
    armedIn = true;
    armedOut = true;
    // Both servos are driving now: move them to LOCKED and park them there.
    disableServo(OCFDirection::BOTH);
}

void OCFLock::moveServoIn(int angle){
    if (!armedIn){
        log_d("Servo in parked, ignoring move to %d", angle);
        return;
    }
    log_d("Moving servo in to %d", angle);
    servoIn.write(angle);
}
void OCFLock::moveServoOut(int angle){
    if (!armedOut){
        log_d("Servo out parked, ignoring move to %d", angle);
        return;
    }
    log_d("Moving servo out to %d", angle);
    servoOut.write(angle);
}
void OCFLock::enableServo(OCFDirection direction){
    if (direction == OCFDirection::BOTH)
    {
        enableServo(OCFDirection::IN);
        enableServo(OCFDirection::OUT);
        return;
    }
    if (!initialized) init();

    if (direction == OCFDirection::IN)
    {
        if (armedIn) return; // already driving, re-attaching would only reconfigure the timer
        log_d("Enable servo in and lock it");
        // Re-binds the pin to the channel this instance already owns. No allocation.
        servoIn.attach(OCF_SERVO_IN_PIN, OCF_SERVO_IN_CHANNEL);
        armedIn = true;
        lock(OCFDirection::IN);
    }else if (direction == OCFDirection::OUT)
    {
        if (armedOut) return;
        log_d("Enable servo out and lock it");
        servoOut.attach(OCF_SERVO_OUT_PIN, OCF_SERVO_OUT_CHANNEL);
        armedOut = true;
        lock(OCFDirection::OUT);
    }
}
void OCFLock::disableServo(OCFDirection direction){
    if (direction == OCFDirection::BOTH)
    {
        if (!armedIn && !armedOut) return;
        lock(OCFDirection::BOTH);
        delay(OCF_SERVO_TRAVEL_MS); // one wait covers both servos
        park(OCFDirection::IN);
        park(OCFDirection::OUT);
        return;
    }
    if (direction == OCFDirection::IN && !armedIn) return;
    if (direction == OCFDirection::OUT && !armedOut) return;
    lock(direction);
    delay(OCF_SERVO_TRAVEL_MS); // the servo needs to actually get there before we cut the pulses
    park(direction);
}
void OCFLock::park(OCFDirection direction){
    if (direction == OCFDirection::IN)
    {
        log_d("Park servo in");
        servoIn.detach(); // ledcDetachPin only, the channel stays ours
        pinMode(OCF_SERVO_IN_PIN, OUTPUT);
        digitalWrite(OCF_SERVO_IN_PIN, LOW); // hold the signal line quiet instead of floating
        armedIn = false;
    }else if (direction == OCFDirection::OUT)
    {
        log_d("Park servo out");
        servoOut.detach();
        pinMode(OCF_SERVO_OUT_PIN, OUTPUT);
        digitalWrite(OCF_SERVO_OUT_PIN, LOW);
        armedOut = false;
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
        unlock(OCFDirection::IN);
        unlock(OCFDirection::OUT);
    }
}

bool OCFLock::isLockedOut(){ return lockedOut;}
bool OCFLock::isLockedIn(){ return lockedIn;}
bool OCFLock::isArmedIn(){ return armedIn;}
bool OCFLock::isArmedOut(){ return armedOut;}
