#pragma once
typedef enum { ESP_LOG_NONE, ESP_LOG_ERROR, ESP_LOG_WARN, ESP_LOG_INFO, ESP_LOG_DEBUG, ESP_LOG_VERBOSE } esp_log_level_t;
static inline void esp_log_level_set(const char *, esp_log_level_t) {}
#define ESP_LOGE(tag, ...) do {} while (0)
#define ESP_LOGW(tag, ...) do {} while (0)
#define ESP_LOGI(tag, ...) do {} while (0)
#define ESP_LOGD(tag, ...) do {} while (0)
#define ESP_LOGV(tag, ...) do {} while (0)
#define log_e(...) do {} while (0)
#define log_w(...) do {} while (0)
#define log_i(...) do {} while (0)
#define log_d(...) do {} while (0)
#define log_v(...) do {} while (0)
