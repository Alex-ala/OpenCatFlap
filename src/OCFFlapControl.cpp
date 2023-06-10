#include <OCFFlapControl.h>

namespace OCFFlapControl {
    Servo servo_in;
    Servo servo_out;
    FlapState flapState;

    void init(){
        enableServos();
        pinMode(OCF_MONTION_INSIDE_PIN, INPUT);
        pinMode(OCF_MONTION_OUTSIDE_PIN, INPUT);
    }

    void enableServos(){
        log_d("Enabling servos");
        servo_in.attach(OCF_SERVO_IN_PIN);
        servo_out.attach(OCF_SERVO_OUT_PIN);
        flapState.servosAttached = true;
    }

    void disableServos(){
        log_d("Disabling servos due to inactivity");
        servo_in.detach();
        servo_out.detach();
        flapState.servosAttached = false;
    }

    void moveServo(Direction direction, int angle){
        if(!flapState.servosAttached) enableServos();
        switch(direction){
            case Direction::IN:
                log_d("Movin servo IN to %d", angle);
                servo_in.write(angle);
                break;
            case Direction::OUT:
                log_d("Movin servo OUT to %d", angle);
                servo_out.write(angle);
                break;
        }
    }
    void setLockState(Direction direction, State state){
        if(direction == Direction::IN){
            flapState.last_change_in = millis();
            flapState.state_lock_in = state;
            if(state == State::UNLOCKED){
                moveServo(direction, OCF_SERVO_IN_UNLOCKED);
            }else{
                moveServo(direction, OCF_SERVO_IN_LOCKED);
            }
        }else if(direction == Direction::OUT){
            flapState.last_change_out = millis();
            flapState.state_lock_out = state;
            if(state == State::UNLOCKED){
                moveServo(direction, OCF_SERVO_OUT_UNLOCKED);
            }else{
                moveServo(direction, OCF_SERVO_OUT_LOCKED);
            } 
        }
    }
    void setAllowState(Direction direction, bool allowed){
        setLockState(direction, State::LOCKED);
        if (direction == Direction::IN || direction == Direction::BOTH){
            flapState.allow_in = allowed;
        }
        if (direction == Direction::OUT || direction == Direction::BOTH)
        {
            flapState.allow_out = allowed;
        }        
    }
    Direction detectMotion(){
        int motion_inside = digitalRead(OCF_MONTION_INSIDE_PIN);
        int motion_outside = digitalRead(OCF_MONTION_OUTSIDE_PIN);
        if (motion_inside == 1 || motion_outside == 1){
            flapState.last_activity = millis();
        }
        if (motion_inside == 1 && motion_outside == 1) return Direction::BOTH;
        if (motion_inside == 1) return Direction::OUT;
        if (motion_outside == 1) return Direction::IN;
        return Direction::NONE;
    }

    void closeAutomatically(){
        unsigned long time = millis();
        unsigned long diff = 0;

        if (time >= flapState.last_change_in){
            diff = time - flapState.last_change_in;
        }else {
            diff = (ULONG_MAX - flapState.last_change_in) + time + 1;
        }
        if(diff > OCF_CLOSE_AFTER_MS && flapState.state_lock_in == State::UNLOCKED){
            log_d("Locking IN due to inactivity (%d > %d)", time, flapState.last_change_in + 2000 );
            setLockState(Direction::IN, State::LOCKED);
        }

        if (time >= flapState.last_change_out){
            diff = time - flapState.last_change_out;
        }else {
            diff = (ULONG_MAX - flapState.last_change_out) + time + 1;
        }          
        if(diff > OCF_CLOSE_AFTER_MS && flapState.state_lock_out == State::UNLOCKED){
            log_d("Locking OUT due to inactivity");
            setLockState(Direction::OUT, State::LOCKED);
        }

        if (time >= flapState.last_activity) {
            diff = time - flapState.last_activity;
        }else{
            diff = (ULONG_MAX - flapState.last_activity) + time + 1;
        }
        if(diff > OCF_DISABLE_SERVOS_AFTER_MS && flapState.servosAttached){
            setLockState(Direction::IN, State::LOCKED);
            setLockState(Direction::OUT, State::LOCKED);
            disableServos();
        }
        
    }
    
    void loop(void* parameter){
        while(true){
            closeAutomatically();
            Direction d = detectMotion();
            if(d == Direction::NONE){
                continue;
            }
            if (d == Direction::IN){
                if (flapState.allow_in) setLockState(d, State::UNLOCKED);
            }
            if (d == Direction::OUT){
                if (flapState.allow_out) setLockState(d, State::UNLOCKED);
            }
        }
    }
}