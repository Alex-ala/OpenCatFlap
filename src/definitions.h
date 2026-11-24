#define OCF_MAX_JSON_SIZE 1500
#define OCF_MAX_FILE_READ_SIZE 8192

#define OCF_SERVO_INSIDE_PIN D10
#define OCF_SERVO_INSIDE_CHANNEL 0
#define OCF_SERVO_INSIDE_LOCKED 35
#define OCF_SERVO_INSIDE_UNLOCKED 64
#define OCF_SERVO_OUTSIDE_PIN A4
#define OCF_SERVO_OUTSIDE_CHANNEL 1
#define OCF_SERVO_OUTSIDE_LOCKED 65
#define OCF_SERVO_OUTSIDE_UNLOCKED 30

// Time a DSM44 needs to reach the target angle before we may cut the pulse train.
#define OCF_SERVO_TRAVEL_MS 300

// TODO: With the new board, PIR is going LOW when motion detected. Also HIGH isnt high enough for digitalRead
#define OCF_MOTION_INSIDE_PIN D5
#define OCF_MOTION_OUTSIDE_PIN A6
#define OCF_MOTION_DELAY 200
#define OCF_MOTION_TIMEOUT_MS 10000
#define OCF_UNLOCK_TAG_TIMEOUT_MS 5000

// TODO: FlapIR can see through transparent PETG
#define OCF_FLAPIR_OUTSIDE_PIN A5
#define OCF_FLAPIR_INSIDE_PIN D9
#define OCF_FLAPIR_THRESHOLD 1000

#define OCF_TUNNELIR_INSIDE_PIN D4
#define OCF_TUNNELIR_OUTSIDE_PIN A7

#define OCF_RFID_OUTSIDE_DMOD D7
#define OCF_RFID_OUTSIDE_CLK D8
#define OCF_RFID_OUTSIDE_EN D6 // LOW to enable
#define OCF_RFID_INSIDE_DMOD D2
#define OCF_RFID_INSIDE_CLK D3
#define OCF_RFID_INSIDE_EN A2 // LOW to enable

// FDX-B runs at RF/32, so one half bit is 16 carrier cycles. DMOD intervals are rounded to
// whole half bits, which puts the short/long boundary at 24 cycles.
#define OCF_RFID_HALF_BIT_CYCLES 16
// Longest DMOD interval, in half bits, still treated as dropped pulses rather than lost signal.
// The tag is clocked by the carrier, so gap lengths stay exact; bits inside become unknown.
#define OCF_RFID_MAX_GAP_HALF_BITS 256
// DMOD intervals buffered between the ISR and the decoder (~160 ms at 6300 edges/s).
#define OCF_RFID_RING_SIZE 1024
// Bit votes are halved every this many frames so a different tag can take over.
#define OCF_RFID_HISTORY_FRAMES 4
// Decode attempts run every this many bits once a full frame of bits is in.
#define OCF_RFID_ATTEMPT_BITS 32
// Of the 24 fixed header and separator bits, this many must match, none may conflict.
#define OCF_RFID_MIN_STRUCTURE_BITS 20
// Unknown CRC-covered bits tried exhaustively. CRC-16 detects all errors of up to 3 bits,
// so 4 keeps a wrong guess passing the CRC at about 2^-16 per frame.
#define OCF_RFID_MAX_GUESS_BITS 4
// Clock edges are counted by PCNT. Filter is in APB (80 MHz) cycles: 100 = 1.25us, which
// rejects glitches while passing the 3.73us half period of the 134 kHz carrier.
// Reflected CRC-16/CCITT (Kermit) polynomial, init 0x0000, covering the first 8 data
// bytes of an ISO11784/11785 FDX-B frame. Verified against the capture in RFID.md.
#define OCF_RFID_CRC_POLY 0x8408
#define OCF_RFID_PCNT_FILTER 100
#define OCF_RFID_PCNT_LIMIT 30000
// Dwell time on one antenna before switching, when both readers are enabled.
#define OCF_RFID_SWITCH_MS 1000
// EM4095 Tset is 35 ms max from SHD low until the PLL is locked to the antenna resonance.
#define OCF_RFID_SETTLE_MS 50
#define OCF_RFID_FREQ_WINDOW_MS 500
// The free-running PCNT wraps after OCF_RFID_PCNT_LIMIT edges (~220 ms at 134 kHz); a longer
// gap between counter reads may have lost a wrap, so the frequency window restarts.
#define OCF_RFID_MAX_GAP_US 150000

#define OCF_DEBUG_PIN D20

#define OCF_DEFAULT_NTP_SERVER "pool.ntp.org"

#define OCF_OTA_VALIDATE_MS 60000 // a new image that does not survive this long is rolled back

#define OCF_MQTT_QUEUE_LENGTH 16
#define OCF_MQTT_POLL_MS 50
#define OCF_MQTT_RETRY_MS 30000

#define OCF_MQTT_CA_PATH "/mqtt_ca"
#define OCF_MQTT_CERT_PATH "/mqtt_cert"
#define OCF_MQTT_KEY_PATH "/mqtt_key"

#define WIFI_CONFIG_FILE "/wifi_configuration.json"
#define MQTT_CONFIG_FILE "/mqtt_configuration.json"
#define CAT_CONFIG_FILE "/cat_configuration.json"
#define FLAP_CONFIG_FILE "/flap_configuration.json"
