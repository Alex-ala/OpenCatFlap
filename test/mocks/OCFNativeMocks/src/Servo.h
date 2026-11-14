#pragma once

#include <vector>

#include <Arduino.h>

namespace mock
{
    struct ServoEvent
    {
        enum Kind { ATTACH, DETACH, WRITE } kind;
        int pin;
        int channel;
        int angle;
    };
    inline std::vector<ServoEvent> servo_events;
    inline bool servo_attach_result = true;

    inline std::vector<ServoEvent> servo_events_for(int pin, ServoEvent::Kind kind)
    {
        std::vector<ServoEvent> out;
        for (const auto &e : servo_events)
            if (e.pin == pin && e.kind == kind) out.push_back(e);
        return out;
    }
    inline int last_servo_angle(int pin)
    {
        auto writes = servo_events_for(pin, ServoEvent::WRITE);
        return writes.empty() ? -1 : writes.back().angle;
    }
}

class Servo
{
public:
    static const int CHANNEL_NOT_ATTACHED = -1;
    static const int PIN_NOT_ATTACHED = -1;

    bool attach(int pin, int channel = CHANNEL_NOT_ATTACHED, int minAngle = 0, int maxAngle = 180,
                int minPulseWidthUs = 544, int maxPulseWidthUs = 2400, int frequency = 50)
    {
        (void)minAngle; (void)maxAngle; (void)minPulseWidthUs; (void)maxPulseWidthUs; (void)frequency;
        mock::servo_events.push_back({mock::ServoEvent::ATTACH, pin, channel, -1});
        if (!mock::servo_attach_result) return false;
        pin_ = pin;
        channel_ = channel;
        pinMode(pin, OUTPUT);
        return true;
    }
    bool detach()
    {
        if (!attached()) return false;
        mock::servo_events.push_back({mock::ServoEvent::DETACH, pin_, channel_, -1});
        pin_ = PIN_NOT_ATTACHED;
        return true;
    }
    void write(int angle)
    {
        if (!attached()) return;
        mock::servo_events.push_back({mock::ServoEvent::WRITE, pin_, channel_, angle});
        angle_ = angle;
    }
    int read() const { return angle_; }
    bool attached() const { return pin_ != PIN_NOT_ATTACHED; }
    int attachedPin() const { return pin_; }

private:
    int pin_ = PIN_NOT_ATTACHED;
    int channel_ = CHANNEL_NOT_ATTACHED;
    int angle_ = 0;
};
