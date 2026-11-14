#include <ocf_test_support.h>

using namespace ocf_test;

static OCFStateMachine *outward = nullptr;
static OCFStateMachine *inward = nullptr;

void setUp()
{
    reset();
    if (!outward) outward = new OCFStateMachine(OCFDirection::OUTWARD);
    if (!inward) inward = new OCFStateMachine(OCFDirection::INWARD);
    outward->rfidTag = 0;
    outward->lastRFID = 0;
    inward->rfidTag = 0;
    inward->lastRFID = 0;
    OCFRFID::init(inward, outward);
}
void tearDown() {}

void test_capture_decodes_to_known_tag()
{
    std::vector<bool> frame = bits(CAPTURED_FRAME);
    TEST_ASSERT_EQUAL(128, frame.size());
    uint8_t raw[128];
    for (int i = 0; i < 128; i++) raw[i] = frame[i];
    uint64_t country = OCFRFID::inside->extractBits(raw, COUNTRY_BITS, COUNTRY_BITS_COUNT);
    uint64_t id = OCFRFID::inside->extractBits(raw, ID_BITS, ID_BITS_COUNT);
    TEST_ASSERT_EQUAL_UINT64(900, country);
    TEST_ASSERT_EQUAL_UINT64(158002010860ULL, id);
}

void test_capture_passes_crc()
{
    std::vector<bool> frame = bits(CAPTURED_FRAME);
    for (int i = 0; i < 128; i++) OCFRFID::inside->data[i] = frame[i];
    TEST_ASSERT_TRUE(OCFRFID::inside->checkFrame());
}

void test_crc_rejects_any_single_flipped_data_bit()
{
    std::vector<bool> frame = bits(CAPTURED_FRAME);
    for (int block = 0; block < 10; block++)
        for (int p = 0; p < 8; p++)
        {
            int index = 11 + block * 9 + p;
            for (int i = 0; i < 128; i++) OCFRFID::inside->data[i] = frame[i];
            OCFRFID::inside->data[index] = !OCFRFID::inside->data[index];
            TEST_ASSERT_FALSE_MESSAGE(OCFRFID::inside->checkFrame(), std::to_string(index).c_str());
        }
}

void test_frame_rejected_when_a_separator_bit_is_missing()
{
    std::vector<bool> frame = bits(CAPTURED_FRAME);
    for (int block = 0; block < 13; block++)
    {
        for (int i = 0; i < 128; i++) OCFRFID::inside->data[i] = frame[i];
        OCFRFID::inside->data[11 + block * 9 + 8] = 0;
        TEST_ASSERT_FALSE(OCFRFID::inside->checkFrame());
    }
}

void test_init_configures_pcnt_for_both_clocks()
{
    struct { pcnt_unit_t unit; int clk; } readers[] = {
        {PCNT_UNIT_0, OCF_RFID_OUTSIDE_CLK},
        {PCNT_UNIT_1, OCF_RFID_INSIDE_CLK},
    };
    for (auto r : readers)
    {
        mock::PcntUnit &u = mock::pcnt[r.unit];
        TEST_ASSERT_TRUE(u.configured);
        TEST_ASSERT_EQUAL(digitalPinToGPIONumber(r.clk), u.config.pulse_gpio_num);
        TEST_ASSERT_EQUAL(PCNT_PIN_NOT_USED, u.config.ctrl_gpio_num);
        TEST_ASSERT_EQUAL(PCNT_COUNT_INC, u.config.pos_mode);
        TEST_ASSERT_EQUAL(PCNT_COUNT_DIS, u.config.neg_mode);
        TEST_ASSERT_EQUAL(PCNT_MODE_KEEP, u.config.lctrl_mode);
        TEST_ASSERT_EQUAL(PCNT_MODE_KEEP, u.config.hctrl_mode);
        TEST_ASSERT_EQUAL(OCF_RFID_PCNT_LIMIT, u.config.counter_h_lim);
        TEST_ASSERT_EQUAL(0, u.config.counter_l_lim);
        TEST_ASSERT_EQUAL(r.unit, u.config.unit);
        TEST_ASSERT_EQUAL(OCF_RFID_PCNT_FILTER, u.filter);
        TEST_ASSERT_TRUE(u.filter_enabled);
        TEST_ASSERT_TRUE(u.paused);
    }
}

