#pragma once

#include <string>
#include <vector>

#include <WiFi.h>

class WiFiClientSecure : public WiFiClient
{
public:
    void setCACert(const char *ca) { ca_ = ca ? ca : ""; }
    void setCertificate(const char *cert) { cert_ = cert ? cert : ""; }
    void setPrivateKey(const char *key) { key_ = key ? key : ""; }
    void setAlpnProtocols(const char **protocols)
    {
        alpn_.clear();
        for (; protocols && *protocols; protocols++) alpn_.push_back(*protocols);
    }

    std::string ca_, cert_, key_;
    std::vector<std::string> alpn_;
};
