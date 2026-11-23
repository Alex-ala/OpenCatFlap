#include <ocf_test_support.h>

using namespace ocf_test;

void setUp() { reset(); }
void tearDown() {}

void test_read_json_file_missing_returns_false()
{
    StaticJsonDocument<OCF_MAX_JSON_SIZE> doc;
    TEST_ASSERT_FALSE(OCFFilesystem::readJsonFile("/nope.json", doc));
    TEST_ASSERT_EQUAL(0, mock::open_files);
}

void test_read_json_file_rejects_directory()
{
    mock::directories.insert("/dir");
    StaticJsonDocument<OCF_MAX_JSON_SIZE> doc;
    TEST_ASSERT_FALSE(OCFFilesystem::readJsonFile("/dir", doc));
}

void test_read_json_file_rejects_garbage_and_closes()
{
    put_file("/bad.json", "{\"allowed_in\": tru");
    StaticJsonDocument<OCF_MAX_JSON_SIZE> doc;
    TEST_ASSERT_FALSE(OCFFilesystem::readJsonFile("/bad.json", doc));
    TEST_ASSERT_EQUAL(0, mock::open_files);
}

void test_read_json_file_parses()
{
    put_file("/ok.json", R"({"a":1,"b":"two","c":[true,false]})");
    StaticJsonDocument<OCF_MAX_JSON_SIZE> doc;
    TEST_ASSERT_TRUE(OCFFilesystem::readJsonFile("/ok.json", doc));
    TEST_ASSERT_EQUAL(1, doc["a"].as<int>());
    TEST_ASSERT_EQUAL_STRING("two", doc["b"].as<const char *>());
    TEST_ASSERT_TRUE(doc["c"][0].as<bool>());
    TEST_ASSERT_EQUAL(0, mock::open_files);
}

void test_write_json_file_dynamic_and_static_overloads()
{
    DynamicJsonDocument dyn(OCF_MAX_JSON_SIZE);
    dyn["x"] = 1;
    TEST_ASSERT_TRUE(OCFFilesystem::writeJsonFile("/dyn.json", dyn));
    TEST_ASSERT_EQUAL_STD_STRING(R"({"x":1})", file("/dyn.json"));

    StaticJsonDocument<OCF_MAX_JSON_SIZE> st;
    st["y"] = "z";
    TEST_ASSERT_TRUE(OCFFilesystem::writeJsonFile("/st.json", st));
    TEST_ASSERT_EQUAL_STD_STRING(R"({"y":"z"})", file("/st.json"));
    TEST_ASSERT_EQUAL(0, mock::open_files);
}

void test_write_json_file_replaces_previous_content()
{
    put_file("/f.json", R"({"old":"content that is much longer than the new one"})");
    DynamicJsonDocument doc(OCF_MAX_JSON_SIZE);
    doc["n"] = 1;
    OCFFilesystem::writeJsonFile("/f.json", doc);
    TEST_ASSERT_EQUAL_STD_STRING(R"({"n":1})", file("/f.json"));
}

void test_write_json_file_fails_when_unopenable()
{
    mock::unopenable.insert("/locked.json");
    DynamicJsonDocument doc(OCF_MAX_JSON_SIZE);
    doc["n"] = 1;
    TEST_ASSERT_FALSE(OCFFilesystem::writeJsonFile("/locked.json", doc));
}

void test_write_mounts_lazily()
{
    OCFFilesystem::initSPIFFS();
    int calls = mock::mount_calls;
    DynamicJsonDocument doc(OCF_MAX_JSON_SIZE);
    doc["n"] = 1;
    OCFFilesystem::writeJsonFile("/f.json", doc);
    TEST_ASSERT_EQUAL(calls, mock::mount_calls);
}

void test_string_file_round_trip_and_size()
{
    const char *pem = "-----BEGIN CERTIFICATE-----\nMIIB\n-----END CERTIFICATE-----\n";
    TEST_ASSERT_TRUE(OCFFilesystem::writeStringFile("/mqtt_ca", pem));
    TEST_ASSERT_EQUAL_STRING(pem, OCFFilesystem::readStringFile("/mqtt_ca").c_str());
    TEST_ASSERT_EQUAL((int)strlen(pem), OCFFilesystem::getFileSize("/mqtt_ca"));
    TEST_ASSERT_EQUAL(0, mock::open_files);
}

