#include "OCF_RFID.h"

OCF_RFID *OCF_RFID::outside = new OCF_RFID();
OCF_RFID *OCF_RFID::inside = new OCF_RFID();
OCF_RFID *OCF_RFID::active_instance = nullptr;

void OCF_RFID::on_clk() {
    if (active_instance) active_instance->onInterruptClk();
}

void OCF_RFID::on_dmod() {
    if (active_instance) active_instance->onInterruptDmod();
}

void OCF_RFID::init() {
    outside->direction = OCFDirection::OUT;
    outside->pin_power = OCF_RFID_OUTSIDE_EN;
    outside->pin_dmod = OCF_RFID_OUTSIDE_DMOD;
    outside->pin_clk = OCF_RFID_OUTSIDE_CLK;
    pinMode(outside->pin_power, OUTPUT);
    digitalWrite(outside->pin_power, HIGH); // Disable initially
    inside->direction = OCFDirection::IN;
    inside->pin_power = OCF_RFID_INSIDE_EN;
    inside->pin_dmod = OCF_RFID_INSIDE_DMOD;
    inside->pin_clk = OCF_RFID_INSIDE_CLK;
    pinMode(inside->pin_power, OUTPUT);
    digitalWrite(inside->pin_power, HIGH); // Disable initially
    enable(OCFDirection::IN);
}

void OCF_RFID::enable(OCFDirection direction) {
    if (direction == OCFDirection::IN) {
        outside->disable();
        inside->enable();
    } else if (direction == OCFDirection::OUT) {
        inside->disable();
        outside->enable();
    }
}

void OCF_RFID::enable() {
    clk_counter = 0;
    data_index = 0;
    for (int i = 0; i < 128; i++){
        data[i] = 0;
    }
    digitalWrite(pin_power, LOW);
    state = WAITING_FOR_HEADER;
    active_instance = this;
    attachInterrupt(pin_clk, OCF_RFID::on_clk, RISING);
    attachInterrupt(pin_dmod, OCF_RFID::on_dmod, CHANGE);
    Serial.printf("Enable RFID in direction: %s\n", (direction == IN) ? "IN" : "OUT");
}

void OCF_RFID::disable() {
    digitalWrite(pin_power, HIGH);
    detachInterrupt(pin_clk);
    detachInterrupt(pin_dmod);
}

void OCF_RFID::onInterruptClk() {
    clk_counter++;
}

void OCF_RFID::onInterruptDmod() {
    if (state == DISABLE) {
        return;
    }
    bool short_pulse = clk_counter < 24;
    clk_counter = 0;
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
        if (data_index >= 128) {  // 128 Bits für ISO11784
            printBitstring();
            data_index = 0;
            expect_short = false;
            zero_counter = 0;
            state = WAITING_FOR_HEADER;
        }
    }
}

uint64_t OCF_RFID::extractBits(const uint8_t* bit_data, const int* bit_indices, int count) {
    uint64_t result = 0;
    for (int i = 0; i < count; i++) {
        result = (result << 1) | (bit_data[bit_indices[i]] != 0);
    }
    return result;
}

void OCF_RFID::printBitstring() {
    uint32_t country = extractBits((const uint8_t*)data, COUNTRY_BITS, COUNTRY_BITS_COUNT);
    uint64_t id = extractBits((const uint8_t*)data, ID_BITS, ID_BITS_COUNT);
    
    Serial.printf("Country (dec): %u\n", country);
    Serial.printf("ID: %llu\n", id);
}
