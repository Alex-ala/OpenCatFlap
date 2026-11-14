#include <ocf_test_support.h>

using namespace ocf_test;

void setUp()
{
    reset();
    OCFLock::init();
}
void tearDown() {}

static const mock::HttpResponse &post(const std::string &uri, const std::map<std::string, std::string> &args)
{
    mock::reset_web_request();
    mock::web.args = args;
    TEST_ASSERT_TRUE_MESSAGE(mock::dispatch(uri, HTTP_POST), uri.c_str());
    TEST_ASSERT_FALSE(mock::web.responses.empty());
    return mock::web.responses.front();
}

static const mock::HttpResponse &api(const std::string &body) { return post("/api", {{"plain", body}}); }

static const mock::HttpResponse &get(const std::string &uri, const std::map<std::string, std::string> &args = {})
{
    mock::reset_web_request();
    mock::web.args = args;
    TEST_ASSERT_TRUE_MESSAGE(mock::dispatch(uri, HTTP_GET), uri.c_str());
    TEST_ASSERT_FALSE(mock::web.responses.empty());
    return mock::web.responses.front();
}

void test_routes_are_registered()
{
    std::vector<std::pair<std::string, HTTPMethod>> expected = {
        {"/debug", HTTP_ANY}, {"/api", HTTP_GET}, {"/api", HTTP_POST}, {"/api/certs", HTTP_POST}, {"/api/cats", HTTP_GET}, {"/api/update", HTTP_POST}};
    for (const auto &e : expected)
    {
        bool found = false;
        for (const auto &r : mock::web.routes)
            if (r.uri == e.first && r.method == e.second) found = true;
        TEST_ASSERT_TRUE_MESSAGE(found, e.first.c_str());
    }
    TEST_ASSERT_EQUAL_STD_STRING("/index.html", mock::web.static_routes["/"]);
    TEST_ASSERT_TRUE(mock::web.begun);
}

void test_get_api()
{
    auto r = get("/api");
    TEST_ASSERT_EQUAL(200, r.code);
}

void test_post_without_body_is_400()
{
    auto r = post("/api", {});
    TEST_ASSERT_EQUAL(400, r.code);
}

void test_post_body_too_large_is_413()
{
    auto r = api(std::string(OCF_MAX_JSON_SIZE + 1, ' '));
    TEST_ASSERT_EQUAL(413, r.code);
}

void test_post_body_at_limit_is_accepted()
{
    std::string body = R"({"command":"setAllowState","direction":"in","allowed":true,"pad":")";
    body += std::string(OCF_MAX_JSON_SIZE - body.size() - 2, 'a');
    body += "\"}";
    TEST_ASSERT_EQUAL(OCF_MAX_JSON_SIZE, body.size());
    auto r = api(body);
    TEST_ASSERT_EQUAL(200, r.code);
    TEST_ASSERT_TRUE(OCFState::config.allow_in);
}

void test_post_invalid_json_is_500()
{
    auto r = api("{command: nope");
    TEST_ASSERT_EQUAL(500, r.code);
}

void test_post_without_command_is_400()
{
    auto r = api(R"({"direction":"in"})");
    TEST_ASSERT_EQUAL(400, r.code);
}

void test_set_allow_state_in_and_out()
{
    TEST_ASSERT_EQUAL(200, api(R"({"command":"setAllowState","direction":"in","allowed":true})").code);
    TEST_ASSERT_TRUE(OCFState::config.allow_in);
    TEST_ASSERT_FALSE(OCFState::config.allow_out);
    TEST_ASSERT_EQUAL_STD_STRING(R"({"allowed_in":true,"allowed_out":false})", file(FLAP_CONFIG_FILE));

    TEST_ASSERT_EQUAL(200, api(R"({"command":"setAllowState","direction":"out","allowed":true})").code);
    TEST_ASSERT_TRUE(OCFState::config.allow_out);

    TEST_ASSERT_EQUAL(200, api(R"({"command":"setAllowState","direction":"in","allowed":false})").code);
    TEST_ASSERT_FALSE(OCFState::config.allow_in);
    TEST_ASSERT_TRUE(OCFState::config.allow_out);
}