void test_string_file_missing()
{
    TEST_ASSERT_TRUE(OCFFilesystem::readStringFile("/none").isEmpty());
    TEST_ASSERT_EQUAL(0, OCFFilesystem::getFileSize("/none"));
}

void test_string_file_size_limit()
{
    put_file("/max", std::string(OCF_MAX_FILE_READ_SIZE, 'a'));
    put_file("/over", std::string(OCF_MAX_FILE_READ_SIZE + 1, 'a'));
    TEST_ASSERT_EQUAL(OCF_MAX_FILE_READ_SIZE, OCFFilesystem::readStringFile("/max").length());
    TEST_ASSERT_TRUE(OCFFilesystem::readStringFile("/over").isEmpty());
    TEST_ASSERT_EQUAL(0, mock::open_files);
}

void test_write_string_file_fails_when_unopenable()
{
    mock::unopenable.insert("/k");
    TEST_ASSERT_FALSE(OCFFilesystem::writeStringFile("/k", "x"));
}

void test_save_config_format()
{
    OCFState::config.allow_in = true;
    OCFState::config.allow_out = false;
    OCFState::saveConfig();
    TEST_ASSERT_EQUAL_STD_STRING(R"({"allowed_in":true,"allowed_out":false})", file(FLAP_CONFIG_FILE));
}

void test_config_round_trip()
{
    for (bool in : {false, true})
        for (bool out : {false, true})
        {
            OCFState::config.allow_in = in;
            OCFState::config.allow_out = out;
            OCFState::saveConfig();
            OCFState::config = OCFConfig();
            OCFState::config.allow_in = !in;
            OCFState::config.allow_out = !out;
            OCFState::loadConfig();
            TEST_ASSERT_EQUAL(in, OCFState::config.allow_in);
            TEST_ASSERT_EQUAL(out, OCFState::config.allow_out);
        }
}

void test_load_config_defaults_to_locked_when_missing()
{
    OCFState::config.allow_in = true;
    OCFState::config.allow_out = true;
    OCFState::loadConfig();
    TEST_ASSERT_FALSE(OCFState::config.allow_in);
    TEST_ASSERT_FALSE(OCFState::config.allow_out);
}

void test_load_config_defaults_to_locked_when_corrupt()
{
    put_file(FLAP_CONFIG_FILE, "garbage");
    OCFState::config.allow_in = true;
    OCFState::loadConfig();
    TEST_ASSERT_FALSE(OCFState::config.allow_in);
}

void test_load_config_partial_file()
{
    put_file(FLAP_CONFIG_FILE, R"({"allowed_out":true})");
    OCFState::loadConfig();
    TEST_ASSERT_FALSE(OCFState::config.allow_in);
    TEST_ASSERT_TRUE(OCFState::config.allow_out);
}

void test_save_cats_format()
{
    add_cat(make_cat("Mia", CAPTURED_TAG, true, false));
    OCFState::saveCats();
    TEST_ASSERT_EQUAL_STD_STRING(
        R"([{"name":"Mia","rfid":900158002010860,"allowed_in":true,"allowed_out":false,"last_seen":0,"location":2}])",
        file(CAT_CONFIG_FILE));
}

void test_save_cats_empty()
{
    OCFState::saveCats();
    TEST_ASSERT_EQUAL_STD_STRING("[]", file(CAT_CONFIG_FILE));
}

void test_load_cats_from_existing_flash_format()
{
    put_file(CAT_CONFIG_FILE,
             R"([{"name":"Mia","rfid":900158002010860,"allowed_in":true,"allowed_out":false,"last_seen":1700000000,"location":1},)"
             R"({"name":"Tom","rfid":276098106543210,"allowed_in":false,"allowed_out":true,"last_seen":0,"location":2}])");
    OCFState::loadCats();
    TEST_ASSERT_EQUAL(2, OCFState::cats.size());
    OCFCat &mia = OCFState::cats.at(900158002010860ULL);
    TEST_ASSERT_EQUAL_STRING("Mia", mia.name);
    TEST_ASSERT_EQUAL_UINT64(900158002010860ULL, mia.rfid);
    TEST_ASSERT_TRUE(mia.allow_in);
    TEST_ASSERT_FALSE(mia.allow_out);
    OCFCat &tom = OCFState::cats.at(276098106543210ULL);
    TEST_ASSERT_EQUAL_STRING("Tom", tom.name);
    TEST_ASSERT_FALSE(tom.allow_in);
    TEST_ASSERT_TRUE(tom.allow_out);
}

