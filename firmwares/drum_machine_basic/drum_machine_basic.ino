#include <Arduino.h>
#include <driver/i2s_std.h>
#include <math.h>

// ─── I2S PCM5102 ───────────────────────────────────────────
#define I2S_LCK      39
#define I2S_DIN      40
#define I2S_BCK      41
#define SAMPLE_RATE  44100
#define BUFFER_SIZE  128

// ─── Pines ─────────────────────────────────────────────────
// Bombo=44, Caja=42, HiHat=0, Crash=45, Modo=47
const uint8_t BTN[5] = {44, 42, 0, 45, 47};

#define POT_VOL    1
#define POT_TEMPO  2
#define POT_KICK   8
#define POT_SNARE  10

// ─── Secuenciador ──────────────────────────────────────────
#define NUM_DRUMS  4
#define NUM_STEPS  16

bool pattern[NUM_DRUMS][NUM_STEPS];
int   cur_step     = 0;
bool  is_playing   = true;
bool  is_grabbing  = false;
float step_samples = 0.0f;
float step_counter = 0.0f;
float bpm          = 120.0f;

// ─── Voces ─────────────────────────────────────────────────
#define MAX_VOICES 10

struct Voice {
  bool    active;
  int     type;       // 0=kick 1=snare 2=hihat 3=crash 4=click
  float   env;        // envelope actual
  float   decay;      // tasa de decaimiento por muestra
  // Osciladores tonales
  float   phase1;     // oscilador 1
  float   phase2;     // oscilador 2 (para snare: 2a parcial)
  float   freq1;
  float   freq2;
  float   freq1_end;
  float   freq1_t;    // progreso pitch drop [0→1]
  float   freq1_rate; // velocidad del pitch drop
  // Ruido
  float   noise;      // estado LCG
  // Filtros paso-banda para platos (2 filtros en serie)
  float   bp1_x1, bp1_x2, bp1_y1, bp1_y2; // biquad 1
  float   bp2_x1, bp2_x2, bp2_y1, bp2_y2; // biquad 2
  float   b0, b1, b2, a1, a2;             // coefs biquad (compartidos entre los 2)
  // Mezcla tono/ruido
  float   tone_mix;   // 0=solo ruido, 1=solo tono
};

Voice voices[MAX_VOICES];

// ─── Ruido blanco LCG ──────────────────────────────────────
inline float noise(float &s) {
  uint32_t u = (uint32_t)s * 1664525u + 1013904223u;
  s = (float)u;
  return (float)(int32_t)u / 2147483648.0f;
}

// ─── Coeficientes biquad paso-banda ────────────────────────
// fc = frecuencia central (Hz), Q = resonancia
void biquadBP(float fc, float Q, float &b0, float &b1, float &b2,
                                  float &a1, float &a2) {
  float w0    = 2.0f * M_PI * fc / SAMPLE_RATE;
  float alpha = sinf(w0) / (2.0f * Q);
  float cosw0 = cosf(w0);
  float norm  = 1.0f + alpha;
  b0 =  alpha / norm;
  b1 =  0.0f;
  b2 = -alpha / norm;
  a1 = (-2.0f * cosw0) / norm;
  a2 = (1.0f - alpha)  / norm;
}

// ─── Procesar biquad (Direct Form II Transposed) ───────────
inline float processBQ(float x, float b0, float b1, float b2,
                                float a1, float a2,
                                float &x1, float &x2,
                                float &y1, float &y2) {
  float y = b0*x + b1*x1 + b2*x2 - a1*y1 - a2*y2;
  x2=x1; x1=x;
  y2=y1; y1=y;
  return y;
}

