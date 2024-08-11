#ifndef __OCFLock_H__
#define __OCFLock_H__

#include <OCFHelper.h>


class OCFLock {
    private:
        static bool lockedIn;
        static bool lockedOut;
        static bool armedIn;
        static bool armedOut;

        static void moveServo(int pin, int angle);

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
    

}


#endif