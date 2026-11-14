#pragma once

#include <string>
#include <vector>

#include <Arduino.h>
#include <WiFi.h>

namespace mock
{
    struct MqttMessage
    {
        std::string topic;
        std::string payload;
    };
    struct MqttState
    {
        bool connect_result = true;
        bool publish_result = true;
        bool loop_result = true;
        std::string server;
        uint16_t port = 0;
        Client *client = nullptr;
        int connects = 0;
        std::string client_id;
        bool used_credentials = false;
        std::string user, password;
        int disconnects = 0;
        std::vector<MqttMessage> published;
    };
    inline MqttState mqtt;
    inline void reset_mqtt() { mqtt = MqttState(); }

    inline std::vector<MqttMessage> published_to(const std::string &topic)
    {
        std::vector<MqttMessage> out;
        for (const auto &m : mqtt.published)
            if (m.topic == topic) out.push_back(m);
        return out;
    }
}

class PubSubClient
{
public:
    PubSubClient &setServer(const char *domain, uint16_t port)
    {
        mock::mqtt.server = domain ? domain : "";
        mock::mqtt.port = port;
        return *this;
    }
    PubSubClient &setClient(Client &client)
    {
        mock::mqtt.client = &client;
        return *this;
    }
    bool connect(const char *id)
    {
        mock::mqtt.connects++;
        mock::mqtt.client_id = id;
        mock::mqtt.used_credentials = false;
        connected_ = mock::mqtt.connect_result;
        return connected_;
    }
    bool connect(const char *id, const char *user, const char *pass)
    {
        connect(id);
        mock::mqtt.used_credentials = true;
        mock::mqtt.user = user ? user : "";
        mock::mqtt.password = pass ? pass : "";
        return connected_;
    }
    void disconnect()
    {
        mock::mqtt.disconnects++;
        connected_ = false;
    }
    bool publish(const char *topic, const char *payload)
    {
        mock::mqtt.published.push_back({topic, payload});
        return mock::mqtt.publish_result;
    }
    bool loop() { return mock::mqtt.loop_result; }
    bool connected() { return connected_; }
    int state() { return connected_ ? 0 : -1; }

private:
    bool connected_ = false;
};
