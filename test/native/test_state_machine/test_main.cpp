#include <ocf_test_support.h>

using namespace ocf_test;

static const unsigned long long MIA = CAPTURED_TAG;
static const unsigned long long STRAY = 250000000000001ULL;

static OCFStateMachine *inward;
static OCFStateMachine *outward;

void setUp()
{
    reset();
    OCFLock::init();
    inward = new OCFStateMachine(OCFDirection::INWARD);
    outward = new OCFStateMachine(OCFDirection::OUTWARD);
    OCFRFID::init(inward, outward);
    OCFState::config.allow_in = true;
    OCFState::config.allow_out = true;
    add_cat(make_cat("Mia", MIA));
    connect_mqtt("flap");
    mock::mqtt.published.clear();
    mock::servo_events.clear();
}

void tearDown()
{
    delete inward;
    delete outward;
}

static void to_reading(OCFStateMachine &m, OCFDirection dir)
{
    set_motion(pir_for(dir), true);
    tick(m);
    TEST_ASSERT_EQUAL(OCFMachineStates::READING, m.getState());
}

static void to_unlocked(OCFStateMachine &m, OCFDirection dir, unsigned long long tag = MIA)
{
    to_reading(m, dir);
    m.readRFID(tag);
    tick(m);
    TEST_ASSERT_EQUAL(OCFMachineStates::UNLOCKED, m.getState());
}

void test_starts_idle_with_pir_pulled_up()
{
    TEST_ASSERT_EQUAL(OCFMachineStates::IDLE, inward->getState());
    TEST_ASSERT_EQUAL(OCFMachineStates::IDLE, outward->getState());
    TEST_ASSERT_EQUAL(INPUT_PULLUP, mock::pin_mode[OCF_MOTION_OUTSIDE_PIN]);
    TEST_ASSERT_EQUAL(INPUT_PULLUP, mock::pin_mode[OCF_MOTION_INSIDE_PIN]);
}

void test_stays_idle_without_motion()
{
    tick(*inward, 500);
    TEST_ASSERT_EQUAL(OCFMachineStates::IDLE, inward->getState());
    TEST_ASSERT_EQUAL(0, mock::mqtt.published.size());
}

void test_pir_is_active_low()
{
    digitalWrite(OCF_MOTION_OUTSIDE_PIN, HIGH);
    tick(*inward);
    TEST_ASSERT_EQUAL(OCFMachineStates::IDLE, inward->getState());
    digitalWrite(OCF_MOTION_OUTSIDE_PIN, LOW);
    tick(*inward);
    TEST_ASSERT_EQUAL(OCFMachineStates::READING, inward->getState());
}

void test_inward_machine_uses_outside_pir_outside_reader_inside_lock()
{
    set_motion(OCF_MOTION_INSIDE_PIN, true);
    tick(*inward);
    TEST_ASSERT_EQUAL(OCFMachineStates::IDLE, inward->getState());

    set_motion(OCF_MOTION_OUTSIDE_PIN, true);
    tick(*inward);
    TEST_ASSERT_EQUAL(OCFMachineStates::READING, inward->getState());
    TEST_ASSERT_EQUAL(LOW, digitalRead(OCF_RFID_OUTSIDE_EN));
    TEST_ASSERT_EQUAL(HIGH, digitalRead(OCF_RFID_INSIDE_EN));
    TEST_ASSERT_TRUE(OCFLock::isArmedInside());
    TEST_ASSERT_FALSE(OCFLock::isArmedOutside());
}

void test_outward_machine_uses_inside_pir_inside_reader_outside_lock()
{
    set_motion(OCF_MOTION_OUTSIDE_PIN, true);
    tick(*outward);
    TEST_ASSERT_EQUAL(OCFMachineStates::IDLE, outward->getState());

    set_motion(OCF_MOTION_INSIDE_PIN, true);
    tick(*outward);
    TEST_ASSERT_EQUAL(OCFMachineStates::READING, outward->getState());
    TEST_ASSERT_EQUAL(LOW, digitalRead(OCF_RFID_INSIDE_EN));
    TEST_ASSERT_EQUAL(HIGH, digitalRead(OCF_RFID_OUTSIDE_EN));
    TEST_ASSERT_TRUE(OCFLock::isArmedOutside());
    TEST_ASSERT_FALSE(OCFLock::isArmedInside());
}

