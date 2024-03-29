#define OCF_MAX_JSON_SIZE 1500

#define OCF_SERVO_IN_PIN D2 // 5 D2
#define OCF_SERVO_IN_LOCKED 90
#define OCF_SERVO_IN_UNLOCKED 120
#define OCF_SERVO_OUT_PIN A7 //14 A7
#define OCF_SERVO_OUT_LOCKED 90
#define OCF_SERVO_OUT_UNLOCKED 60

#define OCF_MOTION_INSIDE_PIN D3 //6
#define OCF_MOTION_OUTSIDE_PIN A6 //13
#define OCF_MOTION_DELAY 200
#define OCF_MOTION_THRESHOLD 0

#define OCF_FLAPIR_OUTSIDE_PIN A5 //12
#define OCF_FLAPIR_INSIDE_PIN D4 //7
#define OCF_FLAPIR_THRESHOLD 1000

#define OCF_RFID_OUTSIDE_PIN D0 //44
#define OCF_RFID_INSIDE_PIN D9 //18

#define OCF_CLOSE_AFTER_S 2
#define OCF_DISABLE_SERVOS_AFTER_S 30

#define OCF_PATHS_FLAP_STATE "/flapState.json"

#define OCF_DEFAULT_NTP_SERVER "pool.ntp.org"

#define OCF_MQTT_CA_PATH "/mqtt_ca"
#define OCF_MQTT_CERT_PATH "/mqtt_cert"
#define OCF_MQTT_KEY_PATH "/mqtt_key"

#define WIFI_CONFIG_FILE "/wifi_configuration.json"
#define MQTT_CONFIG_FILE "/mqtt_configuration.json"
#define CAT_CONFIG_FILE "/cat_configuration.json"