#include "OCF_RFID.h"
OCF_RFID* OCF_RFID::inside = new OCF_RFID(IN); 
OCF_RFID* OCF_RFID::outside = new OCF_RFID(OUT);
OCFDirection OCF_RFID::active_direction = NONE;

void OCF_RFID::init() {
    //enable(OUT);
}
void OCF_RFID::enable(OCFDirection direction) {
    if (direction == IN) {
        outside->disable(); 
        inside->enable();
        active_direction = IN;
    } else if (direction == OUT) {
        inside->disable();
        outside->enable();
        active_direction = OUT;
    }
}
void OCF_RFID::swap_direction() {
    if (active_direction == OUT) {
        enable(IN);
    }
    else {
        enable(OUT);
    }
}

void OCF_RFID::handle_clk() {
    if (active_direction == OUT) {
        outside->findClock();
    } else if (active_direction == IN) {
        inside->findClock();
    }
}
void OCF_RFID::handle_dmod() {
    if (active_direction == OUT) {
        outside->storeTime();
    } else if (active_direction == IN) {
        inside->storeTime();
    }
}

OCF_RFID::OCF_RFID(OCFDirection direction) : direction(direction) {
    timing = 7.5;
    carryover = false;
    clock_cycle_count = 0;
    demod_pos = 0;
    last_micros = 0;
    reading = false;
    zero_count = 0;
    code_received = false;
    if (direction == IN) {
        pin_clk = OCF_RFID_INSIDE_CLK;
        pin_dmod = OCF_RFID_INSIDE_DMOD;
        pin_power = OCF_RFID_INSIDE_EN;
    } else if (direction == OUT) {
        pin_clk = OCF_RFID_OUTSIDE_CLK;
        pin_dmod = OCF_RFID_OUTSIDE_DMOD;
        pin_power = OCF_RFID_OUTSIDE_EN;
    }
    pinMode(pin_power, OUTPUT);
    digitalWrite(pin_power, HIGH); // Disable power by default
    pinMode(pin_clk, INPUT);
    pinMode(pin_dmod, INPUT);
}
void OCF_RFID::enable() {
    digitalWrite(pin_power, LOW);
    clock_cycle_count = 0;
    demod_pos = 0;
    last_micros = 0;
    reading = false;
    zero_count = 0;
    code_received = false;
    attachInterrupt(pin_clk, handle_clk, RISING);
    attachInterrupt(pin_dmod, handle_dmod, RISING);
    Serial.printf("Enable RFID in direction: %s\n", (direction == IN) ? "IN" : "OUT");
}
void OCF_RFID::disable() {
    digitalWrite(pin_power, HIGH);
    detachInterrupt(pin_clk);
    detachInterrupt(pin_dmod);
}

void OCF_RFID::parse_id() {
    long long result = 0;
    int bit_count = 0;
    int skipped_bits = 0;
    // Extract 38bit chip id after header
    for (int i = 11; i < 53; i++) {
        // Turn into long long while removing spacing 1s
        bit_count++;
        if (bit_count > 8) {
        skipped_bits++;
        bit_count = 0;
        continue;
        }
        result |= ((long long)read_code[i] << (i - skipped_bits - 11));
    }
    Serial.printf("Chip id: %lld\n", result);
    }
void OCF_RFID::findClock() {
    if (clock_cycle_count >= 250){
        for (int i = 249; i > 0; i--){
        clock_cycles[i] = clock_cycles[i] - clock_cycles[i-1];
        }
        clock_cycles[0] = 0;
        double average = 0;
        for (int i = 1; i < 250; i++){
        average += clock_cycles[i];
        }
        average = average / 250;
        timing = average;
        clock_cycle_count = 0;
    }
    clock_cycles[clock_cycle_count] = micros();
    clock_cycle_count++;
}
void OCF_RFID::storeTime() {
    unsigned long current_micros = micros();
    unsigned long diff = current_micros - last_micros;
    last_micros = current_micros;
    if (diff < timing*39){   // x * 37
        if (!reading){
            zero_count++;
        }else{
            demod[demod_pos] = 0;
            demod_pos++;
        }
    }else if (diff > timing*40 && diff < timing*55){ // x*40 - x*54.5
        if (!reading){
            if ((carryover && zero_count >= 9) || (!carryover && zero_count >= 10)){
                zero_count = 0;
                carryover = false;
                reading = true;
                for (int i = 0; i < 10; i++){
                    demod[i] = 0;
                }
                demod[10] = 1;
                demod_pos = 11;
            }
            carryover = !carryover;
        }else{
            if (carryover){
                if (demod_pos > 126){
                    reading = false;
                    demod_pos = 0;
                    zero_count = 0;
                    carryover = false;
                }else{
                    demod[demod_pos] = 0;
                    demod[demod_pos + 1] = 1;
                    demod_pos += 2;
                }
            }else{
                demod[demod_pos] = 1;
                demod_pos++;
            }
            carryover = !carryover;
        }
    }else if (diff > timing*55 && diff < timing*73){ // x*55.8 - x*73
        if (!reading){
            if (zero_count >= 10){
                zero_count = 0;
                reading = true;

                for (int i = 0; i < 10; i++){
                    demod[i] = 0;
                }
                demod[10] = 1;
                demod[11] = 1;
                demod_pos = 12;
            }
            carryover = false;
            zero_count = 0;
        }else{
            if (demod_pos > 126){
                reading = false;
                demod_pos = 0;
                return;
            }
            demod[demod_pos] = 1;
            demod[demod_pos+1] = 1;
            demod_pos += 2;
        }
    }else{
        demod_pos = 0;
        reading = false;
        zero_count = 0;
    }
    if (demod_pos >= 128){
        reading = false;
        demod_pos = 0;
        for (int i = 0; i < 128; i++){
            read_code[i] = demod[i];
            demod[i] = 0;
        }
        parse_id();
    }
}
