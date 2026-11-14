#include <ocf_test_support.h>

using namespace ocf_test;

void setUp() { reset(); }
void tearDown() {}

void test_configure_requires_ssid()
{
    auto doc = request(R"({"passphrase":"pw"})");
    TEST_ASSERT_FALSE(OCFWifi::configure(doc));
    TEST_ASSERT_FALSE(OCFWifi::configured);
    TEST_ASSERT_EQUAL(0, mock::files.count(WIFI_CONFIG_FILE));
}

void test_configure_stores_fields_and_defaults()
{
    auto doc = request(R"({"ssid":"Bockenburg","passphrase":"pw","ntpServer":"10.5.0.1"})");
    TEST_ASSERT_TRUE(OCFWifi::configure(doc));
    TEST_ASSERT_TRUE(OCFWifi::configured);
    TEST_ASSERT_EQUAL_STRING("Bockenburg", OCFWifi::config.ssid.c_str());
    TEST_ASSERT_EQUAL_STRING("pw", OCFWifi::config.passphrase.c_str());
    TEST_ASSERT_EQUAL_STRING("", OCFWifi::config.ip.c_str());
    TEST_ASSERT_EQUAL_STRING("255.255.255.0", OCFWifi::config.netmask.c_str());
    TEST_ASSERT_EQUAL_STRING("10.5.0.1", OCFWifi::config.ntpServer.c_str());
}

void test_configure_persists_request_verbatim()
{
    auto doc = request(R"({"command":"wifiConfig","ssid":"Bockenburg","passphrase":"","ntpServer":"10.5.0.1"})");
    OCFWifi::configure(doc);
    TEST_ASSERT_EQUAL_STD_STRING(R"({"command":"wifiConfig","ssid":"Bockenburg","passphrase":"","ntpServer":"10.5.0.1"})",
                                 file(WIFI_CONFIG_FILE));
}

void test_configure_empty_ntp_keeps_previous()
{
    auto first = request(R"({"ssid":"a","ntpServer":"10.5.0.1"})");
    OCFWifi::configure(first);
    auto second = request(R"({"ssid":"a","ntpServer":""})");
    OCFWifi::configure(second);
    TEST_ASSERT_EQUAL_STRING("10.5.0.1", OCFWifi::config.ntpServer.c_str());
}

void test_init_without_config_opens_setup_ap()
{
    OCFWifi::init();
    TEST_ASSERT_FALSE(OCFWifi::configured);
    TEST_ASSERT_TRUE(mock::wifi.ap_started);
    TEST_ASSERT_EQUAL_STD_STRING("OpenCatFlap", mock::wifi.ap_ssid);
    TEST_ASSERT_FALSE(mock::wifi.ap_has_passphrase);
    TEST_ASSERT_FALSE(mock::wifi.begin_called);
}

void test_init_with_saved_config_joins_network()
{
    put_file(WIFI_CONFIG_FILE, R"({"ssid":"Bockenburg","passphrase":"pw","ntpServer":"10.5.0.1"})");
    OCFWifi::init();
    TEST_ASSERT_TRUE(OCFWifi::configured);
    TEST_ASSERT_TRUE(OCFWifi::connected);
    TEST_ASSERT_EQUAL(WIFI_STA, mock::wifi.mode);
    TEST_ASSERT_EQUAL_STD_STRING("Bockenburg", mock::wifi.ssid);
    TEST_ASSERT_EQUAL_STD_STRING("pw", mock::wifi.passphrase);
    TEST_ASSERT_FALSE(mock::wifi.ap_started);
    TEST_ASSERT_FALSE(mock::wifi.static_config_called);
    TEST_ASSERT_EQUAL_STD_STRING("10.5.0.1", mock::ntp.pool_server);
    TEST_ASSERT_GREATER_THAN(0, mock::ntp.updates);
}

void test_connect_times_out_after_10s()
{
    auto doc = request(R"({"ssid":"Nowhere"})");
    OCFWifi::configure(doc);
    mock::wifi.connect_on_begin = false;
    uint32_t start = millis();
    TEST_ASSERT_FALSE(OCFWifi::connectWifi());
    TEST_ASSERT_GREATER_OR_EQUAL(10000, millis() - start);
    TEST_ASSERT_LESS_THAN(11000, millis() - start);
    TEST_ASSERT_FALSE(mock::ntp.pool_server_set);
}

