#ifndef __OCFFLAPCONTROL_H__
#define __OCFFLAPCONTROL_H__

#include <Arduino.h>
#include <Servo.h>
#include <definitions.h>

namespace OCFFlapControl {
    enum Direction { IN, OUT, NONE };
    enum State { LOCKED, UNLOCKED };
    struct FlapState {
        State state_lock_in;
        State state_lock_out;
        bool allow_out;
        bool allow_in;
        uint64_t last_activity;
        uint64_t last_change_in;
        uint64_t last_change_out;
    };
    Servo servo_in;
    Servo servo_out;
    FlapState flapState;

    void init();
    void moveServo(Direction direction, int angle);
    void setLockState(Direction direction, State state);
    Direction detectMotion(int timeout);
};
#endif // __OCFFLAPCONTROL_H__