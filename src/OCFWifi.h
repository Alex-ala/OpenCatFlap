#ifndef __OCFWIFI_H__
#define __OCFWIFI_H__

#include <Arduino.h>
#include <WiFi.h>
#include <OCFFilesystem.h>
#include <definitions.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
struct WifiConfiguration {
    String ssid;
    String passphrase;
    String ip;
    String gateway;
    String netmask;
    String ntpServer;
};
class OCFWifi{
    private:
        static WiFiUDP ntpUDP;
        static NTPClient timeClient;
        static bool configured;
        static bool connected;
        static WifiConfiguration config;
    public:
        static bool configure(StaticJsonDocument<OCF_MAX_JSON_SIZE>& doc);
        static void init();
        static void setupAP();
        static bool connectWifi();
        static void monitorWifi(void* parameter);
        static void reconnect();
        static unsigned long getEpochTime();
};

#endif // __OCFWIFI_H__