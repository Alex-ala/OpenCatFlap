#include "OCFRFID.h"
#include <string.h>
#include <OCFMQTT.h>
#include <OCFStateMachine.h>

OCFRFID *OCFRFID::outside = new OCFRFID();
OCFRFID *OCFRFID::inside = new OCFRFID();
OCFRFID *volatile OCFRFID::active_instance = nullptr;
bool OCFRFID::inside_enabled = false;
bool OCFRFID::outside_enabled = false;
uint32_t OCFRFID::last_switch_ms = 0;
QueueHandle_t OCFRFID::tag_queue = nullptr;

struct RfidRead {
    unsigned long long id;
    OCFDirection direction;
};

static portMUX_TYPE rfid_mux = portMUX_INITIALIZER_UNLOCKED;
static volatile uint32_t crc_failures = 0;
static uint32_t reported_crc_failures = 0;

void OCFRFID::on_dmod() {
    if (active_instance) active_instance->onInterruptDmod();
}

void OCFRFID::init(OCFStateMachine* machine_inward, OCFStateMachine* machine_outward) {
    outside->machine = machine_inward;
    outside->direction = OCFDirection::INWARD;
    outside->pin_power = OCF_RFID_OUTSIDE_EN;
    outside->pin_dmod = OCF_RFID_OUTSIDE_DMOD;
    outside->pin_clk = OCF_RFID_OUTSIDE_CLK;
    outside->pcnt_unit = PCNT_UNIT_0;
    outside->clearPending();
    pinMode(outside->pin_power, OUTPUT);
    digitalWrite(outside->pin_power, HIGH); // Disable initially
    outside->setupCounter();
    inside->machine = machine_outward;
    inside->direction = OCFDirection::OUTWARD;
    inside->pin_power = OCF_RFID_INSIDE_EN;
    inside->pin_dmod = OCF_RFID_INSIDE_DMOD;
    inside->pin_clk = OCF_RFID_INSIDE_CLK;
    inside->pcnt_unit = PCNT_UNIT_1;
    inside->clearPending();
    pinMode(inside->pin_power, OUTPUT);
    digitalWrite(inside->pin_power, HIGH); // Disable initially
    inside->setupCounter();
    inside_enabled = false;
    outside_enabled = false;
    last_switch_ms = millis();
    if (tag_queue == nullptr) tag_queue = xQueueCreate(8, sizeof(RfidRead));
    log_d("RFID initialized");
}

void OCFRFID::setupCounter() {
    int gpio = digitalPinToGPIONumber(pin_clk);
    if (gpio < 0) {
        log_e("Cannot resolve GPIO for clock pin %d", pin_clk);
        return;
    }
    pcnt_config_t cfg = {};
    cfg.pulse_gpio_num = gpio;
    cfg.ctrl_gpio_num = PCNT_PIN_NOT_USED;
    cfg.lctrl_mode = PCNT_MODE_KEEP;
    cfg.hctrl_mode = PCNT_MODE_KEEP;
    cfg.pos_mode = PCNT_COUNT_INC;
    cfg.neg_mode = PCNT_COUNT_DIS;
    cfg.counter_h_lim = OCF_RFID_PCNT_LIMIT;
    cfg.counter_l_lim = 0;
    cfg.unit = pcnt_unit;
    cfg.channel = PCNT_CHANNEL_0;
    esp_err_t err = pcnt_unit_config(&cfg);
    if (err != ESP_OK) {
        log_e("PCNT unit %d config failed on GPIO %d: %s", pcnt_unit, gpio, esp_err_to_name(err));
        return;
    }
    pcnt_set_filter_value(pcnt_unit, OCF_RFID_PCNT_FILTER);
    pcnt_filter_enable(pcnt_unit);
    pcnt_counter_pause(pcnt_unit);
    pcnt_counter_clear(pcnt_unit);
    log_d("PCNT unit %d counting %s clock on GPIO %d", pcnt_unit, writeDirection(direction), gpio);
}

