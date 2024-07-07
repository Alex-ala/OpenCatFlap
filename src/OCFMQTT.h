#ifndef __OCFMQTT_H__
#define __OCFMQTT_H__

#include <Arduino.h>
#include <OCFFilesystem.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <definitions.h>
struct MQTTConfiguration {
    const char* server = "";
    int port = 0;
    const char* user = "";
    const char* password = "";
    const char* ca = "";
    const char* cert = "";
    const char* key = "";
    const char* name = "doorofdurin";
    bool ssl = false;
    bool logActivity = true;
};

class OCFMQTT{
    private:
        static bool connected;
        static bool configured;
        static PubSubClient mqttclient;
        static WiFiClientSecure mqtt_secure;
        static WiFiClient mqtt;
    public:
        static MQTTConfiguration config;
        static bool configure(StaticJsonDocument<OCF_MAX_JSON_SIZE>& doc);
        static void saveConfiguration();
        static void init();
        static bool connectMQTT();
        static void monitorMQTT(void* parameter);
        static void reconnect();
        static void sendMessage(const char *topic, const char *message);
};

#endif // __OCFMQTT_H__