void test_set_allow_state_requires_fields()
{
    TEST_ASSERT_EQUAL(400, api(R"({"command":"setAllowState","direction":"in"})").code);
    TEST_ASSERT_EQUAL(400, api(R"({"command":"setAllowState","allowed":true})").code);
    TEST_ASSERT_FALSE(OCFState::config.allow_in);
}

void test_status_returns_state_json()
{
    OCFFlapControl::init();
    OCFState::config.allow_out = true;
    auto r = api(R"({"command":"status"})");
    TEST_ASSERT_EQUAL(200, r.code);
    TEST_ASSERT_EQUAL_STD_STRING("application/json", r.content_type);
    JsonDocument doc = parse(r.body);
    TEST_ASSERT_TRUE(doc["allow_out"].as<bool>());
    TEST_ASSERT_TRUE(doc["inward_state"].is<int>());
    TEST_ASSERT_TRUE(doc["outward_state"].is<int>());
    TEST_ASSERT_TRUE(doc["time"].is<unsigned long>());
    TEST_ASSERT_TRUE(doc["mem_free_heap"].is<unsigned long>());
}

void test_add_cat_defaults_and_persists()
{
    auto r = api(R"({"command":"cat","rfid":900158002010860,"name":"Mia"})");
    TEST_ASSERT_EQUAL(200, r.code);
    TEST_ASSERT_EQUAL(1, OCFState::cats.count(CAPTURED_TAG));
    OCFCat &cat = OCFState::cats.at(CAPTURED_TAG);
    TEST_ASSERT_EQUAL_STRING("Mia", cat.name);
    TEST_ASSERT_TRUE(cat.allow_in);
    TEST_ASSERT_TRUE(cat.allow_out);
    TEST_ASSERT_EQUAL_STD_STRING(
        R"([{"name":"Mia","rfid":900158002010860,"allowed_in":true,"allowed_out":true,"last_seen":0,"location":2}])",
        file(CAT_CONFIG_FILE));
}

void test_update_cat_keeps_unspecified_fields()
{
    api(R"({"command":"cat","rfid":900158002010860,"name":"Mia","allowed_out":false})");
    OCFState::cats.at(CAPTURED_TAG).last_seen = 1761800000ULL;
    api(R"({"command":"cat","rfid":900158002010860,"name":"Mia the Great"})");
    OCFCat &cat = OCFState::cats.at(CAPTURED_TAG);
    TEST_ASSERT_EQUAL_STRING("Mia the Great", cat.name);
    TEST_ASSERT_TRUE(cat.allow_in);
    TEST_ASSERT_FALSE(cat.allow_out);
    TEST_ASSERT_EQUAL_UINT64(1761800000ULL, cat.last_seen);
    TEST_ASSERT_EQUAL(1, OCFState::cats.size());
}

void test_cat_requires_rfid_and_name()
{
    TEST_ASSERT_EQUAL(400, api(R"({"command":"cat","name":"Mia"})").code);
    TEST_ASSERT_EQUAL(400, api(R"({"command":"cat","rfid":1})").code);
    TEST_ASSERT_EQUAL(0, OCFState::cats.size());
    TEST_ASSERT_EQUAL(0, mock::files.count(CAT_CONFIG_FILE));
}

void test_cat_name_is_truncated()
{
    api(R"({"command":"cat","rfid":7,"name":"Abcdefghijklmnopqrstuvwxyz0123456789"})");
    TEST_ASSERT_EQUAL_STRING("Abcdefghijklmnopqrstuvwxyz012", OCFState::cats.at(7).name);
}

void test_get_cats()
{
    add_cat(make_cat("Mia", CAPTURED_TAG));
    auto r = get("/api/cats");
    TEST_ASSERT_EQUAL(200, r.code);
    TEST_ASSERT_EQUAL_STD_STRING("application/json", r.content_type);
    JsonDocument doc = parse(r.body);
    TEST_ASSERT_EQUAL_UINT64(CAPTURED_TAG, doc[0]["rfid"].as<unsigned long long>());
}

