#ifndef __OCFWEBSERVER_H__
#define __OCFWEBSERVER_H__


#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <OCFFlapControl.h>
class OCFWebserver{
    private:
        static WebServer server;
        static bool initialized;
    public:
        static void handle_debug();
        static void handle_api_get();
        static void handle_api_post();
        static void handle_certs();
        static OCFDirection parseDirection(String direction);
        static void loop(void* parameter);
        static void init();
};

#endif // __OCFWEBSERVER_H__