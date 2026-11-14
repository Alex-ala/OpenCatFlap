#pragma once

#include <freertos/FreeRTOS.h>

struct MockSemaphore
{
    int depth = 0;
    int takes = 0;
};

typedef MockSemaphore *SemaphoreHandle_t;

namespace mock
{
    inline int semaphores_held = 0;
}

inline SemaphoreHandle_t xSemaphoreCreateRecursiveMutex() { return new MockSemaphore(); }

inline BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t s, TickType_t)
{
    s->depth++;
    s->takes++;
    mock::semaphores_held++;
    return pdTRUE;
}

inline BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t s)
{
    if (s->depth == 0) return pdFALSE;
    s->depth--;
    mock::semaphores_held--;
    return pdTRUE;
}
