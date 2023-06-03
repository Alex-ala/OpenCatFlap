#include <OCFWebserver.h>
#include <OCFFilesystem.h>
#include <OCFWifi.h>
#include <definitions.h>

namespace OCFWebserver{
    WebServer server(80);
    bool initialized = false;
    void init(){
        log_d("Initializing webserver...");
        server.on("/debug", handle_debug);
        server.on("/api",HTTPMethod::HTTP_GET, handle_api_get);
        server.on("/api",HTTPMethod::HTTP_POST, handle_api_post);
        server.serveStatic("/static", SPIFFS,"/");
        server.begin();
        log_d("Webserver initialized");
        initialized = true;
    }

    void handle_debug(){
        log_d("Received request on /debug");
        server.send(200, "text/html", "deubg");
    }
    void handle_api_get(){
        log_d("Received GET request on /api");
        server.send(200, "text/html", "api get");
    }
    void handle_api_post(){
        log_d("Received POST request on /api");
        if(!server.hasArg("plain")){
            log_d("Received no request body, return 400");
            server.send(400, "text/html", "No body received.");
            return;
        }
        const char* data = server.arg("plain").c_str();
        if(server.arg("plain").length() > OCF_MAX_JSON_SIZE){
            log_d("Request body too large, return 400");
            server.send(413, "text/html", "Request body too large");
            return;
        }
        StaticJsonDocument<OCF_MAX_JSON_SIZE> doc;
        DeserializationError err = deserializeJson(doc, data);
        if(err){
            log_d("Failed to parse %s: %s", data, err.c_str());
            server.send(500, "text/html", "failed to parse data json");
            return;
        }
        if(!doc.containsKey("command")){
            log_d("data json missing command field: %s", data);
            server.send(400, "text/html", "data json missing command field");
            return;
        }
        if(doc["command"] == "wifi_config"){
            log_d("configuring wifi...");
            OCFWifi::configure(doc);
            log_d("configured wifi, reconnecting to wifi...");
            OCFWifi::reconnect();
        }
        server.send(200, "text/html", "api post");
    }

    void loop(void* parameter){
        while(true){
            if (!initialized){
                sleep(1);
                continue;
            }
            server.handleClient();
        }
    }
}