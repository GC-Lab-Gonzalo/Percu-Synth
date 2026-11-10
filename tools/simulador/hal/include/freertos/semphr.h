#pragma once
#include "FreeRTOS.h"
typedef struct SimSem *SemaphoreHandle_t;
#ifdef __cplusplus
extern "C" {
#endif
SemaphoreHandle_t xSemaphoreCreateMutex(void);
SemaphoreHandle_t xSemaphoreCreateRecursiveMutex(void);
SemaphoreHandle_t xSemaphoreCreateBinary(void);
SemaphoreHandle_t xSemaphoreCreateCounting(UBaseType_t max, UBaseType_t initial);
BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t wait);
BaseType_t xSemaphoreGive(SemaphoreHandle_t s);
void vSemaphoreDelete(SemaphoreHandle_t s);
UBaseType_t uxSemaphoreGetCount(SemaphoreHandle_t s);
#ifdef __cplusplus
}
#endif
#define xSemaphoreTakeRecursive(s, w)     xSemaphoreTake(s, w)
#define xSemaphoreGiveRecursive(s)        xSemaphoreGive(s)
#define xSemaphoreGiveFromISR(s, woken)   xSemaphoreGive(s)
#define xSemaphoreTakeFromISR(s, woken)   xSemaphoreTake(s, 0)
