#include <OCFFlapControl.h>

namespace OCFFlapControl {
    void init(){
        servo_in.attach(OCF_SERVO_IN_PIN);
        servo_out.attach(OCF_SERVO_OUT_PIN);
    }

    void moveServo(Direction direction, int angle){
        switch(direction){
            case Direction::IN:
                servo_in.write(angle);
                break;
            case Direction::OUT:
                servo_out.write(angle);
        }
    }
    void setLockState(Direction direction, State state){
        if(direction == Direction::IN){
            flapState.last_change_in = esp_timer_get_time();
            flapState.state_lock_in = state;
            if(state == State::UNLOCKED){
                moveServo(direction, OCF_SERVO_OUT_UNLOCKED);
            }else{
                moveServo(direction, OCF_SERVO_OUT_LOCKED);
            }
        }else{
            flapState.last_change_out = esp_timer_get_time();
            flapState.state_lock_out = state;
            if(state == State::UNLOCKED){
                moveServo(direction, OCF_SERVO_OUT_UNLOCKED);
            }else{
                moveServo(direction, OCF_SERVO_OUT_LOCKED);
            } 
        }
    }
    Direction detectMotion(int timeout){
        delayMicroseconds(timeout);
        return Direction::IN;
    }
    
    void loop(void* parameter){
        while(true){
            Direction d = detectMotion(100);
            if(d == Direction::NONE){
                if(esp_timer_get_time() > (flapState.last_change_in + 2000) && 
                   flapState.state_lock_in == State::UNLOCKED){
                    setLockState(Direction::IN, State::LOCKED);
                }               
                if(esp_timer_get_time() > (flapState.last_change_out + 2000) && 
                   flapState.state_lock_out == State::UNLOCKED){
                    setLockState(Direction::OUT, State::LOCKED);
                }
                continue;
            }
            setLockState(d, State::UNLOCKED);

        }
    }
}