void test_load_cats_defaults_missing_permissions_to_allowed()
{
    put_file(CAT_CONFIG_FILE, R"([{"name":"Mia","rfid":1}])");
    OCFState::loadCats();
    TEST_ASSERT_TRUE(OCFState::cats.at(1).allow_in);
    TEST_ASSERT_TRUE(OCFState::cats.at(1).allow_out);
    TEST_ASSERT_EQUAL_UINT64(0, OCFState::cats.at(1).last_seen);
    TEST_ASSERT_EQUAL(OCFDirection::NONE, OCFState::cats.at(1).location);
}

void test_load_cats_truncates_long_names()
{
    put_file(CAT_CONFIG_FILE, R"([{"name":"Abcdefghijklmnopqrstuvwxyz0123456789","rfid":5}])");
    OCFState::loadCats();
    TEST_ASSERT_EQUAL_STRING("Abcdefghijklmnopqrstuvwxyz012", OCFState::cats.at(5).name);
}

void test_load_cats_missing_file_leaves_map_empty()
{
    OCFState::loadCats();
    TEST_ASSERT_EQUAL(0, OCFState::cats.size());
}

void test_cats_round_trip_preserves_64bit_ids()
{
    unsigned long long ids[] = {1ULL, 4294967295ULL, 4294967296ULL, 999999999999999ULL, 0xFFFFFFFFFFFFFFFFULL};
    int i = 0;
    for (auto id : ids)
    {
        char name[8];
        snprintf(name, sizeof(name), "cat%d", i);
        add_cat(make_cat(name, id, i % 2, !(i % 2)));
        i++;
    }
    OCFState::saveCats();
    OCFState::cats.clear();
    OCFState::loadCats();
    TEST_ASSERT_EQUAL(5, OCFState::cats.size());
    i = 0;
    for (auto id : ids)
    {
        TEST_ASSERT_EQUAL(1, OCFState::cats.count(id));
        TEST_ASSERT_EQUAL_UINT64(id, OCFState::cats.at(id).rfid);
        TEST_ASSERT_EQUAL((bool)(i % 2), OCFState::cats.at(id).allow_in);
        i++;
    }
}

void test_many_cats_fit_in_json_budget()
{
    for (int i = 0; i < 10; i++)
    {
        char name[30];
        snprintf(name, sizeof(name), "Cat with a long name %02d......", i);
        add_cat(make_cat(name, 900158002010000ULL + i));
    }
    OCFState::saveCats();
    OCFState::cats.clear();
    OCFState::loadCats();
    TEST_ASSERT_EQUAL(10, OCFState::cats.size());
}

void test_get_cats_json_shape()
{
    add_cat(make_cat("Mia", CAPTURED_TAG, true, false));
    String out;
    OCFState::getCatsJSON(out);
    JsonDocument doc = parse(out.c_str());
    TEST_ASSERT_TRUE(doc.is<JsonArray>());
    TEST_ASSERT_EQUAL(1, doc.size());
    JsonObject cat = doc[0];
    TEST_ASSERT_EQUAL_STRING("Mia", cat["name"].as<const char *>());
    TEST_ASSERT_EQUAL_UINT64(CAPTURED_TAG, cat["rfid"].as<unsigned long long>());
    TEST_ASSERT_TRUE(cat["allowed_in"].as<bool>());
    TEST_ASSERT_FALSE(cat["allowed_out"].as<bool>());
    TEST_ASSERT_TRUE(cat["last_seen"].is<unsigned long long>());
    TEST_ASSERT_EQUAL(OCFDirection::NONE, cat["location"].as<int>());
    TEST_ASSERT_NOT_EQUAL(std::string::npos, std::string(out.c_str()).find('\n'));
}