void test_init_leaves_both_readers_powered_down()
{
    TEST_ASSERT_EQUAL(OUTPUT, mock::pin_mode[OCF_RFID_OUTSIDE_EN]);
    TEST_ASSERT_EQUAL(OUTPUT, mock::pin_mode[OCF_RFID_INSIDE_EN]);
    TEST_ASSERT_EQUAL(HIGH, digitalRead(OCF_RFID_OUTSIDE_EN));
    TEST_ASSERT_EQUAL(HIGH, digitalRead(OCF_RFID_INSIDE_EN));
    TEST_ASSERT_EQUAL(0, mock::interrupts.size());
}

void test_pcnt_config_failure_does_not_crash_init()
{
    mock::pcnt.clear();
    mock::pcnt_config_result = ESP_ERR_INVALID_ARG;
    OCFRFID::init(inward, outward);
    TEST_ASSERT_FALSE(mock::pcnt[PCNT_UNIT_0].configured);
    TEST_ASSERT_FALSE(mock::pcnt[PCNT_UNIT_0].filter_enabled);
}

void test_enable_inward_powers_outside_reader_and_starts_counter()
{
    OCFRFID::enable(OCFDirection::INWARD);
    TEST_ASSERT_EQUAL(LOW, digitalRead(OCF_RFID_OUTSIDE_EN));
    TEST_ASSERT_EQUAL(HIGH, digitalRead(OCF_RFID_INSIDE_EN));
    TEST_ASSERT_FALSE(mock::pcnt[PCNT_UNIT_0].paused);
    TEST_ASSERT_TRUE(mock::pcnt[PCNT_UNIT_1].paused);
    TEST_ASSERT_EQUAL(1, mock::interrupts.count(OCF_RFID_OUTSIDE_DMOD));
    TEST_ASSERT_EQUAL(0, mock::interrupts.count(OCF_RFID_INSIDE_DMOD));
}

void test_disable_powers_reader_down()
{
    OCFRFID::enable(OCFDirection::OUTWARD);
    OCFRFID::disable(OCFDirection::OUTWARD);
    TEST_ASSERT_EQUAL(HIGH, digitalRead(OCF_RFID_INSIDE_EN));
    TEST_ASSERT_TRUE(mock::pcnt[PCNT_UNIT_1].paused);
    TEST_ASSERT_EQUAL(0, mock::interrupts.count(OCF_RFID_INSIDE_DMOD));
}

void test_frame_on_outside_reader_reaches_inward_machine()
{
    OCFRFID::enable(OCFDirection::INWARD);
    TEST_ASSERT_EQUAL(encode(bits(CAPTURED_FRAME)).size(), feed_outside(encode(bits(CAPTURED_FRAME))));
    OCFRFID::process();
    TEST_ASSERT_EQUAL_UINT64(CAPTURED_TAG, inward->rfidTag);
    TEST_ASSERT_EQUAL(millis(), inward->lastRFID);
    TEST_ASSERT_EQUAL_UINT64(0, outward->rfidTag);
    TEST_ASSERT_EQUAL(0, mock::critical_depth);
}

void test_frame_on_inside_reader_reaches_outward_machine()
{
    OCFRFID::enable(OCFDirection::OUTWARD);
    feed_inside(encode(bits(CAPTURED_FRAME)));
    OCFRFID::process();
    TEST_ASSERT_EQUAL_UINT64(CAPTURED_TAG, outward->rfidTag);
    TEST_ASSERT_EQUAL_UINT64(0, inward->rfidTag);
}

