#ifndef __OCFFLAPCONTROL_H__
#define __OCFFLAPCONTROL_H__

#include <Arduino.h>
#include <Servo.h>
#include <definitions.h>
#include <ArduinoJson.h>
#include <OCFFilesystem.h>
#include <OCFWifi.h>
#include <OCFMQTT.h>

enum OCFDirection { IN, OUT, NONE, BOTH };
enum OCFState { LOCKED, UNLOCKED };
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
const char* DirectionString(OCFDirection dir);
const char* StateString(OCFState state);
// TODO: Use proper getter/setters
class OCFFlapControl {
    private:
        static OCFFlapControl flap;
        OCFFlapControl() {};
        OCFFlapControl(const OCFFlapControl&);
        OCFFlapControl operator=(const OCFFlapControl&);
        Servo servo_in;
        Servo servo_out;
        int count_motion_inside;
        int count_motion_outside;
    public:
        FlapState flapState;
        static OCFFlapControl& getInstance();
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
        static void loop(void* parameters);
        void getFlapStateJson(String& strOut);
        void detectMovementOCFDirection();
};
#endif // __OCFFLAPCONTROL_H__