#include <OCFWebserver.h>
#include <OCFFilesystem.h>
#include <OCFWifi.h>
#include <definitions.h>
#include <OCFMQTT.h>
#include <OCFFlapControl.h>


bool OCFWebserver::initialized = false;
WebServer OCFWebserver::server;
char data_buf[OCF_MAX_JSON_SIZE+1];

void OCFWebserver::init(){
    log_d("Initializing webserver...");
    server.on("/debug", handle_debug);
    server.on("/api",HTTPMethod::HTTP_GET, handle_api_get);
    server.on("/api",HTTPMethod::HTTP_POST, handle_api_post);
    server.on("/api/certs",HTTPMethod::HTTP_POST, handle_certs);
    server.on("/api/cats", HTTPMethod::HTTP_GET, handle_api_cats);
    server.serveStatic("/", SPIFFS, "/index.html");
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
    if (!server.hasArg("dir") || !server.hasArg("val")){
        server.send(500, "text/html", "debug");
        return;
    }
    int dir = server.arg("dir").toInt();
    int val = server.arg("val").toInt();
    if (dir == 1){
        OCFLock::enableServo(OCFDirection::IN);
        OCFLock::moveServoIn(val);
    }else{
        OCFLock::enableServo(OCFDirection::OUT);
        OCFLock::moveServoOut(val);
    }
    
    server.send(200, "text/html", "debug");
}
void OCFWebserver::handle_api_get(){
    log_d("Received GET request on /api");
    server.send(200, "text/html", "api get");
}
void OCFWebserver::handle_api_cats(){
    String str;
    OCFState::getCatsJSON(str);
    server.send(200, "application/json", str);
    return;
}
void OCFWebserver::handle_api_post(){
    log_d("Received POST request on /api");
    if(!server.hasArg("plain")){
        log_d("Received no request body, return 400");
        server.send(400, "text/html", "No body received.");
        return;
    }
    if(server.arg("plain").length() > OCF_MAX_JSON_SIZE){
        log_d("Request body too large, return 400");
        server.send(413, "text/html", "Request body too large");
        return;
    }
    strcpy(data_buf, server.arg("plain").c_str());
    log_d("Data: %s", data_buf);
    StaticJsonDocument<OCF_MAX_JSON_SIZE> doc;
    DeserializationError err = deserializeJson(doc, data_buf);
    if(err){
        log_d("Failed to parse %s: %s", data_buf, err.c_str());
        server.send(500, "text/html", "failed to parse data json");
        doc.clear();
        for (int i = 0; i<=OCF_MAX_JSON_SIZE; i++) data_buf[i] = 0x0;
        return;
    }
    if(!doc.containsKey("command")){
        log_d("data json missing command field: %s", data_buf);
        server.send(400, "text/html", "data json missing command field");
        doc.clear();
        for (int i = 0; i<=OCF_MAX_JSON_SIZE; i++) data_buf[i] = 0x0;
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
            doc.clear();
            for (int i = 0; i<=OCF_MAX_JSON_SIZE; i++) data_buf[i] = 0x0;
            return;
        }
        server.send(202, "text/html", "Configuring mqtt...");
        OCFMQTT::reconnect();
    }else if (doc["command"] == "setAllowState")
    {
        if (!doc.containsKey("direction") || !doc.containsKey("allowed")){
            log_d("data json missing direction or allowed field: %s", data_buf);
            server.send(400, "text/html", "data json missing direction or allowed field");
            doc.clear();
            for (int i = 0; i<=OCF_MAX_JSON_SIZE; i++) data_buf[i] = 0x0;
            return;
        }
        OCFDirection direction = parseDirection(doc["direction"].as<String>());
        if (direction == OCFDirection::IN || direction == OCFDirection::BOTH){
            OCFState::config.allow_in = doc["allowed"].as<bool>();
        }else if (direction == OCFDirection::OUT || direction == OCFDirection::BOTH){
            OCFState::config.allow_out = doc["allowed"].as<bool>();
        }
        OCFState::saveConfig();
    }else if (doc["command"] == "status"){
        String str;
        OCFState::getStateJSON(str);
        server.send(200, "application/json", str);
        doc.clear();
        for (int i = 0; i<=OCF_MAX_JSON_SIZE; i++) data_buf[i] = 0x0;
        return;
    }else if (doc["command"] == "cat"){
        if (!doc.containsKey("rfid") || !doc.containsKey("name")){
            log_d("data json missing rfid and name: %s", data_buf);
            server.send(400, "text/html", "data json missing rfid or name field");
            doc.clear();
            for (int i = 0; i<=OCF_MAX_JSON_SIZE; i++) data_buf[i] = 0x0;
            return;
        }
        unsigned long long id = doc["rfid"].as<unsigned long long>();
        std::map<unsigned long long, OCFCat>::iterator it = OCFState::cats.find(id);
        OCFCat cat;
        if (it->first == id) {
            cat = it->second;
        }else{
            cat = OCFCat();
        }     
        strcpy(cat.name, doc["name"]);
        cat.rfid = id;
        if (doc.containsKey("allowed_in")) cat.allow_in = doc["allowed_in"].as<bool>();
        if (doc.containsKey("allowed_out")) cat.allow_out = doc["allowed_out"].as<bool>();
        OCFState::cats.insert_or_assign(cat.rfid, cat);
        OCFState::saveCats();
    }
    
    server.send(200, "text/html", "api post");
    doc.clear();
    for (int i = 0; i<=OCF_MAX_JSON_SIZE; i++) data_buf[i] = 0x0;
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
