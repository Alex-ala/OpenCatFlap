#pragma once

#include <string>

#include <Arduino.h>

#define UPDATE_SIZE_UNKNOWN 0xFFFFFFFF
#define U_FLASH 0
#define U_SPIFFS 100

namespace mock
{
    struct UpdateState
    {
        bool running = false;
        bool finished = false;
        bool fail_begin = false;
        bool fail_write = false;
        int error = 0;
        int begins = 0;
        int aborts = 0;
        int command = -1;
        std::string written;
    };
    inline UpdateState update;

    inline void reset_update() { update = UpdateState(); }
}

class UpdateClass
{
public:
    bool begin(size_t = UPDATE_SIZE_UNKNOWN, int command = U_FLASH)
    {
        mock::update.begins++;
        mock::update.command = command;
        mock::update.written.clear();
        mock::update.finished = false;
        mock::update.error = mock::update.fail_begin ? 1 : 0;
        mock::update.running = !mock::update.fail_begin;
        return mock::update.running;
    }
    size_t write(uint8_t *data, size_t len)
    {
        if (hasError() || !mock::update.running) return 0;
        if (mock::update.fail_write)
        {
            mock::update.error = 2;
            mock::update.running = false;
            return 0;
        }
        mock::update.written.append(reinterpret_cast<const char *>(data), len);
        return len;
    }
    bool end(bool = false)
    {
        if (hasError() || !mock::update.running) return false;
        mock::update.running = false;
        mock::update.finished = true;
        return true;
    }
    void abort()
    {
        mock::update.aborts++;
        mock::update.running = false;
        mock::update.error = 3;
    }
    bool hasError() { return mock::update.error != 0; }
    bool isFinished() { return mock::update.finished; }
    const char *errorString() { return hasError() ? "mock update error" : "No Error"; }
};
inline UpdateClass Update;
