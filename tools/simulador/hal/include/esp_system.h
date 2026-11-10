#pragma once
#include <stdint.h>
#include "esp_err.h"
#include "esp_random.h"
typedef enum {
  ESP_RST_UNKNOWN, ESP_RST_POWERON, ESP_RST_EXT, ESP_RST_SW, ESP_RST_PANIC, ESP_RST_INT_WDT,
  ESP_RST_TASK_WDT, ESP_RST_WDT, ESP_RST_DEEPSLEEP, ESP_RST_BROWNOUT, ESP_RST_SDIO
} esp_reset_reason_t;
#ifdef __cplusplus
extern "C" {
#endif
esp_reset_reason_t esp_reset_reason(void);
void esp_restart(void);
uint32_t esp_get_free_heap_size(void);
uint32_t esp_get_minimum_free_heap_size(void);
#ifdef __cplusplus
}
#endif