void test_get_state_json_shape()
{
    OCFFlapControl::init();
    OCFState::config.allow_in = true;
    mock::ntp.epoch = 1761800000UL;
    mock::free_heap = 123456;
    String out;
    OCFState::getStateJSON(out);
    JsonDocument doc = parse(out.c_str());
    TEST_ASSERT_TRUE(doc["allow_in"].as<bool>());
    TEST_ASSERT_FALSE(doc["allow_out"].as<bool>());
    TEST_ASSERT_EQUAL(OCFMachineStates::IDLE, doc["inward_state"].as<int>());
    TEST_ASSERT_EQUAL(OCFMachineStates::IDLE, doc["outward_state"].as<int>());
    TEST_ASSERT_EQUAL(123456, doc["mem_free_heap"].as<int>());
    TEST_ASSERT_EQUAL(1761800000UL, doc["time"].as<unsigned long>());
    TEST_ASSERT_EQUAL(0, doc["rfid"]["inside"]["hz"].as<int>());
    TEST_ASSERT_EQUAL(0, doc["rfid"]["outside"]["hz"].as<int>());
    TEST_ASSERT_FALSE(doc["rfid"]["outside"]["age_ms"].is<int>());
    TEST_ASSERT_EQUAL(0, doc["rfid"]["inside"]["edges"].as<int>());
    TEST_ASSERT_EQUAL(0, doc["rfid"]["inside"]["glitches"].as<int>());
    TEST_ASSERT_EQUAL(0, doc["rfid"]["inside"]["headers"].as<int>());
    TEST_ASSERT_EQUAL(0, doc["rfid"]["inside"]["frames"].as<int>());
    TEST_ASSERT_EQUAL(0, doc["rfid"]["inside"]["crc_errors"].as<int>());
    TEST_ASSERT_EQUAL(0, doc["rfid"]["inside"]["repaired"].as<int>());
    TEST_ASSERT_TRUE(doc["rfid"]["inside"]["repaired"].is<int>());
    TEST_ASSERT_EQUAL(0, doc["rfid"]["outside"]["edges"].as<int>());
    TEST_ASSERT_EQUAL(0, doc["rfid"]["outside"]["glitches"].as<int>());
    TEST_ASSERT_EQUAL(0, doc["rfid"]["outside"]["headers"].as<int>());
    TEST_ASSERT_EQUAL(0, doc["rfid"]["outside"]["frames"].as<int>());
    TEST_ASSERT_EQUAL(0, doc["rfid"]["outside"]["crc_errors"].as<int>());
    TEST_ASSERT_TRUE(doc["rfid"]["inside"]["frames"].is<int>());
    std::string hist = doc["rfid"]["outside"]["hist"].as<std::string>();
    TEST_ASSERT_EQUAL(OCF_RFID_HIST_BINS - 1, std::count(hist.begin(), hist.end(), ','));
}

void test_state_json_reports_measured_frequency()
{
    OCFFlapControl::init();
    OCFRFID::enable(OCFDirection::INWARD);
    run_clock(PCNT_UNIT_0, 133900, 600);
    OCFRFID::disable(OCFDirection::INWARD);
    mock::advance(2000);
    String out;
    OCFState::getStateJSON(out);
    JsonDocument doc = parse(out.c_str());
    TEST_ASSERT_EQUAL(133900, doc["rfid"]["outside"]["hz"].as<int>());
    TEST_ASSERT_GREATER_OR_EQUAL(2000, doc["rfid"]["outside"]["age_ms"].as<int>());
    TEST_ASSERT_EQUAL(0, doc["rfid"]["inside"]["hz"].as<int>());
}

void test_machine_state_enum_values_are_stable()
{
    TEST_ASSERT_EQUAL(0, OCFMachineStates::IDLE);
    TEST_ASSERT_EQUAL(1, OCFMachineStates::READING);
    TEST_ASSERT_EQUAL(2, OCFMachineStates::UNLOCKED);
}


