#ifndef __OCFMQTT_H__
#define __OCFMQTT_H__

#include <Arduino.h>
#include <OCFFilesystem.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <definitions.h>

namespace OCFMQTT{
    extern const char* CONFIG_FILE;
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
        bool logActivity = false;
    };
    extern bool connected;
    extern bool configured;
    extern MQTTConfiguration config;
    extern PubSubClient mqttclient;

    bool configure(StaticJsonDocument<OCF_MAX_JSON_SIZE>& doc);
    void saveConfiguration();
    void init();
    bool connectMQTT();
    void monitorMQTT(void* parameter);
    void reconnect();
    void sendMessage(const char *topic, const char *message);
};

#endif // __OCFMQTT_H__