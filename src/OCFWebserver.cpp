#include <OCFWebserver.h>
#include <OCFFilesystem.h>
#include <OCFWifi.h>
#include <definitions.h>
#include <OCFMQTT.h>


bool OCFWebserver::initialized = false;
WebServer OCFWebserver::server;

void OCFWebserver::init(){
    log_d("Initializing webserver...");
    server.on("/debug", handle_debug);
    server.on("/api",HTTPMethod::HTTP_GET, handle_api_get);
    server.on("/api",HTTPMethod::HTTP_POST, handle_api_post);
    server.on("/api/certs",HTTPMethod::HTTP_POST, handle_certs);
    server.begin();
    log_d("Webserver initialized");
    initialized = true;
}

void OCFWebserver::handle_certs(){
    log_d("Received request on /api/certs");
    if(!server.hasArg("name") || !server.hasArg("plain")){
        server.send(400, "text/html", "No cert name received");
        return;
    }
    String name = server.arg("name");
    if(name == "ca"){
        OCFFilesystem::writeStringFile(OCF_MQTT_CA_PATH, server.arg("plain").c_str());
        log_d("Wrote ca cert");
    }else if(name == "cert"){
        OCFFilesystem::writeStringFile(OCF_MQTT_CERT_PATH, server.arg("plain").c_str());
        log_d("Wrote cert");
    }else if(name == "key"){
        OCFFilesystem::writeStringFile(OCF_MQTT_KEY_PATH, server.arg("plain").c_str());
        log_d("Wrote key");
    }else{
        log_d("Invalid name given");
        server.send(400, "text/html", "No valid certificate name received.");
        return;
    }
    server.send(200, "text/html", "Saved " + name);
}

void OCFWebserver::handle_debug(){
    log_d("Received request on /debug");
    int pir_in = digitalRead(OCF_MOTION_INSIDE_PIN);
    int pir_out = digitalRead(OCF_MOTION_OUTSIDE_PIN);
    int flap_in = digitalRead(OCF_FLAPIR_INSIDE_PIN);
    int flap_out = digitalRead(OCF_FLAPIR_OUTSIDE_PIN);
    int tunnel_in = digitalRead(OCF_TUNNELIR_INSIDE_PIN);
    char buffer[100];
    sprintf(buffer, "pir_in: %d, pir_out: %d, flap_in: %d, flap_out: %d, tunnel_in: %d", pir_in, pir_out, flap_in, flap_out, tunnel_in);
    log_d("pir_in: %d, pir_out: %d, flap_in: %d, flap_out: %d, tunnel_in: %d", pir_in, pir_out, flap_in, flap_out, tunnel_in);
    OCFMQTT::sendMessage("log", buffer);
    server.send(200, "text/html", "debug");
}
void OCFWebserver::handle_api_get(){
    log_d("Received GET request on /api");
    server.send(200, "text/html", "api get");
}
void OCFWebserver::handle_api_post(){
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
    }else if (doc["command"] == "mqttConfig"){
        log_d("configuring mqtt...");
        if(!OCFMQTT::configure(doc)){
            server.send(400, "text/html", "data json missing field");
            return;
        }
        server.send(202, "text/html", "Configuring mqtt...");
        OCFMQTT::reconnect();
    }else if (doc["command"] == "setAllowState")
    {
        if (!doc.containsKey("direction") || !doc.containsKey("allowed")){
            log_d("data json missing direction or allowed field: %s", data);
            server.send(400, "text/html", "data json missing direction or allowed field");
            return;
        }
        OCFDirection direction = parseDirection(doc["direction"].as<String>());
        OCFFlapControl::setAllowState(direction, doc["allowed"].as<bool>());
    }else if (doc["command"] == "status"){
        String str;
        OCFFlapControl::getFlapStateJson(str);
        server.send(200, "application/json", str);
        return;
    }
    
    server.send(200, "text/html", "api post");
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
        if (!initialized){
            sleep(1);
            continue;
        }
        server.handleClient();
    }
}
