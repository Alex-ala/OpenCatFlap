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
        setLockState(OCFDirection::BOTH, OCFState::LOCKED);
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

    void moveServo(OCFDirection OCFDirection, int angle){
        if(!flapState.servosAttached) enableServos();
        switch(OCFDirection){
            case OCFDirection::IN:
                log_d("Movin servo IN to %d", angle);
                servo_in.write(angle);
                break;
            case OCFDirection::OUT:
                log_d("Movin servo OUT to %d", angle);
                servo_out.write(angle);
                break;
        }
    }
    void setLockState(OCFDirection OCFDirection, OCFState state){
        if(OCFDirection == OCFDirection::IN){
            flapState.state_lock_in = state;
            if(state == OCFState::UNLOCKED){
                moveServo(OCFDirection, OCF_SERVO_IN_UNLOCKED);
            }else{
                moveServo(OCFDirection, OCF_SERVO_IN_LOCKED);
            }
        }else if(OCFDirection == OCFDirection::OUT){
            flapState.state_lock_out = state;
            if(state == OCFState::UNLOCKED){
                moveServo(OCFDirection, OCF_SERVO_OUT_UNLOCKED);
            }else{
                moveServo(OCFDirection, OCF_SERVO_OUT_LOCKED);
            } 
        }else if(OCFDirection == OCFDirection::BOTH){
            flapState.state_lock_out = state;
            flapState.state_lock_in = state;
            if(state == OCFState::UNLOCKED){
                moveServo(OCFDirection, OCF_SERVO_OUT_UNLOCKED);
                moveServo(OCFDirection, OCF_SERVO_IN_UNLOCKED);
            }else{
                moveServo(OCFDirection, OCF_SERVO_OUT_LOCKED);
                moveServo(OCFDirection, OCF_SERVO_IN_LOCKED);
            } 
        }
    }
    void setAllowState(OCFDirection OCFDirection, bool allowed){
        setLockState(OCFDirection, OCFState::LOCKED);
        if (OCFDirection == OCFDirection::IN || OCFDirection == OCFDirection::BOTH){
            flapState.allow_in = allowed;
        }
        if (OCFDirection == OCFDirection::OUT || OCFDirection == OCFDirection::BOTH)
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
            flapState.last_activity = 0;
            flapState.last_change_in = 0;
            flapState.last_change_out = 0;
            flapState.allow_in = false;
            flapState.allow_out = false;
            flapState.flap_opened = OCFDirection::NONE;
            flapState.active = false;
            flapState.active_cat = 0;
            flapState.state_lock_in = OCFState::UNLOCKED;
            flapState.state_lock_out = OCFState::UNLOCKED;
            return;
        }
        if (doc.containsKey("allow_out")) OCFFlapControl::flapState.allow_out = doc["allow_out"].as<bool>();
        if (doc.containsKey("allow_in")) OCFFlapControl::flapState.allow_in = doc["allow_in"].as<bool>();
        doc.clear();
        log_d("Loaded flap state. (%d, %d)", flapState.allow_in, flapState.allow_out);
    }
    OCFDirection detectMotion(){
        int motion_inside = analogRead(OCF_MOTION_INSIDE_PIN);
        int motion_outside = analogRead(OCF_MOTION_OUTSIDE_PIN);
        if (motion_inside > OCF_MOTION_THRESHOLD || motion_outside > OCF_MOTION_THRESHOLD){
            flapState.last_activity = OCFWifi::timeClient.getEpochTime();
        }
        if (count_motion_inside > INT_MAX - OCF_MOTION_DELAY) count_motion_inside = OCF_MOTION_DELAY;
        if (count_motion_outside > INT_MAX - OCF_MOTION_DELAY) count_motion_outside = OCF_MOTION_DELAY;
        if (motion_inside > OCF_MOTION_THRESHOLD) count_motion_inside++;
        if (motion_outside > OCF_MOTION_THRESHOLD) count_motion_outside++;

        if (count_motion_inside > OCF_MOTION_DELAY  && motion_inside > OCF_MOTION_THRESHOLD && count_motion_outside > OCF_MOTION_DELAY && motion_outside == OCF_MOTION_THRESHOLD) return OCFDirection::BOTH;
        if (count_motion_inside > OCF_MOTION_DELAY && motion_inside > OCF_MOTION_THRESHOLD) return OCFDirection::OUT;
        if (count_motion_outside > OCF_MOTION_DELAY && motion_outside > OCF_MOTION_THRESHOLD) return OCFDirection::IN;
        return OCFDirection::NONE;
    }

    void closeAutomatically(OCFDirection d){
        unsigned long time = OCFWifi::timeClient.getEpochTime();
        unsigned long diff = 0;
        diff = time - flapState.last_change_in;
        if(diff > OCF_CLOSE_AFTER_S && flapState.state_lock_in == OCFState::UNLOCKED){
            log_d("Locking IN due to inactivity (%d > %d)", time, flapState.last_change_in + OCF_CLOSE_AFTER_S );
            setLockState(OCFDirection::IN, OCFState::LOCKED);
        }
        diff = time - flapState.last_change_out;
        if(diff > OCF_CLOSE_AFTER_S && flapState.state_lock_out == OCFState::UNLOCKED){
            log_d("Locking OUT due to inactivity (%d > %d)", time, flapState.last_change_out + OCF_CLOSE_AFTER_S );
            setLockState(OCFDirection::OUT, OCFState::LOCKED);
        }
        diff = time - flapState.last_activity;
        if(diff > OCF_CLOSE_AFTER_S) {
            count_motion_inside = 0;
            count_motion_outside = 0;
            flapState.active = false;
        }
        if(diff > OCF_DISABLE_SERVOS_AFTER_S && flapState.servosAttached){
            setLockState(OCFDirection::IN, OCFState::LOCKED);
            setLockState(OCFDirection::OUT, OCFState::LOCKED);
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
        doc["memory_usage"] = esp_get_free_heap_size();
        serializeJsonPretty(doc, outStr);
    }

    void detectMovementOCFDirection(){
        int flap_sensor_in = analogRead(OCF_FLAPIR_INSIDE_PIN);
        int flap_sensor_out = analogRead(OCF_FLAPIR_OUTSIDE_PIN);
        if (flap_sensor_in > 2000) flapState.flap_opened = OCFDirection::IN;
        if (flap_sensor_out > 2000) flapState.flap_opened = OCFDirection::OUT;
    }
    
    void loop(void* parameter){
        unsigned long previous_millis = millis();
        unsigned long current_millis = 0;
        bool previously_active = false;
        while(true){
            // Run max every ms
            current_millis = millis();
            if (previous_millis == current_millis){
                vTaskDelay(1);
                continue;
            }
            previous_millis = current_millis;
            // Detect activity
            OCFDirection d = detectMotion();
            previously_active = flapState.active;
            // Handle no activity
            closeAutomatically(d);
            if (flapState.active != previously_active && ! flapState.active){
                log_d("A cat went %d", flapState.flap_opened);
                flapState.flap_opened = OCFDirection::NONE;
            }
            if(d == OCFDirection::NONE){
                vTaskDelay(1);
                continue;
            }
            // Handle activity
            flapState.active = true;
            if (d == OCFDirection::IN){
                if (flapState.allow_in) {
                    flapState.last_change_in = OCFWifi::timeClient.getEpochTime();
                    if(flapState.state_lock_in == OCFState::LOCKED) setLockState(d, OCFState::UNLOCKED);
                }
            }
            if (d == OCFDirection::OUT){
                if (flapState.allow_out) {
                    flapState.last_change_out = OCFWifi::timeClient.getEpochTime();
                    if (flapState.state_lock_out == OCFState::LOCKED) setLockState(d, OCFState::UNLOCKED);
                }
            }
            if (flapState.active) detectMovementOCFDirection();
            vTaskDelay(1);
        }
    }
}