void test_isr_leaves_counter_running()
{
    OCFRFID::enable(OCFDirection::INWARD);
    int clears_before = mock::pcnt[PCNT_UNIT_0].clears;
    feed_outside({SHORT_EDGE, SHORT_EDGE, LONG_EDGE});
    TEST_ASSERT_EQUAL(clears_before, mock::pcnt[PCNT_UNIT_0].clears);
    TEST_ASSERT_EQUAL(2 * SHORT_EDGE + LONG_EDGE, mock::pcnt[PCNT_UNIT_0].count);
}

void test_frame_decodes_across_counter_wrap()
{
    OCFRFID::enable(OCFDirection::INWARD);
    run_clock(PCNT_UNIT_0, 134200, 220);
    TEST_ASSERT_GREATER_THAN(OCF_RFID_PCNT_LIMIT - 2000, mock::pcnt[PCNT_UNIT_0].count);
    feed_outside(encode(bits(CAPTURED_FRAME)));
    TEST_ASSERT_LESS_THAN(OCF_RFID_PCNT_LIMIT - 2000, mock::pcnt[PCNT_UNIT_0].count);
    feed_outside(encode(bits(CAPTURED_FRAME)));
    OCFRFID::process();
    TEST_ASSERT_EQUAL_UINT64(CAPTURED_TAG, inward->rfidTag);
}

void test_frequency_measured_after_settle_and_window()
{
    OCFRFID::enable(OCFDirection::INWARD);
    run_clock(PCNT_UNIT_0, 134200, OCF_RFID_SETTLE_MS + OCF_RFID_FREQ_WINDOW_MS - 20);
    TEST_ASSERT_EQUAL(0, OCFRFID::outsideFrequency().hz);
    run_clock(PCNT_UNIT_0, 134200, 40);
    TEST_ASSERT_EQUAL(134200, OCFRFID::outsideFrequency().hz);
    TEST_ASSERT_EQUAL(0, OCFRFID::insideFrequency().hz);
}

void test_frequency_follows_detuned_antenna()
{
    OCFRFID::enable(OCFDirection::OUTWARD);
    run_clock(PCNT_UNIT_1, 128500, 600);
    TEST_ASSERT_EQUAL(128500, OCFRFID::insideFrequency().hz);
    run_clock(PCNT_UNIT_1, 139900, 1000);
    TEST_ASSERT_EQUAL(139900, OCFRFID::insideFrequency().hz);
}

void test_frequency_held_with_age_after_disable()
{
    OCFRFID::enable(OCFDirection::OUTWARD);
    run_clock(PCNT_UNIT_1, 131000, 600);
    TEST_ASSERT_LESS_THAN(100, OCFRFID::insideFrequency().age_ms);
    OCFRFID::disable(OCFDirection::OUTWARD);
    mock::advance(5000);
    OCFRFIDFrequency f = OCFRFID::insideFrequency();
    TEST_ASSERT_EQUAL(131000, f.hz);
    TEST_ASSERT_GREATER_OR_EQUAL(5000, f.age_ms);
}

void test_each_antenna_measured_while_alternating()
{
    OCFRFID::enable(OCFDirection::INWARD);
    OCFRFID::enable(OCFDirection::OUTWARD);
    for (int i = 0; i < 300; i++)
    {
        for (pcnt_unit_t unit : {PCNT_UNIT_0, PCNT_UNIT_1})
        {
            mock::PcntUnit &u = mock::pcnt[unit];
            if (!u.paused) u.count = (u.count + (unit == PCNT_UNIT_0 ? 1342 : 1305)) % OCF_RFID_PCNT_LIMIT;
        }
        mock::advance(10);
        OCFRFID::process();
    }
    TEST_ASSERT_EQUAL(134200, OCFRFID::outsideFrequency().hz);
    TEST_ASSERT_EQUAL(130500, OCFRFID::insideFrequency().hz);
}