// ─── Disparar drum ─────────────────────────────────────────
void triggerDrum(int type, float kick_p, float snare_p) {
  // Buscar slot libre, si no el más silencioso
  int slot = 0;
  float minEnv = 9999.0f;
  for (int i = 0; i < MAX_VOICES; i++) {
    if (!voices[i].active) { slot = i; break; }
    if (voices[i].env < minEnv) { minEnv = voices[i].env; slot = i; }
  }

  Voice &v = voices[slot];
  memset(&v, 0, sizeof(Voice));
  v.active = true;
  v.type   = type;
  v.env    = 1.0f;
  v.noise  = (float)esp_random();

  switch (type) {

    case 0: // ── BOMBO (modelo anterior, sonaba mejor) ──────────
  v.freq1      = 80.0f  + kick_p * 120.0f;  // 80–200 Hz
  v.freq1_end  = 30.0f  + kick_p * 40.0f;   // 30–70 Hz
  v.freq1_rate = 0.002f;
  v.decay      = 0.00018f;
  v.tone_mix   = 1.0f;
  break;

    case 1: // ── SNARE 808 ──────────────────────────────────
      // Dos osciladores desafinados + ruido de bordón
      v.freq1      = 180.0f + snare_p * 80.0f;  // 180–260 Hz
      v.freq2      = v.freq1 * 1.58f;            // relación inarmónica
      v.freq1_end  = v.freq1 * 0.7f;
      v.freq1_rate = 0.006f;
      v.decay      = 0.0009f;
      v.tone_mix   = 0.45f; // 45% tono, 55% ruido
      break;

    case 2: // ── HI-HAT ─────────────────────────────────────────
  v.freq1      = 0.0f;
  v.freq1_end  = 0.0f;
  v.freq1_rate = 0.0f;
  v.decay      = 0.006f;
  v.tone_mix   = 0.0f;
  // Un solo biquad, frecuencia más baja, Q moderado
  biquadBP(7000.0f, 1.8f, v.b0, v.b1, v.b2, v.a1, v.a2);
  break;

    case 3: // ── CRASH ──────────────────────────────────────
      // Ruido coloreado con filtro más bajo y decay muy largo
      v.freq1      = 5500.0f;
      v.freq1_end  = 5500.0f;
      v.freq1_rate = 0.0f;
      v.decay      = 0.00008f;
      v.tone_mix   = 0.0f;
      biquadBP(5500.0f, 1.2f, v.b0, v.b1, v.b2, v.a1, v.a2);
      break;

    case 4: // ── CLICK METRÓNOMO ────────────────────────────
      v.freq1      = 1000.0f;
      v.freq1_end  = 600.0f;
      v.freq1_rate = 0.02f;
      v.decay      = 0.012f;
      v.tone_mix   = 1.0f;
      break;
  }
}

// ─── Render de una muestra ─────────────────────────────────
float renderVoice(Voice &v) {
  if (!v.active) return 0.0f;

  float s = 0.0f;

  switch (v.type) {

    case 0: { // BOMBO
  v.freq1_t = fminf(v.freq1_t + v.freq1_rate, 1.0f);
  float f   = v.freq1 + (v.freq1_end - v.freq1) * v.freq1_t;
  v.phase1 += f / SAMPLE_RATE;
  if (v.phase1 >= 1.0f) v.phase1 -= 1.0f;
  s = sinf(v.phase1 * 2.0f * M_PI); // senoidal pura, sin click
  break;

    }

    case 1: { // SNARE 808
      v.freq1_t = fminf(v.freq1_t + v.freq1_rate, 1.0f);
      float f1  = v.freq1 + (v.freq1_end - v.freq1) * v.freq1_t;
      float f2  = v.freq2 * (1.0f - v.freq1_t * 0.15f);
      v.phase1 += f1 / SAMPLE_RATE;
      v.phase2 += f2 / SAMPLE_RATE;
      if (v.phase1 >= 1.0f) v.phase1 -= 1.0f;
      if (v.phase2 >= 1.0f) v.phase2 -= 1.0f;
      float tonal = sinf(v.phase1 * 2.0f * M_PI) * 0.6f
                  + sinf(v.phase2 * 2.0f * M_PI) * 0.4f;
      float noisy = noise(v.noise);
      s = tonal * v.tone_mix + noisy * (1.0f - v.tone_mix);
      break;
    }

    case 2: { // HI-HAT — un solo biquad, más natural
  float n = noise(v.noise);
  s = processBQ(n,
        v.b0, v.b1, v.b2, v.a1, v.a2,
        v.bp1_x1, v.bp1_x2, v.bp1_y1, v.bp1_y2);
  break;
}

case 3: { // CRASH — dos biquads + tonal
  float n = noise(v.noise);
  float f1 = processBQ(n,
               v.b0, v.b1, v.b2, v.a1, v.a2,
               v.bp1_x1, v.bp1_x2, v.bp1_y1, v.bp1_y2);
  s = processBQ(f1,
               v.b0, v.b1, v.b2, v.a1, v.a2,
               v.bp2_x1, v.bp2_x2, v.bp2_y1, v.bp2_y2);
  v.phase1 += 4200.0f / SAMPLE_RATE;
  if (v.phase1 >= 1.0f) v.phase1 -= 1.0f;
  s = s * 0.85f + sinf(v.phase1 * 2.0f * M_PI) * 0.15f;
  break;
}

    case 4: { // CLICK
      v.freq1_t = fminf(v.freq1_t + v.freq1_rate, 1.0f);
      float f   = v.freq1 + (v.freq1_end - v.freq1) * v.freq1_t;
      v.phase1 += f / SAMPLE_RATE;
      if (v.phase1 >= 1.0f) v.phase1 -= 1.0f;
      s = sinf(v.phase1 * 2.0f * M_PI);
      break;
    }
  }

  s *= v.env;
  v.env -= v.decay;
  if (v.env <= 0.0f) { v.env = 0.0f; v.active = false; }
  return s;
}

