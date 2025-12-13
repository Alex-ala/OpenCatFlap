#include <Arduino.h>
#include <definitions.h>
#include <OCF_RFID.h>



void setup() {
    Serial.begin(9600);
    Serial.setDebugOutput(true);
    sleep(10);
    Serial.println("Starting up...");
    OCF_RFID::init();
    Serial.println("OCF_RFID initialized.");
}

void loop() {
    Serial.println("Main loop running...");
    delay(5000);

}
