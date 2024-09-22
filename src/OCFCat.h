#ifndef __OCFCAT_H__
#define __OCFCAT_H__

#include <OCFHelper.h>

struct OCFCat{
    OCFDirection location {OCFDirection::NONE};
    const char* name {""};
    const char* rfid {""};
    bool allow_in {false};
    bool allow_out {false};
    uint64_t last_seen {0};
};

#endif // __OCFCAT_H__