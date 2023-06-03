#ifndef __OCFFLAPCONTROL_H__
#define __OCFFLAPCONTROL_H__

#include <Arduino.h>
#include <Servo.h>
#include <definitions.h>

namespace OCFFlapControl {
    enum Direction { IN, OUT };
    enum State { LOCKED, UNLOCKED };
    Servo servo_in;
    Servo servo_out;

    void moveServo(Direction direction, int angle);
    void setLockState(Direction direction, State state);
};
#endif // __OCFFLAPCONTROL_H__