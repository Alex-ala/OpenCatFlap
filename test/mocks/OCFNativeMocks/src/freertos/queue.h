#pragma once

#include <cstring>
#include <deque>
#include <vector>

#include <freertos/FreeRTOS.h>

struct MockQueue
{
    size_t capacity;
    size_t item_size;
    std::deque<std::vector<uint8_t>> items;
};

typedef MockQueue *QueueHandle_t;

inline QueueHandle_t xQueueCreate(UBaseType_t length, UBaseType_t item_size)
{
    return new MockQueue{length, item_size, {}};
}

inline BaseType_t xQueueSendFromISR(QueueHandle_t q, const void *item, BaseType_t *higher_woken)
{
    if (q->items.size() >= q->capacity) return pdFALSE;
    const uint8_t *p = static_cast<const uint8_t *>(item);
    q->items.emplace_back(p, p + q->item_size);
    if (higher_woken) *higher_woken = pdTRUE;
    return pdTRUE;
}

inline BaseType_t xQueueSend(QueueHandle_t q, const void *item, TickType_t)
{
    return xQueueSendFromISR(q, item, nullptr);
}

inline BaseType_t xQueueReceive(QueueHandle_t q, void *buffer, TickType_t)
{
    if (q->items.empty()) return pdFALSE;
    memcpy(buffer, q->items.front().data(), q->item_size);
    q->items.pop_front();
    return pdTRUE;
}

inline UBaseType_t uxQueueMessagesWaiting(QueueHandle_t q) { return q->items.size(); }