void test_stalled_loop_restarts_window()
{
    OCFRFID::enable(OCFDirection::INWARD);
    run_clock(PCNT_UNIT_0, 134200, 100);
    mock::PcntUnit &u = mock::pcnt[PCNT_UNIT_0];
    u.count = (u.count + 134200 * 300 / 1000) % OCF_RFID_PCNT_LIMIT;
    mock::advance(300);
    OCFRFID::process();
    run_clock(PCNT_UNIT_0, 134200, OCF_RFID_FREQ_WINDOW_MS - 10);
    TEST_ASSERT_EQUAL(0, OCFRFID::outsideFrequency().hz);
    run_clock(PCNT_UNIT_0, 134200, 20);
    TEST_ASSERT_EQUAL(134200, OCFRFID::outsideFrequency().hz);
}

void test_decoder_tolerates_carrier_jitter()
{
    OCFRFID::enable(OCFDirection::INWARD);
    std::vector<int16_t> edges;
    int n = 0;
    for (bool bit : bits(CAPTURED_FRAME))
    {
        int16_t wobble = (n++ % 7) - 3;
        if (bit)
            edges.push_back(LONG_EDGE + wobble);
        else
        {
            edges.push_back(SHORT_EDGE + wobble);
            edges.push_back(SHORT_EDGE - wobble);
        }
    }
    feed_outside(edges);
    OCFRFID::process();
    TEST_ASSERT_EQUAL_UINT64(CAPTURED_TAG, inward->rfidTag);
}

void test_short_long_threshold_boundary()
{
    OCFRFID::enable(OCFDirection::INWARD);
    feed_outside(encode(bits(CAPTURED_FRAME), OCF_RFID_BIT_CYCLE_THRESHOLD - 1, OCF_RFID_BIT_CYCLE_THRESHOLD));
    OCFRFID::process();
    TEST_ASSERT_EQUAL_UINT64(CAPTURED_TAG, inward->rfidTag);
}

void test_decoder_syncs_after_garbage()
{
    OCFRFID::enable(OCFDirection::INWARD);
    std::vector<int16_t> edges = {LONG_EDGE, SHORT_EDGE, LONG_EDGE, SHORT_EDGE, SHORT_EDGE, SHORT_EDGE, LONG_EDGE, LONG_EDGE};
    for (int16_t e : encode(bits(CAPTURED_FRAME))) edges.push_back(e);
    feed_outside(edges);
    OCFRFID::process();
    TEST_ASSERT_EQUAL_UINT64(CAPTURED_TAG, inward->rfidTag);
}

void test_back_to_back_frames_are_all_reported()
{
    OCFRFID::enable(OCFDirection::INWARD);
    std::vector<int16_t> edges;
    for (int i = 0; i < 3; i++)
        for (int16_t e : encode(bits(CAPTURED_FRAME))) edges.push_back(e);
    feed_outside(edges);
    TEST_ASSERT_EQUAL(3, uxQueueMessagesWaiting(OCFRFID::tag_queue));
}

void test_corrupted_frame_is_dropped()
{
    OCFRFID::enable(OCFDirection::INWARD);
    std::vector<bool> frame = bits(CAPTURED_FRAME);
    frame[20] = !frame[20];
    feed_outside(encode(frame));
    OCFRFID::process();
    TEST_ASSERT_EQUAL(0, uxQueueMessagesWaiting(OCFRFID::tag_queue));
    TEST_ASSERT_EQUAL_UINT64(0, inward->rfidTag);
}

void test_synthetic_frame_with_valid_crc_decodes()
{
    std::vector<bool> frame = bits(CAPTURED_FRAME);
    frame[11] = !frame[11];
    frame = frame_with_crc(frame);
    OCFRFID::enable(OCFDirection::INWARD);
    feed_outside(encode(frame));
    OCFRFID::process();
    TEST_ASSERT_NOT_EQUAL(0, inward->rfidTag);
    TEST_ASSERT_NOT_EQUAL(CAPTURED_TAG, inward->rfidTag);
}

