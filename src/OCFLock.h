#ifndef __OCFLock_H__
#define __OCFLock_H__

#include <OCFHelper.h>
#include <Servo.h>
#include <definitions.h>


class OCFLock {
    private:
        static bool lockedIn;
        static bool lockedOut;
        static bool armedIn;
        static bool armedOut;

        static Servo servoIn;
        static Servo servoOut;

        static void moveServoIn(int angle);
        static void moveServoOut(int angle);

    public:
        static const char* toString;
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