void test_load_cats_restores_last_seen_and_location()
{
    put_file(CAT_CONFIG_FILE, R"([{"name":"Mia","rfid":900158002010860,"allowed_in":true,"allowed_out":true,"last_seen":1700000000,"location":1}])");
    OCFState::loadCats();
    TEST_ASSERT_EQUAL_UINT64(1700000000ULL, OCFState::cats.at(CAPTURED_TAG).last_seen);
    TEST_ASSERT_EQUAL(OCFDirection::OUTWARD, OCFState::cats.at(CAPTURED_TAG).location);
}

void test_cats_round_trip_last_seen()
{
    OCFCat cat = make_cat("Mia", CAPTURED_TAG);
    cat.last_seen = 1761800000ULL;
    add_cat(cat);
    OCFState::saveCats();
    OCFState::cats.clear();
    OCFState::loadCats();
    TEST_ASSERT_EQUAL_UINT64(1761800000ULL, OCFState::cats.at(CAPTURED_TAG).last_seen);
}

void test_corrupt_cat_file_does_not_create_phantom_cat()
{
    put_file(CAT_CONFIG_FILE, "[{\"name\":");
    OCFState::loadCats();
    TEST_ASSERT_EQUAL(0, OCFState::cats.size());
}

void test_state_operations_take_and_release_lock()
{
    put_file(CAT_CONFIG_FILE, R"([{"name":"Mia","rfid":1}])");
    OCFFlapControl::init();
    std::vector<std::function<void()>> ops = {
        [] { OCFState::loadConfig(); },
        [] { OCFState::saveConfig(); },
        [] { OCFState::loadCats(); },
        [] { OCFState::saveCats(); },
        [] { String s; OCFState::getCatsJSON(s); },
        [] { String s; OCFState::getStateJSON(s); },
    };
    for (auto &op : ops)
    {
        int takes = OCFState::mutex->takes;
        op();
        TEST_ASSERT_GREATER_THAN(takes, OCFState::mutex->takes);
        TEST_ASSERT_EQUAL(0, mock::semaphores_held);
    }
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_read_json_file_missing_returns_false);
    RUN_TEST(test_read_json_file_rejects_directory);
    RUN_TEST(test_read_json_file_rejects_garbage_and_closes);
    RUN_TEST(test_read_json_file_parses);
    RUN_TEST(test_write_json_file_dynamic_and_static_overloads);
    RUN_TEST(test_write_json_file_replaces_previous_content);
    RUN_TEST(test_write_json_file_fails_when_unopenable);
    RUN_TEST(test_write_mounts_lazily);
    RUN_TEST(test_string_file_round_trip_and_size);
    RUN_TEST(test_string_file_missing);
    RUN_TEST(test_string_file_size_limit);
    RUN_TEST(test_write_string_file_fails_when_unopenable);
    RUN_TEST(test_save_config_format);
    RUN_TEST(test_config_round_trip);
    RUN_TEST(test_load_config_defaults_to_locked_when_missing);
    RUN_TEST(test_load_config_defaults_to_locked_when_corrupt);
    RUN_TEST(test_load_config_partial_file);
    RUN_TEST(test_save_cats_format);
    RUN_TEST(test_save_cats_empty);
    RUN_TEST(test_load_cats_from_existing_flash_format);
    RUN_TEST(test_load_cats_defaults_missing_permissions_to_allowed);
    RUN_TEST(test_load_cats_truncates_long_names);
    RUN_TEST(test_load_cats_missing_file_leaves_map_empty);
    RUN_TEST(test_cats_round_trip_preserves_64bit_ids);
    RUN_TEST(test_many_cats_fit_in_json_budget);
    RUN_TEST(test_get_cats_json_shape);
    RUN_TEST(test_get_state_json_shape);
    RUN_TEST(test_state_json_reports_measured_frequency);
    RUN_TEST(test_machine_state_enum_values_are_stable);
    RUN_TEST(test_load_cats_restores_last_seen_and_location);
    RUN_TEST(test_cats_round_trip_last_seen);
    RUN_TEST(test_corrupt_cat_file_does_not_create_phantom_cat);
    RUN_TEST(test_state_operations_take_and_release_lock);
    return UNITY_END();
}
