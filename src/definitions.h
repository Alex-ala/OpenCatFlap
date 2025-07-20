#define OCF_MAX_JSON_SIZE 1500

#define OCF_SERVO_IN_PIN D10
#define OCF_SERVO_IN_LOCKED 35
#define OCF_SERVO_IN_UNLOCKED 64
#define OCF_SERVO_OUT_PIN A4
#define OCF_SERVO_OUT_LOCKED 65
#define OCF_SERVO_OUT_UNLOCKED 30

// TODO: With the new board, PIR is going LOW when motion detected. Also HIGH isnt high enough for digitalRead
#define OCF_MOTION_INSIDE_PIN D5 // Requires Pullup
#define OCF_MOTION_OUTSIDE_PIN A6 // Requires Pullups
#define OCF_MOTION_DELAY 200

// TODO: FlapIR can see through transparent PETG
#define OCF_FLAPIR_OUTSIDE_PIN A5
#define OCF_FLAPIR_INSIDE_PIN D9 
#define OCF_FLAPIR_THRESHOLD 1000

#define OCF_TUNNELIR_INSIDE_PIN D11
#define OCF_TUNNELIR_OUTSIDE_PIN D12

#define OCF_RFID_INSIDE_DMOD D7
#define OCF_RFID_INSIDE_CLK D8
#define OCF_RFID_INSIDE_EN D6 // LOW to enable
#define OCF_RFID_OUTSIDE_DMOD D2
#define OCF_RFID_OUTSIDE_CLK D3
#define OCF_RFID_OUTSIDE_EN A2 // LOW to enable

#define OCF_RFID_BIT_CYCLES 32
#define OCF_RFID_BIT_JITTER 2

#define OCF_DEBUG_PIN D20

#define OCF_DEFAULT_NTP_SERVER "pool.ntp.org"

#define OCF_MQTT_CA_PATH "/mqtt_ca"
#define OCF_MQTT_CERT_PATH "/mqtt_cert"
#define OCF_MQTT_KEY_PATH "/mqtt_key"
#define WIFI_CONFIG_FILE "/wifi_configuration.json"
#define MQTT_CONFIG_FILE "/mqtt_configuration.json"
#define CAT_CONFIG_FILE "/cat_configuration.json"
#define FLAP_CONFIG_FILE "/flap_configuration.json"
