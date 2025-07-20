#ifndef __OCF_RFID_H__
#define __OCF_RFID_H__

#include <definitions.h>
#include <OCFHelper.h>
#include <Arduino.h>

class OCF_RFID {
    public:
        static void init();
        static void swap_direction();
    private:
        static OCFDirection active_direction;
        static OCF_RFID* outside;
        static OCF_RFID* inside;
        static void handle_clk();
        static void handle_dmod();
        
        OCF_RFID(OCFDirection direction);
        void parse_id();
        void findClock();
        void storeTime();
        void enable();
        void disable();
        static void enable(OCFDirection direction);
        OCFDirection direction;

        double timing;

        int clock_cycle_count;
        unsigned long clock_cycles[250];
        bool carryover;
        bool demod[128];
        byte demod_pos;
        unsigned long last_micros;
        bool reading;
        int zero_count;
        bool read_code[128];
        bool code_received;

        int pin_power;
        int pin_dmod;
        int pin_clk;
};

#endif // __OCF_RFID_H__