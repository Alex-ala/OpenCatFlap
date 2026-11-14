#include <OCFMQTT.h>
#include <OCFWifi.h>
#include <OCFHelper.h>

volatile bool OCFMQTT::connected = false;
bool OCFMQTT::configured = false;
volatile bool OCFMQTT::reconnect_requested = false;
uint32_t OCFMQTT::last_attempt_ms = 0;
QueueHandle_t OCFMQTT::outbox = xQueueCreate(OCF_MQTT_QUEUE_LENGTH, sizeof(MQTTOutgoing));
SemaphoreHandle_t OCFMQTT::config_mutex = xSemaphoreCreateRecursiveMutex();
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
    {
        OCFMutexGuard guard(config_mutex);
        if(doc["name"].is<const char*>()) copyString(config.name, sizeof(config.name), doc["name"].as<const char*>());
        if(doc.containsKey("logActivity")) config.logActivity = doc["logActivity"].as<bool>();
        if(doc["server"].is<const char*>()) copyString(config.server, sizeof(config.server), doc["server"].as<const char*>());
        if(doc.containsKey("port")) config.port = doc["port"].as<int>();
        if(doc["user"].is<const char*>()) copyString(config.user, sizeof(config.user), doc["user"].as<const char*>());
        if(doc["password"].is<const char*>()) copyString(config.password, sizeof(config.password), doc["password"].as<const char*>());
        if(doc.containsKey("ssl")) config.ssl = doc["ssl"].as<bool>();
        config.ca = OCFFilesystem::readStringFile(OCF_MQTT_CA_PATH);
        config.cert = OCFFilesystem::readStringFile(OCF_MQTT_CERT_PATH);
        config.key = OCFFilesystem::readStringFile(OCF_MQTT_KEY_PATH);
    }
    sleep(1);
    doc.clear();
    saveConfiguration();
    OCFMutexGuard guard(config_mutex);
    if (config.server[0] == '\0' || config.port == 0){
        configured = false;
        return false;
    }
    if (config.ssl && config.ca.isEmpty()){
        configured = false;
        return false;
    }
    configured = true;
    log_d("Configured MQTT for (%s:%d)", config.server, config.port);
    return true;
}

void OCFMQTT::saveConfiguration(){
    StaticJsonDocument<OCF_MAX_JSON_SIZE> doc;
    {
        OCFMutexGuard guard(config_mutex);
        doc["name"] = config.name;
        doc["logActivity"] = config.logActivity;
        doc["server"] = config.server;
        doc["port"] = config.port;
        doc["user"] = config.user;
        doc["password"] = config.password;
        doc["ssl"] = config.ssl;
    }
    OCFFilesystem::writeJsonFile(MQTT_CONFIG_FILE, doc);
    sleep(1);
    doc.clear();
}

bool OCFMQTT::connectMQTT(){
    OCFMutexGuard guard(config_mutex);
    log_d("Connecting to mqtt %s", config.server);
    mqttclient.setServer(config.server, config.port);
    if (config.ssl) {
        log_d("Using MQTT with certificates...");
        mqtt_secure.setCACert(config.ca.c_str());
        mqtt_secure.setCertificate(config.cert.c_str());
        mqtt_secure.setPrivateKey(config.key.c_str());
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
        unsigned long ts = OCFWifi::getEpochTime();
        char message[48];
        snprintf(message, sizeof(message), "%lu Connected to MQTT", ts);
        sendMessage("startup", message);
    }else{
        log_d("Failed to connect to MQTT at %s:%d", config.server, config.port);
        mqttclient.disconnect();
        return false;
    }
    return true;
}

void OCFMQTT::serviceMQTT(){
    if (reconnect_requested) {
        reconnect_requested = false;
        if (connected) mqttclient.disconnect();
        connected = false;
        last_attempt_ms = millis();
        if (configured) connectMQTT();
    }
    if (connected) {
        publishQueued();
        connected = mqttclient.loop();
        if (!connected) {
            log_d("Lost MQTT connection, retrying in %d ms", OCF_MQTT_RETRY_MS);
            last_attempt_ms = millis();
        }
    } else if (configured && millis() - last_attempt_ms >= OCF_MQTT_RETRY_MS) {
        last_attempt_ms = millis();
        connectMQTT();
    }
}

void OCFMQTT::monitorMQTT(void* params){
    while(true){
        serviceMQTT();
        delay(OCF_MQTT_POLL_MS);
    }
    vTaskDelete(NULL);
}

void OCFMQTT::reconnect(){
    reconnect_requested = true;
}

void OCFMQTT::sendMessage(const char *topic, const char *message){
    if (!connected) return;
    MQTTOutgoing out;
    copyString(out.topic, sizeof(out.topic), topic);
    copyString(out.message, sizeof(out.message), message);
    if (xQueueSend(outbox, &out, 0) != pdTRUE) {
        log_w("MQTT outbox full, dropping message to %s", topic);
    }
}

void OCFMQTT::publishQueued(){
    MQTTOutgoing out;
    while (xQueueReceive(outbox, &out, 0) == pdTRUE) {
        String t = "opencatflap/";
        {
            OCFMutexGuard guard(config_mutex);
            t.concat(config.name);
        }
        t.concat("/");
        t.concat(out.topic);
        log_d("Sending message via MQTT to topic %s", t.c_str());
        if (!mqttclient.publish(t.c_str(), out.message)) {
            log_d("Failed to send message to topic %s, len %d", t.c_str(), t.length());
        }
    }
}