void test_frame_split_across_antenna_switch_is_not_reported()
{
    OCFRFID::enable(OCFDirection::INWARD);
    std::vector<int16_t> edges = encode(bits(CAPTURED_FRAME));
    std::vector<int16_t> first(edges.begin(), edges.begin() + edges.size() / 2);
    std::vector<int16_t> second(edges.begin() + edges.size() / 2, edges.end());
    feed_outside(first);
    OCFRFID::enable(OCFDirection::OUTWARD);
    mock::advance(OCF_RFID_SWITCH_MS);
    OCFRFID::process();
    feed_inside(second);
    OCFRFID::process();
    TEST_ASSERT_EQUAL_UINT64(0, inward->rfidTag);
    TEST_ASSERT_EQUAL_UINT64(0, outward->rfidTag);
}

void test_both_enabled_alternates_antennas()
{
    OCFRFID::enable(OCFDirection::INWARD);
    OCFRFID::enable(OCFDirection::OUTWARD);
    TEST_ASSERT_EQUAL(LOW, digitalRead(OCF_RFID_OUTSIDE_EN));
    TEST_ASSERT_EQUAL(HIGH, digitalRead(OCF_RFID_INSIDE_EN));

    mock::advance(OCF_RFID_SWITCH_MS - 1);
    OCFRFID::process();
    TEST_ASSERT_EQUAL(LOW, digitalRead(OCF_RFID_OUTSIDE_EN));

    mock::advance(1);
    OCFRFID::process();
    TEST_ASSERT_EQUAL(HIGH, digitalRead(OCF_RFID_OUTSIDE_EN));
    TEST_ASSERT_EQUAL(LOW, digitalRead(OCF_RFID_INSIDE_EN));
    TEST_ASSERT_EQUAL(1, mock::interrupts.count(OCF_RFID_INSIDE_DMOD));
    TEST_ASSERT_EQUAL(0, mock::interrupts.count(OCF_RFID_OUTSIDE_DMOD));

    mock::advance(OCF_RFID_SWITCH_MS);
    OCFRFID::process();
    TEST_ASSERT_EQUAL(LOW, digitalRead(OCF_RFID_OUTSIDE_EN));
    TEST_ASSERT_EQUAL(HIGH, digitalRead(OCF_RFID_INSIDE_EN));
}

void test_never_powers_both_readers_at_once()
{
    OCFRFID::enable(OCFDirection::INWARD);
    OCFRFID::enable(OCFDirection::OUTWARD);
    for (int i = 0; i < 50; i++)
    {
        mock::advance(OCF_RFID_SWITCH_MS / 3);
        OCFRFID::process();
        bool in_on = digitalRead(OCF_RFID_OUTSIDE_EN) == LOW;
        bool out_on = digitalRead(OCF_RFID_INSIDE_EN) == LOW;
        TEST_ASSERT_TRUE(in_on != out_on);
    }
}

void test_disabling_active_side_hands_over_to_other()
{
    OCFRFID::enable(OCFDirection::INWARD);
    OCFRFID::enable(OCFDirection::OUTWARD);
    OCFRFID::disable(OCFDirection::INWARD);
    TEST_ASSERT_EQUAL(HIGH, digitalRead(OCF_RFID_OUTSIDE_EN));
    TEST_ASSERT_EQUAL(LOW, digitalRead(OCF_RFID_INSIDE_EN));
    mock::advance(OCF_RFID_SWITCH_MS * 3);
    OCFRFID::process();
    TEST_ASSERT_EQUAL(LOW, digitalRead(OCF_RFID_INSIDE_EN));
}

void test_disabling_inactive_side_keeps_active_one()
{
    OCFRFID::enable(OCFDirection::INWARD);
    OCFRFID::enable(OCFDirection::OUTWARD);
    OCFRFID::disable(OCFDirection::OUTWARD);
    TEST_ASSERT_EQUAL(LOW, digitalRead(OCF_RFID_OUTSIDE_EN));
    TEST_ASSERT_EQUAL(HIGH, digitalRead(OCF_RFID_INSIDE_EN));
}