void test_mqtt_config_accepted()
{
    auto r = api(R"({"command":"mqttConfig","name":"flap","server":"10.5.0.2","port":1883,"ssl":false})");
    TEST_ASSERT_EQUAL(202, r.code);
    TEST_ASSERT_EQUAL_STRING("10.5.0.2", OCFMQTT::config.server);
    TEST_ASSERT_EQUAL(0, mock::mqtt.connects);
    OCFMQTT::serviceMQTT();
    TEST_ASSERT_GREATER_THAN(0, mock::mqtt.connects);
}

void test_mqtt_config_rejected()
{
    auto r = api(R"({"command":"mqttConfig","port":1883})");
    TEST_ASSERT_EQUAL(400, r.code);
    TEST_ASSERT_EQUAL(0, mock::mqtt.connects);
}

void test_wifi_config_accepted()
{
    auto r = api(R"({"command":"wifiConfig","ssid":"Bockenburg","passphrase":"pw"})");
    TEST_ASSERT_EQUAL(202, r.code);
    TEST_ASSERT_EQUAL_STD_STRING("Bockenburg", mock::wifi.ssid);
    TEST_ASSERT_EQUAL(1, mock::files.count(WIFI_CONFIG_FILE));
}

void test_unknown_command_changes_nothing()
{
    api(R"({"command":"selfDestruct"})");
    TEST_ASSERT_EQUAL(0, mock::files.size());
    TEST_ASSERT_FALSE(OCFState::config.allow_in);
}

void test_certs_are_stored()
{
    struct { const char *name; const char *path; } certs[] = {
        {"ca", OCF_MQTT_CA_PATH}, {"cert", OCF_MQTT_CERT_PATH}, {"key", OCF_MQTT_KEY_PATH}};
    for (auto c : certs)
    {
        std::string pem = std::string("-----BEGIN ") + c.name + "-----\nAAAA\n";
        auto r = post("/api/certs", {{"name", c.name}, {"plain", pem}});
        TEST_ASSERT_EQUAL(200, r.code);
        TEST_ASSERT_EQUAL_STD_STRING(std::string("Saved ") + c.name, r.body);
        TEST_ASSERT_EQUAL_STD_STRING(pem, file(c.path));
    }
}

void test_certs_reject_bad_requests()
{
    TEST_ASSERT_EQUAL(400, post("/api/certs", {{"name", "ca"}}).code);
    TEST_ASSERT_EQUAL(400, post("/api/certs", {{"plain", "x"}}).code);
    TEST_ASSERT_EQUAL(400, post("/api/certs", {{"name", "../wifi_configuration.json"}, {"plain", "x"}}).code);
    TEST_ASSERT_EQUAL(0, mock::files.size());
}

void test_debug_moves_servo_within_limits()
{
    get("/debug", {{"dir", "1"}, {"val", "200"}});
    TEST_ASSERT_EQUAL(max(OCF_SERVO_INSIDE_LOCKED, OCF_SERVO_INSIDE_UNLOCKED), mock::last_servo_angle(OCF_SERVO_INSIDE_PIN));
    get("/debug", {{"dir", "1"}, {"val", "-5"}});
    TEST_ASSERT_EQUAL(min(OCF_SERVO_INSIDE_LOCKED, OCF_SERVO_INSIDE_UNLOCKED), mock::last_servo_angle(OCF_SERVO_INSIDE_PIN));
    get("/debug", {{"dir", "0"}, {"val", "0"}});
    TEST_ASSERT_EQUAL(min(OCF_SERVO_OUTSIDE_LOCKED, OCF_SERVO_OUTSIDE_UNLOCKED), mock::last_servo_angle(OCF_SERVO_OUTSIDE_PIN));
    get("/debug", {{"dir", "0"}, {"val", "50"}});
    TEST_ASSERT_EQUAL(50, mock::last_servo_angle(OCF_SERVO_OUTSIDE_PIN));
}

void test_debug_requires_args()
{
    TEST_ASSERT_EQUAL(500, get("/debug", {{"dir", "1"}}).code);
}


