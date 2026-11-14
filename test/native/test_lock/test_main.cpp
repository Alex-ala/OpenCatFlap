#include <ocf_test_support.h>

using namespace ocf_test;
using Ev = mock::ServoEvent;

void setUp() { reset(); }
void tearDown() {}

static int auto_allocated_attaches()
{
    int n = 0;
    for (const auto &e : mock::servo_events)
        if (e.kind == Ev::ATTACH && e.channel == Servo::CHANNEL_NOT_ATTACHED) n++;
    return n;
}

void test_init_attaches_with_explicit_channels()
{
    OCFLock::init();
    auto in = mock::servo_events_for(OCF_SERVO_INSIDE_PIN, Ev::ATTACH);
    auto out = mock::servo_events_for(OCF_SERVO_OUTSIDE_PIN, Ev::ATTACH);
    TEST_ASSERT_EQUAL(1, in.size());
    TEST_ASSERT_EQUAL(1, out.size());
    TEST_ASSERT_EQUAL(OCF_SERVO_INSIDE_CHANNEL, in[0].channel);
    TEST_ASSERT_EQUAL(OCF_SERVO_OUTSIDE_CHANNEL, out[0].channel);
}

void test_init_parks_both_servos_locked()
{
    uint32_t start = millis();
    OCFLock::init();
    TEST_ASSERT_EQUAL(OCF_SERVO_INSIDE_LOCKED, mock::last_servo_angle(OCF_SERVO_INSIDE_PIN));
    TEST_ASSERT_EQUAL(OCF_SERVO_OUTSIDE_LOCKED, mock::last_servo_angle(OCF_SERVO_OUTSIDE_PIN));
    TEST_ASSERT_TRUE(OCFLock::isLockedInside());
    TEST_ASSERT_TRUE(OCFLock::isLockedOutside());
    TEST_ASSERT_FALSE(OCFLock::isArmedInside());
    TEST_ASSERT_FALSE(OCFLock::isArmedOutside());
    TEST_ASSERT_EQUAL(1, mock::servo_events_for(OCF_SERVO_INSIDE_PIN, Ev::DETACH).size());
    TEST_ASSERT_EQUAL(1, mock::servo_events_for(OCF_SERVO_OUTSIDE_PIN, Ev::DETACH).size());
    TEST_ASSERT_EQUAL(OCF_SERVO_TRAVEL_MS, millis() - start);
}

void test_init_is_idempotent()
{
    OCFLock::init();
    size_t events = mock::servo_events.size();
    OCFLock::init();
    TEST_ASSERT_EQUAL(events, mock::servo_events.size());
}

void test_parked_pins_are_held_low()
{
    OCFLock::init();
    for (int pin : {OCF_SERVO_INSIDE_PIN, OCF_SERVO_OUTSIDE_PIN})
    {
        TEST_ASSERT_EQUAL(OUTPUT, mock::pin_mode[pin]);
        TEST_ASSERT_EQUAL(LOW, digitalRead(pin));
    }
}

void test_enable_reattaches_on_owned_channel_and_locks()
{
    OCFLock::init();
    mock::servo_events.clear();
    OCFLock::enableServo(OCFDirection::INWARD);
    auto attaches = mock::servo_events_for(OCF_SERVO_INSIDE_PIN, Ev::ATTACH);
    TEST_ASSERT_EQUAL(1, attaches.size());
    TEST_ASSERT_EQUAL(OCF_SERVO_INSIDE_CHANNEL, attaches[0].channel);
    TEST_ASSERT_EQUAL(OCF_SERVO_INSIDE_LOCKED, mock::last_servo_angle(OCF_SERVO_INSIDE_PIN));
    TEST_ASSERT_TRUE(OCFLock::isArmedInside());
    TEST_ASSERT_FALSE(OCFLock::isArmedOutside());
    TEST_ASSERT_EQUAL(0, mock::servo_events_for(OCF_SERVO_OUTSIDE_PIN, Ev::ATTACH).size());
}

void test_enable_when_already_armed_does_nothing()
{
    OCFLock::init();
    OCFLock::enableServo(OCFDirection::OUTWARD);
    mock::servo_events.clear();
    OCFLock::enableServo(OCFDirection::OUTWARD);
    TEST_ASSERT_EQUAL(0, mock::servo_events.size());
}

void test_enable_before_init_initializes()
{
    OCFLock::enableServo(OCFDirection::INWARD);
    TEST_ASSERT_TRUE(OCFLock::initialized);
    TEST_ASSERT_TRUE(OCFLock::isArmedInside());
    TEST_ASSERT_EQUAL(OCF_SERVO_INSIDE_LOCKED, mock::last_servo_angle(OCF_SERVO_INSIDE_PIN));
}

