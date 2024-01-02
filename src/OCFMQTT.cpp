#include <OCFMQTT.h>

bool OCFMQTT::connected = false;
bool OCFMQTT::configured = false;
PubSubClient OCFMQTT::mqttclient;
WiFiClientSecure OCFMQTT::mqtt_secure;
WiFiClient OCFMQTT::mqtt;
MQTTConfiguration OCFMQTT::config;
// Load Wifi settings, create a configuration object and initalize state variables
void OCFMQTT::init() {
    StaticJsonDocument<OCF_MAX_JSON_SIZE> doc;
    bool file_read = OCFFilesystem::readJsonFile(MQTT_CONFIG_FILE, doc);
    if (configure(doc)){
        connected = connectMQTT();
    }else{
        configured = false;
    }
}

bool OCFMQTT::configure(StaticJsonDocument<OCF_MAX_JSON_SIZE>& doc){
    log_d("%s", config.name);
    if(doc.containsKey("name")) config.name = doc["name"].as<const char*>();
    log_d("%s", config.name);
    if(doc.containsKey("logActivity")) config.logActivity = doc["logActivity"].as<bool>();
    if(doc.containsKey("server")) config.server = doc["server"].as<const char*>();
    if(doc.containsKey("port")) config.port = doc["port"].as<int>();
    if(doc.containsKey("user")) config.user = doc["user"].as<const char*>();
    if(doc.containsKey("password")) config.password = doc["password"].as<const char*>();
    if(doc.containsKey("ssl")) config.ssl = doc["ssl"].as<bool>();
    int size = OCFFilesystem::getFileSize(OCF_MQTT_CA_PATH);
    char* tmp = (char*)malloc(sizeof(char)*size);
    OCFFilesystem::readStringFile(OCF_MQTT_CA_PATH, tmp, size);
    config.ca = tmp;
    size = OCFFilesystem::getFileSize(OCF_MQTT_CERT_PATH);
    char* tmp2 = (char*)malloc(sizeof(char)*size);
    OCFFilesystem::readStringFile(OCF_MQTT_CERT_PATH, tmp2, size);
    config.cert = tmp2;
    size = OCFFilesystem::getFileSize(OCF_MQTT_KEY_PATH);
    char* tmp3 = (char*)malloc(sizeof(char)*size);
    OCFFilesystem::readStringFile(OCF_MQTT_KEY_PATH, tmp3, size);
    config.key = tmp3;
    sleep(1);
    doc.clear();
    saveConfiguration();
    if (config.server == "" || config.port == 0){
        configured = false;
        return false;
    }
    if (config.ssl && config.ca == ""){
        configured = false;
        return false;
    }
    configured = true;
    log_d("Configured MQTT for (%s:%d)", config.server, config.port);
    return true;
}

void OCFMQTT::saveConfiguration(){
    StaticJsonDocument<OCF_MAX_JSON_SIZE> doc;
    doc["name"] = config.name;
    doc["logActivity"] = config.logActivity;
    doc["server"] = config.server;
    doc["port"] = config.port;
    doc["user"] = config.user;
    doc["password"] = config.password;
    doc["ssl"] = config.ssl;
    OCFFilesystem::writeJsonFile(MQTT_CONFIG_FILE, doc);
    sleep(1);
    doc.clear();
}

bool OCFMQTT::connectMQTT(){
    log_d("Connecting to mqtt %s", config.server);
    mqttclient.setServer(config.server, config.port);
    if (config.ssl) {
        log_d("Using MQTT with certificates...");
        mqtt_secure.setCACert(config.ca);
        mqtt_secure.setCertificate(config.cert);
        mqtt_secure.setPrivateKey(config.key);
        const char* protos[] = {"mqtt", NULL};
        mqtt_secure.setAlpnProtocols(protos);
        mqttclient.setClient(mqtt_secure);
    }else{
        mqttclient.setClient(mqtt);
    }
    if (strcmp(config.user, "NONE") != 0 ){
        log_d("Connecting to mqtt with user/password %d", mqttclient.state());
        connected = mqttclient.connect("OpenCatFlap", config.user, config.password);
    }else{
        log_d("Connecting to mqtt with ssl %d", mqttclient.state());
        connected = mqttclient.connect("OpenCatFlap");
    }
    if (connected){
        log_d("Connected to MQTT at %s:%d", config.server, config.port);
        sendMessage("startup", "Connected to MQTT");
    }else{
        log_d("Failed to connect to MQTT at %s:%d", config.server, config.port);
        mqttclient.disconnect();
        return false;
    }
    return true;
}

// Indefinately long running task that checks if the wifi is connected or an AP is booted up. 
// If it is not connected for 30s, it tries to reset the Wifi functionality.
void OCFMQTT::monitorMQTT(void* params){
    while(true){
        while(connected || !configured){
            if(connected) {
                connected = mqttclient.loop();
            }
            sleep(10);
        }
        log_d("Lost MQTT connection, waiting...");
        int max_wait = 30;
        for(int i=0;i<max_wait; i++){
            sleep(1);
            if(connected) break;
        }
        if(connected) continue;
        log_d("MQTT connection timed out");
        connectMQTT();
        sleep(10);
    }
    vTaskDelete(NULL);
}

void OCFMQTT::reconnect(){
    if(connected){
        mqttclient.disconnect();
    }
    connectMQTT();
}

void OCFMQTT::sendMessage(const char *topic, const char *message){
    if (! connected) return;
    String t = "opencatflap/";
    t.concat(config.name);
    t.concat("/");
    t.concat(topic);
    const char* c_topic = t.c_str();
    log_d("Sending message via MQTT to topic %s", c_topic);
    bool success = mqttclient.publish(c_topic, message);
    if (!success) {
        log_d("Failed to send message to topic %s, len %d", c_topic, t.length());
    }
}