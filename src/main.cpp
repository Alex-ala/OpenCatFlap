#include <Arduino.h>
#include <OCFFilesystem.h>
#include <OCFWifi.h>
#include <OCFWebserver.h>
#include <OCFFlapControl.h>
#include <OCFMQTT.h>
#include <esp_ota_ops.h>

extern "C" bool verifyRollbackLater() { return true; }

void setup() {
  Serial.begin(9600);
  Serial.setDebugOutput(true);
  OCFFilesystem::initSPIFFS();
  sleep(20);
  Serial.write("INIT");
  // start flap task
  OCFFlapControl::init();
  xTaskCreate(OCFFlapControl::loop, "flapcontrol", 15000, NULL, 3, NULL);
  // start Wifi monitor task
  OCFWifi::init();
  xTaskCreate(OCFWifi::monitorWifi,"monitorwifi", 2500, NULL, 2, NULL);
  // start Webserver task
  OCFWebserver::init();
  xTaskCreate(OCFWebserver::loop, "webserver", 10000, NULL, 2, NULL);
  // // start MQTT monitor task
  OCFMQTT::init();
  xTaskCreate(OCFMQTT::monitorMQTT,"monitormqtt",5000,NULL, 2, NULL);
}

void loop() {
  static bool app_validated = false;
  if (!app_validated && millis() > OCF_OTA_VALIDATE_MS) {
    esp_ota_mark_app_valid_cancel_rollback();
    app_validated = true;
    log_d("Marked app valid");
  }
  delay(1000);
}