void test_enable_both()
{
    OCFLock::init();
    OCFLock::enableServo(OCFDirection::BOTH);
    TEST_ASSERT_TRUE(OCFLock::isArmedInside());
    TEST_ASSERT_TRUE(OCFLock::isArmedOutside());
}

void test_unlock_and_lock_angles()
{
    OCFLock::init();
    OCFLock::enableServo(OCFDirection::BOTH);

    OCFLock::unlock(OCFDirection::INWARD);
    TEST_ASSERT_EQUAL(OCF_SERVO_INSIDE_UNLOCKED, mock::last_servo_angle(OCF_SERVO_INSIDE_PIN));
    TEST_ASSERT_FALSE(OCFLock::isLockedInside());
    TEST_ASSERT_TRUE(OCFLock::isLockedOutside());

    OCFLock::unlock(OCFDirection::OUTWARD);
    TEST_ASSERT_EQUAL(OCF_SERVO_OUTSIDE_UNLOCKED, mock::last_servo_angle(OCF_SERVO_OUTSIDE_PIN));
    TEST_ASSERT_FALSE(OCFLock::isLockedOutside());

    OCFLock::lock(OCFDirection::BOTH);
    TEST_ASSERT_EQUAL(OCF_SERVO_INSIDE_LOCKED, mock::last_servo_angle(OCF_SERVO_INSIDE_PIN));
    TEST_ASSERT_EQUAL(OCF_SERVO_OUTSIDE_LOCKED, mock::last_servo_angle(OCF_SERVO_OUTSIDE_PIN));
    TEST_ASSERT_TRUE(OCFLock::isLockedInside());
    TEST_ASSERT_TRUE(OCFLock::isLockedOutside());

    OCFLock::unlock(OCFDirection::BOTH);
    TEST_ASSERT_FALSE(OCFLock::isLockedInside());
    TEST_ASSERT_FALSE(OCFLock::isLockedOutside());
}

void test_lock_and_unlock_none_are_noops()
{
    OCFLock::init();
    OCFLock::enableServo(OCFDirection::BOTH);
    mock::servo_events.clear();
    OCFLock::lock(OCFDirection::NONE);
    OCFLock::unlock(OCFDirection::NONE);
    TEST_ASSERT_EQUAL(0, mock::servo_events.size());
}

void test_moves_are_ignored_while_parked()
{
    OCFLock::init();
    mock::servo_events.clear();
    OCFLock::moveServoInside(OCF_SERVO_INSIDE_UNLOCKED);
    OCFLock::moveServoOutside(OCF_SERVO_OUTSIDE_UNLOCKED);
    OCFLock::unlock(OCFDirection::BOTH);
    TEST_ASSERT_EQUAL(0, mock::servo_events_for(OCF_SERVO_INSIDE_PIN, Ev::WRITE).size());
    TEST_ASSERT_EQUAL(0, mock::servo_events_for(OCF_SERVO_OUTSIDE_PIN, Ev::WRITE).size());
}

void test_disable_locks_waits_and_parks_one_side()
{
    OCFLock::init();
    OCFLock::enableServo(OCFDirection::BOTH);
    OCFLock::unlock(OCFDirection::INWARD);
    mock::servo_events.clear();
    uint32_t start = millis();
    OCFLock::disableServo(OCFDirection::INWARD);
    TEST_ASSERT_EQUAL(OCF_SERVO_INSIDE_LOCKED, mock::last_servo_angle(OCF_SERVO_INSIDE_PIN));
    TEST_ASSERT_EQUAL(OCF_SERVO_TRAVEL_MS, millis() - start);
    TEST_ASSERT_EQUAL(1, mock::servo_events_for(OCF_SERVO_INSIDE_PIN, Ev::DETACH).size());
    TEST_ASSERT_FALSE(OCFLock::isArmedInside());
    TEST_ASSERT_TRUE(OCFLock::isArmedOutside());
    TEST_ASSERT_EQUAL(0, mock::servo_events_for(OCF_SERVO_OUTSIDE_PIN, Ev::DETACH).size());
}

