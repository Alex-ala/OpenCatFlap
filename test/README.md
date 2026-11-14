# Tests

| Command | What it checks | Needs |
|---|---|---|
| `pio test -e native` | All firmware logic in `src/` on the host, against fakes of the Arduino core, ESP-IDF drivers and network libs in `test/mocks/`. ArduinoJson and ArxContainer are the real libraries, same version spec as the firmware (`[deps]` in `platformio.ini`). | nothing |
| `pio run -e arduino_nano_esp32` | Firmware still compiles against the current espressif32 platform, Arduino core, ServoESP32, PubSubClient, NTPClient. | nothing |
| `pio test -e nano_esp32_test` | PCNT counts the real EM4095 clock, LEDC servo PWM survives repeated park cycles, SPIFFS + ArduinoJson round trip with 64-bit tag ids on the real target. | flap board on USB |

After a dependency update, run the first three. `pio pkg update` pulls the newest versions the specs in `platformio.ini` allow.

The RFID suite feeds the frame captured in `debug rfid.txt` (tag 900 158002010860) through the real ISR, PCNT and queue path.

`nano_esp32_test` reads the clock from both readers and moves both servos to their locked position; it writes only `/ocf_test.*` on SPIFFS and removes them afterwards.
