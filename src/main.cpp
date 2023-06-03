#include <Arduino.h>
#include <OCFFilesystem.h>
#include <OCFWifi.h>
#include <OCFWebserver.h>

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  OCFFilesystem::initSPIFFS();
  OCFWifi::init();  // start flap task
  // start Wifi monitor task  
  xTaskCreate(OCFWifi::monitorWifi,"monitorwifi",2500,NULL, 2, NULL);
  // start Webserver task
  OCFWebserver::init();
  xTaskCreate(OCFWebserver::loop, "webserver", 10000, NULL, 3, NULL);
}

void loop() {
}
