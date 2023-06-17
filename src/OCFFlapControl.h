#ifndef __OCFFLAPCONTROL_H__
#define __OCFFLAPCONTROL_H__

#include <Arduino.h>
#include <Servo.h>
#include <definitions.h>
#include <ArduinoJson.h>
#include <OCFFilesystem.h>
#include <OCFWifi.h>

enum OCFDirection { IN, OUT, NONE, BOTH };
enum OCFState { LOCKED, UNLOCKED };

namespace OCFFlapControl {
    struct FlapState {
        OCFState state_lock_in;
        OCFState state_lock_out;
        bool allow_out;
        bool allow_in;
        bool active;
        int active_cat;
        OCFDirection flap_opened;
        uint64_t last_activity;
        uint64_t last_change_in;
        uint64_t last_change_out;
        bool servosAttached;
    };
    extern Servo servo_in;
    extern Servo servo_out;
    extern FlapState flapState;
    extern int count_motion_inside;
    extern int count_motion_outside;

    void init();
    void deinit();
    void enableServos();
    void disableServos();
    void moveServo(OCFDirection direction, int angle);
    void setLockState(OCFDirection direction, OCFState state);
    void setAllowState(OCFDirection direction, bool allowed);
    void persistState();
    void loadState();
    OCFDirection detectMotion();
    void closeAutomatically(OCFDirection d);
    void loop(void* parameters);
    void getFlapStateJson(String& strOut);
    void detectMovementDirection();
};
#endif // __OCFFLAPCONTROL_H__