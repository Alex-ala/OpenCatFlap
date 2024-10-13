#include <OCFStateMachine.h>

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
        pin_motion = OCF_MOTION_INSIDE_PIN;
        pin_rfid_power = OCF_RFID_INSIDE_EN;
        pin_rfid_read = OCF_RFID_INSIDE_RX;
        pin_rfid_write = OCF_RFID_INSIDE_TX;
        pin_rfid_reset = OCF_RFID_INSIDE_RST;
        serial = &Serial1;
    }
    else if (direction == OCFDirection::OUT)
    {
        pin_motion = OCF_MOTION_OUTSIDE_PIN;
        pin_rfid_power = OCF_RFID_OUTSIDE_EN;
        pin_rfid_read = OCF_RFID_OUTSIDE_RX;
        pin_rfid_write = OCF_RFID_OUTSIDE_TX;
        pin_rfid_reset = OCF_RFID_OUTSIDE_RST;
        serial = &Serial2;
    }
    serial->begin(9600, SERIAL_8N2, pin_rfid_read, pin_rfid_write);
    currentState = OCFMachineStates::IDLE;
}

void OCFStateMachine::resetRFID()
{
    serial->read();
    lastRFID = millis();
    rfidReading = false;
    digitalWrite(pin_rfid_reset, HIGH);
    digitalWrite(pin_rfid_reset, LOW);
}

long long OCFStateMachine::readRFID()
{
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
                break;
            }
        }
        return 0;
    }
    // try to read tag id
    if (!rfidReading && serial->available() < 29)
        return 0;
    serial->readBytesUntil(0x03, rfidTag, 29);
    char tag_country_bytes[5];
    char tag_id_bytes[11];
    for (int i = 3; i >= 0; i--)
    {
        tag_country_bytes[3 - i] = rfidTag[10 + i];
    }
    tag_country_bytes[4] = 0x0;
    for (int i = 9; i >= 0; i--)
    {
        tag_id_bytes[9 - i] = rfidTag[i];
    }
    tag_id_bytes[10] = 0x0;
    long long tag_id = strtoll(tag_id_bytes, NULL, 16);
    long tag_country = strtol(tag_country_bytes, NULL, 16);
    serial->read();
    for (int x = 0; x < 29; x++)
        rfidTag[x] = 0x0;
    rfidReading = false;
    lastRFID = millis();
    return tag_id;
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
    if (motion)
        lastMotion = millis();
    // Return to Idle
    if (lastMotion <= millis() - 10000)
        transitionReadingToIdle();
    // Reset RFID after 5s
    if (lastRFID <= millis() - 2000)
        resetRFID();
    // Read until init byte is received
    long long id = readRFID();
    if (id == 0)
        return;
    transitionReadingToUnlocked();
}

void OCFStateMachine::updateUnlocked()
{
    if (lastRFID <= millis() - 5000)
        transitionUnlockedToReading();
    long long tag = readRFID();
}

void OCFStateMachine::transitionIdleToReading()
{
    OCFLock::enableServo(direction);
    servoAttached = true;
    rfidReading = false;
    lastRFID = 0;
    digitalWrite(pin_rfid_power, HIGH);
    digitalWrite(pin_rfid_reset, HIGH);
    digitalWrite(pin_rfid_reset, LOW);
    currentState = OCFMachineStates::READING;
}

void OCFStateMachine::transitionReadingToIdle()
{
    OCFLock::disableServo(direction);
    servoAttached = false;
    rfidReading = false;
    lastMotion = 0;
    lastRFID = 0;
    digitalWrite(pin_rfid_power, LOW);
    serial->read();
    currentState = OCFMachineStates::IDLE;
}

void OCFStateMachine::transitionReadingToUnlocked()
{
    resetRFID();
    OCFLock::unlock(direction);
    currentState = OCFMachineStates::UNLOCKED;
}

void OCFStateMachine::transitionUnlockedToReading()
{
    resetRFID();
    OCFLock::lock(direction);
    currentState = OCFMachineStates::READING;
}
