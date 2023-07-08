#ifndef __OCFCAT_H__
#define __OCFCAT_H__

#include <OCFFlapControl.h>

class OCFCat{
    static int max_id;
    OCFDirection location;
    const char* name;
    const char* rfid;
    int id;
    uint64_t last_seen;

    OCFCat(const char* name, const char* rfid);
    void seen();
    void setLocation(OCFDirection new_location);
    void setName(const char* new_name);
    void setRFID(const char* new_rfid);

};

#endif // __OCFCAT_H__