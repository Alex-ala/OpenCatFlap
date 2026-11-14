#pragma once

#include <FS.h>

class SPIFFSFS : public fs::FS
{
public:
    bool begin(bool formatOnFail = false, const char * = "/spiffs", uint8_t = 10, const char * = nullptr)
    {
        (void)formatOnFail;
        mock::mount_calls++;
        return mock::mount_ok;
    }
};

inline SPIFFSFS SPIFFS;
