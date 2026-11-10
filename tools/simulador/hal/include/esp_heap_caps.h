// En el simulador toda la memoria es una sola: las "capacidades" se ignoran.
#pragma once
#include <stdlib.h>
#include <stdint.h>
#define MALLOC_CAP_EXEC      (1<<0)
#define MALLOC_CAP_32BIT     (1<<1)
#define MALLOC_CAP_8BIT      (1<<2)
#define MALLOC_CAP_DMA       (1<<3)
#define MALLOC_CAP_SPIRAM    (1<<10)
#define MALLOC_CAP_INTERNAL  (1<<11)
#define MALLOC_CAP_DEFAULT   (1<<12)
static inline void *heap_caps_malloc(size_t s, uint32_t) { return malloc(s); }
static inline void *heap_caps_calloc(size_t n, size_t s, uint32_t) { return calloc(n, s); }
static inline void *heap_caps_realloc(void *p, size_t s, uint32_t) { return realloc(p, s); }
static inline void *heap_caps_aligned_alloc(size_t a, size_t s, uint32_t) { return aligned_alloc(a, (s + a - 1) / a * a); }
static inline void  heap_caps_free(void *p) { free(p); }
static inline size_t heap_caps_get_free_size(uint32_t caps) { return (caps & MALLOC_CAP_SPIRAM) ? 8u * 1024 * 1024 : 280u * 1024; }
static inline size_t heap_caps_get_largest_free_block(uint32_t caps) { return (caps & MALLOC_CAP_SPIRAM) ? 4u * 1024 * 1024 : 110u * 1024; }
static inline size_t heap_caps_get_total_size(uint32_t caps) { return (caps & MALLOC_CAP_SPIRAM) ? 8u * 1024 * 1024 : 320u * 1024; }