void test_entering_reading_arms_servo_locked_and_reports()
{
    to_reading(*inward, OCFDirection::INWARD);
    TEST_ASSERT_TRUE(OCFLock::isArmedInside());
    TEST_ASSERT_FALSE(OCFLock::isArmedOutside());
    TEST_ASSERT_EQUAL(OCF_SERVO_INSIDE_LOCKED, mock::last_servo_angle(OCF_SERVO_INSIDE_PIN));
    auto msgs = payloads("opencatflap/flap/in");
    TEST_ASSERT_EQUAL(1, msgs.size());
    TEST_ASSERT_EQUAL_STD_STRING("Reading", msgs[0]);
}

void test_reading_times_out_to_idle_after_10s_without_motion()
{
    to_reading(*inward, OCFDirection::INWARD);
    set_motion(OCF_MOTION_OUTSIDE_PIN, false);
    tick(*inward, 1, 9990);
    TEST_ASSERT_EQUAL(OCFMachineStates::READING, inward->getState());
    tick(*inward, 1, 10);
    TEST_ASSERT_EQUAL(OCFMachineStates::IDLE, inward->getState());
    TEST_ASSERT_FALSE(OCFLock::isArmedInside());
    TEST_ASSERT_TRUE(OCFLock::isLockedInside());
    TEST_ASSERT_EQUAL(HIGH, digitalRead(OCF_RFID_INSIDE_EN));
    auto msgs = payloads("opencatflap/flap/in");
    TEST_ASSERT_EQUAL_STD_STRING("Idle", msgs.back());
}

void test_motion_keeps_reading_alive()
{
    to_reading(*inward, OCFDirection::INWARD);
    for (int i = 0; i < 6; i++) tick(*inward, 1, 5000);
    TEST_ASSERT_EQUAL(OCFMachineStates::READING, inward->getState());
}

void test_allowed_cat_unlocks_its_direction()
{
    to_unlocked(*inward, OCFDirection::INWARD);
    TEST_ASSERT_EQUAL(OCF_SERVO_INSIDE_UNLOCKED, mock::last_servo_angle(OCF_SERVO_INSIDE_PIN));
    TEST_ASSERT_FALSE(OCFLock::isLockedInside());
    TEST_ASSERT_TRUE(OCFLock::isLockedOutside());
    auto msgs = payloads("opencatflap/flap/in");
    TEST_ASSERT_EQUAL_STD_STRING("Unlocked", msgs.back());
    auto activity = payloads("opencatflap/flap/activity");
    TEST_ASSERT_FALSE(activity.empty());
    TEST_ASSERT_EQUAL_STD_STRING("Cat looks in: Mia", activity[0]);
}

void test_allowed_cat_unlocks_outward()
{
    to_unlocked(*outward, OCFDirection::OUTWARD);
    TEST_ASSERT_EQUAL(OCF_SERVO_OUTSIDE_UNLOCKED, mock::last_servo_angle(OCF_SERVO_OUTSIDE_PIN));
    TEST_ASSERT_TRUE(OCFLock::isLockedInside());
    TEST_ASSERT_EQUAL_STD_STRING("Cat looks out: Mia", payloads("opencatflap/flap/activity")[0]);
}

void test_unknown_tag_does_not_unlock_and_is_reported()
{
    to_reading(*inward, OCFDirection::INWARD);
    inward->readRFID(STRAY);
    tick(*inward);
    TEST_ASSERT_EQUAL(OCFMachineStates::READING, inward->getState());
    TEST_ASSERT_TRUE(OCFLock::isLockedInside());
    TEST_ASSERT_EQUAL_STD_STRING("Unknown cat looks in: 250000000000001", payloads("opencatflap/flap/activity")[0]);
    TEST_ASSERT_EQUAL(0, OCFState::cats.count(STRAY));
}

void test_cat_denied_in_but_allowed_out()
{
    OCFState::cats[MIA].allow_in = false;
    to_reading(*inward, OCFDirection::INWARD);
    inward->readRFID(MIA);
    tick(*inward);
    TEST_ASSERT_EQUAL(OCFMachineStates::READING, inward->getState());

    to_unlocked(*outward, OCFDirection::OUTWARD);
}

