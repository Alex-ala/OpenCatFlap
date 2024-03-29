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
    pinMode(D2, OUTPUT);
    pinMode(A7, OUTPUT);
    Serial1.begin(9600,SERIAL_8N2,D9,D10);
    loadState();
    enableServos();
    pinMode(OCF_MOTION_INSIDE_PIN, INPUT);
    pinMode(OCF_MOTION_OUTSIDE_PIN, INPUT);
    pinMode(OCF_FLAPIR_OUTSIDE_PIN, INPUT);
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
    int motion_inside = digitalRead(OCF_MOTION_INSIDE_PIN);
    int motion_outside = digitalRead(OCF_MOTION_OUTSIDE_PIN);
    if (motion_inside > OCF_MOTION_THRESHOLD || motion_outside > OCF_MOTION_THRESHOLD){
        flapState.last_activity = OCFWifi::getEpochTime();
    }
    if (count_motion_inside > INT_MAX - OCF_MOTION_DELAY) count_motion_inside = OCF_MOTION_DELAY;
    if (count_motion_outside > INT_MAX - OCF_MOTION_DELAY) count_motion_outside = OCF_MOTION_DELAY;
    if (motion_inside > OCF_MOTION_THRESHOLD) count_motion_inside++;
    if (motion_outside > OCF_MOTION_THRESHOLD) count_motion_outside++;

    if (count_motion_inside > OCF_MOTION_DELAY  && motion_inside > OCF_MOTION_THRESHOLD && count_motion_outside > OCF_MOTION_DELAY && motion_outside == OCF_MOTION_THRESHOLD) {
        return OCFDirection::BOTH;
    }
    if (count_motion_inside > OCF_MOTION_DELAY && motion_inside > OCF_MOTION_THRESHOLD) {
        return OCFDirection::OUT;
    }
    if (count_motion_outside > OCF_MOTION_DELAY && motion_outside > OCF_MOTION_THRESHOLD) {
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

/*
loop()
    case state1: state1();
    case state1: state2();

statelessActions();
state1():
    while(true):
        do stuff
        check transition condition
            transition to state x
            break
        statelessActions()
        vtaskdelay(1)
state2():
    while(true):
        do stuff
        check transition condition
            transition to state x
            break
        statelessActions()
        vtaskdelay(1)
*/


/*

There are two coexisting machines:
    1. Sensor statemachine
    2. Flap machine (layered)
Global variables:
    Active_tags with directions/locations and time (maybe add a bool or len) 

Sensor-Machine:
    Idle:
        Check PIRs for movement
        If movement is detected, transition to ReadTag
    ReadTag:
        Init:
            Ensure RFID Sensors are powered on
            Store current time
        Deinit:
            Power off RFID sensors
            Clear active_tags
        Loop:
            Check if a tag was read
                If tag was read add or update active_tags
                Remove tags from active_tags that weren't seen for more than 5s
            Check for any movement
                If no movement detected for more than 10s, deinit and transition to Idle
                If movement detected, store current time
    
Flap-Machine:
    Variables: 
        Lock-Status with timer per direction
        enable_unlock
        flap_is_open (direction)
        time_since_flap_open
    Idle layer: Do nothing (vtaskdelay)
    Proximity layer (if active_tags is not empty):
        Check permissions on tags and flap
        Enable unlock-layer
    Unlock-Layer (if enabled by proximity layer):
        Check permissions on tags and flap
        Check Servos and enable them
        Unlock accordingly
            store state and time in lock-status
        Check lock-status 
            Lock if unlocked for >5s
            if both directions locked: disable servos and unlock layer
    Possible-Movement-Layer (enabled if lock-status has an unlocked state):
        Check flap rotation
            set direction flap_is_open
            store current time in time_since_flap_open
        If flap is closed for more than 2s:
            flap_is_open = None        
    Movement-Layer (if flap_is_open):
        Depending on rotation and active_tag location+unlock state determine tag movement
        Consult active_tag and (future IR, outside rfid) to see if multiple tags moved.


*/