void test_disable_both_powers_everything_down()
{
    OCFRFID::enable(OCFDirection::INWARD);
    OCFRFID::enable(OCFDirection::OUTWARD);
    OCFRFID::disable(OCFDirection::INWARD);
    OCFRFID::disable(OCFDirection::OUTWARD);
    TEST_ASSERT_EQUAL(HIGH, digitalRead(OCF_RFID_OUTSIDE_EN));
    TEST_ASSERT_EQUAL(HIGH, digitalRead(OCF_RFID_INSIDE_EN));
    TEST_ASSERT_EQUAL(0, mock::interrupts.size());
}

void test_queue_overflow_drops_without_crashing()
{
    OCFRFID::enable(OCFDirection::INWARD);
    for (int i = 0; i < 12; i++) feed_outside(encode(bits(CAPTURED_FRAME)));
    TEST_ASSERT_EQUAL(8, uxQueueMessagesWaiting(OCFRFID::tag_queue));
    OCFRFID::process();
    TEST_ASSERT_EQUAL(0, uxQueueMessagesWaiting(OCFRFID::tag_queue));
    TEST_ASSERT_EQUAL_UINT64(CAPTURED_TAG, inward->rfidTag);
}

void test_process_logs_read_to_serial()
{
    OCFRFID::enable(OCFDirection::INWARD);
    feed_outside(encode(bits(CAPTURED_FRAME)));
    OCFRFID::process();
    TEST_ASSERT_NOT_EQUAL(std::string::npos, Serial.output.find("RFID read: 900158002010860"));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_capture_decodes_to_known_tag);
    RUN_TEST(test_capture_passes_crc);
    RUN_TEST(test_crc_rejects_any_single_flipped_data_bit);
    RUN_TEST(test_frame_rejected_when_a_separator_bit_is_missing);
    RUN_TEST(test_init_configures_pcnt_for_both_clocks);
    RUN_TEST(test_init_leaves_both_readers_powered_down);
    RUN_TEST(test_pcnt_config_failure_does_not_crash_init);
    RUN_TEST(test_enable_inward_powers_outside_reader_and_starts_counter);
    RUN_TEST(test_disable_powers_reader_down);
    RUN_TEST(test_frame_on_outside_reader_reaches_inward_machine);
    RUN_TEST(test_frame_on_inside_reader_reaches_outward_machine);
    RUN_TEST(test_isr_leaves_counter_running);
    RUN_TEST(test_frame_decodes_across_counter_wrap);
    RUN_TEST(test_frequency_measured_after_settle_and_window);
    RUN_TEST(test_frequency_follows_detuned_antenna);
    RUN_TEST(test_frequency_held_with_age_after_disable);
    RUN_TEST(test_each_antenna_measured_while_alternating);
    RUN_TEST(test_stalled_loop_restarts_window);
    RUN_TEST(test_decoder_tolerates_carrier_jitter);
    RUN_TEST(test_short_long_threshold_boundary);
    RUN_TEST(test_decoder_syncs_after_garbage);
    RUN_TEST(test_back_to_back_frames_are_all_reported);
    RUN_TEST(test_corrupted_frame_is_dropped);
    RUN_TEST(test_synthetic_frame_with_valid_crc_decodes);
    RUN_TEST(test_frame_split_across_antenna_switch_is_not_reported);
    RUN_TEST(test_both_enabled_alternates_antennas);
    RUN_TEST(test_never_powers_both_readers_at_once);
    RUN_TEST(test_disabling_active_side_hands_over_to_other);
    RUN_TEST(test_disabling_inactive_side_keeps_active_one);
    RUN_TEST(test_disable_both_powers_everything_down);
    RUN_TEST(test_queue_overflow_drops_without_crashing);
    RUN_TEST(test_process_logs_read_to_serial);
    return UNITY_END();
}