void test_static_gateway_and_netmask_are_applied()
{
    auto doc = request(R"({"ssid":"s","ip":"10.5.0.60","gateway":"10.5.0.1","netmask":"255.255.0.0"})");
    OCFWifi::configure(doc);
    TEST_ASSERT_TRUE(OCFWifi::connectWifi());
    TEST_ASSERT_TRUE(mock::wifi.static_config_called);
    TEST_ASSERT_EQUAL_STRING("10.5.0.1", mock::wifi.static_gateway.toString().c_str());
    TEST_ASSERT_EQUAL_STRING("255.255.0.0", mock::wifi.static_netmask.toString().c_str());
}

void test_static_config_failure_aborts_connect()
{
    auto doc = request(R"({"ssid":"s","ip":"10.5.0.60"})");
    OCFWifi::configure(doc);
    mock::wifi.config_result = false;
    TEST_ASSERT_FALSE(OCFWifi::connectWifi());
    TEST_ASSERT_FALSE(mock::wifi.begin_called);
}

void test_reconnect_disconnects_then_connects()
{
    auto doc = request(R"({"ssid":"s"})");
    OCFWifi::configure(doc);
    OCFWifi::reconnect();
    TEST_ASSERT_EQUAL(1, mock::wifi.disconnects);
    TEST_ASSERT_EQUAL(1, mock::wifi.begin_calls);
}

void test_epoch_time_comes_from_ntp()
{
    mock::ntp.epoch = 1761800000UL;
    TEST_ASSERT_EQUAL(1761800000UL, OCFWifi::getEpochTime());
}


void test_static_ip_is_applied()
{
    auto doc = request(R"({"ssid":"s","ip":"10.5.0.60","gateway":"10.5.0.1"})");
    OCFWifi::configure(doc);
    TEST_ASSERT_TRUE(OCFWifi::connectWifi());
    TEST_ASSERT_EQUAL_STRING("10.5.0.60", mock::wifi.static_ip.toString().c_str());
    TEST_ASSERT_EQUAL_STRING("255.255.255.0", mock::wifi.static_netmask.toString().c_str());
}

void test_invalid_static_address_aborts_connect()
{
    const char *configs[] = {
        R"({"ssid":"s","ip":"10.5.0.600"})",
        R"({"ssid":"s","ip":"10.5.0.60","gateway":"gateway"})",
        R"({"ssid":"s","ip":"10.5.0.60","netmask":"255.255"})",
    };
    for (const char *json : configs)
    {
        mock::reset_wifi();
        auto doc = request(json);
        OCFWifi::configure(doc);
        TEST_ASSERT_FALSE_MESSAGE(OCFWifi::connectWifi(), json);
        TEST_ASSERT_FALSE(mock::wifi.static_config_called);
        TEST_ASSERT_FALSE(mock::wifi.begin_called);
    }
}

void test_default_ntp_server_is_used()
{
    auto doc = request(R"({"ssid":"s"})");
    OCFWifi::configure(doc);
    OCFWifi::connectWifi();
    TEST_ASSERT_EQUAL_STD_STRING(OCF_DEFAULT_NTP_SERVER, mock::ntp.pool_server);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_configure_requires_ssid);
    RUN_TEST(test_configure_stores_fields_and_defaults);
    RUN_TEST(test_configure_persists_request_verbatim);
    RUN_TEST(test_configure_empty_ntp_keeps_previous);
    RUN_TEST(test_init_without_config_opens_setup_ap);
    RUN_TEST(test_init_with_saved_config_joins_network);
    RUN_TEST(test_connect_times_out_after_10s);
    RUN_TEST(test_static_gateway_and_netmask_are_applied);
    RUN_TEST(test_static_config_failure_aborts_connect);
    RUN_TEST(test_reconnect_disconnects_then_connects);
    RUN_TEST(test_epoch_time_comes_from_ntp);
    RUN_TEST(test_static_ip_is_applied);
    RUN_TEST(test_invalid_static_address_aborts_connect);
    RUN_TEST(test_default_ntp_server_is_used);
    return UNITY_END();
}
