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
// TODO: Use proper getter/setters
class OCFWifi{
    private:
        static OCFWifi wifi;
        OCFWifi();
        OCFWifi(const OCFWifi&);
        OCFWifi operator=(const OCFWifi&);
        WiFiUDP ntpUDP;
        NTPClient timeClient;
        bool configured;
        bool connected;

        WifiConfiguration config;
    public:
        static OCFWifi& getInstance();
        bool configure(StaticJsonDocument<OCF_MAX_JSON_SIZE>& doc);
        void init();
        void setupAP();
        bool connectWifi();
        static void monitorWifi(void* parameter);
        void reconnect();
        unsigned long getEpochTime();
};

#endif // __OCFWIFI_H__