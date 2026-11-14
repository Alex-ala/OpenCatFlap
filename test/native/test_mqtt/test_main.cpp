#include <ocf_test_support.h>

using namespace ocf_test;

static const char *FULL_CONFIG =
    R"({"name":"flap","server":"mosquitto.ahome","port":8011,"ssl":true,"logActivity":false,"user":"cat","password":"s3cret"})";

void setUp() { reset(); }
void tearDown() {}

static void install_certs()
{
    put_file(OCF_MQTT_CA_PATH, "CA-PEM");
    put_file(OCF_MQTT_CERT_PATH, "CERT-PEM");
    put_file(OCF_MQTT_KEY_PATH, "KEY-PEM");
}

void test_configure_full_document()
{
    install_certs();
    auto doc = request(FULL_CONFIG);
    TEST_ASSERT_TRUE(OCFMQTT::configure(doc));
    TEST_ASSERT_TRUE(OCFMQTT::configured);
    TEST_ASSERT_EQUAL_STRING("flap", OCFMQTT::config.name);
    TEST_ASSERT_EQUAL_STRING("mosquitto.ahome", OCFMQTT::config.server);
    TEST_ASSERT_EQUAL(8011, OCFMQTT::config.port);
    TEST_ASSERT_TRUE(OCFMQTT::config.ssl);
    TEST_ASSERT_FALSE(OCFMQTT::config.logActivity);
    TEST_ASSERT_EQUAL_STRING("cat", OCFMQTT::config.user);
    TEST_ASSERT_EQUAL_STRING("s3cret", OCFMQTT::config.password);
    TEST_ASSERT_EQUAL_STRING("CA-PEM", OCFMQTT::config.ca.c_str());
    TEST_ASSERT_EQUAL_STRING("CERT-PEM", OCFMQTT::config.cert.c_str());
    TEST_ASSERT_EQUAL_STRING("KEY-PEM", OCFMQTT::config.key.c_str());
}

void test_configure_persists_format()
{
    install_certs();
    auto doc = request(FULL_CONFIG);
    OCFMQTT::configure(doc);
    TEST_ASSERT_EQUAL_STD_STRING(
        R"({"name":"flap","logActivity":false,"server":"mosquitto.ahome","port":8011,"user":"cat","password":"s3cret","ssl":true})",
        file(MQTT_CONFIG_FILE));
}

void test_persisted_config_reloads_identically()
{
    install_certs();
    auto doc = request(FULL_CONFIG);
    OCFMQTT::configure(doc);
    std::string saved = file(MQTT_CONFIG_FILE);
    OCFMQTT::config = MQTTConfiguration();
    StaticJsonDocument<OCF_MAX_JSON_SIZE> reread;
    TEST_ASSERT_TRUE(OCFFilesystem::readJsonFile(MQTT_CONFIG_FILE, reread));
    TEST_ASSERT_TRUE(OCFMQTT::configure(reread));
    TEST_ASSERT_EQUAL_STD_STRING(saved, file(MQTT_CONFIG_FILE));
    TEST_ASSERT_EQUAL(8011, OCFMQTT::config.port);
}

void test_ssl_without_ca_is_rejected()
{
    auto doc = request(FULL_CONFIG);
    TEST_ASSERT_FALSE(OCFMQTT::configure(doc));
    TEST_ASSERT_FALSE(OCFMQTT::configured);
}

void test_plain_config_without_certs_is_accepted()
{
    auto doc = request(R"({"server":"10.5.0.2","port":1883,"ssl":false})");
    TEST_ASSERT_TRUE(OCFMQTT::configure(doc));
}

void test_missing_server_or_port_is_rejected()
{
    auto no_server = request(R"({"port":1883})");
    TEST_ASSERT_FALSE(OCFMQTT::configure(no_server));
    OCFMQTT::config = MQTTConfiguration();
    auto no_port = request(R"({"server":"broker"})");
    TEST_ASSERT_FALSE(OCFMQTT::configure(no_port));
}

void test_partial_update_keeps_previous_values()
{
    install_certs();
    auto doc = request(FULL_CONFIG);
    OCFMQTT::configure(doc);
    auto update = request(R"({"port":8883})");
    TEST_ASSERT_TRUE(OCFMQTT::configure(update));
    TEST_ASSERT_EQUAL(8883, OCFMQTT::config.port);
    TEST_ASSERT_EQUAL_STRING("mosquitto.ahome", OCFMQTT::config.server);
    TEST_ASSERT_EQUAL_STRING("cat", OCFMQTT::config.user);
}

