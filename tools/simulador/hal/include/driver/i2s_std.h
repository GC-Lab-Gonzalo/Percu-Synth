// =============================================================================
// driver/i2s_std.h de la placa virtual — la misma API del I2S "nuevo" de ESP-IDF 5
// que usan los firmwares del PercuSynth. Los campos van en el MISMO orden que en
// IDF para que los inicializadores designados de los sketches compilen tal cual.
//
// Canal de salida: lo que el sketch escribe con i2s_channel_write() va al anillo
// de audio del SimBus y la interfaz lo toca por Web Audio. Igual que el DMA real,
// la escritura BLOQUEA cuando la cola está llena: eso marca el tempo del sketch.
// Canal de entrada (micrófono INMP441): por ahora entrega silencio al ritmo real.
// =============================================================================
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "hal/gpio_types.h"

typedef enum { I2S_NUM_0 = 0, I2S_NUM_1 = 1, I2S_NUM_AUTO } i2s_port_t;
typedef enum { I2S_ROLE_MASTER, I2S_ROLE_SLAVE } i2s_role_t;
typedef enum { I2S_DIR_RX = 1, I2S_DIR_TX = 2 } i2s_dir_t;
typedef enum {
  I2S_DATA_BIT_WIDTH_8BIT = 8, I2S_DATA_BIT_WIDTH_16BIT = 16, I2S_DATA_BIT_WIDTH_24BIT = 24, I2S_DATA_BIT_WIDTH_32BIT = 32
} i2s_data_bit_width_t;
typedef enum {
  I2S_SLOT_BIT_WIDTH_AUTO = 0, I2S_SLOT_BIT_WIDTH_8BIT = 8, I2S_SLOT_BIT_WIDTH_16BIT = 16,
  I2S_SLOT_BIT_WIDTH_24BIT = 24, I2S_SLOT_BIT_WIDTH_32BIT = 32
} i2s_slot_bit_width_t;
typedef enum { I2S_SLOT_MODE_MONO = 1, I2S_SLOT_MODE_STEREO = 2 } i2s_slot_mode_t;
typedef enum { I2S_STD_SLOT_LEFT = 1, I2S_STD_SLOT_RIGHT = 2, I2S_STD_SLOT_BOTH = 3 } i2s_std_slot_mask_t;
typedef enum { I2S_CLK_SRC_DEFAULT = 0, I2S_CLK_SRC_XTAL, I2S_CLK_SRC_PLL_160M, I2S_CLK_SRC_APLL, I2S_CLK_SRC_EXTERNAL } i2s_clock_src_t;
typedef enum {
  I2S_MCLK_MULTIPLE_128 = 128, I2S_MCLK_MULTIPLE_192 = 192, I2S_MCLK_MULTIPLE_256 = 256,
  I2S_MCLK_MULTIPLE_384 = 384, I2S_MCLK_MULTIPLE_512 = 512, I2S_MCLK_MULTIPLE_576 = 576,
  I2S_MCLK_MULTIPLE_768 = 768, I2S_MCLK_MULTIPLE_1024 = 1024, I2S_MCLK_MULTIPLE_1152 = 1152
} i2s_mclk_multiple_t;

typedef struct sim_i2s_chan *i2s_chan_handle_t;

typedef struct {
  i2s_port_t id;
  i2s_role_t role;
  uint32_t dma_desc_num;
  uint32_t dma_frame_num;
  bool auto_clear;
  bool auto_clear_before_cb;
  bool allow_pd;
  int intr_priority;
} i2s_chan_config_t;

typedef struct {
  uint32_t sample_rate_hz;
  i2s_clock_src_t clk_src;
  uint32_t ext_clk_freq_hz;
  i2s_mclk_multiple_t mclk_multiple;
} i2s_std_clk_config_t;

typedef struct {
  i2s_data_bit_width_t data_bit_width;
  i2s_slot_bit_width_t slot_bit_width;
  i2s_slot_mode_t slot_mode;
  i2s_std_slot_mask_t slot_mask;
  uint32_t ws_width;
  bool ws_pol;
  bool bit_shift;
  bool left_align;
  bool big_endian;
  bool bit_order_lsb;
} i2s_std_slot_config_t;

