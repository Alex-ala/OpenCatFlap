#pragma once

#include <cstdint>

typedef int BaseType_t;
typedef unsigned int UBaseType_t;
typedef uint32_t TickType_t;
typedef void *TaskHandle_t;
typedef void (*TaskFunction_t)(void *);

#define pdFALSE ((BaseType_t)0)
#define pdTRUE ((BaseType_t)1)
#define pdPASS pdTRUE
#define pdFAIL pdFALSE
#define portMAX_DELAY ((TickType_t)0xffffffffUL)
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))

typedef struct
{
    int owner;
} portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED {0}

namespace mock
{
    inline int critical_depth = 0;
    inline int critical_max_depth = 0;
    inline int yields_from_isr = 0;

    inline void enter_critical(portMUX_TYPE *mux)
    {
        mux->owner++;
        critical_depth++;
        if (critical_depth > critical_max_depth) critical_max_depth = critical_depth;
    }
    inline void exit_critical(portMUX_TYPE *mux)
    {
        mux->owner--;
        critical_depth--;
    }
}

#define portENTER_CRITICAL(mux) mock::enter_critical(mux)
#define portEXIT_CRITICAL(mux) mock::exit_critical(mux)
#define portENTER_CRITICAL_ISR(mux) mock::enter_critical(mux)
#define portEXIT_CRITICAL_ISR(mux) mock::exit_critical(mux)
#define portYIELD_FROM_ISR() (mock::yields_from_isr++)

inline BaseType_t xTaskCreate(TaskFunction_t, const char *, uint32_t, void *, UBaseType_t, TaskHandle_t *) { return pdPASS; }
inline void vTaskDelete(TaskHandle_t) {}