void test_cat_denied_out_but_allowed_in()
{
    OCFState::cats[MIA].allow_out = false;
    to_reading(*outward, OCFDirection::OUTWARD);
    outward->readRFID(MIA);
    tick(*outward);
    TEST_ASSERT_EQUAL(OCFMachineStates::READING, outward->getState());

    to_unlocked(*inward, OCFDirection::INWARD);
}

void test_global_flag_overrides_cat_permission()
{
    OCFState::config.allow_in = false;
    to_reading(*inward, OCFDirection::INWARD);
    inward->readRFID(MIA);
    tick(*inward);
    TEST_ASSERT_EQUAL(OCFMachineStates::READING, inward->getState());

    OCFState::config.allow_out = false;
    to_reading(*outward, OCFDirection::OUTWARD);
    outward->readRFID(MIA);
    tick(*outward);
    TEST_ASSERT_EQUAL(OCFMachineStates::READING, outward->getState());
}

void test_default_config_denies_everything()
{
    OCFState::config = OCFConfig();
    to_reading(*inward, OCFDirection::INWARD);
    inward->readRFID(MIA);
    tick(*inward);
    TEST_ASSERT_EQUAL(OCFMachineStates::READING, inward->getState());
}

void test_stale_tag_older_than_2s_is_ignored()
{
    to_reading(*inward, OCFDirection::INWARD);
    OCFState::config.allow_in = false;
    inward->readRFID(MIA);
    tick(*inward, 1, 1);
    OCFState::config.allow_in = true;
    tick(*inward, 1, 2001);
    TEST_ASSERT_EQUAL(OCFMachineStates::READING, inward->getState());
}

void test_tag_within_2s_still_unlocks()
{
    to_reading(*inward, OCFDirection::INWARD);
    inward->readRFID(MIA);
    tick(*inward, 1, 1999);
    TEST_ASSERT_EQUAL(OCFMachineStates::UNLOCKED, inward->getState());
}

void test_tag_read_while_idle_is_discarded_on_wakeup()
{
    inward->readRFID(MIA);
    set_motion(OCF_MOTION_OUTSIDE_PIN, true);
    tick(*inward);
    tick(*inward);
    TEST_ASSERT_EQUAL(OCFMachineStates::READING, inward->getState());
}

void test_last_seen_set_from_ntp()
{
    mock::ntp.epoch = 1761800000UL;
    to_unlocked(*inward, OCFDirection::INWARD);
    TEST_ASSERT_EQUAL_UINT64(1761800000ULL, OCFState::cats[MIA].last_seen);
}

void test_unlocked_relocks_on_unknown_tag()
{
    to_unlocked(*inward, OCFDirection::INWARD);
    inward->readRFID(STRAY);
    tick(*inward);
    TEST_ASSERT_EQUAL(OCFMachineStates::READING, inward->getState());
    TEST_ASSERT_TRUE(OCFLock::isLockedInside());
    TEST_ASSERT_EQUAL(OCF_SERVO_INSIDE_LOCKED, mock::last_servo_angle(OCF_SERVO_INSIDE_PIN));
}

void test_unlocked_relocks_without_motion_and_eventually_idles()
{
    to_unlocked(*inward, OCFDirection::INWARD);
    set_motion(OCF_MOTION_OUTSIDE_PIN, false);
    tick(*inward, 1, 10001);
    TEST_ASSERT_TRUE(OCFLock::isLockedInside());
    tick(*inward, 1, 10001);
    TEST_ASSERT_EQUAL(OCFMachineStates::IDLE, inward->getState());
    TEST_ASSERT_FALSE(OCFLock::isArmedInside());
}

void test_machines_are_independent()
{
    to_unlocked(*inward, OCFDirection::INWARD);
    TEST_ASSERT_EQUAL(OCFMachineStates::IDLE, outward->getState());
    TEST_ASSERT_TRUE(OCFLock::isLockedOutside());
    TEST_ASSERT_FALSE(OCFLock::isArmedOutside());
}

void test_no_mqtt_when_disconnected()
{
    OCFMQTT::connected = false;
    to_unlocked(*inward, OCFDirection::INWARD);
    TEST_ASSERT_EQUAL(0, mock::mqtt.published.size());
}