typedef struct {
  gpio_num_t mclk;
  gpio_num_t bclk;
  gpio_num_t ws;
  gpio_num_t dout;
  gpio_num_t din;
  struct {
    uint32_t mclk_inv : 1;
    uint32_t bclk_inv : 1;
    uint32_t ws_inv : 1;
  } invert_flags;
} i2s_std_gpio_config_t;

typedef struct {
  i2s_std_clk_config_t clk_cfg;
  i2s_std_slot_config_t slot_cfg;
  i2s_std_gpio_config_t gpio_cfg;
} i2s_std_config_t;

typedef struct {
  i2s_dir_t dir;
  i2s_role_t role;
  int mode;
  i2s_data_bit_width_t data_bit_width;
  i2s_slot_mode_t slot_mode;
  uint32_t sample_rate_hz;
} i2s_chan_info_t;

#define I2S_GPIO_UNUSED GPIO_NUM_NC

#define I2S_CHANNEL_DEFAULT_CONFIG(i2s_num, i2s_role) { \
  .id = i2s_num, .role = i2s_role, .dma_desc_num = 6, .dma_frame_num = 240, \
  .auto_clear = false, .auto_clear_before_cb = false, .allow_pd = false, .intr_priority = 0 }

#define I2S_STD_CLK_DEFAULT_CONFIG(rate) { \
  .sample_rate_hz = rate, .clk_src = I2S_CLK_SRC_DEFAULT, .ext_clk_freq_hz = 0, .mclk_multiple = I2S_MCLK_MULTIPLE_256 }

#define I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(bits_per_sample, mono_or_stereo) { \
  .data_bit_width = bits_per_sample, .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO, .slot_mode = mono_or_stereo, \
  .slot_mask = I2S_STD_SLOT_BOTH, .ws_width = bits_per_sample, .ws_pol = false, .bit_shift = true, \
  .left_align = true, .big_endian = false, .bit_order_lsb = false }

#define I2S_STD_MSB_SLOT_DEFAULT_CONFIG(bits_per_sample, mono_or_stereo) { \
  .data_bit_width = bits_per_sample, .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO, .slot_mode = mono_or_stereo, \
  .slot_mask = I2S_STD_SLOT_BOTH, .ws_width = bits_per_sample, .ws_pol = false, .bit_shift = false, \
  .left_align = true, .big_endian = false, .bit_order_lsb = false }

#define I2S_STD_PCM_SLOT_DEFAULT_CONFIG(bits_per_sample, mono_or_stereo) { \
  .data_bit_width = bits_per_sample, .slot_bit_width = I2S_SLOT_BIT_WIDTH_AUTO, .slot_mode = mono_or_stereo, \
  .slot_mask = I2S_STD_SLOT_BOTH, .ws_width = 1, .ws_pol = true, .bit_shift = true, \
  .left_align = true, .big_endian = false, .bit_order_lsb = false }

#ifdef __cplusplus
extern "C" {
#endif
esp_err_t i2s_new_channel(const i2s_chan_config_t *cfg, i2s_chan_handle_t *tx, i2s_chan_handle_t *rx);
esp_err_t i2s_del_channel(i2s_chan_handle_t h);
esp_err_t i2s_channel_init_std_mode(i2s_chan_handle_t h, const i2s_std_config_t *cfg);
esp_err_t i2s_channel_reconfig_std_clock(i2s_chan_handle_t h, const i2s_std_clk_config_t *clk);
esp_err_t i2s_channel_reconfig_std_slot(i2s_chan_handle_t h, const i2s_std_slot_config_t *slot);
esp_err_t i2s_channel_reconfig_std_gpio(i2s_chan_handle_t h, const i2s_std_gpio_config_t *gpio);
esp_err_t i2s_channel_enable(i2s_chan_handle_t h);
esp_err_t i2s_channel_disable(i2s_chan_handle_t h);
esp_err_t i2s_channel_write(i2s_chan_handle_t h, const void *src, size_t size, size_t *written, uint32_t timeout_ms);
esp_err_t i2s_channel_read(i2s_chan_handle_t h, void *dest, size_t size, size_t *read, uint32_t timeout_ms);
esp_err_t i2s_channel_preload_data(i2s_chan_handle_t h, const void *src, size_t size, size_t *loaded);
esp_err_t i2s_channel_get_info(i2s_chan_handle_t h, i2s_chan_info_t *info);
#ifdef __cplusplus
}
#endif
