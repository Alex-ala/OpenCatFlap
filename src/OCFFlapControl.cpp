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
        pinMode(OCF_MOTION_INSIDE_PIN, INPUT);
        pinMode(OCF_MOTION_OUTSIDE_PIN, INPUT);
        pinMode(OCF_FLAPIR_OUTSIDE_PIN, INPUT);
        setLockState(Direction::BOTH, State::LOCKED);
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
            flapState.state_lock_in = state;
            if(state == State::UNLOCKED){
                moveServo(direction, OCF_SERVO_IN_UNLOCKED);
            }else{
                moveServo(direction, OCF_SERVO_IN_LOCKED);
            }
        }else if(direction == Direction::OUT){
            flapState.state_lock_out = state;
            if(state == State::UNLOCKED){
                moveServo(direction, OCF_SERVO_OUT_UNLOCKED);
            }else{
                moveServo(direction, OCF_SERVO_OUT_LOCKED);
            } 
        }else if(direction == Direction::BOTH){
            flapState.state_lock_out = state;
            flapState.state_lock_in = state;
            if(state == State::UNLOCKED){
                moveServo(direction, OCF_SERVO_OUT_UNLOCKED);
                moveServo(direction, OCF_SERVO_IN_UNLOCKED);
            }else{
                moveServo(direction, OCF_SERVO_OUT_LOCKED);
                moveServo(direction, OCF_SERVO_IN_LOCKED);
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
            flapState.flap_opened = Direction::NONE;
            flapState.active = false;
            flapState.active_cat = 0;
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
        int motion_inside = analogRead(OCF_MOTION_INSIDE_PIN);
        int motion_outside = analogRead(OCF_MOTION_OUTSIDE_PIN);
        if (motion_inside > OCF_MOTION_THRESHOLD || motion_outside > OCF_MOTION_THRESHOLD){
            flapState.last_activity = millis();
        }
        if (count_motion_inside > INT_MAX - OCF_MOTION_DELAY) count_motion_inside = OCF_MOTION_DELAY;
        if (count_motion_outside > INT_MAX - OCF_MOTION_DELAY) count_motion_outside = OCF_MOTION_DELAY;
        if (motion_inside > OCF_MOTION_THRESHOLD) count_motion_inside++;
        if (motion_outside > OCF_MOTION_THRESHOLD) count_motion_outside++;

        if (count_motion_inside > OCF_MOTION_DELAY  && motion_inside > OCF_MOTION_THRESHOLD && count_motion_outside > OCF_MOTION_DELAY && motion_outside == OCF_MOTION_THRESHOLD) return Direction::BOTH;
        if (count_motion_inside > OCF_MOTION_DELAY && motion_inside > OCF_MOTION_THRESHOLD) return Direction::OUT;
        if (count_motion_outside > OCF_MOTION_DELAY && motion_outside > OCF_MOTION_THRESHOLD) return Direction::IN;
        return Direction::NONE;
    }

    void closeAutomatically(Direction d){
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
            flapState.active = false;
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

    void detectMovementDirection(){
        int flap_sensor_in = analogRead(OCF_FLAPIR_INSIDE_PIN);
        int flap_sensor_out = analogRead(OCF_FLAPIR_OUTSIDE_PIN);
        if (flap_sensor_in > 2000) flapState.flap_opened = Direction::IN;
        if (flap_sensor_out > 2000) flapState.flap_opened = Direction::OUT;
    }
    
    void loop(void* parameter){
        unsigned long previous_millis = millis();
        unsigned long current_millis = 0;
        bool previously_active = false;
        while(true){
            current_millis = millis();
            if (previous_millis == current_millis){
                vTaskDelay(1);
                continue;
            }
            previous_millis = current_millis;
            Direction d = detectMotion();
            previously_active = flapState.active;
            closeAutomatically(d);
            if (flapState.active != previously_active && ! flapState.active){
                log_d("A cat went %d", flapState.flap_opened);
                flapState.flap_opened = Direction::NONE;
            }
            if(d == Direction::NONE){
                vTaskDelay(1);
                continue;
            }
            flapState.active = true;
            if (d == Direction::IN){
                if (flapState.allow_in) {
                    flapState.last_change_in = millis();
                    if(flapState.state_lock_in == State::LOCKED) setLockState(d, State::UNLOCKED);
                }
            }
            if (d == Direction::OUT){
                if (flapState.allow_out) {
                    flapState.last_change_out = millis();
                    if (flapState.state_lock_out == State::LOCKED) setLockState(d, State::UNLOCKED);
                }
            }
            if (flapState.active) detectMovementDirection();
            vTaskDelay(1);
        }
    }
}