#ifndef __OCFWEBSERVER_H__
#define __OCFWEBSERVER_H__


#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

namespace OCFWebserver{
    extern WebServer server;
    extern bool initialized;
    void handle_debug();
    void handle_api_get();
    void handle_api_post();
    void loop(void* parameter);
    void init();

};

#endif // __OCFWEBSERVER_H__