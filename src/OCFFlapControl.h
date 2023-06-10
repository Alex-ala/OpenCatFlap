#ifndef __OCFFLAPCONTROL_H__
#define __OCFFLAPCONTROL_H__

#include <Arduino.h>
#include <Servo.h>
#include <definitions.h>
#include <ArduinoJson.h>
#include <OCFFilesystem.h>

namespace OCFFlapControl {
    enum Direction { IN, OUT, NONE, BOTH };
    enum State { LOCKED, UNLOCKED };
    struct FlapState {
        State state_lock_in;
        State state_lock_out;
        bool allow_out;
        bool allow_in;
        uint64_t last_activity;
        uint64_t last_change_in;
        uint64_t last_change_out;
        bool servosAttached;
    };
    extern Servo servo_in;
    extern Servo servo_out;
    extern FlapState flapState;

    void init();
    void deinit();
    void enableServos();
    void disableServos();
    void moveServo(Direction direction, int angle);
    void setLockState(Direction direction, State state);
    void setAllowState(Direction direction, bool allowed);
    void persistState();
    void loadState();
    Direction detectMotion();
    void closeAutomatically();
    void loop(void* parameters);
};
#endif // __OCFFLAPCONTROL_H__