#pragma once

#include <string>
#include <vector>

#include <Arduino.h>

class IPAddress
{
public:
    IPAddress() {}
    IPAddress(uint8_t a, uint8_t b, uint8_t c, uint8_t d) : addr_(a | (b << 8) | (c << 16) | ((uint32_t)d << 24)) {}
    IPAddress(uint32_t addr) : addr_(addr) {}
    operator uint32_t() const { return addr_; }
    bool operator==(const IPAddress &o) const { return addr_ == o.addr_; }

    bool fromString(const char *s)
    {
        unsigned a, b, c, d;
        char tail;
        if (sscanf(s, "%u.%u.%u.%u%c", &a, &b, &c, &d, &tail) != 4 || a > 255 || b > 255 || c > 255 || d > 255)
            return false;
        *this = IPAddress(a, b, c, d);
        return true;
    }
    String toString() const
    {
        char buf[16];
        snprintf(buf, sizeof(buf), "%u.%u.%u.%u", addr_ & 0xff, (addr_ >> 8) & 0xff, (addr_ >> 16) & 0xff, addr_ >> 24);
        return String(buf);
    }

private:
    uint32_t addr_ = 0;
};

typedef enum
{
    WL_IDLE_STATUS = 0,
    WL_NO_SSID_AVAIL = 1,
    WL_CONNECTED = 3,
    WL_CONNECT_FAILED = 4,
    WL_DISCONNECTED = 6,
} wl_status_t;

typedef enum
{
    WIFI_OFF = 0,
    WIFI_STA = 1,
    WIFI_AP = 2,
    WIFI_AP_STA = 3,
} wifi_mode_t;

class Client : public Stream
{
public:
    size_t write(uint8_t) override { return 1; }
    using Print::write;
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
};

class WiFiClient : public Client
{
};

class UDP
{
};

class WiFiUDP : public UDP
{
};

namespace mock
{
    struct WiFiState
    {
        wl_status_t status = WL_DISCONNECTED;
        bool connect_on_begin = true;
        bool config_result = true;
        wifi_mode_t mode = WIFI_OFF;
        std::string ssid;
        std::string passphrase;
        bool begin_called = false;
        int begin_calls = 0;
        bool static_config_called = false;
        IPAddress static_ip, static_gateway, static_netmask;
        bool ap_started = false;
        std::string ap_ssid;
        bool ap_has_passphrase = false;
        int disconnects = 0;
    };
    inline WiFiState wifi;
    inline void reset_wifi() { wifi = WiFiState(); }
}

class WiFiClass
{
public:
    bool mode(wifi_mode_t m) { mock::wifi.mode = m; return true; }
    bool config(IPAddress ip, IPAddress gateway, IPAddress netmask)
    {
        mock::wifi.static_config_called = true;
        mock::wifi.static_ip = ip;
        mock::wifi.static_gateway = gateway;
        mock::wifi.static_netmask = netmask;
        return mock::wifi.config_result;
    }
    wl_status_t begin(const char *ssid, const char *passphrase = nullptr)
    {
        mock::wifi.begin_called = true;
        mock::wifi.begin_calls++;
        mock::wifi.ssid = ssid ? ssid : "";
        mock::wifi.passphrase = passphrase ? passphrase : "";
        if (mock::wifi.connect_on_begin) mock::wifi.status = WL_CONNECTED;
        return mock::wifi.status;
    }
    wl_status_t status()
    {
        if (mock::wifi.status != WL_CONNECTED) mock::advance(1);
        return mock::wifi.status;
    }
    bool disconnect(bool = false, bool = false)
    {
        mock::wifi.disconnects++;
        mock::wifi.status = WL_DISCONNECTED;
        return true;
    }
    bool softAP(const char *ssid, const char *passphrase = nullptr)
    {
        mock::wifi.ap_started = true;
        mock::wifi.ap_ssid = ssid ? ssid : "";
        mock::wifi.ap_has_passphrase = passphrase != nullptr;
        return true;
    }
    bool softAPdisconnect(bool = false) { mock::wifi.ap_started = false; return true; }
    IPAddress softAPIP() { return IPAddress(192, 168, 4, 1); }
    IPAddress localIP() { return IPAddress(10, 5, 0, 60); }
    IPAddress dnsIP(uint8_t = 0) { return IPAddress(10, 5, 0, 1); }
};

inline WiFiClass WiFi;
