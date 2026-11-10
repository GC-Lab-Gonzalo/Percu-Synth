// FreeRTOS de la placa virtual: cada tarea es un hilo (pthread) del navegador.
// El tick es de 1 ms, como en arduino-esp32 (configTICK_RATE_HZ = 1000).
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "esp_attrs.h"
#include "esp_err.h"
typedef uint32_t TickType_t;
typedef int32_t  BaseType_t;
typedef uint32_t UBaseType_t;
typedef uint32_t StackType_t;
#define pdTRUE  1
#define pdFALSE 0
#define pdPASS  1
#define pdFAIL  0
#define errQUEUE_FULL 0
#define portMAX_DELAY ((TickType_t)0xFFFFFFFF)
#define configTICK_RATE_HZ 1000
#define portTICK_PERIOD_MS 1
#define portTICK_RATE_MS 1
#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define pdTICKS_TO_MS(t)  ((uint32_t)(t))
#define configMAX_PRIORITIES 25
#define configMINIMAL_STACK_SIZE 768
#define tskIDLE_PRIORITY 0
#define tskNO_AFFINITY 0x7FFFFFFF
#define portNUM_PROCESSORS 2
#define ARDUINO_RUNNING_CORE 1
#define CONFIG_ARDUINO_RUNNING_CORE 1
#define CONFIG_FREERTOS_HZ 1000

// portMUX: un candado de giro (en el ESP32 es exactamente eso)
typedef struct { volatile int owner; volatile int count; } portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED { 0, 0 }
#define SPINLOCK_INITIALIZER { 0, 0 }
#ifdef __cplusplus
extern "C" {
#endif
void sim_mux_lock(portMUX_TYPE *m);
void sim_mux_unlock(portMUX_TYPE *m);
BaseType_t xPortGetCoreID(void);
void sim_task_yield(void);
#ifdef __cplusplus
}
#endif
#define portENTER_CRITICAL(m)      sim_mux_lock(m)
#define portEXIT_CRITICAL(m)       sim_mux_unlock(m)
#define portENTER_CRITICAL_ISR(m)  sim_mux_lock(m)
#define portEXIT_CRITICAL_ISR(m)   sim_mux_unlock(m)
#define portENTER_CRITICAL_SAFE(m) sim_mux_lock(m)
#define portEXIT_CRITICAL_SAFE(m)  sim_mux_unlock(m)
#define taskENTER_CRITICAL(m)      sim_mux_lock(m)
#define taskEXIT_CRITICAL(m)       sim_mux_unlock(m)
#define taskENTER_CRITICAL_ISR(m)  sim_mux_lock(m)
#define taskEXIT_CRITICAL_ISR(m)   sim_mux_unlock(m)
#define portYIELD_FROM_ISR(...)    do {} while (0)
#define portYIELD()                sim_task_yield()
#define taskYIELD()                sim_task_yield()
#define portDISABLE_INTERRUPTS()
#define portENABLE_INTERRUPTS()
