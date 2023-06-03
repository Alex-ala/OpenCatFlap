#ifndef __OCFWIFI_H__
#define __OCFWIFI_H__

#include <Arduino.h>
#include <WiFi.h>
#include <OCFFilesystem.h>
#include <definitions.h>

namespace OCFWifi{
    extern const char* CONFIG_FILE;
    struct WifiConfiguration {
        String ssid;
        String passphrase;
        String ip;
        String gateway;
        String netmask;
    };
    extern bool connected;
    extern bool configured;
    extern WifiConfiguration config;

    void configure(StaticJsonDocument<OCF_MAX_JSON_SIZE>& doc);
    void init();
    void setupAP();
    bool connectWifi();
    void monitorWifi(void* parameter);
    void reconnect();
};

#endif // __OCFWIFI_H__