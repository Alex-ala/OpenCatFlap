#ifndef __OCFLock_H__
#define __OCFLock_H__

#include <OCFHelper.h>
#include <Servo.h>
#include <definitions.h>

class OCFLock
{
private:
    inline static bool lockedInside;
    inline static bool lockedOutside;
    inline static bool armedInside;
    inline static bool armedOutside;
    inline static bool initialized {false};

    inline static Servo servoInside;
    inline static Servo servoOutside;

    // Stop driving a servo without releasing its LEDC channel.
    static void park(OCFDirection direction);

public:
    static void moveServoInside(int angle);
    static void moveServoOutside(int angle);
    static void enableServo(OCFDirection direction);
    static void disableServo(OCFDirection direction);
    static void lock(OCFDirection direction);
    static void unlock(OCFDirection direction);
    static bool isLockedOutside();
    static bool isLockedInside();
    static bool isArmedInside();
    static bool isArmedOutside();
    static void init();
};

#endif
