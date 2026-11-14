#include "OCFRFID.h"
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
    pinMode(outside->pin_power, OUTPUT);
    digitalWrite(outside->pin_power, HIGH); // Disable initially
    outside->setupCounter();
    inside->machine = machine_outward;
    inside->direction = OCFDirection::OUTWARD;
    inside->pin_power = OCF_RFID_INSIDE_EN;
    inside->pin_dmod = OCF_RFID_INSIDE_DMOD;
    inside->pin_clk = OCF_RFID_INSIDE_CLK;
    inside->pcnt_unit = PCNT_UNIT_1;
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
    RfidRead read;
    while (xQueueReceive(tag_queue, &read, 0) == pdTRUE) {
        //Serial.printf("RFID read: %llu\n", read.id);
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
    portENTER_CRITICAL(&rfid_mux);
    data_index = 0;
    for (int i = 0; i < 128; i++){
        data[i] = 0;
    }
    state = WAITING_FOR_HEADER;
    expect_short = false;
    zero_counter = 0;
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
    BaseType_t higher_woken = pdFALSE;
    portENTER_CRITICAL_ISR(&rfid_mux);
    harvest();
    uint32_t interval = total_edges - edge_mark;
    edge_mark = total_edges;
    decodeEdge(interval > INT16_MAX ? INT16_MAX : (int16_t)interval, &higher_woken);
    portEXIT_CRITICAL_ISR(&rfid_mux);
    if (higher_woken) portYIELD_FROM_ISR();
}

void OCFRFID::decodeEdge(int16_t clk_count, BaseType_t* higher_woken) {
    if (state == DISABLE) {
        return;
    }
    bool short_pulse = clk_count < OCF_RFID_BIT_CYCLE_THRESHOLD;
    if (short_pulse && !expect_short) {
        expect_short = true;
        return;
    }else if (short_pulse && expect_short) {
        data[data_index] = 0;
        expect_short = false;
    }else if (!short_pulse && !expect_short) {
        data[data_index] = 1;
    }else if (!short_pulse && expect_short) {
        data_index = 0;
        expect_short = false;
        zero_counter = 0;
        state = WAITING_FOR_HEADER;
        return;
    }
    if (state == WAITING_FOR_HEADER) {
        if (data[data_index] == 0 && data_index < 10) {
            zero_counter++;
            data_index++;
        }else if (data_index == 10 && data[data_index] == 1 && zero_counter == 10){
            data_index++;
            zero_counter = 0;
            expect_short = false;
            state = READING;
            return;
        }else{
            data_index = 0;
            expect_short = false;
            zero_counter = 0;
            state = WAITING_FOR_HEADER;
            return;
        }
    }else if (state == READING) {
        data_index++;
        if (data_index >= 128) {
            uint32_t country = extractBits((const uint8_t*)data, COUNTRY_BITS, COUNTRY_BITS_COUNT);
            unsigned long long id = extractBits((const uint8_t*)data, ID_BITS, ID_BITS_COUNT);
            id = id + (country * 1000000000000ULL);
            if (checkFrame()) {
                RfidRead read = { id, direction };
                xQueueSendFromISR(tag_queue, &read, higher_woken);
            } else {
                crc_failures++;
            }
            data_index = 0;
            expect_short = false;
            zero_counter = 0;
            state = WAITING_FOR_HEADER;
        }
    }
}

uint8_t OCFRFID::frameByte(int block) {
    uint8_t value = 0;
    int base = 11 + block * 9;
    for (int p = 0; p < 8; p++) {
        if (data[base + p]) value |= (1 << p);
    }
    return value;
}

bool OCFRFID::checkFrame() {
    for (int block = 0; block < 13; block++) {
        if (!data[11 + block * 9 + 8]) return false;
    }
    uint16_t crc = 0;
    for (int block = 0; block < 8; block++) {
        crc ^= frameByte(block);
        for (int i = 0; i < 8; i++) {
            crc = (crc & 1) ? (crc >> 1) ^ OCF_RFID_CRC_POLY : (crc >> 1);
        }
    }
    return crc == (uint16_t)(frameByte(8) | (frameByte(9) << 8));
}

uint64_t OCFRFID::extractBits(const uint8_t* bit_data, const int* bit_indices, int count) {
    uint64_t result = 0;
    for (int i = 0; i < count; i++) {
        result = (result << 1) | (bit_data[bit_indices[i]] != 0);
    }
    return result;
}

void OCFRFID::printBitstring() {

}
