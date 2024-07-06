#include <OCFFlapControl.h>

const char* DirectionString(OCFDirection dir){
    switch(dir){
        case OCFDirection::IN: return "in";
        case OCFDirection::OUT: return "out";
        case OCFDirection::NONE: return "none";
        case OCFDirection::BOTH: return "both";
        default: return "error";
    }
}
const char* StateString(OCFState state){
    switch(state){
        case OCFState::LOCKED: return "locked";
        case OCFState::UNLOCKED: return "unlocked";
        default: return "error";
    }
}

Servo OCFFlapControl::servo_in;
Servo OCFFlapControl::servo_out;
FlapState OCFFlapControl::flapState;
int OCFFlapControl::count_motion_inside = 0;
int OCFFlapControl::count_motion_outside = 0;

void OCFFlapControl::init(){
    log_d("Initializing Flapcontrol");
    pinMode(OCF_SERVO_IN_PIN, OUTPUT);
    pinMode(OCF_SERVO_OUT_PIN, OUTPUT);
    pinMode(OCF_MOTION_INSIDE_PIN, INPUT_PULLUP);
    pinMode(OCF_MOTION_OUTSIDE_PIN, INPUT_PULLUP);
    pinMode(OCF_FLAPIR_OUTSIDE_PIN, INPUT);
    pinMode(OCF_FLAPIR_INSIDE_PIN, INPUT);
    pinMode(OCF_TUNNELIR_INSIDE_PIN, INPUT);
    pinMode(OCF_TUNNELIR_OUTSIDE_PIN, INPUT);
    Serial1.begin(9600,SERIAL_8N2,OCF_RFID_INSIDE_RX,D12);
    loadState();
    enableServos();
    setLockState(OCFDirection::BOTH, OCFState::LOCKED);
    count_motion_inside = 0;
    count_motion_outside = 0;
}

void OCFFlapControl::enableServos(){
    log_d("Enabling servos");
    servo_in.attach(OCF_SERVO_IN_PIN);
    servo_out.attach(OCF_SERVO_OUT_PIN);
    flapState.servosAttached = true;
}

void OCFFlapControl::disableServos(){
    log_d("Disabling servos due to inactivity");
    servo_in.detach();
    servo_out.detach();
    flapState.servosAttached = false;
}

