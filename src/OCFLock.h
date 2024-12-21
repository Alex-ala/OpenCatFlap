#ifndef __OCFLock_H__
#define __OCFLock_H__

#include <OCFHelper.h>
#include <Servo.h>
#include <definitions.h>

class OCFLock
{
private:
    inline static bool lockedIn;
    inline static bool lockedOut;
    inline static bool armedIn;
    inline static bool armedOut;

    inline static Servo servoIn;
    inline static Servo servoOut;

    

public:
    static void moveServoIn(int angle);
    static void moveServoOut(int angle);
    static const char *toString;
    static void enableServo(OCFDirection direction);
    static void disableServo(OCFDirection direction);
    static void lock(OCFDirection direction);
    static void unlock(OCFDirection direction);
    static bool isLockedOut();
    static bool isLockedIn();
    static bool isArmedIn();
    static bool isArmedOut();
    static void init();
};

#endif
