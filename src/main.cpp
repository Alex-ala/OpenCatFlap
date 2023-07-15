#include <Arduino.h>
#include <OCFFilesystem.h>
#include <OCFWifi.h>
#include <OCFWebserver.h>
#include <OCFFlapControl.h>
#include <OCFMQTT.h>

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  OCFFilesystem::initSPIFFS();
  // start flap task
  OCFFlapControl::init();
  xTaskCreate(OCFFlapControl::loop, "flapcontrol", 10000, NULL, 3, NULL);
  // start Wifi monitor task  
  OCFWifi::init();  
  xTaskCreate(OCFWifi::monitorWifi,"monitorwifi",2500,NULL, 2, NULL);
  // start Webserver task
  OCFWebserver::init();
  xTaskCreate(OCFWebserver::loop, "webserver", 10000, NULL, 2, NULL);
  // start MQTT monitor task  
  OCFMQTT::init();  
  xTaskCreate(OCFMQTT::monitorMQTT,"monitormqtt",2500,NULL, 2, NULL);
}

void loop() {
}
