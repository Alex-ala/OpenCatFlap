#ifndef __OCFFLAPCONTROL_H__
#define __OCFFLAPCONTROL_H__

#include <Arduino.h>
//#include <ESP32Servo.h>
#include <Servo.h>
#include <definitions.h>
#include <ArduinoJson.h>
#include <OCFFilesystem.h>
#include <OCFWifi.h>
#include <OCFMQTT.h>

/*  NEW IDEAS
std::queue<const char*> activityLog;
public void reportActiviy(const char* activity)
    Called by state machines
    Updates log queue and writes mqtt

Replace FlapState with OCFConfig:  
    allowIn, allowOut, cats


*/ 
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
class OCFFlapControl {
    private:
        static OCFFlapControl flap;
        static Servo servo_in;
        static Servo servo_out;
        static int count_motion_inside;
        static int count_motion_outside;
    public:
        static FlapState flapState;
        static void init();
        static void deinit();
        static void enableServos();
        static void disableServos();
        static void moveServo(OCFDirection direction, int angle);
        static void setLockState(OCFDirection direction, OCFState state);
        static void setAllowState(OCFDirection direction, bool allowed);
        static void persistState();
        static void loadState();
        static OCFDirection detectMotion();
        static void closeAutomatically(OCFDirection d);
        static void loop(void* parameters);
        static void getFlapStateJson(String& strOut);
        static void detectMovementOCFDirection();
        static void OCFFlapControl::setActiveState(bool active);
};
#endif // __OCFFLAPCONTROL_H__