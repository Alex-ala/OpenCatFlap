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
    if (!servoInside.attach(OCF_SERVO_INSIDE_PIN, OCF_SERVO_INSIDE_CHANNEL))
        log_e("Failed to attach servo inside on pin %d", OCF_SERVO_INSIDE_PIN);
    if (!servoOutside.attach(OCF_SERVO_OUTSIDE_PIN, OCF_SERVO_OUTSIDE_CHANNEL))
        log_e("Failed to attach servo outside on pin %d", OCF_SERVO_OUTSIDE_PIN);
    initialized = true;
    armedInside = true;
    armedOutside = true;
    // Both servos are driving now: move them to LOCKED and park them there.
    disableServo(OCFDirection::BOTH);
}

void OCFLock::moveServoInside(int angle){
    if (!armedInside){
        log_d("Servo inside parked, ignoring move to %d", angle);
        return;
    }
    log_d("Moving servo inside to %d", angle);
    servoInside.write(angle);
}
void OCFLock::moveServoOutside(int angle){
    if (!armedOutside){
        log_d("Servo outside parked, ignoring move to %d", angle);
        return;
    }
    log_d("Moving servo outside to %d", angle);
    servoOutside.write(angle);
}
void OCFLock::enableServo(OCFDirection direction){
    if (direction == OCFDirection::BOTH)
    {
        enableServo(OCFDirection::INWARD);
        enableServo(OCFDirection::OUTWARD);
        return;
    }
    if (!initialized) init();

    if (direction == OCFDirection::INWARD)
    {
        if (armedInside) return; // already driving, re-attaching would only reconfigure the timer
        log_d("Enable servo inside and lock it");
        // Re-binds the pin to the channel this instance already owns. No allocation.
        servoInside.attach(OCF_SERVO_INSIDE_PIN, OCF_SERVO_INSIDE_CHANNEL);
        armedInside = true;
        lock(OCFDirection::INWARD);
    }else if (direction == OCFDirection::OUTWARD)
    {
        if (armedOutside) return;
        log_d("Enable servo outside and lock it");
        servoOutside.attach(OCF_SERVO_OUTSIDE_PIN, OCF_SERVO_OUTSIDE_CHANNEL);
        armedOutside = true;
        lock(OCFDirection::OUTWARD);
    }
}
void OCFLock::disableServo(OCFDirection direction){
    if (direction == OCFDirection::BOTH)
    {
        if (!armedInside && !armedOutside) return;
        lock(OCFDirection::BOTH);
        delay(OCF_SERVO_TRAVEL_MS); // one wait covers both servos
        park(OCFDirection::INWARD);
        park(OCFDirection::OUTWARD);
        return;
    }
    if (direction == OCFDirection::INWARD && !armedInside) return;
    if (direction == OCFDirection::OUTWARD && !armedOutside) return;
    lock(direction);
    delay(OCF_SERVO_TRAVEL_MS); // the servo needs to actually get there before we cut the pulses
    park(direction);
}
void OCFLock::park(OCFDirection direction){
    if (direction == OCFDirection::INWARD)
    {
        log_d("Park servo inside");
        servoInside.detach(); // ledcDetachPin only, the channel stays ours
        pinMode(OCF_SERVO_INSIDE_PIN, OUTPUT);
        digitalWrite(OCF_SERVO_INSIDE_PIN, LOW); // hold the signal line quiet instead of floating
        armedInside = false;
    }else if (direction == OCFDirection::OUTWARD)
    {
        log_d("Park servo outside");
        servoOutside.detach();
        pinMode(OCF_SERVO_OUTSIDE_PIN, OUTPUT);
        digitalWrite(OCF_SERVO_OUTSIDE_PIN, LOW);
        armedOutside = false;
    }
}
void OCFLock::lock(OCFDirection direction){
    if (direction == OCFDirection::INWARD)
    {
        log_d("Lock inward");
        moveServoInside(OCF_SERVO_INSIDE_LOCKED);
        lockedInside = true;
    }else if (direction == OCFDirection::OUTWARD)
    {
        log_d("Lock outward");
        moveServoOutside(OCF_SERVO_OUTSIDE_LOCKED);
        lockedOutside = true;
    }else if (direction == OCFDirection::BOTH)
    {
        lock(OCFDirection::INWARD);
        lock(OCFDirection::OUTWARD);
    }
}
void OCFLock::unlock(OCFDirection direction){
    if (direction == OCFDirection::INWARD)
    {
        log_d("Unlock inward");
        moveServoInside(OCF_SERVO_INSIDE_UNLOCKED);
        lockedInside = false;
    }else if (direction == OCFDirection::OUTWARD)
    {
        log_d("Unlock outward");
        moveServoOutside(OCF_SERVO_OUTSIDE_UNLOCKED);
        lockedOutside = false;
    }else if (direction == OCFDirection::BOTH)
    {
        unlock(OCFDirection::INWARD);
        unlock(OCFDirection::OUTWARD);
    }
}

bool OCFLock::isLockedOutside(){ return lockedOutside;}
bool OCFLock::isLockedInside(){ return lockedInside;}
bool OCFLock::isArmedInside(){ return armedInside;}
bool OCFLock::isArmedOutside(){ return armedOutside;}
