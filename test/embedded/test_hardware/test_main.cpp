#include <Arduino.h>
#include <SPIFFS.h>
#include <unity.h>

#include <definitions.h>
#include <OCFFilesystem.h>
#include <OCFLock.h>
#include <OCFRFID.h>

static const char *TMP_JSON = "/ocf_test.json";
static const char *TMP_TEXT = "/ocf_test.txt";

void setUp() {}
void tearDown()
{
    SPIFFS.remove(TMP_JSON);
    SPIFFS.remove(TMP_TEXT);
}

static int16_t count_clock(int dmod_pin, pcnt_unit_t unit, uint32_t window_ms)
{
    detachInterrupt(dmod_pin);
    pcnt_counter_clear(unit);
    delay(window_ms);
    int16_t count = 0;
    pcnt_get_counter_value(unit, &count);
    return count;
}

static void assert_clock_near_134khz(int16_t count, uint32_t window_ms)
{
    int32_t expected = 134200L * window_ms / 1000;
    TEST_ASSERT_INT32_WITHIN_MESSAGE(expected / 10, expected, count, "EM4095 clock not counted near 134.2 kHz");
}

void test_pcnt_counts_outside_reader_clock()
{
    OCFRFID::enable(OCFDirection::INWARD);
    delay(20);
    assert_clock_near_134khz(count_clock(OCF_RFID_OUTSIDE_DMOD, PCNT_UNIT_0, 50), 50);
    OCFRFID::disable(OCFDirection::INWARD);
}

void test_pcnt_counts_inside_reader_clock()
{
    OCFRFID::enable(OCFDirection::OUTWARD);
    delay(20);
    assert_clock_near_134khz(count_clock(OCF_RFID_INSIDE_DMOD, PCNT_UNIT_1, 50), 50);
    OCFRFID::disable(OCFDirection::OUTWARD);
}

void test_pcnt_paused_when_reader_disabled()
{
    OCFRFID::enable(OCFDirection::INWARD);
    OCFRFID::disable(OCFDirection::INWARD);
    pcnt_counter_clear(PCNT_UNIT_0);
    delay(20);
    int16_t count = -1;
    pcnt_get_counter_value(PCNT_UNIT_0, &count);
    TEST_ASSERT_EQUAL(0, count);
}

static int sample_highs(int pin, uint32_t window_ms)
{
    int highs = 0;
    uint32_t start = millis();
    while (millis() - start < window_ms)
        if (digitalRead(pin) == HIGH) highs++;
    return highs;
}

void test_servo_pwm_survives_repeated_park_cycles()
{
    OCFLock::init();
    for (int cycle = 0; cycle < 3 * SOC_LEDC_CHANNEL_NUM; cycle++)
    {
        OCFLock::enableServo(OCFDirection::INWARD);
        OCFLock::enableServo(OCFDirection::OUTWARD);
        TEST_ASSERT_GREATER_THAN_MESSAGE(0, sample_highs(OCF_SERVO_INSIDE_PIN, 45), "servo in has no PWM");
        TEST_ASSERT_GREATER_THAN_MESSAGE(0, sample_highs(OCF_SERVO_OUTSIDE_PIN, 45), "servo out has no PWM");
        OCFLock::disableServo(cycle % 2 ? OCFDirection::INWARD : OCFDirection::OUTWARD);
        OCFLock::disableServo(OCFDirection::BOTH);
        TEST_ASSERT_EQUAL(0, sample_highs(OCF_SERVO_INSIDE_PIN, 45));
        TEST_ASSERT_EQUAL(0, sample_highs(OCF_SERVO_OUTSIDE_PIN, 45));
    }
}

void test_spiffs_json_round_trip_with_64bit_ids()
{
    DynamicJsonDocument doc(OCF_MAX_JSON_SIZE);
    JsonArray arr = doc.to<JsonArray>();
    JsonObject cat = arr.add<JsonObject>();
    cat["name"] = "Mia";
    cat["rfid"] = 900158002010860ULL;
    cat["max"] = 0xFFFFFFFFFFFFFFFFULL;
    TEST_ASSERT_TRUE(OCFFilesystem::writeJsonFile(TMP_JSON, doc));
    TEST_ASSERT_EQUAL_STRING(R"([{"name":"Mia","rfid":900158002010860,"max":18446744073709551615}])",
                             OCFFilesystem::readStringFile(TMP_JSON).c_str());

    StaticJsonDocument<OCF_MAX_JSON_SIZE> read;
    TEST_ASSERT_TRUE(OCFFilesystem::readJsonFile(TMP_JSON, read));
    TEST_ASSERT_TRUE(read[0]["rfid"].as<unsigned long long>() == 900158002010860ULL);
    TEST_ASSERT_TRUE(read[0]["max"].as<unsigned long long>() == 0xFFFFFFFFFFFFFFFFULL);
}

