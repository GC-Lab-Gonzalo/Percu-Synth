#pragma once
#include "FreeRTOS.h"
typedef struct SimQueue *QueueHandle_t;
#ifdef __cplusplus
extern "C" {
#endif
QueueHandle_t xQueueCreate(UBaseType_t len, UBaseType_t itemSize);
void vQueueDelete(QueueHandle_t q);
BaseType_t xQueueSend(QueueHandle_t q, const void *item, TickType_t wait);
BaseType_t xQueueSendToFront(QueueHandle_t q, const void *item, TickType_t wait);
BaseType_t xQueueOverwrite(QueueHandle_t q, const void *item);
BaseType_t xQueueReceive(QueueHandle_t q, void *item, TickType_t wait);
BaseType_t xQueuePeek(QueueHandle_t q, void *item, TickType_t wait);
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t q);
UBaseType_t uxQueueSpacesAvailable(QueueHandle_t q);
BaseType_t xQueueReset(QueueHandle_t q);
#ifdef __cplusplus
}
#endif
#define xQueueSendToBack(q, i, w)        xQueueSend(q, i, w)
#define xQueueSendFromISR(q, i, woken)   xQueueSend(q, i, 0)
#define xQueueSendToBackFromISR(q, i, w) xQueueSend(q, i, 0)
#define xQueueOverwriteFromISR(q, i, w)  xQueueOverwrite(q, i)
#define xQueueReceiveFromISR(q, i, w)    xQueueReceive(q, i, 0)
#define uxQueueMessagesWaitingFromISR(q) uxQueueMessagesWaiting(q)