void test_long_cat_name_fits_activity_message()
{
    OCFState::cats[MIA] = make_cat("Princess Fluffington von Purr", MIA);
    to_unlocked(*inward, OCFDirection::INWARD);
    TEST_ASSERT_EQUAL_STD_STRING("Cat looks in: Princess Fluffington von Purr", payloads("opencatflap/flap/activity")[0]);
}

void test_end_to_end_outside_tag_unlocks_inside_lock()
{
    set_motion(OCF_MOTION_OUTSIDE_PIN, true);
    tick(*inward);
    TEST_ASSERT_GREATER_THAN(0, feed_outside(encode(bits(CAPTURED_FRAME))));
    OCFRFID::process();
    tick(*inward);
    TEST_ASSERT_EQUAL(OCFMachineStates::UNLOCKED, inward->getState());
    TEST_ASSERT_EQUAL(OCFMachineStates::IDLE, outward->getState());
    TEST_ASSERT_EQUAL(OCF_SERVO_INSIDE_UNLOCKED, mock::last_servo_angle(OCF_SERVO_INSIDE_PIN));
    TEST_ASSERT_FALSE(OCFLock::isLockedInside());
    TEST_ASSERT_TRUE(OCFLock::isLockedOutside());
}

void test_end_to_end_inside_tag_unlocks_outside_lock()
{
    set_motion(OCF_MOTION_INSIDE_PIN, true);
    tick(*outward);
    TEST_ASSERT_GREATER_THAN(0, feed_inside(encode(bits(CAPTURED_FRAME))));
    OCFRFID::process();
    tick(*outward);
    TEST_ASSERT_EQUAL(OCFMachineStates::UNLOCKED, outward->getState());
    TEST_ASSERT_EQUAL(OCFMachineStates::IDLE, inward->getState());
    TEST_ASSERT_EQUAL(OCF_SERVO_OUTSIDE_UNLOCKED, mock::last_servo_angle(OCF_SERVO_OUTSIDE_PIN));
    TEST_ASSERT_FALSE(OCFLock::isLockedOutside());
    TEST_ASSERT_TRUE(OCFLock::isLockedInside());
}

void test_tag_on_wrong_side_is_ignored()
{
    set_motion(OCF_MOTION_OUTSIDE_PIN, true);
    tick(*inward);
    TEST_ASSERT_EQUAL(0, feed_inside(encode(bits(CAPTURED_FRAME))));
    OCFRFID::process();
    tick(*inward);
    TEST_ASSERT_EQUAL(OCFMachineStates::READING, inward->getState());
}

void test_flap_control_init_wires_everything()
{
    reset();
    put_file(FLAP_CONFIG_FILE, R"({"allowed_in":true,"allowed_out":false})");
    put_file(CAT_CONFIG_FILE, R"([{"name":"Mia","rfid":900158002010860,"allowed_in":true,"allowed_out":true}])");
    OCFFlapControl::init();
    TEST_ASSERT_TRUE(OCFState::config.allow_in);
    TEST_ASSERT_FALSE(OCFState::config.allow_out);
    TEST_ASSERT_EQUAL(1, OCFState::cats.size());
    TEST_ASSERT_EQUAL(OCFMachineStates::IDLE, OCFFlapControl::getInwardState());
    TEST_ASSERT_EQUAL(OCFMachineStates::IDLE, OCFFlapControl::getOutwardState());
    TEST_ASSERT_TRUE(OCFLock::initialized);

    set_motion(OCF_MOTION_OUTSIDE_PIN, true);
    OCFFlapControl::stateMachineInward->update();
    feed_outside(encode(bits(CAPTURED_FRAME)));
    OCFRFID::process();
    OCFFlapControl::stateMachineInward->update();
    TEST_ASSERT_EQUAL(OCFMachineStates::UNLOCKED, OCFFlapControl::getInwardState());
}


void test_unlocked_stays_open_while_tag_is_reread()
{
    to_unlocked(*inward, OCFDirection::INWARD);
    for (int i = 0; i < 300; i++)
    {
        if (i % 10 == 0) inward->readRFID(MIA);
        tick(*inward);
        TEST_ASSERT_EQUAL(OCFMachineStates::UNLOCKED, inward->getState());
    }
    TEST_ASSERT_FALSE(OCFLock::isLockedInside());
}

