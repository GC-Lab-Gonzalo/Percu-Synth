#include <Arduino.h>
#include <driver/i2s_std.h>
#include <math.h>

// ─── I2S PCM5102 ───────────────────────────────────────────
#define I2S_LCK   39
#define I2S_DIN   40
#define I2S_BCK   41
#define SAMPLE_RATE 44100
#define BUFFER_SAMPLES 128

// ─── Botones (INPUT_PULLUP) ────────────────────────────────
#define NUM_BUTTONS 5
const uint8_t BTN_PINS[NUM_BUTTONS] = {44, 42, 0, 45, 47};

// Notas: Do4, Re4, Mi4, Fa4, Sol4
const float NOTE_FREQ[NUM_BUTTONS] = {261.63f, 293.66f, 329.63f, 349.23f, 392.00f};

// ─── Potenciómetros ────────────────────────────────────────
#define POT_VOL   1    // Volumen general
#define POT_MIX   2    // Mezcla ondas (seno→cuadrada→sierra)
#define POT_FILTER 8   // Filtro paso-bajo (cutoff)
#define POT_LFO   10   // LFO vibrato (velocidad)

// ─── Estado del sintetizador ───────────────────────────────
static i2s_chan_handle_t tx_chan;

// Oscilador por voz (polifónico: una voz por botón)
struct Voice {
  bool     active;
  float    freq;
  float    phase;       // fase principal
  float    envelope;    // amplitud actual (attack/release suave)
  bool     pressed;
};

Voice voices[NUM_BUTTONS];

// LFO
float lfo_phase = 0.0f;

// Parámetros globales leídos de pots
float g_volume   = 0.5f;
float g_mix      = 0.0f;   // 0=seno, 0.5=cuadrada, 1.0=sierra
float g_cutoff   = 1.0f;   // 0.0 a 1.0 (frecuencia de corte normalizada)
float g_lfo_rate = 2.0f;   // Hz

// Estado filtro paso-bajo (one-pole por canal, compartido)
float filter_state = 0.0f;

// ─── Leer ADC con promedio simple (anti-ruido) ─────────────
float readPot(uint8_t pin) {
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogRead(pin);
  return (float)(sum >> 3) / 4095.0f;
}

// ─── Forma de onda mezclada ────────────────────────────────
// mix: 0.0=seno, 0.5=cuadrada, 1.0=sierra  (interpolación entre las 3)
float mixedWave(float phase, float mix) {
  float sine  = sinf(phase * 2.0f * M_PI);
  float square = (phase < 0.5f) ? 1.0f : -1.0f;
  // Sierra: va de -1 a +1 en un ciclo
  float saw   = 2.0f * phase - 1.0f;

  float t = mix * 2.0f; // 0-1 = seno→cuadrada, 1-2 = cuadrada→sierra
  if (t <= 1.0f) {
    return sine   * (1.0f - t) + square * t;
  } else {
    t -= 1.0f;
    return square * (1.0f - t) + saw    * t;
  }
}

// ─── Setup I2S ─────────────────────────────────────────────
void i2s_init() {
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  chan_cfg.auto_clear = true;
  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_chan, NULL));

  i2s_std_config_t std_cfg = {
    .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
    .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
                  I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
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

// ─── Setup ─────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  for (int i = 0; i < NUM_BUTTONS; i++) {
    pinMode(BTN_PINS[i], INPUT_PULLUP);
    voices[i] = {false, NOTE_FREQ[i], 0.0f, 0.0f, false};
  }

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db); // Rango 0-3.3V

  i2s_init();
  Serial.println("Sintetizador listo.");
}

// ─── Loop ──────────────────────────────────────────────────
void loop() {
  // — Leer potenciómetros —
  g_volume   = readPot(POT_VOL);
  g_mix      = readPot(POT_MIX);
  g_cutoff   = readPot(POT_FILTER);   // 0=cerrado, 1=abierto
  g_lfo_rate = readPot(POT_LFO) * 8.0f + 0.2f; // 0.2 a 8.2 Hz

  // — Leer botones —
  for (int i = 0; i < NUM_BUTTONS; i++) {
    bool pressed = (digitalRead(BTN_PINS[i]) == LOW);
    if (pressed && !voices[i].pressed) {
      voices[i].active  = true;
      voices[i].pressed = true;
    }
    if (!pressed && voices[i].pressed) {
      voices[i].pressed = false;
      // release: deja que el envelope decaiga (no corta abrupto)
    }
  }

  // — Generar buffer de audio —
  int16_t buffer[BUFFER_SAMPLES * 2];

  for (int n = 0; n < BUFFER_SAMPLES; n++) {
    // LFO senoidal para vibrato
    float lfo = sinf(lfo_phase * 2.0f * M_PI) * 0.012f; // ±1.2% desafinación
    lfo_phase += g_lfo_rate / SAMPLE_RATE;
    if (lfo_phase >= 1.0f) lfo_phase -= 1.0f;

    // Mezclar voces activas
    float sample = 0.0f;

    for (int i = 0; i < NUM_BUTTONS; i++) {
      Voice &v = voices[i];
      if (!v.active) continue;

      // Envelope: attack rápido, release suave
      float target = v.pressed ? 1.0f : 0.0f;
      float rate   = v.pressed ? 0.005f : 0.002f; // attack / release
      v.envelope += (target - v.envelope) * rate;

      if (!v.pressed && v.envelope < 0.001f) {
        v.active   = false;
        v.envelope = 0.0f;
        continue;
      }

      // Frecuencia con vibrato del LFO
      float freq = v.freq * (1.0f + lfo);

      // Avanzar fase
      v.phase += freq / SAMPLE_RATE;
      if (v.phase >= 1.0f) v.phase -= 1.0f;

      sample += mixedWave(v.phase, g_mix) * v.envelope;
    }

    // Normalizar por número de voces posibles
    sample /= NUM_BUTTONS;

    // Filtro paso-bajo one-pole
    // cutoff=0 → coef≈1 (bloquea todo), cutoff=1 → coef≈0 (pasa todo)
    float coef = g_cutoff * g_cutoff; // curva cuadrática más natural
    filter_state += coef * (sample - filter_state);
    sample = filter_state;

    // Volumen y conversión a 16-bit
    int16_t out = (int16_t)(sample * g_volume * 28000.0f);
    buffer[n * 2]     = out; // L
    buffer[n * 2 + 1] = out; // R
  }

  size_t written;
  i2s_channel_write(tx_chan, buffer, sizeof(buffer), &written, portMAX_DELAY);
}