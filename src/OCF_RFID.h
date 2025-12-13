#ifndef __OCF_RFID_H__
#define __OCF_RFID_H__

#include <definitions.h>
#include <OCFHelper.h>
#include <Arduino.h>

enum OFC_RFID_STATE {
    WAITING_FOR_HEADER,
    READING,
    DISABLE
};

class OCF_RFID {
    public:
        static void init();
    private:
        static OCF_RFID* outside;
        static OCF_RFID* inside;
        static OCF_RFID* active_instance;
        static void enable(OCFDirection direction);
        static void on_clk();
        static void on_dmod();

        void onInterruptClk();
        void onInterruptDmod();
        void printBitstring();
        void enable();
        void disable();

        OCFDirection direction;
        int pin_power;
        int pin_dmod;
        int pin_clk;

        int clk_counter;
        OFC_RFID_STATE state;
        bool data[128];
        int data_index;
        bool expect_short = false;
        int zero_counter = 0;

};

#endif // __OCF_RFID_H__