// ─── I2S ───────────────────────────────────────────────────
static i2s_chan_handle_t tx_chan;

void i2s_init() {
  i2s_chan_config_t ch = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  ch.auto_clear = true;
  ESP_ERROR_CHECK(i2s_new_channel(&ch, &tx_chan, NULL));
  i2s_std_config_t cfg = {
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
  ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &cfg));
  ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));
}

// ─── Debounce ──────────────────────────────────────────────
bool     btn_raw[5]  = {};
bool     btn_cur[5]  = {};
bool     btn_last[5] = {};
bool     btn_edge[5] = {};
uint32_t btn_time[5] = {};
#define  DEBOUNCE_MS 25

void readButtons() {
  uint32_t now = millis();
  for (int i = 0; i < 5; i++) {
    bool r = (digitalRead(BTN[i]) == LOW);
    if (r != btn_raw[i]) btn_time[i] = now;
    if ((now - btn_time[i]) > DEBOUNCE_MS) btn_cur[i] = r;
    btn_raw[i]  = r;
    btn_edge[i] = btn_cur[i] && !btn_last[i];
    btn_last[i] = btn_cur[i];
  }
}

// ─── ADC ───────────────────────────────────────────────────
float readPot(uint8_t pin) {
  uint32_t s = 0;
  for (int i = 0; i < 8; i++) s += analogRead(pin);
  return (float)(s >> 3) / 4095.0f;
}

// ─── Setup ─────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  for (int i = 0; i < 5; i++) pinMode(BTN[i], INPUT_PULLUP);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
  memset(pattern, 0, sizeof(pattern));
  memset(voices,  0, sizeof(voices));
  i2s_init();
  step_samples = (60.0f / bpm / 4.0f) * SAMPLE_RATE;
  Serial.println("Drum machine lista.");
  Serial.println("BTN5 = toggle GRAB/PLAY");
}

// ─── Loop ──────────────────────────────────────────────────
void loop() {
  float vol        = readPot(POT_VOL);
  float tempo_raw  = readPot(POT_TEMPO);
  float kick_p     = readPot(POT_KICK);
  float snare_p    = readPot(POT_SNARE);

  bpm          = 60.0f + tempo_raw * 180.0f;
  step_samples = (60.0f / bpm / 4.0f) * SAMPLE_RATE;

  readButtons();

  // ── Botón 5: toggle GRAB / PLAYBACK ──────────────────────
  if (btn_edge[4]) {
    is_grabbing = !is_grabbing;
    if (is_grabbing) {
      memset(pattern, 0, sizeof(pattern));
      cur_step     = 0;
      step_counter = 0.0f;
      Serial.println("► GRAB — patrón borrado, graba!");
    } else {
      Serial.println("► PLAYBACK");
    }
  }

  // ── Botones 1-4: disparar + grabar ───────────────────────
  for (int i = 0; i < NUM_DRUMS; i++) {
    if (btn_edge[i]) {
      triggerDrum(i, kick_p, snare_p);
      if (is_grabbing && is_playing) {
        pattern[i][cur_step] = true;
      }
    }
  }

  // ── Generar audio ─────────────────────────────────────────
  int16_t buf[BUFFER_SIZE * 2];

  for (int n = 0; n < BUFFER_SIZE; n++) {

    // Avanzar secuenciador
    step_counter += 1.0f;
    if (step_counter >= step_samples) {
      step_counter -= step_samples;
      cur_step = (cur_step + 1) % NUM_STEPS;

      // Click SOLO en modo GRAB
      if (is_grabbing) {
        bool beat = (cur_step % 4 == 0); // tiempos fuertes más fuerte
        triggerDrum(4, 0, 0);
        // Si es tiempo fuerte, disparar segundo click para más presencia
        if (beat) triggerDrum(4, 0, 0);
      }

      // Reproducir patrón en PLAYBACK (sin click)
      if (!is_grabbing) {
        for (int d = 0; d < NUM_DRUMS; d++) {
          if (pattern[d][cur_step]) {
            triggerDrum(d, kick_p, snare_p);
          }
        }
      }
    }

    // Mezclar voces
    float mix = 0.0f;
    for (int i = 0; i < MAX_VOICES; i++) mix += renderVoice(voices[i]);

    // Saturación suave (waveshaper tanh)
    mix = tanhf(mix * 0.6f);

    int16_t out = (int16_t)(mix * vol * 30000.0f);
    buf[n * 2]     = out;
    buf[n * 2 + 1] = out;
  }

  size_t written;
  i2s_channel_write(tx_chan, buf, sizeof(buf), &written, portMAX_DELAY);
}