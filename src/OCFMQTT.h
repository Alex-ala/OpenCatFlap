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
    bool logActivity = false;
};

// TODO: Use proper getter/setters
class OCFMQTT{
    private:
        static OCFMQTT ocfmqtt;
        OCFMQTT();
        OCFMQTT(const OCFMQTT&);
        OCFMQTT operator=(const OCFMQTT&);
        bool connected;
        bool configured;
        PubSubClient mqttclient;
        WiFiClientSecure mqtt_secure;
        WiFiClient mqtt;
    public:
        MQTTConfiguration config;
        static OCFMQTT& getInstance();
        bool configure(StaticJsonDocument<OCF_MAX_JSON_SIZE>& doc);
        void saveConfiguration();
        void init();
        bool connectMQTT();
        static void monitorMQTT(void* parameter);
        void reconnect();
        void sendMessage(const char *topic, const char *message);
};

#endif // __OCFMQTT_H__