void test_non_string_fields_are_ignored()
{
    auto doc = request(R"({"server":42,"port":1883,"name":["x"]})");
    TEST_ASSERT_FALSE(OCFMQTT::configure(doc));
    TEST_ASSERT_EQUAL_STRING("", OCFMQTT::config.server);
    TEST_ASSERT_EQUAL_STRING("", OCFMQTT::config.name);
}

void test_field_lengths_are_bounded()
{
    auto doc = request(R"({"server":"broker.example-domain.org","port":1,"name":"a-flap-name-that-is-too-long"})");
    OCFMQTT::configure(doc);
    TEST_ASSERT_EQUAL(sizeof(OCFMQTT::config.server) - 1, strlen(OCFMQTT::config.server));
    TEST_ASSERT_EQUAL(sizeof(OCFMQTT::config.name) - 1, strlen(OCFMQTT::config.name));
    TEST_ASSERT_EQUAL(20, sizeof(OCFMQTT::config.server));
    TEST_ASSERT_EQUAL(20, sizeof(OCFMQTT::config.name));
    TEST_ASSERT_EQUAL(20, sizeof(OCFMQTT::config.user));
    TEST_ASSERT_EQUAL(64, sizeof(OCFMQTT::config.password));
}

void test_init_without_file_stays_unconfigured()
{
    OCFMQTT::init();
    TEST_ASSERT_FALSE(OCFMQTT::configured);
    TEST_ASSERT_FALSE(OCFMQTT::connected);
    TEST_ASSERT_EQUAL(0, mock::mqtt.connects);
}

void test_init_with_saved_config_connects()
{
    put_file(MQTT_CONFIG_FILE, R"({"name":"flap","server":"10.5.0.2","port":1883,"ssl":false,"user":"NONE"})");
    OCFMQTT::init();
    TEST_ASSERT_TRUE(OCFMQTT::configured);
    TEST_ASSERT_TRUE(OCFMQTT::connected);
    TEST_ASSERT_EQUAL_STD_STRING("10.5.0.2", mock::mqtt.server);
    TEST_ASSERT_EQUAL(1883, mock::mqtt.port);
}

void test_connect_plain_with_credentials_and_announces()
{
    auto doc = request(R"({"name":"flap","server":"broker","port":1883,"user":"cat","password":"pw"})");
    OCFMQTT::configure(doc);
    mock::ntp.epoch = 1761800000UL;
    TEST_ASSERT_TRUE(OCFMQTT::connectMQTT());
    TEST_ASSERT_EQUAL_PTR(&OCFMQTT::mqtt, mock::mqtt.client);
    TEST_ASSERT_EQUAL_STD_STRING("OpenCatFlap", mock::mqtt.client_id);
    TEST_ASSERT_TRUE(mock::mqtt.used_credentials);
    TEST_ASSERT_EQUAL_STD_STRING("cat", mock::mqtt.user);
    TEST_ASSERT_EQUAL_STD_STRING("pw", mock::mqtt.password);
    auto startup = payloads("opencatflap/flap/startup");
    TEST_ASSERT_EQUAL(1, startup.size());
    TEST_ASSERT_EQUAL_STD_STRING("1761800000 Connected to MQTT", startup[0]);
}

void test_connect_user_none_skips_credentials()
{
    auto doc = request(R"({"server":"broker","port":1883,"user":"NONE"})");
    OCFMQTT::configure(doc);
    OCFMQTT::connectMQTT();
    TEST_ASSERT_FALSE(mock::mqtt.used_credentials);
}

void test_connect_ssl_installs_certificates_and_alpn()
{
    install_certs();
    auto doc = request(FULL_CONFIG);
    OCFMQTT::configure(doc);
    TEST_ASSERT_TRUE(OCFMQTT::connectMQTT());
    TEST_ASSERT_EQUAL_PTR(&OCFMQTT::mqtt_secure, mock::mqtt.client);
    TEST_ASSERT_EQUAL_STD_STRING("CA-PEM", OCFMQTT::mqtt_secure.ca_);
    TEST_ASSERT_EQUAL_STD_STRING("CERT-PEM", OCFMQTT::mqtt_secure.cert_);
    TEST_ASSERT_EQUAL_STD_STRING("KEY-PEM", OCFMQTT::mqtt_secure.key_);
    TEST_ASSERT_EQUAL(1, OCFMQTT::mqtt_secure.alpn_.size());
    TEST_ASSERT_EQUAL_STD_STRING("mqtt", OCFMQTT::mqtt_secure.alpn_[0]);
}