void OCFRFID::process() {
    if (inside_enabled && outside_enabled && (millis() - last_switch_ms >= OCF_RFID_SWITCH_MS)) {
        activate(active_instance == inside ? outside : inside);
    }
    uint32_t failures = crc_failures;
    if (failures != reported_crc_failures) {
        log_d("RFID frames rejected by CRC: %u", failures);
        reported_crc_failures = failures;
    }
    if (tag_queue == nullptr) return;
    outside->decodePending();
    inside->decodePending();
    RfidRead read;
    while (xQueueReceive(tag_queue, &read, 0) == pdTRUE) {
        OCFRFID *reader = readerFor(read.direction);
        if (reader && reader->machine) reader->machine->readRFID(read.id);
    }
    OCFRFID *reader = active_instance;
    if (reader && reader->powered) reader->updateFrequency();
}

void OCFRFID::harvest() {
    int16_t count = 0;
    pcnt_get_counter_value(pcnt_unit, &count);
    total_edges += (count - last_count + OCF_RFID_PCNT_LIMIT) % OCF_RFID_PCNT_LIMIT;
    last_count = count;
}

void OCFRFID::updateFrequency() {
    uint32_t now_us = micros();
    portENTER_CRITICAL(&rfid_mux);
    harvest();
    uint32_t edges = total_edges;
    portEXIT_CRITICAL(&rfid_mux);
    uint32_t now_ms = millis();
    bool settling = now_ms - enabled_ms < OCF_RFID_SETTLE_MS;
    bool gap = now_us - last_update_us > OCF_RFID_MAX_GAP_US;
    last_update_us = now_us;
    if (settling || gap) {
        window_start_us = now_us;
        window_start_edges = edges;
        return;
    }
    uint32_t elapsed_us = now_us - window_start_us;
    if (elapsed_us < OCF_RFID_FREQ_WINDOW_MS * 1000UL) return;
    uint32_t hz = (uint64_t)(edges - window_start_edges) * 1000000ULL / elapsed_us;
    portENTER_CRITICAL(&rfid_mux);
    frequency_hz = hz;
    measured_ms = now_ms;
    portEXIT_CRITICAL(&rfid_mux);
    window_start_us = now_us;
    window_start_edges = edges;
}

OCFRFIDFrequency OCFRFID::frequency() {
    portENTER_CRITICAL(&rfid_mux);
    OCFRFIDFrequency result = { frequency_hz, frequency_hz ? millis() - measured_ms : 0 };
    portEXIT_CRITICAL(&rfid_mux);
    return result;
}

OCFRFIDFrequency OCFRFID::insideFrequency() {
    return inside->frequency();
}

OCFRFIDFrequency OCFRFID::outsideFrequency() {
    return outside->frequency();
}

OCFRFID* OCFRFID::readerFor(OCFDirection direction) {
    if (direction == OCFDirection::INWARD) return outside;
    if (direction == OCFDirection::OUTWARD) return inside;
    return nullptr;
}

void OCFRFID::enable(OCFDirection direction) {
    if (direction == OCFDirection::INWARD) {
        outside_enabled = true;
        if (!inside_enabled) {
            activate(outside);
        }
    } else if (direction == OCFDirection::OUTWARD) {
        inside_enabled = true;
        if (!outside_enabled) {
            activate(inside);
        }
    }
}
void OCFRFID::disable(OCFDirection direction) {
    if (direction == OCFDirection::INWARD) {
        outside_enabled = false;
        if (inside_enabled && active_instance != inside) {
            activate(inside);
        }else{
            outside->disable();
        }
    } else if (direction == OCFDirection::OUTWARD) {
        inside_enabled = false;
        if (outside_enabled && active_instance != outside) {
            activate(outside);
        }else{
            inside->disable();
        }
    }
}

void OCFRFID::activate(OCFRFID* reader) {
    OCFRFID* other = (reader == inside) ? outside : inside;
    other->disable();
    reader->enable();
    log_d("Enable RFID %s", reader == inside ? "inside" : "outside");
    last_switch_ms = millis();
}

void OCFRFID::enable() {
    clearPending();
    portENTER_CRITICAL(&rfid_mux);
    active_instance = this;
    last_count = 0;
    edge_mark = total_edges;
    powered = true;
    enabled_ms = millis();
    portEXIT_CRITICAL(&rfid_mux);
    digitalWrite(pin_power, LOW);
    pcnt_counter_clear(pcnt_unit);
    pcnt_counter_resume(pcnt_unit);
    attachInterrupt(pin_dmod, OCFRFID::on_dmod, CHANGE);
}

void OCFRFID::disable() {
    detachInterrupt(pin_dmod);
    pcnt_counter_pause(pcnt_unit);
    digitalWrite(pin_power, HIGH);
    powered = false;
}