void test_set_allow_state_both()
{
    api(R"({"command":"setAllowState","direction":"both","allowed":true})");
    TEST_ASSERT_TRUE(OCFState::config.allow_in);
    TEST_ASSERT_TRUE(OCFState::config.allow_out);
    api(R"({"command":"setAllowState","direction":"both","allowed":false})");
    TEST_ASSERT_FALSE(OCFState::config.allow_in);
    TEST_ASSERT_FALSE(OCFState::config.allow_out);
    TEST_ASSERT_EQUAL_STD_STRING(R"({"allowed_in":false,"allowed_out":false})", file(FLAP_CONFIG_FILE));
}

void test_set_allow_state_rejects_unknown_direction()
{
    OCFState::config.allow_in = true;
    TEST_ASSERT_EQUAL(400, api(R"({"command":"setAllowState","direction":"sideways","allowed":false})").code);
    TEST_ASSERT_TRUE(OCFState::config.allow_in);
    TEST_ASSERT_EQUAL(0, mock::files.count(FLAP_CONFIG_FILE));
}

void test_unknown_command_is_400()
{
    TEST_ASSERT_EQUAL(400, api(R"({"command":"selfDestruct"})").code);
}

void test_every_request_gets_exactly_one_response()
{
    OCFFlapControl::init();
    const char *bodies[] = {
        R"({"command":"wifiConfig","ssid":"s"})",
        R"({"command":"mqttConfig","server":"broker","port":1883})",
        R"({"command":"mqttConfig","port":1883})",
        R"({"command":"setAllowState","direction":"in","allowed":true})",
        R"({"command":"setAllowState","direction":"x","allowed":true})",
        R"({"command":"setAllowState"})",
        R"({"command":"status"})",
        R"({"command":"cat","rfid":1,"name":"Mia"})",
        R"({"command":"cat"})",
        R"({"command":"nope"})",
        R"({"nocommand":1})",
        "not json",
    };
    for (const char *body : bodies)
    {
        api(body);
        TEST_ASSERT_EQUAL_MESSAGE(1, mock::web.responses.size(), body);
    }
}

void test_api_mutations_hold_state_lock()
{
    int takes = OCFState::mutex->takes;
    api(R"({"command":"cat","rfid":1,"name":"Mia"})");
    api(R"({"command":"setAllowState","direction":"in","allowed":true})");
    TEST_ASSERT_GREATER_THAN(takes + 1, OCFState::mutex->takes);
    TEST_ASSERT_EQUAL(0, mock::semaphores_held);
}

static const mock::HttpResponse &upload_firmware(const std::vector<std::string> &chunks)
{
    mock::reset_web_request();
    mock::web.upload.filename = "firmware.bin";
    TEST_ASSERT_TRUE(mock::dispatch_upload("/api/update", HTTP_POST, UPLOAD_FILE_START));
    for (const auto &c : chunks)
        mock::dispatch_upload("/api/update", HTTP_POST, UPLOAD_FILE_WRITE, reinterpret_cast<const uint8_t *>(c.data()), c.size());
    mock::dispatch_upload("/api/update", HTTP_POST, UPLOAD_FILE_END);
    TEST_ASSERT_TRUE(mock::dispatch("/api/update", HTTP_POST));
    TEST_ASSERT_EQUAL(1, mock::web.responses.size());
    return mock::web.responses.front();
}

void test_update_writes_firmware_and_restarts()
{
    OCFFlapControl::init();
    auto r = upload_firmware({"abc", "defg"});
    TEST_ASSERT_EQUAL(200, r.code);
    TEST_ASSERT_EQUAL(U_FLASH, mock::update.command);
    TEST_ASSERT_EQUAL_STD_STRING("abcdefg", mock::update.written);
    TEST_ASSERT_TRUE(mock::update.finished);
    TEST_ASSERT_EQUAL(1, mock::restarts);
    TEST_ASSERT_EQUAL(1, OCFFlapControl::loop_mutex->depth);
    OCFFlapControl::resume();
}