void test_connect_failure_disconnects_and_stays_quiet()
{
    auto doc = request(R"({"server":"broker","port":1883})");
    OCFMQTT::configure(doc);
    mock::mqtt.connect_result = false;
    TEST_ASSERT_FALSE(OCFMQTT::connectMQTT());
    TEST_ASSERT_FALSE(OCFMQTT::connected);
    TEST_ASSERT_EQUAL(1, mock::mqtt.disconnects);
    TEST_ASSERT_EQUAL(0, mock::mqtt.published.size());
}

void test_send_message_topic_layout()
{
    connect_mqtt("garage");
    OCFMQTT::sendMessage("activity", "hello");
    OCFMQTT::publishQueued();
    TEST_ASSERT_EQUAL(1, mock::mqtt.published.size());
    TEST_ASSERT_EQUAL_STD_STRING("opencatflap/garage/activity", mock::mqtt.published[0].topic);
    TEST_ASSERT_EQUAL_STD_STRING("hello", mock::mqtt.published[0].payload);
}

void test_send_message_when_disconnected_is_dropped()
{
    OCFMQTT::sendMessage("activity", "hello");
    TEST_ASSERT_EQUAL(0, mock::mqtt.published.size());
}

void test_send_message_survives_publish_failure()
{
    connect_mqtt();
    mock::mqtt.publish_result = false;
    OCFMQTT::sendMessage("in", "Reading");
    TEST_ASSERT_TRUE(OCFMQTT::connected);
}

void test_reconnect_disconnects_first_when_connected()
{
    auto doc = request(R"({"server":"broker","port":1883})");
    OCFMQTT::configure(doc);
    OCFMQTT::connectMQTT();
    int connects = mock::mqtt.connects;
    OCFMQTT::reconnect();
    OCFMQTT::serviceMQTT();
    TEST_ASSERT_EQUAL(1, mock::mqtt.disconnects);
    TEST_ASSERT_EQUAL(connects + 1, mock::mqtt.connects);
}


static void configure_plain()
{
    auto doc = request(R"({"name":"flap","server":"broker","port":1883})");
    OCFMQTT::configure(doc);
}

void test_send_message_only_queues()
{
    connect_mqtt();
    OCFMQTT::sendMessage("in", "Reading");
    TEST_ASSERT_EQUAL(0, mock::mqtt.published.size());
    TEST_ASSERT_EQUAL(1, uxQueueMessagesWaiting(OCFMQTT::outbox));
}

void test_service_publishes_queue_in_order()
{
    configure_plain();
    OCFMQTT::connectMQTT();
    OCFMQTT::publishQueued();
    mock::mqtt.published.clear();
    OCFMQTT::sendMessage("in", "Reading");
    OCFMQTT::sendMessage("activity", "Cat looks in: Mia");
    OCFMQTT::sendMessage("in", "Unlocked");
    OCFMQTT::serviceMQTT();
    TEST_ASSERT_EQUAL(3, mock::mqtt.published.size());
    TEST_ASSERT_EQUAL_STD_STRING("opencatflap/flap/in", mock::mqtt.published[0].topic);
    TEST_ASSERT_EQUAL_STD_STRING("Reading", mock::mqtt.published[0].payload);
    TEST_ASSERT_EQUAL_STD_STRING("opencatflap/flap/activity", mock::mqtt.published[1].topic);
    TEST_ASSERT_EQUAL_STD_STRING("Unlocked", mock::mqtt.published[2].payload);
    TEST_ASSERT_EQUAL(0, uxQueueMessagesWaiting(OCFMQTT::outbox));
}

void test_full_outbox_drops_without_blocking()
{
    connect_mqtt();
    for (int i = 0; i < OCF_MQTT_QUEUE_LENGTH + 5; i++) OCFMQTT::sendMessage("activity", "x");
    TEST_ASSERT_EQUAL(OCF_MQTT_QUEUE_LENGTH, uxQueueMessagesWaiting(OCFMQTT::outbox));
}

