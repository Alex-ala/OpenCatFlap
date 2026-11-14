#include <OCFWebserver.h>
#include <OCFFilesystem.h>
#include <OCFWifi.h>
#include <definitions.h>
#include <OCFMQTT.h>
#include <OCFFlapControl.h>


bool OCFWebserver::initialized = false;
bool OCFWebserver::update_suspended = false;
bool OCFWebserver::update_rejected = false;
WebServer OCFWebserver::server;
char data_buf[OCF_MAX_JSON_SIZE+1];

void OCFWebserver::init(){
    log_d("Initializing webserver...");
    server.on("/debug", handle_debug);
    server.on("/api",HTTPMethod::HTTP_GET, handle_api_get);
    server.on("/api",HTTPMethod::HTTP_POST, handle_api_post);
    server.on("/api/certs",HTTPMethod::HTTP_POST, handle_certs);
    server.on("/api/cats", HTTPMethod::HTTP_GET, handle_api_cats);
    server.on("/api/update", HTTPMethod::HTTP_POST, handle_update, handle_update_upload);
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
        val = constrain(val, min(OCF_SERVO_INSIDE_LOCKED, OCF_SERVO_INSIDE_UNLOCKED),
                             max(OCF_SERVO_INSIDE_LOCKED, OCF_SERVO_INSIDE_UNLOCKED));
        OCFLock::enableServo(OCFDirection::INWARD);
        OCFLock::moveServoInside(val);
    }else{
        val = constrain(val, min(OCF_SERVO_OUTSIDE_UNLOCKED, OCF_SERVO_OUTSIDE_LOCKED),
                             max(OCF_SERVO_OUTSIDE_UNLOCKED, OCF_SERVO_OUTSIDE_LOCKED));
        OCFLock::enableServo(OCFDirection::OUTWARD);
        OCFLock::moveServoOutside(val);
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
void OCFWebserver::handle_update_upload(){
    HTTPUpload& upload = server.upload();
    switch(upload.status){
        case UPLOAD_FILE_START:
            update_rejected = false;
            update_suspended = OCFFlapControl::suspend();
            if(!update_suspended){
                log_d("Flap busy, rejecting firmware update");
                update_rejected = true;
                return;
            }
            log_d("Starting firmware update from %s", upload.filename.c_str());
            if(!Update.begin(UPDATE_SIZE_UNKNOWN, U_FLASH)) log_e("Update begin failed: %s", Update.errorString());
            break;
        case UPLOAD_FILE_WRITE:
            if(!update_suspended || Update.hasError()) return;
            if(Update.write(upload.buf, upload.currentSize) != upload.currentSize) log_e("Update write failed: %s", Update.errorString());
            break;
        case UPLOAD_FILE_END:
            if(!update_suspended || Update.hasError()) return;
            if(Update.end(true)) log_d("Firmware update written, %u bytes", (unsigned)upload.totalSize);
            else log_e("Update end failed: %s", Update.errorString());
            break;
        case UPLOAD_FILE_ABORTED:
            log_e("Firmware upload aborted");
            Update.abort();
            if(update_suspended) OCFFlapControl::resume();
            update_suspended = false;
            break;
    }
}

void OCFWebserver::handle_update(){
    if(update_rejected){
        update_rejected = false;
        server.send(409, "text/html", "Flap busy, try again later");
        return;
    }
    if(!update_suspended || Update.hasError() || !Update.isFinished()){
        String error = Update.hasError() ? Update.errorString() : "no firmware received";
        Update.abort();
        if(update_suspended) OCFFlapControl::resume();
        update_suspended = false;
        server.send(500, "text/html", "Update failed: " + error);
        return;
    }
    server.send(200, "text/html", "Update ok, rebooting");
    delay(500);
    ESP.restart();
}

void OCFWebserver::handle_api_post(){
    log_d("Received POST request on /api");
    if(!server.hasArg("plain")){
        log_d("Received no request body, return 400");
        server.send(400, "text/html", "No body received.");
        return;
    }
    if(server.arg("plain").length() > OCF_MAX_JSON_SIZE){
        log_d("Request body too large, return 413");
        server.send(413, "text/html", "Request body too large");
        return;
    }
    copyString(data_buf, sizeof(data_buf), server.arg("plain").c_str());
    log_d("Data: %s", data_buf);
    StaticJsonDocument<OCF_MAX_JSON_SIZE> doc;
    DeserializationError err = deserializeJson(doc, data_buf);
    if(err){
        log_d("Failed to parse %s: %s", data_buf, err.c_str());
        server.send(500, "text/html", "failed to parse data json");
    }else{
        handle_command(doc);
    }
    doc.clear();
    memset(data_buf, 0, sizeof(data_buf));
}

void OCFWebserver::handle_command(StaticJsonDocument<OCF_MAX_JSON_SIZE>& doc){
    if(!doc.containsKey("command")){
        log_d("data json missing command field: %s", data_buf);
        server.send(400, "text/html", "data json missing command field");
        return;
    }
    String command = doc["command"].as<String>();
    if(command == "wifiConfig"){
        log_d("configuring wifi...");
        OCFWifi::configure(doc);
        server.send(202, "text/html", "Configuring wifi...");
        log_d("configured wifi, reconnecting to wifi...");
        OCFWifi::reconnect();
    }else if(command == "mqttConfig"){
        log_d("configuring mqtt...");
        if(!OCFMQTT::configure(doc)){
            server.send(400, "text/html", "data json missing field");
            return;
        }
        server.send(202, "text/html", "Configuring mqtt...");
        OCFMQTT::reconnect();
    }else if(command == "setAllowState"){
        if(!doc.containsKey("direction") || !doc.containsKey("allowed")){
            log_d("data json missing direction or allowed field: %s", data_buf);
            server.send(400, "text/html", "data json missing direction or allowed field");
            return;
        }
        OCFDirection direction = parseDirection(doc["direction"].as<String>());
        if(direction == OCFDirection::NONE){
            server.send(400, "text/html", "direction must be in, out or both");
            return;
        }
        bool allowed = doc["allowed"].as<bool>();
        {
            OCFState::Guard guard;
            if(direction == OCFDirection::INWARD || direction == OCFDirection::BOTH) OCFState::config.allow_in = allowed;
            if(direction == OCFDirection::OUTWARD || direction == OCFDirection::BOTH) OCFState::config.allow_out = allowed;
            OCFState::saveConfig();
        }
        server.send(200, "text/html", "api post");
    }else if(command == "status"){
        String str;
        OCFState::getStateJSON(str);
        server.send(200, "application/json", str);
    }else if(command == "cat"){
        if(!doc.containsKey("rfid") || !doc.containsKey("name")){
            log_d("data json missing rfid and name: %s", data_buf);
            server.send(400, "text/html", "data json missing rfid or name field");
            return;
        }
        unsigned long long id = doc["rfid"].as<unsigned long long>();
        {
            OCFState::Guard guard;
            std::map<unsigned long long, OCFCat>::iterator it = OCFState::cats.find(id);
            OCFCat cat = (it != OCFState::cats.end()) ? it->second : OCFCat();
            copyString(cat.name, sizeof(cat.name), doc["name"].as<const char*>());
            cat.rfid = id;
            if (doc.containsKey("allowed_in")) cat.allow_in = doc["allowed_in"].as<bool>();
            if (doc.containsKey("allowed_out")) cat.allow_out = doc["allowed_out"].as<bool>();
            OCFState::cats.insert_or_assign(cat.rfid, cat);
            OCFState::saveCats();
        }
        server.send(200, "text/html", "api post");
    }else{
        log_d("Unknown command %s", command.c_str());
        server.send(400, "text/html", "unknown command");
    }
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
