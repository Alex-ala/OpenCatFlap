#include <OCFWifi.h>

WiFiUDP OCFWifi::ntpUDP;
NTPClient OCFWifi::timeClient(OCFWifi::ntpUDP);
bool OCFWifi::configured;
bool OCFWifi::connected;
WifiConfiguration OCFWifi::config;

// Load Wifi settings, create a configuration object and initalize state variables
void OCFWifi::init() {
    StaticJsonDocument<OCF_MAX_JSON_SIZE> doc;
    bool file_read = OCFFilesystem::readJsonFile(WIFI_CONFIG_FILE, doc);
    if (configure(doc)){
        connected = connectWifi();
    }else{
        configured = false;
        setupAP();
    }
}

bool OCFWifi::configure(StaticJsonDocument<OCF_MAX_JSON_SIZE>& doc){
    if(doc.containsKey("ssid")) config.ssid = doc["ssid"].as<String>(); else return false;
    config.passphrase = (doc.containsKey("passphrase")) ? doc["passphrase"].as<String>() : "";
    config.ip = (doc.containsKey("ip")) ? doc["ip"].as<String>() : "";
    config.gateway = (doc.containsKey("gateway")) ? doc["gateway"].as<String>() : "";
    config.netmask = (doc.containsKey("netmask")) ? doc["netmask"].as<String>() : "255.255.255.0";
    if (doc.containsKey("ntpServer") && doc["ntpServer"].as<String>() != ""){
        config.ntpServer = doc["ntpServer"].as<String>();  
        log_d("Setting ntp server to %s...", config.ntpServer);
    } 
    OCFFilesystem::writeJsonFile(WIFI_CONFIG_FILE, doc);
    doc.clear();
    configured = true;
    return configured;
}


void OCFWifi::setupAP() {
    log_d("Starting access point for setup...");
    WiFi.softAP("OpenCatFlap", NULL);
    IPAddress ip = WiFi.softAPIP();
    log_d("Started access point with IP %s", ip.toString());
}

bool OCFWifi::connectWifi(){
    log_d("Connecting to wifi %s", config.ssid);
    WiFi.mode(WIFI_STA);
    if (config.ip != ""){
        IPAddress ip = IPAddress().fromString(config.ip.c_str());
        IPAddress gateway;
        IPAddress netmask;
        if(config.gateway != "") gateway.fromString(config.gateway.c_str());
        if(config.netmask != "") netmask.fromString(config.netmask.c_str());
        if (!WiFi.config(ip, gateway, netmask)){
            log_d("Failed to configure static ip/gw/mask (%s,%s,%s)",config.ip, config.gateway, config.netmask);
            return false;
        }
        log_d("Configured static ip/gw/mask(%s,%s,%s)",config.ip, config.gateway, config.netmask);
    }
    WiFi.begin(config.ssid.c_str(), config.passphrase.c_str());
    unsigned long previousMillis;
    unsigned long currentMillis = millis();
    const long wait_interval = 10000;
    previousMillis = currentMillis;
    while(WiFi.status() != WL_CONNECTED) {
        currentMillis = millis();
        if (currentMillis - previousMillis >= wait_interval) {
        log_e("Failed to connect to wifi %s.", config.ssid);
        return false;
        }
    }
    log_d("Connected to wifi %s with IP %s", config.ssid, WiFi.localIP().toString());
    timeClient.setPoolServerName(config.ntpServer.c_str());
    timeClient.update();
    log_d("Set ntp server to %s", config.ntpServer);
    timeClient.update();
    return true;
}

// Indefinately long running task that checks if the wifi is connected or an AP is booted up. 
// If it is not connected for 30s, it tries to reset the Wifi functionality.
void OCFWifi::monitorWifi(void* params){
    while(true){
        while(WiFi.status() == WL_CONNECTED || !configured){
            sleep(10);
        }
        log_d("Lost WiFi connection (%d), waiting...", WiFi.status());
        int max_wait = 30;
        for(int i=0;i<max_wait; i++){
            sleep(1);
            if(WiFi.status() == WL_CONNECTED) break;
        }
        if(WiFi.status() == WL_CONNECTED) continue;
        log_d("Wifi connection timed out (%d)", WiFi.status());
        if(!configured){
            log_d("Trying to restart access point");
            WiFi.softAPdisconnect();
            setupAP();
            sleep(10);
        }else{
            log_d("Trying to reconnect to wifi");
            reconnect();
        }

    }

    vTaskDelete(NULL);
}

void OCFWifi::reconnect(){
    WiFi.disconnect();
    connectWifi();
}

unsigned long OCFWifi::getEpochTime(){
    return timeClient.getEpochTime();
}