void OCFRFID::onInterruptDmod() {
    portENTER_CRITICAL_ISR(&rfid_mux);
    harvest();
    uint32_t interval = total_edges - edge_mark;
    edge_mark = total_edges;
    uint16_t next = (ring_head + 1) % OCF_RFID_RING_SIZE;
    if (next == ring_tail) {
        ring_overflow = true;
    } else {
        ring[ring_head] = interval > UINT16_MAX ? UINT16_MAX : (uint16_t)interval;
        ring_head = next;
    }
    portEXIT_CRITICAL_ISR(&rfid_mux);
}

void OCFRFID::clearPending() {
    portENTER_CRITICAL(&rfid_mux);
    ring_head = 0;
    ring_tail = 0;
    ring_overflow = false;
    portEXIT_CRITICAL(&rfid_mux);
    resetDecoder(true);
}

void OCFRFID::decodePending() {
    uint16_t batch[64];
    for (;;) {
        int n = 0;
        portENTER_CRITICAL(&rfid_mux);
        bool overflowed = ring_overflow;
        ring_overflow = false;
        if (overflowed) ring_tail = ring_head;
        while (n < 64 && ring_tail != ring_head) {
            batch[n++] = ring[ring_tail];
            ring_tail = (ring_tail + 1) % OCF_RFID_RING_SIZE;
        }
        portEXIT_CRITICAL(&rfid_mux);
        if (overflowed) resetDecoder(false);
        for (int i = 0; i < n; i++) decodeInterval(batch[i]);
        if (n == 0) return;
    }
}

void OCFRFID::resetDecoder(bool reference_at_start) {
    position = 0;
    have_reference = reference_at_start;
    for (int i = 0; i < 2; i++) {
        lanes[i].phase = i;
        resetLane(lanes[i]);
    }
}

void OCFRFID::resetLane(Lane& lane) {
    lane.count = 0;
    memset(lane.ones, 0, sizeof(lane.ones));
    memset(lane.zeros, 0, sizeof(lane.zeros));
    memset(lane.last_known, 0, sizeof(lane.last_known));
}

void OCFRFID::decodeInterval(uint16_t cycles) {
    uint32_t units = ((uint32_t)cycles + OCF_RFID_HALF_BIT_CYCLES / 2) / OCF_RFID_HALF_BIT_CYCLES;
    if (!have_reference) {
        have_reference = units > 0;
        position = 0;
        return;
    }
    if (units == 0) {
        resetDecoder(false);
        return;
    }
    if (units > OCF_RFID_MAX_GAP_HALF_BITS) {
        resetDecoder(true);
        return;
    }
    uint32_t from = position;
    position += units;
    stepLane(lanes[0], from, position);
    stepLane(lanes[1], from, position);
}

void OCFRFID::stepLane(Lane& lane, uint32_t from, uint32_t to) {
    uint16_t mids = 0;
    uint16_t boundaries = 0;
    for (uint32_t q = from + 1; q != to; q++) {
        if ((q & 1) == lane.phase) boundaries++;
        else mids++;
    }
    if (mids == 0) {
        if (boundaries & 1) resetLane(lane);
    } else if (mids == 1) {
        emitBit(lane, (boundaries & 1) ? 0 : 1, true);
    } else {
        for (uint16_t m = 0; m < mids; m++) emitBit(lane, 0, false);
    }
    if ((to & 1) != lane.phase) emitBit(lane, 0, true);
}

void OCFRFID::emitBit(Lane& lane, uint8_t value, bool known) {
    uint8_t slot = lane.count % 128;
    lane.last_val[slot] = value;
    lane.last_known[slot] = known;
    if (known) {
        uint8_t& votes = value ? lane.ones[slot] : lane.zeros[slot];
        if (votes < UINT8_MAX) votes++;
    }
    lane.count++;
    if (lane.count % (128 * OCF_RFID_HISTORY_FRAMES) == 0) {
        for (int i = 0; i < 128; i++) {
            lane.ones[i] >>= 1;
            lane.zeros[i] >>= 1;
        }
    }
    if (lane.count >= 128 && lane.count % OCF_RFID_ATTEMPT_BITS == 0) attemptLane(lane);
}

