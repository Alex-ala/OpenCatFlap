#ifndef __OCFFLAPCONTROL_H__
#define __OCFFLAPCONTROL_H__

#include <Arduino.h>
#include <OCFStateMachine.h>
#include <OCFConfig.h>

/*  NEW IDEAS
std::queue<const char*> activityLog;
public void reportActiviy(const char* activity)
    Called by state machines
    Updates log queue and writes mqtt

Replace FlapState with OCFConfig:  
    allowIn, allowOut, cats


*/ 

class OCFFlapControl {
    private:
        static OCFStateMachine* stateMachineInside;
        static OCFStateMachine* stateMachineOutside;
    public:
        static OCFConfig config;
        static void init();
        static void deinit();
        static void loop(void* parameters);
};
#endif // __OCFFLAPCONTROL_H__