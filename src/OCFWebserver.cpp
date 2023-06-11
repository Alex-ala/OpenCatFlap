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
        if(OCFFlapControl::flapState.allow_in){
            OCFFlapControl::flapState.allow_in = false;
        }else{
            OCFFlapControl::flapState.allow_in = true;
        }
        server.send(200, "text/html", "debug");
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
        if(doc["command"] == "wifiConfig"){
            log_d("configuring wifi...");
            OCFWifi::configure(doc);
            server.send(202, "text/html", "Configuring wifi...");
            log_d("configured wifi, reconnecting to wifi...");
            OCFWifi::reconnect();
        }else if (doc["command"] == "setAllowState")
        {
            if (!doc.containsKey("direction") || !doc.containsKey("allowed")){
                log_d("data json missing direction or allowed field: %s", data);
                server.send(400, "text/html", "data json missing direction or allowed field");
                return;
            }
            OCFFlapControl::Direction direction = parseDirection(doc["direction"].as<String>());
            OCFFlapControl::setAllowState(direction, doc["allowed"].as<bool>());
        }
        
        server.send(200, "text/html", "api post");
    }

    OCFFlapControl::Direction parseDirection(String direction){
        if (direction == "in") return OCFFlapControl::Direction::IN;
        if (direction == "out") return OCFFlapControl::Direction::OUT;
        if (direction == "both") return OCFFlapControl::Direction::BOTH;
        log_d("Failed to parse direction %s", direction);
        return OCFFlapControl::Direction::NONE;
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