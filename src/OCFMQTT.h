#ifndef __OCFMQTT_H__
#define __OCFMQTT_H__

#include <Arduino.h>
#include <OCFFilesystem.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include <definitions.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
struct MQTTConfiguration {
    char server[20];
    int port = 0;
    char user[20];
    char password [64];
    String ca;
    String cert;
    String key;
    char name[20];
    bool ssl = false;
    bool logActivity = true;
};

struct MQTTOutgoing {
    char topic[24];
    char message[64];
};

class OCFMQTT{
    private:
        static volatile bool connected;
        static bool configured;
        static volatile bool reconnect_requested;
        static uint32_t last_attempt_ms;
        static QueueHandle_t outbox;
        static SemaphoreHandle_t config_mutex;
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
        static void serviceMQTT();
        static void publishQueued();
        static void reconnect();
        static void sendMessage(const char *topic, const char *message);
};

#endif // __OCFMQTT_H__