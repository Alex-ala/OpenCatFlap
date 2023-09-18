#include <OCFWebserver.h>
#include <OCFFilesystem.h>
#include <OCFWifi.h>
#include <definitions.h>
#include <OCFMQTT.h>

OCFWebserver OCFWebserver::web;

OCFWebserver::OCFWebserver() : server(80){
    initialized = false;
}

OCFWebserver& OCFWebserver::getInstance(){
    return web;
}

void OCFWebserver::init(){
    log_d("Initializing webserver...");
    server.on("/debug", handle_debug);
    server.on("/api",HTTPMethod::HTTP_GET, handle_api_get);
    server.on("/api",HTTPMethod::HTTP_POST, handle_api_post);
    server.on("/api/certs",HTTPMethod::HTTP_POST, handle_certs);
    server.serveStatic("/static", SPIFFS,"/");
    server.begin();
    log_d("Webserver initialized");
    initialized = true;
}

void OCFWebserver::handle_certs(){
    log_d("Received request on /api/certs");
    if(!getInstance().server.hasArg("name") || !getInstance().server.hasArg("plain")){
        getInstance().server.send(400, "text/html", "No cert name received");
        return;
    }
    String name = getInstance().server.arg("name");
    if(name == "ca"){
        OCFFilesystem::writeStringFile(OCF_MQTT_CA_PATH, getInstance().server.arg("plain").c_str());
        log_d("Wrote ca cert");
    }else if(name == "cert"){
        OCFFilesystem::writeStringFile(OCF_MQTT_CERT_PATH, getInstance().server.arg("plain").c_str());
        log_d("Wrote cert");
    }else if(name == "key"){
        OCFFilesystem::writeStringFile(OCF_MQTT_KEY_PATH, getInstance().server.arg("plain").c_str());
        log_d("Wrote key");
    }else{
        log_d("Invalid name given");
        getInstance().server.send(400, "text/html", "No valid certificate name received.");
        return;
    }
    getInstance().server.send(200, "text/html", "Saved " + name);
}

void OCFWebserver::handle_debug(){
    log_d("Received request on /debug");
    if(OCFFlapControl::getInstance().flapState.allow_in){
        OCFFlapControl::getInstance().flapState.allow_in = false;
    }else{
        OCFFlapControl::getInstance().flapState.allow_in = true;
    }
    getInstance().server.send(200, "text/html", "debug");
}
void OCFWebserver::handle_api_get(){
    log_d("Received GET request on /api");
    getInstance().server.send(200, "text/html", "api get");
}
void OCFWebserver::handle_api_post(){
    log_d("Received POST request on /api");
    if(!getInstance().server.hasArg("plain")){
        log_d("Received no request body, return 400");
        getInstance().server.send(400, "text/html", "No body received.");
        return;
    }
    const char* data = getInstance().server.arg("plain").c_str();
    if(getInstance().server.arg("plain").length() > OCF_MAX_JSON_SIZE){
        log_d("Request body too large, return 400");
        getInstance().server.send(413, "text/html", "Request body too large");
        return;
    }
    StaticJsonDocument<OCF_MAX_JSON_SIZE> doc;
    DeserializationError err = deserializeJson(doc, data);
    if(err){
        log_d("Failed to parse %s: %s", data, err.c_str());
        getInstance().server.send(500, "text/html", "failed to parse data json");
        return;
    }
    if(!doc.containsKey("command")){
        log_d("data json missing command field: %s", data);
        getInstance().server.send(400, "text/html", "data json missing command field");
        return;
    }
    if(doc["command"] == "wifiConfig"){
        log_d("configuring wifi...");
        OCFWifi::getInstance().configure(doc);
        getInstance().server.send(202, "text/html", "Configuring wifi...");
        log_d("configured wifi, reconnecting to wifi...");
        OCFWifi::getInstance().reconnect();
    }else if (doc["command"] == "mqttConfig"){
        log_d("configuring mqtt...");
        if(!OCFMQTT::getInstance().configure(doc)){
            getInstance().server.send(400, "text/html", "data json missing field");
            return;
        }
        getInstance().server.send(202, "text/html", "Configuring mqtt...");
        OCFMQTT::getInstance().reconnect();
    }else if (doc["command"] == "setAllowState")
    {
        if (!doc.containsKey("direction") || !doc.containsKey("allowed")){
            log_d("data json missing direction or allowed field: %s", data);
            getInstance().server.send(400, "text/html", "data json missing direction or allowed field");
            return;
        }
        OCFDirection direction = getInstance().parseDirection(doc["direction"].as<String>());
        OCFFlapControl::getInstance().setAllowState(direction, doc["allowed"].as<bool>());
    }else if (doc["command"] == "status"){
        String str;
        OCFFlapControl::getInstance().getFlapStateJson(str);
        getInstance().server.send(200, "application/json", str);
        return;
    }
    
    getInstance().server.send(200, "text/html", "api post");
}

OCFDirection OCFWebserver::parseDirection(String direction){
    if (direction == "in") return OCFDirection::IN;
    if (direction == "out") return OCFDirection::OUT;
    if (direction == "both") return OCFDirection::BOTH;
    log_d("Failed to parse direction %s", direction);
    return OCFDirection::NONE;
}

void OCFWebserver::loop(void* parameter){
    while(true){
        if (!getInstance().initialized){
            sleep(1);
            continue;
        }
        getInstance().server.handleClient();
    }
}
