#include <OCFStateMachine.h>

// TODO: Enable WIFI & MQTT to debug with powersupply
OCFStateMachine::OCFStateMachine(OCFDirection direction) : direction(direction)
{
    OCFLock::enableServo(direction);
    sleep(1);
    OCFLock::disableServo(direction);
    servoAttached = false;
    lastMotion = 0;
    lastRFID = 0;
    rfidReading = false;

    if (direction == OCFDirection::IN)
    {
        pin_motion = OCF_MOTION_OUTSIDE_PIN;
        // Current hardware build has outside and inside antennas swapped....
        pin_rfid_power = OCF_RFID_OUTSIDE_EN;
        pin_rfid_read = OCF_RFID_OUTSIDE_RX;
        pin_rfid_write = OCF_RFID_OUTSIDE_TX;
        pin_rfid_reset = OCF_RFID_OUTSIDE_RST;
        serial = &Serial1;
        prefix = "In:";
    }
    else if (direction == OCFDirection::OUT)
    {
        pin_motion = OCF_MOTION_INSIDE_PIN;
        pin_rfid_power = OCF_RFID_INSIDE_EN;
        pin_rfid_read = OCF_RFID_INSIDE_RX;
        pin_rfid_write = OCF_RFID_INSIDE_TX;
        pin_rfid_reset = OCF_RFID_INSIDE_RST;
        serial = &Serial2;
        prefix = "Out:";
    }
    pinMode(pin_motion, INPUT_PULLUP);
    digitalWrite(pin_rfid_power, LOW);
    digitalWrite(pin_rfid_reset, HIGH);
    pinMode(pin_rfid_power, OUTPUT);
    pinMode(pin_rfid_reset, OUTPUT);
    serial->begin(9600, SERIAL_8N2, pin_rfid_read, pin_rfid_write);
    currentState = OCFMachineStates::IDLE;
    log_d("%s StateMachine initialized", prefix);
}

void OCFStateMachine::flushRFID(){
    while(serial->available() > 0){
        serial->read();
    }
}

void OCFStateMachine::resetRFID()
{
    log_d("%s reset RFID", prefix);
    flushRFID();
    lastRead = millis();
    rfidReading = false;
    digitalWrite(pin_rfid_reset, LOW);
    delay(10);
    digitalWrite(pin_rfid_reset, HIGH);
}

long long OCFStateMachine::readRFID()
{
    if (serial->available() == 0) return 0;
    if (serial->available() >= 1 && !rfidReading)
    {
        char input[1];
        input[0] = 0x0;
        while (serial->available())
        {
            serial->read(input, 1);
            if (input[0] == 0x02)
            {
                rfidReading = true;
                log_d("%s Received RFID header", prefix);
                break;
            }
        }
        return 0;
    }
    log_d("%s Reading RFID...", prefix);
    //900 215007389064
    // 38 38 35 36 30 37 46 30 32 33 - 34 38 33 30 - 31 - 31 - 30 30 30 30 - 37 30 32 42 31 30 - 0A F5 03
    // 38 38 35 36 30 37 46 30 32 33 -> Reverse -> 33 32 30 46 37 30 36 35 38 38 -> ASCII -> 320F706588 -> int -> 215007389064
    // 34 38 33 30 -> Reverse -> 30 33 38 34 -> ASCII -> 0384 -> int -> 900
    // try to read tag id
    if (serial->available() < 29)
        return 0;
    serial->readBytes(rfidTag, 29);
    char tag_country_bytes[5];
    char tag_id_bytes[11];
    for (int i = 9; i >= 0; i--)
    {
        tag_id_bytes[9 - i] = rfidTag[i];
    }
    tag_id_bytes[10] = 0x0;
    for (int i = 3; i >= 0; i--)
    {
        tag_country_bytes[3 - i] = rfidTag[10 + i];
    }
    tag_country_bytes[4] = 0x0;
    long long tag_id = strtoull(tag_id_bytes, NULL, 16);
    long tag_country = strtoul(tag_country_bytes, NULL, 16);
    serial->read();
    for (int x = 0; x < 29; x++)
        rfidTag[x] = 0x0;
    rfidReading = false;
    lastRFID = millis();
    lastRead = lastRFID;
    Serial.println(tag_id); // TODO: log_d and/or sprintf are not threadsafe
    return tag_id;
    //TODO: tag read wrong
}

void OCFStateMachine::update()
{
    if (currentState == OCFMachineStates::IDLE)
        updateIdle();
    else if (currentState == OCFMachineStates::READING)
        updateReading();
    else if (currentState == OCFMachineStates::UNLOCKED)
        updateUnlocked();
}

void OCFStateMachine::updateIdle()
{
    bool motion = digitalRead(pin_motion) == LOW;
    if (!motion)
        return;
    lastMotion = millis();
    transitionIdleToReading();
}

void OCFStateMachine::updateReading()
{
    bool motion = digitalRead(pin_motion) == LOW;
    if (motion) {
        lastMotion = millis();
    }
    // Return to Idle
    if (lastMotion <= millis() - 10000)
        transitionReadingToIdle();
    // Reset RFID after 5s
    if (lastRead <= millis() - 2000)
        resetRFID();
    // Read until init byte is received
    long long id = readRFID();
    if (id == 0)
        return;
    transitionReadingToUnlocked();
}

void OCFStateMachine::updateUnlocked()
{
    bool motion = digitalRead(pin_motion) == LOW;
    if (motion) {
        lastMotion = millis();
    }
    if (lastRFID <= millis() - 5000)
        transitionUnlockedToReading();
    if (lastRead <= millis() - 2000)
        resetRFID();
    long long tag = readRFID();
}

void OCFStateMachine::transitionIdleToReading()
{
    OCFLock::enableServo(direction);
    servoAttached = true;
    rfidReading = false;
    lastRFID = 0;
    lastRead = 0;
    flushRFID();
    digitalWrite(pin_rfid_power, HIGH);
    resetRFID();
    currentState = OCFMachineStates::READING;
    log_d("%s Transition to Reading", prefix);
}

void OCFStateMachine::transitionReadingToIdle()
{
    OCFLock::disableServo(direction);
    servoAttached = false;
    rfidReading = false;
    lastMotion = 0;
    lastRFID = 0;
    lastRead = 0;
    digitalWrite(pin_rfid_power, LOW);
    flushRFID();
    currentState = OCFMachineStates::IDLE;
    log_d("%s Transition to Idle", prefix);
}

void OCFStateMachine::transitionReadingToUnlocked()
{
    resetRFID();
    OCFLock::unlock(direction);
    digitalWrite(pin_rfid_power, HIGH);
    currentState = OCFMachineStates::UNLOCKED;
    log_d("%s Transition to Unlocked", prefix);
}

void OCFStateMachine::transitionUnlockedToReading()
{
    resetRFID();
    OCFLock::lock(direction);
    currentState = OCFMachineStates::READING;
    log_d("%s Transition to Reading", prefix);
}
