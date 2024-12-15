#ifndef __OCFCAT_H__
#define __OCFCAT_H__

#include <Arduino.h>
#include <OCFHelper.h>

struct OCFCat{
    OCFDirection location {OCFDirection::NONE};
    char name[30];
    unsigned long long rfid;
    bool allow_in {true};
    bool allow_out {true};
    uint64_t last_seen {0};
};

#endif // __OCFCAT_H__