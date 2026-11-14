#pragma once

#include <string>

#include <Arduino.h>
#include <WiFiUdp.h>

namespace mock
{
    struct NtpState
    {
        unsigned long epoch = 1700000000UL;
        std::string pool_server;
        bool pool_server_set = false;
        int updates = 0;
    };
    inline NtpState ntp;
    inline void reset_ntp() { ntp = NtpState(); }
}

class NTPClient
{
public:
    explicit NTPClient(UDP &) {}
    void setPoolServerName(const char *name)
    {
        mock::ntp.pool_server = name ? name : "";
        mock::ntp.pool_server_set = true;
    }
    bool update() { mock::ntp.updates++; return true; }
    unsigned long getEpochTime() const { return mock::ntp.epoch; }
};