void OCFRFID::attemptLane(Lane& lane) {
    uint8_t slot_val[128];
    uint8_t slot_known[128];
    uint8_t frame_val[128];
    uint8_t frame_known[128];
    bool aligned = false;
    for (int source = 0; source < 2; source++) {
        for (int i = 0; i < 128; i++) {
            if (source == 0) {
                slot_val[i] = lane.last_val[i];
                slot_known[i] = lane.last_known[i];
            } else {
                slot_val[i] = lane.ones[i] > lane.zeros[i];
                slot_known[i] = lane.ones[i] != lane.zeros[i];
            }
        }
        int offset;
        if (!alignFrame(slot_val, slot_known, &offset)) continue;
        aligned = true;
        for (int j = 0; j < 128; j++) {
            frame_val[j] = slot_val[(offset + j) % 128];
            frame_known[j] = slot_known[(offset + j) % 128];
        }
        unsigned long long id = 0;
        if (!solveFrame(frame_val, frame_known, &id)) continue;
        RfidRead read = { id, direction };
        xQueueSend(tag_queue, &read, 0);
        resetLane(lane);
        return;
    }
    if (aligned && lane.count % 128 == 0) crc_failures++;
}

bool OCFRFID::alignFrame(const uint8_t* val, const uint8_t* known, int* offset) {
    int best_matches = -1;
    for (int o = 0; o < 128; o++) {
        int matches = 0;
        bool conflict = false;
        for (int j = 0; j < 24 && !conflict; j++) {
            int pos = j < 11 ? j : 11 + (j - 11) * 9 + 8;
            uint8_t expected = pos >= 10;
            int slot = (o + pos) % 128;
            if (!known[slot]) continue;
            if (val[slot] == expected) matches++;
            else conflict = true;
        }
        if (!conflict && matches > best_matches) {
            best_matches = matches;
            *offset = o;
        }
    }
    return best_matches >= OCF_RFID_MIN_STRUCTURE_BITS;
}

bool OCFRFID::solveFrame(const uint8_t* val, const uint8_t* known, unsigned long long* id) {
    uint8_t work[128];
    int unknown[OCF_RFID_MAX_GUESS_BITS];
    int unknowns = 0;
    for (int i = 0; i < 128; i++) work[i] = known[i] ? val[i] : 0;
    for (int i = 0; i < 10; i++) work[i] = 0;
    work[10] = 1;
    for (int block = 0; block < 13; block++) work[11 + block * 9 + 8] = 1;
    for (int block = 0; block < 10; block++) {
        for (int p = 0; p < 8; p++) {
            int i = 11 + block * 9 + p;
            if (known[i]) continue;
            if (unknowns == OCF_RFID_MAX_GUESS_BITS) return false;
            unknown[unknowns++] = i;
        }
    }
    for (uint32_t combo = 0; combo < (1u << unknowns); combo++) {
        for (int k = 0; k < unknowns; k++) work[unknown[k]] = (combo >> k) & 1;
        if (!checkFrame(work)) continue;
        uint32_t country = extractBits(work, COUNTRY_BITS, COUNTRY_BITS_COUNT);
        *id = extractBits(work, ID_BITS, ID_BITS_COUNT) + country * 1000000000000ULL;
        return true;
    }
    return false;
}

uint8_t OCFRFID::frameByte(const uint8_t* bits, int block) {
    uint8_t value = 0;
    int base = 11 + block * 9;
    for (int p = 0; p < 8; p++) {
        if (bits[base + p]) value |= (1 << p);
    }
    return value;
}

bool OCFRFID::checkFrame(const uint8_t* bits) {
    for (int block = 0; block < 13; block++) {
        if (!bits[11 + block * 9 + 8]) return false;
    }
    uint16_t crc = 0;
    for (int block = 0; block < 8; block++) {
        crc ^= frameByte(bits, block);
        for (int i = 0; i < 8; i++) {
            crc = (crc & 1) ? (crc >> 1) ^ OCF_RFID_CRC_POLY : (crc >> 1);
        }
    }
    return crc == (uint16_t)(frameByte(bits, 8) | (frameByte(bits, 9) << 8));
}

uint64_t OCFRFID::extractBits(const uint8_t* bit_data, const int* bit_indices, int count) {
    uint64_t result = 0;
    for (int i = 0; i < count; i++) {
        result = (result << 1) | (bit_data[bit_indices[i]] != 0);
    }
    return result;
}
