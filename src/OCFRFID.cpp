#include "OCFRFID.h"
#include <OCFMQTT.h>
#include <OCFStateMachine.h>
#include <mutex>

OCFRFID *OCFRFID::outside = new OCFRFID();
OCFRFID *OCFRFID::inside = new OCFRFID();
OCFRFID *OCFRFID::active_instance = nullptr;
bool OCFRFID::inside_enabled = false;
bool OCFRFID::outside_enabled = false;
unsigned int OCFRFID::switch_counter = 0;
std::mutex rfid_mutex;

void OCFRFID::on_clk() {
    if (active_instance) active_instance->onInterruptClk();
    if (inside_enabled && outside_enabled) {
        switch_counter++;
        if (switch_counter >= 134000) {
            switch_counter = 0;
            if (active_instance == inside) {
                activate(OCFDirection::OUT);
            } else {
                activate(OCFDirection::IN);
            }
        }
    }
}

void OCFRFID::on_dmod() {
    if (active_instance) active_instance->onInterruptDmod();
}

void OCFRFID::init(OCFStateMachine* machine_outside, OCFStateMachine* machine_inside) {
    outside->machine = machine_outside;
    outside->direction = OCFDirection::OUT;
    outside->pin_power = OCF_RFID_OUTSIDE_EN;
    outside->pin_dmod = OCF_RFID_OUTSIDE_DMOD;
    outside->pin_clk = OCF_RFID_OUTSIDE_CLK;
    pinMode(outside->pin_power, OUTPUT);
    digitalWrite(outside->pin_power, HIGH); // Disable initially
    inside->machine = machine_inside;
    inside->direction = OCFDirection::IN;
    inside->pin_power = OCF_RFID_INSIDE_EN;
    inside->pin_dmod = OCF_RFID_INSIDE_DMOD;
    inside->pin_clk = OCF_RFID_INSIDE_CLK;
    pinMode(inside->pin_power, OUTPUT);
    digitalWrite(inside->pin_power, HIGH); // Disable initially
    inside_enabled = false;
    outside_enabled = false;
    switch_counter = 0;
    log_d("RFID initialized");
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
}
void OCFRFID::deactivate(OCFDirection direction) {
    if (direction == OCFDirection::IN) {
        inside->disable();
    } else if (direction == OCFDirection::OUT) {
        outside->disable();
    }
}

void OCFRFID::enable() {
    clk_counter = 0;
    data_index = 0;
    for (int i = 0; i < 128; i++){
        data[i] = 0;
    }
    digitalWrite(pin_power, LOW);
    state = WAITING_FOR_HEADER;
    active_instance = this;
    attachInterrupt(pin_clk, OCFRFID::on_clk, RISING);
    attachInterrupt(pin_dmod, OCFRFID::on_dmod, CHANGE);
}

void OCFRFID::disable() {
    detachInterrupt(pin_clk);
    detachInterrupt(pin_dmod);
    digitalWrite(pin_power, HIGH);
}

void OCFRFID::onInterruptClk() {
    clk_counter++;
}

void OCFRFID::onInterruptDmod() {
    if (state == DISABLE) {
        return;
    }
    std::lock_guard<std::mutex> lock(rfid_mutex);
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
        if (data_index >= 128) {
            uint32_t country = extractBits((const uint8_t*)data, COUNTRY_BITS, COUNTRY_BITS_COUNT);
            unsigned long long id = extractBits((const uint8_t*)data, ID_BITS, ID_BITS_COUNT);
            id = id + (country * 1000000000000ULL);
            machine->readRFID(id);
            Serial.printf("RFID read: %llu\n", id);
            data_index = 0;
            expect_short = false;
            zero_counter = 0;
            state = WAITING_FOR_HEADER;
        }
    }
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
