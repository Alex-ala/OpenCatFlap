#ifndef __OCFHelper_H__
#define __OCFHelper_H__

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

enum OCFDirection { INWARD, OUTWARD, NONE, BOTH };
OCFDirection parseDirection(String direction);
const char* writeDirection(OCFDirection dir);

size_t copyString(char *dest, size_t dest_size, const char *src);

class OCFMutexGuard {
    public:
        explicit OCFMutexGuard(SemaphoreHandle_t mutex) : held(mutex) { xSemaphoreTakeRecursive(held, portMAX_DELAY); }
        ~OCFMutexGuard() { xSemaphoreGiveRecursive(held); }
        OCFMutexGuard(const OCFMutexGuard&) = delete;
        OCFMutexGuard& operator=(const OCFMutexGuard&) = delete;
    private:
        SemaphoreHandle_t held;
};
#endif