void test_long_messages_are_truncated()
{
    connect_mqtt();
    std::string longer(100, 'm');
    OCFMQTT::sendMessage("activity", longer.c_str());
    auto msgs = payloads("opencatflap/flap/activity");
    TEST_ASSERT_EQUAL(1, msgs.size());
    TEST_ASSERT_EQUAL(sizeof(MQTTOutgoing::message) - 1, msgs[0].size());
}

void test_reconnect_is_deferred_to_mqtt_task()
{
    configure_plain();
    OCFMQTT::reconnect();
    TEST_ASSERT_EQUAL(0, mock::mqtt.connects);
    OCFMQTT::serviceMQTT();
    TEST_ASSERT_EQUAL(1, mock::mqtt.connects);
    TEST_ASSERT_TRUE(OCFMQTT::connected);
    OCFMQTT::serviceMQTT();
    TEST_ASSERT_EQUAL(1, mock::mqtt.connects);
}

void test_lost_connection_retries_after_interval()
{
    configure_plain();
    OCFMQTT::connected = OCFMQTT::connectMQTT();
    mock::mqtt.loop_result = false;
    OCFMQTT::serviceMQTT();
    TEST_ASSERT_FALSE(OCFMQTT::connected);
    int connects = mock::mqtt.connects;
    mock::advance(OCF_MQTT_RETRY_MS - 1);
    OCFMQTT::serviceMQTT();
    TEST_ASSERT_EQUAL(connects, mock::mqtt.connects);
    mock::advance(1);
    mock::mqtt.loop_result = true;
    OCFMQTT::serviceMQTT();
    TEST_ASSERT_EQUAL(connects + 1, mock::mqtt.connects);
    TEST_ASSERT_TRUE(OCFMQTT::connected);
}

void test_unconfigured_never_connects()
{
    for (int i = 0; i < 5; i++)
    {
        mock::advance(OCF_MQTT_RETRY_MS);
        OCFMQTT::serviceMQTT();
    }
    TEST_ASSERT_EQUAL(0, mock::mqtt.connects);
}

void test_config_access_releases_lock()
{
    install_certs();
    auto doc = request(FULL_CONFIG);
    OCFMQTT::configure(doc);
    OCFMQTT::connectMQTT();
    OCFMQTT::serviceMQTT();
    TEST_ASSERT_GREATER_THAN(0, OCFMQTT::config_mutex->takes);
    TEST_ASSERT_EQUAL(0, mock::semaphores_held);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_configure_full_document);
    RUN_TEST(test_configure_persists_format);
    RUN_TEST(test_persisted_config_reloads_identically);
    RUN_TEST(test_ssl_without_ca_is_rejected);
    RUN_TEST(test_plain_config_without_certs_is_accepted);
    RUN_TEST(test_missing_server_or_port_is_rejected);
    RUN_TEST(test_partial_update_keeps_previous_values);
    RUN_TEST(test_non_string_fields_are_ignored);
    RUN_TEST(test_field_lengths_are_bounded);
    RUN_TEST(test_init_without_file_stays_unconfigured);
    RUN_TEST(test_init_with_saved_config_connects);
    RUN_TEST(test_connect_plain_with_credentials_and_announces);
    RUN_TEST(test_connect_user_none_skips_credentials);
    RUN_TEST(test_connect_ssl_installs_certificates_and_alpn);
    RUN_TEST(test_connect_failure_disconnects_and_stays_quiet);
    RUN_TEST(test_send_message_topic_layout);
    RUN_TEST(test_send_message_when_disconnected_is_dropped);
    RUN_TEST(test_send_message_survives_publish_failure);
    RUN_TEST(test_reconnect_disconnects_first_when_connected);
    RUN_TEST(test_send_message_only_queues);
    RUN_TEST(test_service_publishes_queue_in_order);
    RUN_TEST(test_full_outbox_drops_without_blocking);
    RUN_TEST(test_long_messages_are_truncated);
    RUN_TEST(test_reconnect_is_deferred_to_mqtt_task);
    RUN_TEST(test_lost_connection_retries_after_interval);
    RUN_TEST(test_unconfigured_never_connects);
    RUN_TEST(test_config_access_releases_lock);
    return UNITY_END();
}
