#include <OCFMQTT.h>

namespace OCFMQTT {
    const char* CONFIG_FILE = "/mqtt_configuration.json";
    bool connected = false;
    bool configured = false;
    MQTTConfiguration config = MQTTConfiguration();
    PubSubClient mqttclient;
    WiFiClientSecure mqtt_secure;
    WiFiClient mqtt;
    // Load Wifi settings, create a configuration object and initalize state variables
    void init() {
        StaticJsonDocument<OCF_MAX_JSON_SIZE> doc;
        bool file_read = OCFFilesystem::readJsonFile(CONFIG_FILE, doc);
        if (configure(doc)){
            connected = connectMQTT();
        }else{
            configured = false;
        }
    }

    bool configure(StaticJsonDocument<OCF_MAX_JSON_SIZE>& doc){
        config.ssl = false;
        config.user = "NONE";
        config.password = "NONE";
        if(doc.containsKey("server")) config.server = doc["server"].as<const char*>(); else return false;
        if(doc.containsKey("port")) config.port = doc["port"].as<int>(); else return false;
        if(doc.containsKey("user")) config.user = doc["user"].as<const char*>();
        if(doc.containsKey("password")) config.password = doc["password"].as<const char*>();
        if(doc.containsKey("ssl")) config.ssl = doc["ssl"].as<bool>(); else config.ssl = false;
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
        log_d("read all");
        OCFFilesystem::writeJsonFile(CONFIG_FILE, doc);
        sleep(1);
        doc.clear();
        configured = true;
        log_d("Configured MQTT for (%s:%d)", config.server, config.port);
        return configured;
    }

    bool connectMQTT(){
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
            //connected = mqttclient.connect("OpenCatFlap");
        }
        if (connected){
            log_d("Connected to MQTT at %s:%d", config.server, config.port);
        }else{
            log_d("Failed to connect to MQTT at %s:%d", config.server, config.port);
            mqttclient.disconnect();
            return false;
        }
        return true;
    }

    // Indefinately long running task that checks if the wifi is connected or an AP is booted up. 
    // If it is not connected for 30s, it tries to reset the Wifi functionality.
    void monitorMQTT(void* params){
        while(true){
            while(connected || !configured){
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

    void reconnect(){
        if(connected){
            mqttclient.disconnect();
        }
        connectMQTT();
    }
}