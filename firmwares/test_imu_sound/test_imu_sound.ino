// ==============================================================================================================================================
// PERCU-SYNTH — Test IMU con sonido (sin USB / sin Serial) — GC Lab Chile
// ==============================================================================================================================================
// Desarrollado por: Gonzalo - GC Lab Chile · MIT License
// ==============================================================================================================================================
// HARDWARE
// ==============================================================================================================================================
// - ESP32-S3 + DAC PCM5102 vía I2S |LCK -> 39, DIN -> 40, BCK -> 41|
// - IMU MPU6050 (I2C) |SDA -> 21, SCL -> 38|  (dir. 0x68 o 0x69, autodetectada)
// ==============================================================================================================================================
// ARDUINO IDE
// ==============================================================================================================================================
// - Board: ESP32S3 Dev Module · Flash Mode: DIO · PSRAM: OPI PSRAM
// - Librerías: ESP32 core ≥ 3.x (driver/i2s_std.h) + Wire.h
// ==============================================================================================================================================
// DESCRIPCIÓN
// ==============================================================================================================================================
// Comprobación del IMU usando SONIDO (no LEDs, no USB, no Serial → evita los
// reinicios por el CDC). Sale por el jack/DAC:
//   · Al encender       → un BIP de arranque (~0.5 s, 660 Hz) que confirma que el
//                         firmware y el audio funcionan. Si no lo oyes, no es el IMU.
//   · IMU OK            → tono cuyo PITCH cambia al INCLINAR el equipo (350–1400 Hz).
//                         Si el sonido sube/baja al moverlo, el IMU funciona. :)
//   · IMU NO detectado  → un bip GRAVE intermitente (~220 Hz, on/off) = error de IMU.
// Autodetecta la dirección I2C 0x68 y 0x69. Volumen alto y rango agudo para que se
// oiga en cualquier parlante (un seno grave a bajo volumen casi no se escucha).
// ==============================================================================================================================================

#include <Arduino.h>
#include <driver/i2s_std.h>
#include <Wire.h>
#include <math.h>

// ─── I2S ───────────────────────────────────────────────────
#define I2S_LCK 39
#define I2S_DIN 40
#define I2S_BCK 41
#define SAMPLE_RATE     44100
#define BUFFER_SAMPLES  128

// ─── IMU ───────────────────────────────────────────────────
#define SDA_PIN 21
#define SCL_PIN 38

bool    imuOK = false;
uint8_t imuAddr = 0x68;
float   tiltX = 0.0f;        // aceleración eje X suavizada (-1..+1 g)

// ─── Oscilador ─────────────────────────────────────────────
float phase = 0.0f;
float freq  = 440.0f;        // frecuencia actual (con glide)

static i2s_chan_handle_t tx_chan;

// ─── IMU: detectar dirección y arrancar ────────────────────
bool detectIMU(uint8_t addr) {
  Wire.beginTransmission(addr);
  Wire.write(0x75);                       // WHO_AM_I
  if (Wire.endTransmission(false) != 0) return false;
  Wire.requestFrom((int)addr, 1, true);
  return (Wire.available() && (Wire.read() == 0x68));
}

void initIMU() {
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);
  if      (detectIMU(0x68)) { imuAddr = 0x68; imuOK = true; }
  else if (detectIMU(0x69)) { imuAddr = 0x69; imuOK = true; }
  else                       imuOK = false;
  // Despertar
  Wire.beginTransmission(imuAddr); Wire.write(0x6B); Wire.write(0x00); Wire.endTransmission(true);
  delay(50);
}

void readAccelX() {
  Wire.beginTransmission(imuAddr);
  Wire.write(0x3B);                       // ACCEL_XOUT_H
  Wire.endTransmission(false);
  Wire.requestFrom((int)imuAddr, 2, true);
  if (Wire.available() >= 2) {
    int16_t ax = (Wire.read() << 8) | Wire.read();
    float x = ax / 16384.0f;              // g (±2g por defecto)
    tiltX += (x - tiltX) * 0.2f;          // suavizado
  }
}

// ─── I2S init ──────────────────────────────────────────────
void i2s_init() {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.auto_clear = true;
  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_chan, NULL));
  i2s_std_config_t std_cfg = {
    .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
    .gpio_cfg = {
      .mclk = I2S_GPIO_UNUSED,
      .bclk = (gpio_num_t)I2S_BCK,
      .ws   = (gpio_num_t)I2S_LCK,
      .dout = (gpio_num_t)I2S_DIN,
      .din  = I2S_GPIO_UNUSED,
      .invert_flags = { false, false, false },
    },
  };
  ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &std_cfg));
  ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));
}

void setup() {
  esp_log_level_set("*", ESP_LOG_NONE);
  initIMU();
  i2s_init();
}

void loop() {
  static uint32_t elapsed = 0;          // muestras desde el arranque
  static float gatePhase = 0.0f;
  const float buffersPerSec = (float)SAMPLE_RATE / BUFFER_SAMPLES;  // ~344

  float targetFreq, amp;

  if (elapsed < (uint32_t)(SAMPLE_RATE * 0.5f)) {
    // — Bip de ARRANQUE (~0.5 s): confirma que el audio funciona —
    targetFreq = 660.0f; amp = 0.8f;
  } else if (imuOK) {
    // — IMU OK: tono que cambia de altura al inclinar (eje X) —
    readAccelX();
    float x = tiltX; if (x < -1) x = -1; if (x > 1) x = 1;
    targetFreq = 350.0f + (x + 1.0f) * 0.5f * 1050.0f;   // 350–1400 Hz
    amp = 0.8f;
  } else {
    // — Sin IMU: bip GRAVE intermitente (error) —
    targetFreq = 220.0f;
    gatePhase += 3.0f / buffersPerSec;                    // ~3 Hz
    if (gatePhase >= 1.0f) gatePhase -= 1.0f;
    amp = (gatePhase < 0.5f) ? 0.8f : 0.0f;
  }

  freq += (targetFreq - freq) * 0.08f;                    // glide suave

  int16_t buffer[BUFFER_SAMPLES * 2];
  for (int n = 0; n < BUFFER_SAMPLES; n++) {
    phase += freq / SAMPLE_RATE;
    if (phase >= 1.0f) phase -= 1.0f;
    float s = sinf(phase * 2.0f * (float)M_PI) * amp;
    int16_t out = (int16_t)(s * 22000.0f);                // bien audible
    buffer[n * 2] = out;
    buffer[n * 2 + 1] = out;
  }

  size_t written;
  i2s_channel_write(tx_chan, buffer, sizeof(buffer), &written, portMAX_DELAY);
  elapsed += BUFFER_SAMPLES;
}