void test_lock_write_precedes_detach()
{
    OCFLock::init();
    OCFLock::enableServo(OCFDirection::OUTWARD);
    OCFLock::unlock(OCFDirection::OUTWARD);
    mock::servo_events.clear();
    OCFLock::disableServo(OCFDirection::OUTWARD);
    TEST_ASSERT_EQUAL(2, mock::servo_events.size());
    TEST_ASSERT_EQUAL(Ev::WRITE, mock::servo_events[0].kind);
    TEST_ASSERT_EQUAL(Ev::DETACH, mock::servo_events[1].kind);
}

void test_disable_both_waits_only_once()
{
    OCFLock::init();
    OCFLock::enableServo(OCFDirection::BOTH);
    uint32_t start = millis();
    OCFLock::disableServo(OCFDirection::BOTH);
    TEST_ASSERT_EQUAL(OCF_SERVO_TRAVEL_MS, millis() - start);
    TEST_ASSERT_FALSE(OCFLock::isArmedInside());
    TEST_ASSERT_FALSE(OCFLock::isArmedOutside());
}

void test_disable_when_parked_does_nothing()
{
    OCFLock::init();
    mock::servo_events.clear();
    uint32_t start = millis();
    OCFLock::disableServo(OCFDirection::INWARD);
    OCFLock::disableServo(OCFDirection::BOTH);
    TEST_ASSERT_EQUAL(0, mock::servo_events.size());
    TEST_ASSERT_EQUAL(start, millis());
}

void test_many_cycles_never_use_auto_channel_allocation()
{
    OCFLock::init();
    for (int i = 0; i < 100; i++)
    {
        OCFDirection first = (i % 2) ? OCFDirection::INWARD : OCFDirection::OUTWARD;
        OCFDirection second = (i % 2) ? OCFDirection::OUTWARD : OCFDirection::INWARD;
        OCFLock::enableServo(first);
        OCFLock::enableServo(second);
        OCFLock::unlock(first);
        OCFLock::disableServo(first);
        OCFLock::disableServo(second);
    }
    TEST_ASSERT_EQUAL(0, auto_allocated_attaches());
    for (const auto &e : mock::servo_events)
    {
        if (e.kind != Ev::ATTACH) continue;
        if (e.pin == OCF_SERVO_INSIDE_PIN) TEST_ASSERT_EQUAL(OCF_SERVO_INSIDE_CHANNEL, e.channel);
        if (e.pin == OCF_SERVO_OUTSIDE_PIN) TEST_ASSERT_EQUAL(OCF_SERVO_OUTSIDE_CHANNEL, e.channel);
    }
}

void test_attach_failure_at_init_does_not_crash()
{
    mock::servo_attach_result = false;
    OCFLock::init();
    TEST_ASSERT_TRUE(OCFLock::initialized);
}

void test_servo_constants_are_within_servo_range()
{
    for (int angle : {OCF_SERVO_INSIDE_LOCKED, OCF_SERVO_INSIDE_UNLOCKED, OCF_SERVO_OUTSIDE_LOCKED, OCF_SERVO_OUTSIDE_UNLOCKED})
    {
        TEST_ASSERT_GREATER_OR_EQUAL(0, angle);
        TEST_ASSERT_LESS_OR_EQUAL(180, angle);
    }
    TEST_ASSERT_NOT_EQUAL(OCF_SERVO_INSIDE_CHANNEL, OCF_SERVO_OUTSIDE_CHANNEL);
    TEST_ASSERT_NOT_EQUAL(OCF_SERVO_INSIDE_PIN, OCF_SERVO_OUTSIDE_PIN);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_init_attaches_with_explicit_channels);
    RUN_TEST(test_init_parks_both_servos_locked);
    RUN_TEST(test_init_is_idempotent);
    RUN_TEST(test_parked_pins_are_held_low);
    RUN_TEST(test_enable_reattaches_on_owned_channel_and_locks);
    RUN_TEST(test_enable_when_already_armed_does_nothing);
    RUN_TEST(test_enable_before_init_initializes);
    RUN_TEST(test_enable_both);
    RUN_TEST(test_unlock_and_lock_angles);
    RUN_TEST(test_lock_and_unlock_none_are_noops);
    RUN_TEST(test_moves_are_ignored_while_parked);
    RUN_TEST(test_disable_locks_waits_and_parks_one_side);
    RUN_TEST(test_lock_write_precedes_detach);
    RUN_TEST(test_disable_both_waits_only_once);
    RUN_TEST(test_disable_when_parked_does_nothing);
    RUN_TEST(test_many_cycles_never_use_auto_channel_allocation);
    RUN_TEST(test_attach_failure_at_init_does_not_crash);
    RUN_TEST(test_servo_constants_are_within_servo_range);
    return UNITY_END();
}
