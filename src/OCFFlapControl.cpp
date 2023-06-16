#include <OCFFlapControl.h>

namespace OCFFlapControl {
    Servo servo_in;
    Servo servo_out;
    FlapState flapState;
    int count_motion_inside = 0;
    int count_motion_outside = 0;

    void init(){
        log_d("Initializing Flapcontrol");
        loadState();
        enableServos();
        pinMode(OCF_MONTION_INSIDE_PIN, INPUT);
        pinMode(OCF_MONTION_OUTSIDE_PIN, INPUT);
        pinMode(OCF_FLAPIR_OUTSIDE_PIN, INPUT);
        count_motion_inside = 0;
        count_motion_outside = 0;
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
        persistState();    
    }
    void persistState(){
        DynamicJsonDocument doc(OCF_MAX_JSON_SIZE);
        doc["allow_out"] = flapState.allow_out;
        doc["allow_in"] = flapState.allow_in;
        OCFFilesystem::writeJsonFile(OCF_PATHS_FLAP_STATE, doc);
        doc.clear();
    }
    void loadState(){
        StaticJsonDocument<OCF_MAX_JSON_SIZE> doc;
        bool loaded = OCFFilesystem::readJsonFile(OCF_PATHS_FLAP_STATE, doc);
        if (!loaded){
            log_d("Failed to load state");
            flapState.last_activity = millis();
            flapState.last_change_in = millis();
            flapState.last_change_out = millis();
            flapState.allow_in = false;
            flapState.allow_out = false;
            flapState.state_lock_in = State::UNLOCKED;
            flapState.state_lock_out = State::UNLOCKED;
            return;
        }
        if (doc.containsKey("allow_out")) OCFFlapControl::flapState.allow_out = doc["allow_out"].as<bool>();
        if (doc.containsKey("allow_in")) OCFFlapControl::flapState.allow_in = doc["allow_in"].as<bool>();
        doc.clear();
        log_d("Loaded flap state. (%d, %d)", flapState.allow_in, flapState.allow_out);
    }
    Direction detectMotion(){
        int motion_inside = digitalRead(OCF_MONTION_INSIDE_PIN);
        int motion_outside = digitalRead(OCF_MONTION_OUTSIDE_PIN);
        if (motion_inside == 1 || motion_outside == 1){
            flapState.last_activity = millis();
        }
        if (count_motion_inside > INT_MAX - 100) count_motion_inside = 100;
        if (count_motion_outside > INT_MAX - 100) count_motion_outside = 100;
        if (motion_inside == 1) count_motion_inside++;
        if (motion_outside == 1) count_motion_outside++;

        if (count_motion_inside > 100  && motion_inside == 1 && count_motion_outside > 100 && motion_outside == 1) return Direction::BOTH;
        if (count_motion_inside > 100 && motion_inside == 1) return Direction::OUT;
        if (count_motion_outside > 100 && motion_outside == 1) return Direction::IN;
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
        if(diff > OCF_CLOSE_AFTER_MS) {
            count_motion_inside = 0;
            count_motion_outside = 0;
        }
        if(diff > OCF_DISABLE_SERVOS_AFTER_MS && flapState.servosAttached){
            setLockState(Direction::IN, State::LOCKED);
            setLockState(Direction::OUT, State::LOCKED);
            disableServos();
        }
        
    }
    
    void getFlapStateJson(String& outStr){
        DynamicJsonDocument doc(OCF_MAX_JSON_SIZE);
        doc["allow_out"] = flapState.allow_out;
        doc["allow_in"] = flapState.allow_in;
        doc["state_lock_in"] = flapState.state_lock_in;
        doc["state_lock_out"] = flapState.state_lock_out;
        doc["servosAttached"] = flapState.servosAttached;
        doc["last_activity"] = flapState.last_activity;
        doc["last_change_in"] = flapState.last_change_in;
        doc["last_change_out"] = flapState.last_change_out;
        serializeJsonPretty(doc, outStr);
    }
    
    void loop(void* parameter){
        while(true){
            closeAutomatically();
            Direction d = detectMotion();
            if(d == Direction::NONE){
                vTaskDelay(1);
                continue;
            }else{
                if (d == Direction::IN && flapState.state_lock_in == State::LOCKED){
                    if (flapState.allow_in) setLockState(d, State::UNLOCKED);
                }
                if (d == Direction::OUT && flapState.state_lock_out == State::LOCKED){
                    if (flapState.allow_out) setLockState(d, State::UNLOCKED);
                }
            }
            vTaskDelay(1);
        }
    }
}