void test_one_unlock_per_passage()
{
    to_reading(*inward, OCFDirection::INWARD);
    for (int i = 0; i < 150; i++)
    {
        if (i % 3 == 0) inward->readRFID(MIA);
        tick(*inward);
    }
    int unlocks = 0;
    for (const auto &p : payloads("opencatflap/flap/in"))
        if (p == "Unlocked") unlocks++;
    TEST_ASSERT_EQUAL(1, unlocks);
    TEST_ASSERT_EQUAL(1, mock::servo_events_for(OCF_SERVO_INSIDE_PIN, mock::ServoEvent::WRITE).size() - 1);
}

void test_relocks_5s_after_last_read()
{
    to_unlocked(*inward, OCFDirection::INWARD);
    tick(*inward, 1, OCF_UNLOCK_TAG_TIMEOUT_MS - 20);
    TEST_ASSERT_EQUAL(OCFMachineStates::UNLOCKED, inward->getState());
    tick(*inward, 2, 10);
    TEST_ASSERT_EQUAL(OCFMachineStates::READING, inward->getState());
    TEST_ASSERT_TRUE(OCFLock::isLockedInside());
    TEST_ASSERT_EQUAL(OCF_SERVO_INSIDE_LOCKED, mock::last_servo_angle(OCF_SERVO_INSIDE_PIN));
    TEST_ASSERT_EQUAL_STD_STRING("Reading", payloads("opencatflap/flap/in").back());
}

void test_reread_extends_unlock()
{
    to_unlocked(*inward, OCFDirection::INWARD);
    tick(*inward, 1, 3000);
    inward->readRFID(MIA);
    tick(*inward, 1, 4000);
    TEST_ASSERT_EQUAL(OCFMachineStates::UNLOCKED, inward->getState());
    tick(*inward, 1, 1000);
    TEST_ASSERT_EQUAL(OCFMachineStates::READING, inward->getState());
}

void test_relocked_flap_unlocks_again_for_next_read()
{
    to_unlocked(*inward, OCFDirection::INWARD);
    tick(*inward, 1, OCF_UNLOCK_TAG_TIMEOUT_MS);
    TEST_ASSERT_EQUAL(OCFMachineStates::READING, inward->getState());
    inward->readRFID(MIA);
    tick(*inward);
    TEST_ASSERT_EQUAL(OCFMachineStates::UNLOCKED, inward->getState());
}

void test_activity_reported_once_per_visit()
{
    OCFState::cats[MIA].allow_in = false;
    to_reading(*inward, OCFDirection::INWARD);
    for (int i = 0; i < 200; i++)
    {
        inward->readRFID(MIA);
        tick(*inward);
    }
    TEST_ASSERT_EQUAL(1, payloads("opencatflap/flap/activity").size());

    set_motion(OCF_MOTION_OUTSIDE_PIN, false);
    tick(*inward, 1, OCF_MOTION_TIMEOUT_MS);
    TEST_ASSERT_EQUAL(OCFMachineStates::IDLE, inward->getState());
    to_reading(*inward, OCFDirection::INWARD);
    inward->readRFID(MIA);
    tick(*inward);
    TEST_ASSERT_EQUAL(2, payloads("opencatflap/flap/activity").size());
}

void test_activity_reports_each_tag_of_a_visit()
{
    to_reading(*inward, OCFDirection::INWARD);
    OCFState::cats[MIA].allow_in = false;
    for (int i = 0; i < 5; i++)
    {
        inward->readRFID(STRAY);
        tick(*inward);
        inward->readRFID(MIA);
        tick(*inward);
    }
    auto activity = payloads("opencatflap/flap/activity");
    TEST_ASSERT_EQUAL(2, activity.size());
    TEST_ASSERT_EQUAL_STD_STRING("Unknown cat looks in: 250000000000001", activity[0]);
    TEST_ASSERT_EQUAL_STD_STRING("Cat looks in: Mia", activity[1]);
}

void test_last_seen_updated_on_every_read()
{
    to_reading(*inward, OCFDirection::INWARD);
    OCFState::cats[MIA].allow_in = false;
    mock::ntp.epoch = 1761800000UL;
    inward->readRFID(MIA);
    tick(*inward);
    mock::ntp.epoch = 1761800042UL;
    inward->readRFID(MIA);
    tick(*inward);
    TEST_ASSERT_EQUAL_UINT64(1761800042ULL, OCFState::cats[MIA].last_seen);
}

