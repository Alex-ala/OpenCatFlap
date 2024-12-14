#ifndef __OCFFLAPCONTROL_H__
#define __OCFFLAPCONTROL_H__

#include <Arduino.h>
#include <ArxContainer.h>
#include <OCFStateMachine.h>
#include <OCFConfig.h>
#include <OCFCat.h>

/*  NEW IDEAS
std::queue<const char*> activityLog;
public void reportActiviy(const char* activity)
    Called by state machines
    Updates log queue and writes mqtt

Replace FlapState with OCFConfig:
    allowIn, allowOut, cats


*/

class OCFFlapControl
{
private:
    inline static OCFStateMachine *stateMachineInside;
    inline static OCFStateMachine *stateMachineOutside;

public:
    inline static OCFConfig config;
    inline static std::map<unsigned long long, OCFCat> cats;
    static void init();
    static void deinit();
    static void loop(void *parameters);

    static void getStateJSON(String& outString);
    static void saveCats();
};
#endif // __OCFFLAPCONTROL_H__
