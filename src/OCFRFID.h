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

struct OCFRFIDStats {
    uint32_t edges;
    uint32_t glitches;
    uint32_t headers;
    uint32_t frames;
    uint32_t repaired;
    uint32_t crc_errors;
    uint32_t hist[OCF_RFID_HIST_BINS];
};

struct OCFRFIDFrequency {
    uint32_t hz;
    uint32_t age_ms;
};

class OCFRFID {
    struct Lane {
        uint8_t phase;
        uint32_t count;
        uint8_t ones[128];
        uint8_t zeros[128];
        uint8_t last_val[128];
        uint8_t last_known[128];
    };
    public:
        static void init(OCFStateMachine* machine_inward, OCFStateMachine* machine_outward);
        static void enable(OCFDirection direction);
        static void disable(OCFDirection direction);
        static void process();
        static OCFRFIDFrequency insideFrequency();
        static OCFRFIDFrequency outsideFrequency();
        static OCFRFIDStats insideStats();
        static OCFRFIDStats outsideStats();
    private:
        static OCFRFID* outside;
        static OCFRFID* inside;
        static OCFRFID* volatile active_instance;
        static bool outside_enabled;
        static bool inside_enabled;
        static uint32_t last_switch_ms;
        static QueueHandle_t tag_queue;
        static void on_dmod();
        static void activate(OCFRFID* reader);
        static OCFRFID* readerFor(OCFDirection direction);
        static uint8_t frameByte(const uint8_t* bits, int block);
        static bool checkFrame(const uint8_t* bits);
        static uint64_t extractBits(const uint8_t* bit_data, const int* bit_indices, int count);


        void onInterruptDmod();
        void harvest();
        void updateFrequency();
        OCFRFIDFrequency frequency();
        OCFRFIDStats readStats();
        void setupCounter();
        void enable();
        void disable();
        void clearPending();
        void decodePending();
        void resetDecoder(bool reference_at_start);
        void resetLane(Lane& lane);
        void decodeInterval(uint16_t cycles);
        void stepLane(Lane& lane, uint32_t from, uint32_t to);
        void emitBit(Lane& lane, uint8_t value, bool known);
        void attemptLane(Lane& lane);
        static bool alignFrame(const uint8_t* val, const uint8_t* known, int* offset);
        static bool solveFrame(const uint8_t* val, const uint8_t* known, unsigned long long* id, bool* guessed);

        OCFDirection direction;
        OCFStateMachine* machine;
        int pin_power;
        int pin_dmod;
        int pin_clk;
        pcnt_unit_t pcnt_unit;
        bool powered = false;
        uint32_t total_edges = 0;
        uint32_t edge_mark = 0;
        int16_t last_count = 0;
        uint32_t enabled_ms = 0;
        uint32_t window_start_us = 0;
        uint32_t window_start_edges = 0;
        uint32_t last_update_us = 0;
        uint32_t frequency_hz = 0;
        uint32_t measured_ms = 0;
        OCFRFIDStats stats = {};

        uint16_t ring[OCF_RFID_RING_SIZE];
        volatile uint16_t ring_head = 0;
        volatile uint16_t ring_tail = 0;
        volatile bool ring_overflow = false;
        uint32_t position = 0;
        bool have_reference = false;
        Lane lanes[2];

};

#endif // __OCFRFID_H__