void test_spiffs_string_round_trip()
{
    String pem = "-----BEGIN CERTIFICATE-----\n";
    for (int i = 0; i < 40; i++) pem += "MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEA0123456789abcdef\n";
    pem += "-----END CERTIFICATE-----\n";
    TEST_ASSERT_TRUE(OCFFilesystem::writeStringFile(TMP_TEXT, pem.c_str()));
    TEST_ASSERT_EQUAL(pem.length(), OCFFilesystem::getFileSize(TMP_TEXT));
    TEST_ASSERT_EQUAL_STRING(pem.c_str(), OCFFilesystem::readStringFile(TMP_TEXT).c_str());
}

void test_heap_is_stable_across_json_cycles()
{
    DynamicJsonDocument doc(OCF_MAX_JSON_SIZE);
    doc["x"] = 1;
    OCFFilesystem::writeJsonFile(TMP_JSON, doc);
    uint32_t before = esp_get_free_heap_size();
    for (int i = 0; i < 50; i++)
    {
        StaticJsonDocument<OCF_MAX_JSON_SIZE> read;
        OCFFilesystem::readJsonFile(TMP_JSON, read);
        OCFFilesystem::writeJsonFile(TMP_JSON, read);
    }
    TEST_ASSERT_INT32_WITHIN(1024, before, esp_get_free_heap_size());
}

static void assert_firmware_frequency(OCFDirection direction, pcnt_unit_t unit, int dmod_pin, OCFRFIDFrequency (*read)())
{
    OCFRFID::enable(direction);
    uint32_t start = millis();
    while (millis() - start < OCF_RFID_SETTLE_MS + OCF_RFID_FREQ_WINDOW_MS + 100)
    {
        OCFRFID::process();
        delay(10);
    }
    OCFRFIDFrequency f = read();
    int32_t raw = 1000L * count_clock(dmod_pin, unit, 50) / 50;
    OCFRFID::disable(direction);
    TEST_ASSERT_GREATER_OR_EQUAL_MESSAGE(100000, f.hz, "below EM4095 PLL range");
    TEST_ASSERT_LESS_OR_EQUAL_MESSAGE(150000, f.hz, "above EM4095 PLL range");
    TEST_ASSERT_INT32_WITHIN_MESSAGE(raw / 100, raw, (int32_t)f.hz, "firmware readout disagrees with raw PCNT count");
    TEST_ASSERT_LESS_THAN(500, f.age_ms);
}

void test_firmware_reports_outside_frequency()
{
    assert_firmware_frequency(OCFDirection::INWARD, PCNT_UNIT_0, OCF_RFID_OUTSIDE_DMOD, OCFRFID::outsideFrequency);
}

void test_firmware_reports_inside_frequency()
{
    assert_firmware_frequency(OCFDirection::OUTWARD, PCNT_UNIT_1, OCF_RFID_INSIDE_DMOD, OCFRFID::insideFrequency);
}

void setup()
{
    delay(2000);
    OCFFilesystem::initSPIFFS();
    OCFRFID::init(nullptr, nullptr);
    UNITY_BEGIN();
    RUN_TEST(test_spiffs_json_round_trip_with_64bit_ids);
    RUN_TEST(test_spiffs_string_round_trip);
    RUN_TEST(test_heap_is_stable_across_json_cycles);
    RUN_TEST(test_servo_pwm_survives_repeated_park_cycles);
    RUN_TEST(test_pcnt_paused_when_reader_disabled);
    RUN_TEST(test_pcnt_counts_outside_reader_clock);
    RUN_TEST(test_pcnt_counts_inside_reader_clock);
    RUN_TEST(test_firmware_reports_outside_frequency);
    RUN_TEST(test_firmware_reports_inside_frequency);
    UNITY_END();
}

void loop() {}
