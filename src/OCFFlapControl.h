#ifndef __OCFFLAPCONTROL_H__
#define __OCFFLAPCONTROL_H__

#include <Arduino.h>
#include <ArxContainer.h>
#include <OCFStateMachine.h>
#include <OCFConfig.h>
#include <OCFCat.h>
#include <OCFRFID.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

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
    inline static OCFStateMachine *stateMachineInward;
    inline static OCFStateMachine *stateMachineOutward;
    inline static SemaphoreHandle_t loop_mutex = xSemaphoreCreateRecursiveMutex();

public:
    static void init();
    static void deinit();
    static void loop(void *parameters);
    static OCFMachineStates getInwardState();
    static OCFMachineStates getOutwardState();
    static bool suspend();
    static void resume();
};
#endif // __OCFFLAPCONTROL_H__