void OCFFlapControl::moveServo(OCFDirection direction, int angle){
    if(!flapState.servosAttached) enableServos();
    switch(direction){
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
void OCFFlapControl::setLockState(OCFDirection direction, OCFState state){
    if(direction == OCFDirection::IN){
        flapState.state_lock_in = state;
        if(state == OCFState::UNLOCKED){
            moveServo(direction, OCF_SERVO_IN_UNLOCKED);
        }else{
            moveServo(direction, OCF_SERVO_IN_LOCKED);
        }
    }else if(direction == OCFDirection::OUT){
        flapState.state_lock_out = state;
        if(state == OCFState::UNLOCKED){
            moveServo(direction, OCF_SERVO_OUT_UNLOCKED);
        }else{
            moveServo(direction, OCF_SERVO_OUT_LOCKED);
        } 
    }else if(direction == OCFDirection::BOTH){
        flapState.state_lock_out = state;
        flapState.state_lock_in = state;
        if(state == OCFState::UNLOCKED){
            moveServo(direction, OCF_SERVO_OUT_UNLOCKED);
            moveServo(direction, OCF_SERVO_IN_UNLOCKED);
        }else{
            moveServo(direction, OCF_SERVO_OUT_LOCKED);
            moveServo(direction, OCF_SERVO_IN_LOCKED);
        } 
    }
    if (OCFMQTT::config.logActivity){
        String s = String("{\"direction\": \"") + DirectionString(direction) + "\", \"state\": \"" + StateString(state) + "\"}";
        OCFMQTT::sendMessage("activity", s.c_str());
    }
}

void OCFFlapControl::setAllowState(OCFDirection direction, bool allowed){
    setLockState(direction, OCFState::LOCKED);
    if (direction == OCFDirection::IN || direction == OCFDirection::BOTH){
        flapState.allow_in = allowed;
    }
    if (direction == OCFDirection::OUT || direction == OCFDirection::BOTH)
    {
        flapState.allow_out = allowed;
    }    
    persistState();
    String str;
    getFlapStateJson(str);
    OCFMQTT::sendMessage("status", str.c_str());
}

void OCFFlapControl::persistState(){
    DynamicJsonDocument doc(OCF_MAX_JSON_SIZE);
    doc["allow_out"] = flapState.allow_out;
    doc["allow_in"] = flapState.allow_in;
    OCFFilesystem::writeJsonFile(OCF_PATHS_FLAP_STATE, doc);
    doc.clear();
}

void OCFFlapControl::loadState(){
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

OCFDirection OCFFlapControl::detectMotion(){
    bool motion_inside = digitalRead(OCF_MOTION_INSIDE_PIN) == LOW;
    bool motion_outside = digitalRead(OCF_MOTION_OUTSIDE_PIN) == LOW;
    if (motion_inside || motion_outside){
        flapState.last_activity = OCFWifi::getEpochTime();
    }
    if (count_motion_inside > INT_MAX - OCF_MOTION_DELAY) count_motion_inside = OCF_MOTION_DELAY;
    if (count_motion_outside > INT_MAX - OCF_MOTION_DELAY) count_motion_outside = OCF_MOTION_DELAY;
    if (motion_inside) count_motion_inside++;
    if (motion_outside) count_motion_outside++;

    if (count_motion_inside > OCF_MOTION_DELAY  && motion_inside && count_motion_outside > OCF_MOTION_DELAY && motion_outside) {
        return OCFDirection::BOTH;
    }
    if (count_motion_inside > OCF_MOTION_DELAY && motion_inside) {
        return OCFDirection::OUT;
    }
    if (count_motion_outside > OCF_MOTION_DELAY && motion_outside) {
        return OCFDirection::IN;
    }
    return OCFDirection::NONE;
}

void OCFFlapControl::closeAutomatically(OCFDirection d){
    unsigned long time = OCFWifi::getEpochTime();
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

void OCFFlapControl::getFlapStateJson(String& outStr){
    DynamicJsonDocument doc(OCF_MAX_JSON_SIZE);
    doc["allow_out"] = flapState.allow_out;
    doc["allow_in"] = flapState.allow_in;
    doc["state_lock_in"] = flapState.state_lock_in;
    doc["state_lock_out"] = flapState.state_lock_out;
    doc["flap_opened"] = DirectionString(flapState.flap_opened);
    doc["servosAttached"] = flapState.servosAttached;
    doc["last_activity"] = flapState.last_activity;
    doc["last_change_in"] = flapState.last_change_in;
    doc["last_change_out"] = flapState.last_change_out;
    // doc["memory_usage"] = esp_get_free_heap_size();
    doc["time"] = OCFWifi::getEpochTime();
    serializeJsonPretty(doc, outStr);
}

void OCFFlapControl::detectMovementOCFDirection(){
    int flap_sensor_in = analogRead(OCF_FLAPIR_INSIDE_PIN);
    int flap_sensor_out = analogRead(OCF_FLAPIR_OUTSIDE_PIN);
    if (flap_sensor_in > OCF_FLAPIR_THRESHOLD) flapState.flap_opened = OCFDirection::IN;
    if (flap_sensor_out > OCF_FLAPIR_THRESHOLD) flapState.flap_opened = OCFDirection::OUT;
}

void OCFFlapControl::loop(void* parameter){
    unsigned long previous_millis = millis();
    unsigned long current_millis = 0;
    bool previously_active = false;
    unsigned long millis_status_update = 0;
    bool reading_rfid = false;
    char input[1];
    char tag[29];
    while(true){
        // Run max every ms
        current_millis = millis();
        if (previous_millis == current_millis){
            vTaskDelay(1);
            continue;
        }
        previous_millis = current_millis;
        // Detect activity
        OCFDirection motion_direction = flap.detectMotion();
        if (motion_direction != OCFDirection::NONE) previously_active = flap.flapState.active;
        // Handle no activity
        flap.closeAutomatically(motion_direction);
        if (previously_active && ! flap.flapState.active){
            previously_active = false;
            OCFMQTT::sendMessage("log", DirectionString(flap.flapState.flap_opened));
            flap.flapState.flap_opened = OCFDirection::NONE;
            reading_rfid = false;
            input[0] = 0x0;
            for (int x = 0; x < 29; x++) tag[x] = 0x0;
            Serial1.read();
        }
        if(motion_direction == OCFDirection::NONE) Serial1.read();
        if(motion_direction != OCFDirection::NONE){
            // Handle activity
            flap.flapState.active = true;
            // Read RFID
            if (!reading_rfid){
                input[0] = 0x0;
                for (int x = 0; x < 29; x++) tag[x] = 0x0;
            }
            if (Serial1.available() >= 1 && !reading_rfid){
                while(Serial1.available()){
                    Serial1.read(input, 1);
                    if (input[0] == 0x02){
                        reading_rfid = true;
                        break;
                    }
                }
            } else if (reading_rfid && Serial1.available() >= 29){
                Serial1.readBytesUntil(0x03,tag,29);
                char tag_country_bytes[5]; 
                char tag_id_bytes[11];
                for (int i = 3; i>=0; i--){
                    tag_country_bytes[3-i] = tag[10+i];
                }
                tag_country_bytes[4] = 0x0;
                for (int i = 9; i>=0; i--){
                    tag_id_bytes[9-i] = tag[i];
                }
                tag_id_bytes[10] = 0x0;
                long long tag_id = strtoll(tag_id_bytes, NULL, 16);
                long tag_country = strtol(tag_country_bytes, NULL, 16);
                OCFMQTT::sendMessage("rfid", (String(tag_country) + (tag_id)).c_str());
                Serial1.read();
                input[0] = 0x0;
                for (int x = 0; x < 29; x++) tag[x] = 0x0;
                reading_rfid = false;
            }
            // Open flap
            if (motion_direction == OCFDirection::IN){
                if (flap.flapState.allow_in) {
                    flap.flapState.last_change_in = OCFWifi::getEpochTime();
                    if(flap.flapState.state_lock_in == OCFState::LOCKED) flap.setLockState(motion_direction, OCFState::UNLOCKED);
                }
            }
            if (motion_direction == OCFDirection::OUT){
                if (flap.flapState.allow_out) {
                    flap.flapState.last_change_out = OCFWifi::getEpochTime();
                    if (flap.flapState.state_lock_out == OCFState::LOCKED) flap.setLockState(motion_direction, OCFState::UNLOCKED);
                }
            }
            if (flap.flapState.active) flap.detectMovementOCFDirection();
        }
        // Update Status to MQTT
        if (millis_status_update + 6000 < current_millis){
            String str;
            flap.getFlapStateJson(str);
            OCFMQTT::sendMessage("status", str.c_str());
            millis_status_update = current_millis;
        }
        vTaskDelay(1);
    }
}
