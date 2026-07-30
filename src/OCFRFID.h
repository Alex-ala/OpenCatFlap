#ifndef __OCFRFID_H__
#define __OCFRFID_H__

#include <definitions.h>
#include <OCFHelper.h>
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <driver/pcnt.h>


#define COUNTRY_BITS_COUNT 10
#define ID_BITS_COUNT 38
const int COUNTRY_BITS[COUNTRY_BITS_COUNT] = {63,62,61,60,59,58,57,56,54,53};
const int ID_BITS[ID_BITS_COUNT] = {52,51,50,49,48,47,45,44,43,42,41,40,39,38,36,35,34,33,32,31,30,29,27,26,25,24,23,22,21,20,18,17,16,15,14,13,12,11};

class OCFStateMachine;
class OCFRFID {
    enum OFC_RFID_STATE {
        WAITING_FOR_HEADER,
        READING,
        DISABLE
    };
    public:
        static void init(OCFStateMachine* machine_outside, OCFStateMachine* machine_inside);
        static void enable(OCFDirection direction);
        static void disable(OCFDirection direction);
        static void process();
    private:
        static OCFRFID* outside;
        static OCFRFID* inside;
        static OCFRFID* volatile active_instance;
        static bool outside_enabled;
        static bool inside_enabled;
        static uint32_t last_switch_ms;
        static QueueHandle_t tag_queue;
        static void on_dmod();
        static void activate(OCFDirection direction);
        static void deactivate(OCFDirection direction);


        void onInterruptDmod();
        void decodeEdge(int16_t clk_count, BaseType_t* higher_woken);
        uint8_t frameByte(int block);
        bool checkFrame();
        void setupCounter();
        void printBitstring();
        void enable();
        void disable();
        uint64_t extractBits(const uint8_t* bit_data, const int* bit_indices, int count);

        OCFDirection direction;
        OCFStateMachine* machine;
        int pin_power;
        int pin_dmod;
        int pin_clk;
        pcnt_unit_t pcnt_unit;
        OFC_RFID_STATE state;
        bool data[128];
        int data_index;
        bool expect_short = false;
        int zero_counter = 0;

};

#endif // __OCFRFID_H__
