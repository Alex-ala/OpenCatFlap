#ifndef __OCFWEBSERVER_H__
#define __OCFWEBSERVER_H__


#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <OCFFlapControl.h>
// TODO: Use proper getter/setters
class OCFWebserver{
    private:
        static OCFWebserver web;
        OCFWebserver();
        OCFWebserver(const OCFWebserver&);
        OCFWebserver operator=(const OCFWebserver&);
        WebServer server;
        bool initialized;
    public:
        static OCFWebserver& getInstance();
        static void handle_debug();
        static void handle_api_get();
        static void handle_api_post();
        static void handle_certs();
        OCFDirection parseDirection(String direction);
        static void loop(void* parameter);
        void init();
};

#endif // __OCFWEBSERVER_H__