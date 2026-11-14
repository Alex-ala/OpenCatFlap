#ifndef __OCFWEBSERVER_H__
#define __OCFWEBSERVER_H__


#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <OCFFlapControl.h>
#include <OCFHelper.h>

class OCFWebserver{
    private:
        static WebServer server;
        static bool initialized;
        static bool update_suspended;
        static bool update_rejected;
    public:
        static void handle_debug();
        static void handle_api_get();
        static void handle_api_post();
        static void handle_command(StaticJsonDocument<OCF_MAX_JSON_SIZE>& doc);
        static void handle_certs();
        static void handle_api_cats();
        static void handle_update();
        static void handle_update_upload();
        static void loop(void* parameter);
        static void init();
};

#endif // __OCFWEBSERVER_H__