void test_update_rejected_while_flap_busy()
{
    OCFFlapControl::init();
    OCFFlapControl::stateMachineInward->currentState = READING;
    auto r = upload_firmware({"abc"});
    TEST_ASSERT_EQUAL(409, r.code);
    TEST_ASSERT_EQUAL(0, mock::update.begins);
    TEST_ASSERT_EQUAL(0, mock::restarts);
    TEST_ASSERT_EQUAL(0, OCFFlapControl::loop_mutex->depth);
}

void test_update_error_resumes_flap()
{
    OCFFlapControl::init();
    mock::update.fail_write = true;
    auto r = upload_firmware({"abc"});
    TEST_ASSERT_EQUAL(500, r.code);
    TEST_ASSERT_EQUAL(0, mock::restarts);
    TEST_ASSERT_EQUAL(0, OCFFlapControl::loop_mutex->depth);
}

void test_update_aborted_upload_resumes_flap()
{
    OCFFlapControl::init();
    mock::reset_web_request();
    mock::dispatch_upload("/api/update", HTTP_POST, UPLOAD_FILE_START);
    mock::dispatch_upload("/api/update", HTTP_POST, UPLOAD_FILE_ABORTED);
    TEST_ASSERT_EQUAL(1, mock::update.aborts);
    TEST_ASSERT_EQUAL(0, OCFFlapControl::loop_mutex->depth);
    TEST_ASSERT_TRUE(mock::dispatch("/api/update", HTTP_POST));
    TEST_ASSERT_EQUAL(500, mock::web.responses.front().code);
    TEST_ASSERT_EQUAL(0, mock::restarts);
}

void test_update_without_file_is_500()
{
    mock::reset_web_request();
    TEST_ASSERT_TRUE(mock::dispatch("/api/update", HTTP_POST));
    TEST_ASSERT_EQUAL(500, mock::web.responses.front().code);
    TEST_ASSERT_EQUAL(0, mock::restarts);
}

int main()
{
    reset();
    OCFWebserver::init();
    UNITY_BEGIN();
    RUN_TEST(test_update_writes_firmware_and_restarts);
    RUN_TEST(test_update_rejected_while_flap_busy);
    RUN_TEST(test_update_error_resumes_flap);
    RUN_TEST(test_update_aborted_upload_resumes_flap);
    RUN_TEST(test_update_without_file_is_500);
    RUN_TEST(test_routes_are_registered);
    RUN_TEST(test_get_api);
    RUN_TEST(test_post_without_body_is_400);
    RUN_TEST(test_post_body_too_large_is_413);
    RUN_TEST(test_post_body_at_limit_is_accepted);
    RUN_TEST(test_post_invalid_json_is_500);
    RUN_TEST(test_post_without_command_is_400);
    RUN_TEST(test_set_allow_state_in_and_out);
    RUN_TEST(test_set_allow_state_requires_fields);
    RUN_TEST(test_status_returns_state_json);
    RUN_TEST(test_add_cat_defaults_and_persists);
    RUN_TEST(test_update_cat_keeps_unspecified_fields);
    RUN_TEST(test_cat_requires_rfid_and_name);
    RUN_TEST(test_cat_name_is_truncated);
    RUN_TEST(test_get_cats);
    RUN_TEST(test_mqtt_config_accepted);
    RUN_TEST(test_mqtt_config_rejected);
    RUN_TEST(test_wifi_config_accepted);
    RUN_TEST(test_unknown_command_changes_nothing);
    RUN_TEST(test_certs_are_stored);
    RUN_TEST(test_certs_reject_bad_requests);
    RUN_TEST(test_debug_moves_servo_within_limits);
    RUN_TEST(test_debug_requires_args);
    RUN_TEST(test_set_allow_state_both);
    RUN_TEST(test_set_allow_state_rejects_unknown_direction);
    RUN_TEST(test_unknown_command_is_400);
    RUN_TEST(test_every_request_gets_exactly_one_response);
    RUN_TEST(test_api_mutations_hold_state_lock);
    return UNITY_END();
}
