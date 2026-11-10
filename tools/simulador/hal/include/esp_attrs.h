// Atributos de sección del ESP32: en el simulador no hay IRAM ni DRAM, todo es memoria lineal.
#pragma once
#define IRAM_ATTR
#define DRAM_ATTR
#define RTC_DATA_ATTR
#define RTC_NOINIT_ATTR
#define EXT_RAM_ATTR
#define EXT_RAM_BSS_ATTR
#define WORD_ALIGNED_ATTR __attribute__((aligned(4)))
#define DMA_ATTR WORD_ALIGNED_ATTR
#define NOINLINE_ATTR __attribute__((noinline))
#define FORCE_INLINE_ATTR static inline __attribute__((always_inline))
#define ARDUINO_ISR_ATTR