void test_each_read_is_evaluated_once()
{
    to_reading(*inward, OCFDirection::INWARD);
    OCFState::cats[MIA].allow_in = false;
    inward->readRFID(MIA);
    tick(*inward);
    OCFState::cats[MIA].allow_in = true;
    tick(*inward, 50);
    TEST_ASSERT_EQUAL(OCFMachineStates::READING, inward->getState());
}

void test_tag_check_holds_state_lock()
{
    int takes = OCFState::mutex->takes;
    to_unlocked(*inward, OCFDirection::INWARD);
    TEST_ASSERT_GREATER_THAN(takes, OCFState::mutex->takes);
    TEST_ASSERT_EQUAL(0, mock::semaphores_held);
}

void test_timeouts_survive_millis_wrap()
{
    mock::now_ms = 0xFFFFFFFFu - 3000;
    to_unlocked(*inward, OCFDirection::INWARD);
    tick(*inward, 1, 4000);
    TEST_ASSERT_EQUAL(OCFMachineStates::UNLOCKED, inward->getState());
    tick(*inward, 1, 1000);
    TEST_ASSERT_EQUAL(OCFMachineStates::READING, inward->getState());
    set_motion(OCF_MOTION_OUTSIDE_PIN, false);
    tick(*inward, 1, OCF_MOTION_TIMEOUT_MS - 10);
    TEST_ASSERT_EQUAL(OCFMachineStates::READING, inward->getState());
    tick(*inward, 1, 10);
    TEST_ASSERT_EQUAL(OCFMachineStates::IDLE, inward->getState());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_starts_idle_with_pir_pulled_up);
    RUN_TEST(test_stays_idle_without_motion);
    RUN_TEST(test_pir_is_active_low);
    RUN_TEST(test_inward_machine_uses_outside_pir_outside_reader_inside_lock);
    RUN_TEST(test_outward_machine_uses_inside_pir_inside_reader_outside_lock);
    RUN_TEST(test_entering_reading_arms_servo_locked_and_reports);
    RUN_TEST(test_reading_times_out_to_idle_after_10s_without_motion);
    RUN_TEST(test_motion_keeps_reading_alive);
    RUN_TEST(test_allowed_cat_unlocks_its_direction);
    RUN_TEST(test_allowed_cat_unlocks_outward);
    RUN_TEST(test_unknown_tag_does_not_unlock_and_is_reported);
    RUN_TEST(test_cat_denied_in_but_allowed_out);
    RUN_TEST(test_cat_denied_out_but_allowed_in);
    RUN_TEST(test_global_flag_overrides_cat_permission);
    RUN_TEST(test_default_config_denies_everything);
    RUN_TEST(test_stale_tag_older_than_2s_is_ignored);
    RUN_TEST(test_tag_within_2s_still_unlocks);
    RUN_TEST(test_tag_read_while_idle_is_discarded_on_wakeup);
    RUN_TEST(test_last_seen_set_from_ntp);
    RUN_TEST(test_unlocked_relocks_on_unknown_tag);
    RUN_TEST(test_unlocked_relocks_without_motion_and_eventually_idles);
    RUN_TEST(test_machines_are_independent);
    RUN_TEST(test_no_mqtt_when_disconnected);
    RUN_TEST(test_long_cat_name_fits_activity_message);
    RUN_TEST(test_end_to_end_outside_tag_unlocks_inside_lock);
    RUN_TEST(test_end_to_end_inside_tag_unlocks_outside_lock);
    RUN_TEST(test_tag_on_wrong_side_is_ignored);
    RUN_TEST(test_flap_control_init_wires_everything);
    RUN_TEST(test_unlocked_stays_open_while_tag_is_reread);
    RUN_TEST(test_one_unlock_per_passage);
    RUN_TEST(test_relocks_5s_after_last_read);
    RUN_TEST(test_reread_extends_unlock);
    RUN_TEST(test_relocked_flap_unlocks_again_for_next_read);
    RUN_TEST(test_activity_reported_once_per_visit);
    RUN_TEST(test_activity_reports_each_tag_of_a_visit);
    RUN_TEST(test_last_seen_updated_on_every_read);
    RUN_TEST(test_each_read_is_evaluated_once);
    RUN_TEST(test_tag_check_holds_state_lock);
    RUN_TEST(test_timeouts_survive_millis_wrap);
    return UNITY_END();
}
