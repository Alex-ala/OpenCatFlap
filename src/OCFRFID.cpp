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

void OCFRFID::init(OCFStateMachine* machine_outside, OCFStateMachine* machine_inside) {
    outside->machine = machine_outside;
    outside->direction = OCFDirection::OUT;
    outside->pin_power = OCF_RFID_OUTSIDE_EN;
    outside->pin_dmod = OCF_RFID_OUTSIDE_DMOD;
    outside->pin_clk = OCF_RFID_OUTSIDE_CLK;
    outside->pcnt_unit = PCNT_UNIT_0;
    pinMode(outside->pin_power, OUTPUT);
    digitalWrite(outside->pin_power, HIGH); // Disable initially
    outside->setupCounter();
    inside->machine = machine_inside;
    inside->direction = OCFDirection::IN;
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
        activate(active_instance == inside ? OCFDirection::OUT : OCFDirection::IN);
    }
    uint32_t failures = crc_failures;
    if (failures != reported_crc_failures) {
        log_d("RFID frames rejected by CRC: %u", failures);
        reported_crc_failures = failures;
    }
    if (tag_queue == nullptr) return;
    RfidRead read;
    while (xQueueReceive(tag_queue, &read, 0) == pdTRUE) {
        Serial.printf("RFID read: %llu\n", read.id);
        OCFRFID *reader = (read.direction == OCFDirection::IN) ? inside : outside;
        if (reader->machine) reader->machine->readRFID(read.id);
    }
}

void OCFRFID::enable(OCFDirection direction) {
    if (direction == OCFDirection::IN) {
        inside_enabled = true;
        if (!outside_enabled) {
            activate(OCFDirection::IN);
        }
    } else if (direction == OCFDirection::OUT) {
        outside_enabled = true;
        if (!inside_enabled) {
            activate(OCFDirection::OUT);
        }
    }
}
void OCFRFID::disable(OCFDirection direction) {
    if (direction == OCFDirection::IN) {
        inside_enabled = false;
        if (outside_enabled && active_instance != outside) {
            activate(OCFDirection::OUT);
        }else{
            deactivate(direction);
        }
    } else if (direction == OCFDirection::OUT) {
        outside_enabled = false;
        if (inside_enabled && active_instance != inside) {
            activate(OCFDirection::IN);
        }else{
            deactivate(direction);
        }
    }
}

void OCFRFID::activate(OCFDirection direction) {
    if (direction == OCFDirection::IN) {
        outside->disable();
        inside->enable();
        log_d("Enable RFID inside");
    } else if (direction == OCFDirection::OUT) {
        inside->disable();
        outside->enable();
        log_d("Enable RFID outside");
    }
    last_switch_ms = millis();
}
void OCFRFID::deactivate(OCFDirection direction) {
    if (direction == OCFDirection::IN) {
        inside->disable();
    } else if (direction == OCFDirection::OUT) {
        outside->disable();
    }
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
}

void OCFRFID::onInterruptDmod() {
    int16_t clk_count = 0;
    pcnt_get_counter_value(pcnt_unit, &clk_count);
    pcnt_counter_clear(pcnt_unit);
    BaseType_t higher_woken = pdFALSE;
    portENTER_CRITICAL_ISR(&rfid_mux);
    decodeEdge(clk_count, &higher_woken);
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
