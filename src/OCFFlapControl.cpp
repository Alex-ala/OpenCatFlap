#include <OCFFlapControl.h>

namespace OCFFlapControl {
    enum Direction { IN, OUT };
    enum State { LOCKED, UNLOCKED };
    Servo servo_in;
    Servo servo_out;

    void moveServo(Direction direction, int angle);
    void setLockState(Direction direction, State state);
    bool detectMotion(int timeout);
    
    void loop(void* parameter);
}