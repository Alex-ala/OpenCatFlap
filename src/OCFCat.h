#ifndef __OCFCAT_H__
#define __OCFCAT_H__

#include <Arduino.h>
#include <OCFHelper.h>

enum ALLOW_STATE {
    UNDEF = -1,
    FALSE,
    TRUE
};
struct OCFCat{
    OCFDirection location {OCFDirection::NONE};
    char name[30];
    unsigned long long rfid;
    ALLOW_STATE allow_in {UNDEF};
    ALLOW_STATE allow_out {UNDEF};
    uint64_t last_seen {0};
};

static ALLOW_STATE parseAllowState(String allowState){
    if (allowState == "true") return TRUE;
    if (allowState == "false") return FALSE;
    return UNDEF;
}

#endif // __OCFCAT_H__