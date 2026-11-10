// =============================================================================
// core.cpp — el núcleo de la placa virtual: arranque, tiempo, GPIO/ADC, Serial,
// FreeRTOS (tareas = pthreads), I2S y el SimBus que lee la interfaz.
// =============================================================================
#include "Arduino.h"
#include "driver/i2s_std.h"
#include <emscripten.h>
#include <pthread.h>
#include <sched.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <atomic>
#include <new>

// ---------------------------------------------------------------- SimBus
static SimBus g_bus;
extern "C" SimBus *sim_bus() { return &g_bus; }

#define OFF(campo) (int)offsetof(SimBus, campo)
extern "C" EMSCRIPTEN_KEEPALIVE const char *sim_layout() {
  static char json[4096];
  snprintf(json, sizeof json,
    "{\"base\":%u,\"size\":%d,\"magic\":%d,\"pin_in\":%d,\"adc\":%d,\"piezo_seq\":%d,\"piezo_amp\":%d,"
    "\"usb_nativo\":%d,\"usb_com\":%d,\"audio_live\":%d,\"arranque\":%d,\"pin_mode\":%d,\"pin_out\":%d,"
    "\"led_seq\":%d,\"led_count\":%d,\"led_rgb\":%d,\"rgb48_seq\":%d,\"rgb48\":%d,\"audio_rate\":%d,"
    "\"a_w\":%d,\"a_r\":%d,\"audio\":%d,\"audio_frames\":%d,\"audio_huecos\":%d,\"tx\":%d,\"rx\":%d,"
    "\"usbmidi\":%d,\"ring_size\":%d,\"ring_bytes\":%d,\"serial1_baud\":%d,\"reinicio\":%d,\"hilos\":%d,\"max_leds\":%d,"
    "\"imu_on\":%d,\"imu_acc\":%d,\"imu_gyr\":%d,\"piso_seq\":%d,\"piso_amp\":%d,\"oled_on\":%d,\"sd_on\":%d,"
    "\"mic_w\":%d,\"mic_r\":%d,\"mic_rate\":%d,\"mic_live\":%d,\"mic\":%d,\"mic_frames\":%d,"
    "\"oled_seq\":%d,\"oled_ram\":%d,\"oled_cfg\":%d,\"sd_seq\":%d,\"i2s_rx\":%d,"
    "\"dht_tipo\":%d,\"dht_temp\":%d,\"dht_hum\":%d}",
    (unsigned)(uintptr_t)&g_bus, (int)sizeof(SimBus), OFF(magic), OFF(pin_in), OFF(adc), OFF(piezo_seq), OFF(piezo_amp),
    OFF(usb_nativo), OFF(usb_com), OFF(audio_live), OFF(arranque), OFF(pin_mode), OFF(pin_out),
    OFF(led_seq), OFF(led_count), OFF(led_rgb), OFF(rgb48_seq), OFF(rgb48), OFF(audio_rate),
    OFF(a_w), OFF(a_r), OFF(audio), SIM_AUDIO_FRAMES, OFF(audio_huecos), OFF(tx), OFF(rx),
    OFF(usbmidi), (int)sizeof(SimRing), SIM_RING, OFF(serial1_baud), OFF(reinicio), OFF(hilos), SIM_MAX_LEDS,
    OFF(imu_on), OFF(imu_acc), OFF(imu_gyr), OFF(piso_seq), OFF(piso_amp), OFF(oled_on), OFF(sd_on),
    OFF(mic_w), OFF(mic_r), OFF(mic_rate), OFF(mic_live), OFF(mic), SIM_MIC_FRAMES,
    OFF(oled_seq), OFF(oled_ram), OFF(oled_cfg), OFF(sd_seq), OFF(i2s_rx),
    OFF(dht_tipo), OFF(dht_temp), OFF(dht_hum));
  return json;
}

// ---------------------------------------------------------------- tiempo
static double g_t0 = 0;
static inline double ahora_ms() { return emscripten_get_now(); }
unsigned long millis() { return (unsigned long)(ahora_ms() - g_t0); }
// Reloj fino del hilo durante una trama de un sensor de un hilo (DHT): en el navegador un hilo puede
// quedar parado 50–100 µs cuando el sistema operativo atiende otra cosa, y con eso se pierden bits de
// 26 µs que en el ESP32 nunca se pierden. Mientras dura la trama, micros(), delayMicroseconds() y la
// línea del sensor usan un reloj que avanza con el real pero sin saltos de más de 2 µs por consulta:
// para el sketch el tiempo corre parejo, como en la placa. Al terminar la trama vuelve al reloj real.
static thread_local bool   t_fino = false;
static thread_local double t_fino_us = 0, t_fino_real = 0;
static double reloj_us() {
  double real = (ahora_ms() - g_t0) * 1000.0;
  if (!t_fino) return real;
  double d = real - t_fino_real;
  t_fino_real = real;
  if (d > 3000) { t_fino = false; return real; }      // el sketch abandonó la trama: reloj real otra vez
  if (d > 0) t_fino_us += d < 2.0 ? d : 2.0;
  return t_fino_us;
}
unsigned long micros() { return (unsigned long)reloj_us(); }
static void dormir_us(double us) {
  if (us <= 0) { sched_yield(); return; }
  struct timespec ts; ts.tv_sec = (time_t)(us / 1e6); ts.tv_nsec = (long)((us - ts.tv_sec * 1e6) * 1000.0);
  nanosleep(&ts, nullptr);
}
void delay(uint32_t ms) { if (ms == 0) { sched_yield(); return; } dormir_us(ms * 1000.0); }
void delayMicroseconds(uint32_t us) {
  if (t_fino) { reloj_us(); t_fino_us += us; return; }   // dentro de una trama: exacto
  // espera activa corta (como en el ESP32); larga, se duerme
  if (us > 2000) { dormir_us(us); return; }
  double fin = ahora_ms() + us / 1000.0;
  while (ahora_ms() < fin) {}
}
void yield() { sched_yield(); }
uint32_t EspClass::getCycleCount() { return (uint32_t)((ahora_ms() - g_t0) * 240000.0); }

// ---------------------------------------------------------------- azar
static uint32_t g_xs = 0x9E3779B9u;
static pthread_mutex_t g_rand_mx = PTHREAD_MUTEX_INITIALIZER;
extern "C" uint32_t esp_random(void) {
  pthread_mutex_lock(&g_rand_mx);
  uint32_t x = g_xs ^ (uint32_t)(ahora_ms() * 1000.0);
  x ^= x << 13; x ^= x >> 17; x ^= x << 5;
  g_xs = x;
  pthread_mutex_unlock(&g_rand_mx);
  return x;
}
extern "C" void esp_fill_random(void *buf, size_t len) {
  uint8_t *p = (uint8_t *)buf;
  while (len) { uint32_t r = esp_random(); size_t k = len < 4 ? len : 4; memcpy(p, &r, k); p += k; len -= k; }
}
static bool g_semilla = false;
static uint32_t g_lcg = 1;
static uint32_t azar32() {
  if (!g_semilla) return esp_random();
  g_lcg = g_lcg * 1103515245u + 12345u;
  return (g_lcg >> 1) ^ (g_lcg << 15);
}
void randomSeed(unsigned long seed) { if (seed) { g_semilla = true; g_lcg = (uint32_t)seed; } }
long random(long howbig) { if (howbig <= 0) return 0; return (long)(azar32() % (uint32_t)howbig); }
long random(long howsmall, long howbig) { if (howsmall >= howbig) return howsmall; return howsmall + random(howbig - howsmall); }
long map(long x, long in_min, long in_max, long out_min, long out_max) {
  long dividend = out_max - out_min, divisor = in_max - in_min;
  if (divisor == 0) return -1;
  return (x - in_min) * dividend / divisor + out_min;
}

// ---------------------------------------------------------------- GPIO / ADC
static int g_adc_bits = 12;

// Sensor DHT11 / DHT22 en un pin (EXT1 = GPIO 3, EXT2 = GPIO 9): el protocolo de un hilo como el
// chip, pin a pin. El sketch baja la línea (OUTPUT + LOW) al menos 18 ms (DHT11) o 1 ms (DHT22) y la
// suelta; el sensor espera ~30 µs, contesta 80 µs bajo + 80 µs alto y manda 40 bits: cada uno 50 µs
// bajo + 26 µs alto (0) o 70 µs alto (1), y al final 50 µs bajo. Así funciona la librería DHT.h y
// también un código que lea los pulsos a mano con digitalRead / pulseIn / micros.
struct SimDHT { double bajo_desde = -1; double t0 = -1; int tipo = 0; uint8_t d[5]; };
static SimDHT g_dht[64];
static bool linea_baja(int pin) {
  int m = g_bus.pin_mode[pin];
  return (m == OUTPUT || m == OUTPUT_OPEN_DRAIN) && g_bus.pin_out[pin] == 0;
}
static void dht_trama(int pin, int tipo) {
  float t = g_bus.dht_temp[pin], h = g_bus.dht_hum[pin];
  uint8_t *d = g_dht[pin].d;
  if (tipo == 11) {
    if (t < 0) t = 0; if (t > 50) t = 50; if (h < 5) h = 5; if (h > 95) h = 95;
    int dec = (int)lroundf(t * 10);
    d[0] = (uint8_t)lroundf(h); d[1] = 0; d[2] = (uint8_t)(dec / 10); d[3] = (uint8_t)(dec % 10);
  } else {
    if (t < -40) t = -40; if (t > 80) t = 80; if (h < 0) h = 0; if (h > 100) h = 100;
    int hh = (int)lroundf(h * 10), tt = (int)lroundf(fabsf(t) * 10) | (t < 0 ? 0x8000 : 0);
    d[0] = (uint8_t)(hh >> 8); d[1] = (uint8_t)hh; d[2] = (uint8_t)(tt >> 8); d[3] = (uint8_t)tt;
  }
  d[4] = (uint8_t)(d[0] + d[1] + d[2] + d[3]);
}
// el sketch cambió el modo o el nivel del pin: ¿soltó la línea después de bajarla el tiempo suficiente?
static void dht_cambio(int pin, bool estaba_baja) {
  int tipo = g_bus.dht_tipo[pin];
  bool baja = linea_baja(pin);
  double ahora = ahora_ms();
  if (!estaba_baja && baja) { g_dht[pin].bajo_desde = ahora; g_dht[pin].t0 = -1; return; }
  if (estaba_baja && !baja && tipo && g_dht[pin].bajo_desde >= 0) {
    double largo = ahora - g_dht[pin].bajo_desde;
    if (largo >= (tipo == 11 ? 18.0 : 0.8)) {
      dht_trama(pin, tipo);
      t_fino = false; double real = reloj_us();
      t_fino = true; t_fino_us = t_fino_real = real;     // desde acá el hilo corre con el reloj fino
      g_dht[pin].t0 = real; g_dht[pin].tipo = tipo;
    }
    g_dht[pin].bajo_desde = -1;
  }
}
static int dht_nivel(int pin) {
  SimDHT &s = g_dht[pin];
  if (s.t0 < 0) return HIGH;                         // en reposo, el pull-up del módulo
  if (!t_fino) { s.t0 = -1; return HIGH; }           // la trama se abandonó (o la lee otro hilo)
  double t = reloj_us() - s.t0;                      // µs desde que el sketch soltó la línea
  if (t < 30) return HIGH;
  if (t < 110) return LOW;
  if (t < 190) return HIGH;
  double c = 190;
  for (int i = 0; i < 40; i++) {
    if (t < c + 50) return LOW;
    c += 50;
    double alto = (s.d[i >> 3] & (0x80 >> (i & 7))) ? 70 : 26;
    if (t < c + alto) return HIGH;
    c += alto;
  }
  if (t < c + 50) return LOW;
  s.t0 = -1; t_fino = false;                         // terminó la trama: reloj real otra vez
  return HIGH;
}

void pinMode(uint8_t pin, uint8_t mode) {
  if (pin >= 64) return;
  bool b = linea_baja(pin);
  g_bus.pin_mode[pin] = mode;
  dht_cambio(pin, b);
}
void digitalWrite(uint8_t pin, uint8_t val) {
  if (pin >= 64) return;
  bool b = linea_baja(pin);
  g_bus.pin_out[pin] = val ? 1 : 0;
  dht_cambio(pin, b);
}
int digitalRead(uint8_t pin) {
  if (pin >= 64) return 0;
  int m = g_bus.pin_mode[pin];
  if (m == OUTPUT) return g_bus.pin_out[pin];
  if (g_bus.dht_tipo[pin]) return linea_baja(pin) ? LOW : dht_nivel(pin);
  return g_bus.pin_in[pin] ? HIGH : LOW;
}
// golpe del piezo: un pulso que decae en pocos ms (la entrada lleva Schottky: sólo la parte positiva)
static uint32_t g_piezo_visto[4];
static double   g_piezo_t0[4] = { -1e9, -1e9, -1e9, -1e9 };
static float    g_piezo_a[4];
static int lectura_piezo(int k) {
  if (g_bus.piezo_seq[k] != g_piezo_visto[k]) {
    g_piezo_visto[k] = g_bus.piezo_seq[k];
    g_piezo_t0[k] = ahora_ms();
    g_piezo_a[k] = g_bus.piezo_amp[k];
  }
  double t = ahora_ms() - g_piezo_t0[k];
  if (t < 0 || t > 40) return 0;
  double env = exp(-t / 4.0);
  double osc = fabs(sin(2 * M_PI * 0.32 * t));          // ~320 Hz, sólo la mitad positiva
  return (int)(g_piezo_a[k] * 4095.0 * env * (0.55 + 0.45 * osc));
}
uint16_t analogRead(uint8_t pin) {
  if (pin >= 64) return 0;
  int v;
  if (pin >= 4 && pin <= 7) v = lectura_piezo(pin - 4);
  else v = g_bus.adc[pin];
  // el ADC del S3 nunca es perfectamente quieto: ±2 cuentas de ruido
  v += (int)(esp_random() % 5) - 2;
  if (v < 0) v = 0; if (v > 4095) v = 4095;
  if (g_adc_bits != 12) v = g_adc_bits > 12 ? v << (g_adc_bits - 12) : v >> (12 - g_adc_bits);
  return (uint16_t)v;
}
uint32_t analogReadMilliVolts(uint8_t pin) {
  int b = g_adc_bits; g_adc_bits = 12; uint32_t v = analogRead(pin); g_adc_bits = b;
  return v * 3100 / 4095;
}
void analogReadResolution(uint8_t bits) { if (bits >= 9 && bits <= 16) g_adc_bits = bits; }
void analogSetWidth(uint8_t bits) { analogReadResolution(bits); }
void analogSetAttenuation(int) {}
void analogSetPinAttenuation(uint8_t, int) {}
void analogWrite(uint8_t pin, int value) { if (pin < 64) g_bus.pin_out[pin] = value; }
void rgbLedWrite(uint8_t pin, uint8_t r, uint8_t g, uint8_t b) {
  if (pin != 48) return;
  g_bus.rgb48[0] = r; g_bus.rgb48[1] = g; g_bus.rgb48[2] = b;
  __atomic_add_fetch(&g_bus.rgb48_seq, 1, __ATOMIC_RELEASE);
}
void neopixelWrite(uint8_t pin, uint8_t r, uint8_t g, uint8_t b) { rgbLedWrite(pin, r, g, b); }
void attachInterrupt(uint8_t, void (*)(void), int) {}
void detachInterrupt(uint8_t) {}
uint16_t touchRead(uint8_t) { return 0; }
// como el de Arduino: espera que termine el pulso en curso, que empiece uno con el nivel pedido y lo mide (µs)
unsigned long pulseIn(uint8_t pin, uint8_t state, unsigned long timeout) {
  double fin = reloj_us() + timeout;
  while (digitalRead(pin) == state) if (reloj_us() > fin) return 0;
  while (digitalRead(pin) != state) if (reloj_us() > fin) return 0;
  double t0 = reloj_us();
  while (digitalRead(pin) == state) if (reloj_us() > fin) return 0;
  return (unsigned long)(reloj_us() - t0);
}
unsigned long pulseInLong(uint8_t pin, uint8_t state, unsigned long timeout) { return pulseIn(pin, state, timeout); }

// ---------------------------------------------------------------- ESP, PSRAM, WDT
EspClass ESP;
void EspClass::restart() {
  g_bus.reinicio = 1;
  for (;;) dormir_us(1e6);
}
extern "C" void esp_restart(void) { ESP.restart(); }
extern "C" esp_reset_reason_t esp_reset_reason(void) { return ESP_RST_POWERON; }
extern "C" uint32_t esp_get_free_heap_size(void) { return 280 * 1024; }
extern "C" uint32_t esp_get_minimum_free_heap_size(void) { return 250 * 1024; }
bool psramFound() { return true; }
bool psramInit() { return true; }
void *ps_malloc(size_t s) { return malloc(s); }
void *ps_calloc(size_t n, size_t s) { return calloc(n, s); }
void *ps_realloc(void *p, size_t s) { return realloc(p, s); }
void disableCore0WDT() {}
void disableCore1WDT() {}
void enableCore0WDT() {}
void enableCore1WDT() {}
void disableLoopWDT() {}
void enableLoopWDT() {}

// ---------------------------------------------------------------- anillos
static uint32_t ring_espacio(const SimRing *r) {
  return SIM_RING - (__atomic_load_n(&r->w, __ATOMIC_RELAXED) - __atomic_load_n(&r->r, __ATOMIC_ACQUIRE));
}
static size_t ring_poner(SimRing *r, const uint8_t *d, size_t n) {
  uint32_t esp = ring_espacio(r);
  if (n > esp) n = esp;
  uint32_t w = r->w;
  for (size_t i = 0; i < n; i++) r->buf[(w + i) & (SIM_RING - 1)] = d[i];
  __atomic_store_n(&r->w, w + (uint32_t)n, __ATOMIC_RELEASE);
  return n;
}
static int ring_hay(const SimRing *r) {
  return (int)(__atomic_load_n(&r->w, __ATOMIC_ACQUIRE) - __atomic_load_n(&r->r, __ATOMIC_RELAXED));
}
static int ring_mirar(SimRing *r) { return ring_hay(r) > 0 ? r->buf[r->r & (SIM_RING - 1)] : -1; }
static int ring_sacar(SimRing *r) {
  if (ring_hay(r) <= 0) return -1;
  uint8_t c = r->buf[r->r & (SIM_RING - 1)];
  __atomic_store_n(&r->r, r->r + 1, __ATOMIC_RELEASE);
  return c;
}
extern "C" void sim_midi_usb(uint8_t s, uint8_t d1, uint8_t d2) {
  if (!g_bus.usb_nativo) return;            // sin cable USB no hay USB-MIDI
  uint8_t m[3] = { s, d1, d2 };
  if (ring_espacio(&g_bus.usbmidi) >= 3) ring_poner(&g_bus.usbmidi, m, 3);
}

// ---------------------------------------------------------------- Serial
HWCDC Serial;
HardwareSerial Serial0(0);
HardwareSerial Serial1(1);
HardwareSerial Serial2(2);
static unsigned long g_baud[3] = { 115200, 0, 0 };

bool SimSerialBase::conectado() const { return true; }
bool HWCDC::conectado() const { return g_bus.usb_nativo != 0; }
size_t SimSerialBase::write(uint8_t c) { return write(&c, 1); }
size_t SimSerialBase::write(const uint8_t *buf, size_t n) {
  if (tx_idx < 0) return n;
  SimRing *r = &g_bus.tx[tx_idx];
  if (tx_idx == 0 && !g_bus.usb_nativo) return n;     // nadie escucha el CDC
  if (tx_idx == 1 && !g_bus.usb_com) return n;        // sin cable en COM, el UART igual sale (a nadie)
  size_t hecho = 0;
  double limite = ahora_ms() + tx_timeout_ms;
  while (hecho < n) {
    hecho += ring_poner(r, buf + hecho, n - hecho);
    if (hecho >= n || ahora_ms() >= limite) break;
    dormir_us(500);
  }
  return n;
}
int SimSerialBase::available() { return rx_idx < 0 ? 0 : ring_hay(&g_bus.rx[rx_idx]); }
int SimSerialBase::read() { return rx_idx < 0 ? -1 : ring_sacar(&g_bus.rx[rx_idx]); }
int SimSerialBase::peek() { return rx_idx < 0 ? -1 : ring_mirar(&g_bus.rx[rx_idx]); }
int SimSerialBase::availableForWrite() { return tx_idx < 0 ? 0 : (int)ring_espacio(&g_bus.tx[tx_idx]); }

HardwareSerial::HardwareSerial(int uart_nr) : SimSerialBase(uart_nr == 0 ? 1 : -1, uart_nr == 0 ? 1 : -1), num(uart_nr) {}
void HardwareSerial::begin(unsigned long baud, uint32_t, int8_t, int8_t txPin, bool, unsigned long, uint8_t) {
  started = true;
  if (num < 3) g_baud[num] = baud;
  if (num == 1) {
    // UART1 con TX en el 43 = el conector DIN-5 de la placa
    tx_idx = (txPin == 43) ? 2 : -1;
    g_bus.serial1_baud = (txPin == 43) ? (int32_t)baud : 0;
  }
}
void HardwareSerial::updateBaudRate(unsigned long baud) { if (num < 3) g_baud[num] = baud; if (num == 1 && tx_idx == 2) g_bus.serial1_baud = baud; }
bool HardwareSerial::setPins(int8_t, int8_t txPin, int8_t, int8_t) {
  if (num == 1) { tx_idx = (txPin == 43) ? 2 : -1; g_bus.serial1_baud = tx_idx == 2 ? (int32_t)g_baud[1] : 0; }
  return true;
}
unsigned long HardwareSerial::baudRate() const { return num < 3 ? g_baud[num] : 0; }

// Stream
int Stream::timedRead() {
  double fin = ahora_ms() + _timeout;
  do { int c = read(); if (c >= 0) return c; dormir_us(500); } while (ahora_ms() < fin);
  return -1;
}
int Stream::timedPeek() {
  double fin = ahora_ms() + _timeout;
  do { int c = peek(); if (c >= 0) return c; dormir_us(500); } while (ahora_ms() < fin);
  return -1;
}
size_t Stream::readBytes(char *buf, size_t n) {
  size_t k = 0; while (k < n) { int c = timedRead(); if (c < 0) break; buf[k++] = (char)c; } return k;
}
size_t Stream::readBytesUntil(char term, char *buf, size_t n) {
  size_t k = 0; while (k < n) { int c = timedRead(); if (c < 0 || c == term) break; buf[k++] = (char)c; } return k;
}
String Stream::readString() { std::string s; int c; while ((c = timedRead()) >= 0) s += (char)c; return String(s); }
String Stream::readStringUntil(char term) { std::string s; int c; while ((c = timedRead()) >= 0 && c != term) s += (char)c; return String(s); }
long Stream::parseInt() {
  int c; bool neg = false; long v = 0;
  do { c = timedPeek(); if (c < 0) return 0; if (c == '-' || (c >= '0' && c <= '9')) break; read(); } while (1);
  if (c == '-') { neg = true; read(); }
  while ((c = timedPeek()) >= '0' && c <= '9') { v = v * 10 + (c - '0'); read(); }
  return neg ? -v : v;
}
float Stream::parseFloat() {
  std::string s; int c;
  do { c = timedPeek(); if (c < 0) return 0; if (c == '-' || c == '.' || (c >= '0' && c <= '9')) break; read(); } while (1);
  while ((c = timedPeek()) >= 0 && (c == '-' || c == '.' || (c >= '0' && c <= '9'))) { s += (char)c; read(); }
  return (float)atof(s.c_str());
}
bool Stream::find(const char *target) {
  size_t n = strlen(target), k = 0; int c;
  while ((c = timedRead()) >= 0) { if (c == target[k]) { if (++k == n) return true; } else k = (c == target[0]) ? 1 : 0; }
  return false;
}

// ---------------------------------------------------------------- FreeRTOS
struct SimTask {
  pthread_t hilo;
  TaskFunction_t fn;
  void *param;
  char nombre[24];
  int nucleo;
  UBaseType_t prioridad;
  pthread_mutex_t mx;
  pthread_cond_t cv;
  uint32_t notif;
};
static thread_local SimTask *t_actual = nullptr;
static SimTask g_tarea_loop;

static void tarea_init(SimTask *t, const char *nombre, int nucleo, UBaseType_t prio) {
  snprintf(t->nombre, sizeof t->nombre, "%s", nombre ? nombre : "tarea");
  t->nucleo = nucleo; t->prioridad = prio; t->notif = 0;
  pthread_mutex_init(&t->mx, nullptr); pthread_cond_init(&t->cv, nullptr);
}
static void *trampolin(void *p) {
  SimTask *t = (SimTask *)p;
  t_actual = t;
  t->fn(t->param);
  // en FreeRTOS una tarea que retorna es un error; acá simplemente termina
  return nullptr;
}
extern "C" BaseType_t xTaskCreatePinnedToCore(TaskFunction_t fn, const char *name, uint32_t stack, void *param,
                                              UBaseType_t prio, TaskHandle_t *handle, BaseType_t core) {
  SimTask *t = new SimTask();
  tarea_init(t, name, core == tskNO_AFFINITY ? 0 : (int)core, prio);
  t->fn = fn; t->param = param;
  pthread_attr_t at; pthread_attr_init(&at);
  size_t pila = stack * 4;                       // el "stack" del ESP32 está en bytes; acá damos holgura
  if (pila < 256 * 1024) pila = 256 * 1024;
  pthread_attr_setstacksize(&at, pila);
  int e = pthread_create(&t->hilo, &at, trampolin, t);
  pthread_attr_destroy(&at);
  if (e) { delete t; return pdFAIL; }
  pthread_detach(t->hilo);
  __atomic_add_fetch(&g_bus.hilos, 1, __ATOMIC_RELAXED);
  if (handle) *handle = t;
  return pdPASS;
}
extern "C" BaseType_t xTaskCreate(TaskFunction_t fn, const char *name, uint32_t stack, void *param,
                                  UBaseType_t prio, TaskHandle_t *handle) {
  return xTaskCreatePinnedToCore(fn, name, stack, param, prio, handle, tskNO_AFFINITY);
}
extern "C" void vTaskDelay(TickType_t ticks) {
  if (ticks == portMAX_DELAY) { for (;;) dormir_us(1e6); }
  if (ticks == 0) { sched_yield(); return; }
  dormir_us(ticks * 1000.0);
}
extern "C" TickType_t xTaskGetTickCount(void) { return (TickType_t)millis(); }
extern "C" TickType_t xTaskGetTickCountFromISR(void) { return (TickType_t)millis(); }
extern "C" BaseType_t xTaskDelayUntil(TickType_t *prev, TickType_t inc) {
  TickType_t objetivo = *prev + inc;
  int32_t falta = (int32_t)(objetivo - xTaskGetTickCount());
  if (falta > 0) dormir_us(falta * 1000.0);
  *prev = objetivo;
  return falta > 0 ? pdTRUE : pdFALSE;
}
extern "C" void vTaskDelayUntil(TickType_t *prev, TickType_t inc) { xTaskDelayUntil(prev, inc); }
extern "C" void vTaskDelete(TaskHandle_t t) {
  if (t == nullptr || t == t_actual) {
    if (t_actual == &g_tarea_loop) { for (;;) dormir_us(1e6); }   // loopTask: no se puede salir de main
    pthread_exit(nullptr);
  }
  // borrar OTRA tarea no se puede con pthreads; queda corriendo (raro en los sketches)
}
extern "C" void vTaskSuspend(TaskHandle_t t) { if (t == nullptr || t == t_actual) for (;;) dormir_us(1e6); }
extern "C" void vTaskResume(TaskHandle_t) {}
extern "C" TaskHandle_t xTaskGetCurrentTaskHandle(void) { return t_actual; }
extern "C" UBaseType_t uxTaskGetStackHighWaterMark(TaskHandle_t) { return 2048; }
extern "C" UBaseType_t uxTaskPriorityGet(TaskHandle_t t) { return t ? t->prioridad : (t_actual ? t_actual->prioridad : 1); }
extern "C" void vTaskPrioritySet(TaskHandle_t t, UBaseType_t p) { if (!t) t = t_actual; if (t) t->prioridad = p; }
extern "C" const char *pcTaskGetName(TaskHandle_t t) { if (!t) t = t_actual; return t ? t->nombre : "?"; }
extern "C" BaseType_t xPortGetCoreID(void) { return t_actual ? t_actual->nucleo : 1; }
extern "C" void sim_task_yield(void) { sched_yield(); }
extern "C" void vTaskSuspendAll(void) {}
extern "C" BaseType_t xTaskResumeAll(void) { return pdFALSE; }

static void tiempo_limite(struct timespec *ts, TickType_t espera) {
  clock_gettime(CLOCK_REALTIME, ts);
  long long ns = ts->tv_nsec + (long long)espera * 1000000LL;
  ts->tv_sec += ns / 1000000000LL; ts->tv_nsec = ns % 1000000000LL;
}
static bool esperar(pthread_cond_t *cv, pthread_mutex_t *mx, TickType_t espera, const struct timespec *lim) {
  if (espera == portMAX_DELAY) { pthread_cond_wait(cv, mx); return true; }
  return pthread_cond_timedwait(cv, mx, lim) != ETIMEDOUT;
}
extern "C" uint32_t ulTaskNotifyTake(BaseType_t clear, TickType_t wait) {
  SimTask *t = t_actual ? t_actual : &g_tarea_loop;
  struct timespec lim; tiempo_limite(&lim, wait);
  pthread_mutex_lock(&t->mx);
  while (t->notif == 0) { if (wait == 0 || !esperar(&t->cv, &t->mx, wait, &lim)) break; }
  uint32_t v = t->notif;
  if (v) t->notif = clear ? 0 : v - 1;
  pthread_mutex_unlock(&t->mx);
  return v;
}
extern "C" BaseType_t xTaskNotifyGive(TaskHandle_t t) {
  if (!t) return pdFAIL;
  pthread_mutex_lock(&t->mx); t->notif++; pthread_cond_broadcast(&t->cv); pthread_mutex_unlock(&t->mx);
  return pdPASS;
}
extern "C" void vTaskNotifyGiveFromISR(TaskHandle_t t, BaseType_t *w) { if (w) *w = pdFALSE; xTaskNotifyGive(t); }
extern "C" BaseType_t xTaskNotify(TaskHandle_t t, uint32_t v, eNotifyAction a) {
  if (!t) return pdFAIL;
  pthread_mutex_lock(&t->mx);
  switch (a) {
    case eSetBits: t->notif |= v; break;
    case eIncrement: t->notif++; break;
    case eSetValueWithOverwrite: case eSetValueWithoutOverwrite: t->notif = v; break;
    default: break;
  }
  pthread_cond_broadcast(&t->cv); pthread_mutex_unlock(&t->mx);
  return pdPASS;
}
extern "C" BaseType_t xTaskNotifyFromISR(TaskHandle_t t, uint32_t v, eNotifyAction a, BaseType_t *w) { if (w) *w = pdFALSE; return xTaskNotify(t, v, a); }
extern "C" BaseType_t xTaskNotifyWait(uint32_t clearEntry, uint32_t clearExit, uint32_t *value, TickType_t wait) {
  SimTask *t = t_actual ? t_actual : &g_tarea_loop;
  struct timespec lim; tiempo_limite(&lim, wait);
  pthread_mutex_lock(&t->mx);
  t->notif &= ~clearEntry;
  bool ok = t->notif != 0;
  while (!ok && wait) { if (!esperar(&t->cv, &t->mx, wait, &lim)) break; ok = t->notif != 0; }
  if (value) *value = t->notif;
  if (ok) t->notif &= ~clearExit;
  pthread_mutex_unlock(&t->mx);
  return ok ? pdTRUE : pdFALSE;
}

// candado de giro de portMUX (reentrante para el mismo hilo, como en el ESP32)
static std::atomic<int> g_id_hilo{ 1 };
static thread_local int t_id = 0;
static int mi_id() { if (!t_id) t_id = g_id_hilo.fetch_add(1); return t_id; }
extern "C" void sim_mux_lock(portMUX_TYPE *m) {
  int yo = mi_id();
  if (__atomic_load_n(&m->owner, __ATOMIC_ACQUIRE) == yo) { m->count++; return; }
  int libre = 0;
  while (!__atomic_compare_exchange_n(&m->owner, &libre, yo, false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) { libre = 0; }
  m->count = 1;
}
extern "C" void sim_mux_unlock(portMUX_TYPE *m) {
  if (--m->count <= 0) { m->count = 0; __atomic_store_n(&m->owner, 0, __ATOMIC_RELEASE); }
}

// semáforos (mutex, binario, contador) con mutex + condición
struct SimSem { pthread_mutex_t mx; pthread_cond_t cv; UBaseType_t cuenta, max; bool recursivo; int dueno; int prof; };
static SimSem *sem_nuevo(UBaseType_t max, UBaseType_t ini, bool rec) {
  SimSem *s = new SimSem(); pthread_mutex_init(&s->mx, nullptr); pthread_cond_init(&s->cv, nullptr);
  s->cuenta = ini; s->max = max; s->recursivo = rec; s->dueno = 0; s->prof = 0; return s;
}
extern "C" SemaphoreHandle_t xSemaphoreCreateMutex(void) { return sem_nuevo(1, 1, false); }
extern "C" SemaphoreHandle_t xSemaphoreCreateRecursiveMutex(void) { return sem_nuevo(1, 1, true); }
extern "C" SemaphoreHandle_t xSemaphoreCreateBinary(void) { return sem_nuevo(1, 0, false); }
extern "C" SemaphoreHandle_t xSemaphoreCreateCounting(UBaseType_t max, UBaseType_t ini) { return sem_nuevo(max, ini, false); }
extern "C" BaseType_t xSemaphoreTake(SemaphoreHandle_t s, TickType_t wait) {
  if (!s) return pdFAIL;
  struct timespec lim; tiempo_limite(&lim, wait);
  pthread_mutex_lock(&s->mx);
  if (s->recursivo && s->dueno == mi_id() && s->prof > 0) { s->prof++; pthread_mutex_unlock(&s->mx); return pdTRUE; }
  while (s->cuenta == 0) { if (wait == 0 || !esperar(&s->cv, &s->mx, wait, &lim)) break; }
  bool ok = s->cuenta > 0;
  if (ok) { s->cuenta--; s->dueno = mi_id(); s->prof = 1; }
  pthread_mutex_unlock(&s->mx);
  return ok ? pdTRUE : pdFALSE;
}
extern "C" BaseType_t xSemaphoreGive(SemaphoreHandle_t s) {
  if (!s) return pdFAIL;
  pthread_mutex_lock(&s->mx);
  if (s->recursivo && s->prof > 1) { s->prof--; pthread_mutex_unlock(&s->mx); return pdTRUE; }
  bool ok = s->cuenta < s->max;
  if (ok) { s->cuenta++; s->prof = 0; s->dueno = 0; pthread_cond_signal(&s->cv); }
  pthread_mutex_unlock(&s->mx);
  return ok ? pdTRUE : pdFALSE;
}
extern "C" void vSemaphoreDelete(SemaphoreHandle_t s) { delete s; }
extern "C" UBaseType_t uxSemaphoreGetCount(SemaphoreHandle_t s) { return s ? s->cuenta : 0; }

// colas
struct SimQueue { pthread_mutex_t mx; pthread_cond_t cv; uint8_t *buf; UBaseType_t largo, tam, cab, n; };
extern "C" QueueHandle_t xQueueCreate(UBaseType_t len, UBaseType_t itemSize) {
  SimQueue *q = new SimQueue(); pthread_mutex_init(&q->mx, nullptr); pthread_cond_init(&q->cv, nullptr);
  q->largo = len ? len : 1; q->tam = itemSize; q->buf = (uint8_t *)calloc(q->largo, itemSize ? itemSize : 1); q->cab = q->n = 0; return q;
}
extern "C" void vQueueDelete(QueueHandle_t q) { if (q) { free(q->buf); delete q; } }
static BaseType_t cola_poner(QueueHandle_t q, const void *item, TickType_t wait, bool alFrente) {
  if (!q) return pdFAIL;
  struct timespec lim; tiempo_limite(&lim, wait);
  pthread_mutex_lock(&q->mx);
  while (q->n == q->largo) { if (wait == 0 || !esperar(&q->cv, &q->mx, wait, &lim)) break; }
  bool ok = q->n < q->largo;
  if (ok) {
    UBaseType_t pos;
    if (alFrente) { q->cab = (q->cab + q->largo - 1) % q->largo; pos = q->cab; }
    else pos = (q->cab + q->n) % q->largo;
    memcpy(q->buf + pos * q->tam, item, q->tam); q->n++;
    pthread_cond_broadcast(&q->cv);
  }
  pthread_mutex_unlock(&q->mx);
  return ok ? pdTRUE : errQUEUE_FULL;
}
extern "C" BaseType_t xQueueSend(QueueHandle_t q, const void *item, TickType_t wait) { return cola_poner(q, item, wait, false); }
extern "C" BaseType_t xQueueSendToFront(QueueHandle_t q, const void *item, TickType_t wait) { return cola_poner(q, item, wait, true); }
extern "C" BaseType_t xQueueOverwrite(QueueHandle_t q, const void *item) {
  pthread_mutex_lock(&q->mx);
  if (q->n == q->largo) { q->cab = (q->cab + 1) % q->largo; q->n--; }
  memcpy(q->buf + ((q->cab + q->n) % q->largo) * q->tam, item, q->tam); q->n++;
  pthread_cond_broadcast(&q->cv); pthread_mutex_unlock(&q->mx);
  return pdTRUE;
}
static BaseType_t cola_sacar(QueueHandle_t q, void *item, TickType_t wait, bool mirar) {
  if (!q) return pdFAIL;
  struct timespec lim; tiempo_limite(&lim, wait);
  pthread_mutex_lock(&q->mx);
  while (q->n == 0) { if (wait == 0 || !esperar(&q->cv, &q->mx, wait, &lim)) break; }
  bool ok = q->n > 0;
  if (ok) {
    memcpy(item, q->buf + q->cab * q->tam, q->tam);
    if (!mirar) { q->cab = (q->cab + 1) % q->largo; q->n--; pthread_cond_broadcast(&q->cv); }
  }
  pthread_mutex_unlock(&q->mx);
  return ok ? pdTRUE : pdFALSE;
}
extern "C" BaseType_t xQueueReceive(QueueHandle_t q, void *item, TickType_t wait) { return cola_sacar(q, item, wait, false); }
extern "C" BaseType_t xQueuePeek(QueueHandle_t q, void *item, TickType_t wait) { return cola_sacar(q, item, wait, true); }
extern "C" UBaseType_t uxQueueMessagesWaiting(QueueHandle_t q) { return q ? q->n : 0; }
extern "C" UBaseType_t uxQueueSpacesAvailable(QueueHandle_t q) { return q ? q->largo - q->n : 0; }
extern "C" BaseType_t xQueueReset(QueueHandle_t q) { pthread_mutex_lock(&q->mx); q->n = q->cab = 0; pthread_cond_broadcast(&q->cv); pthread_mutex_unlock(&q->mx); return pdPASS; }

// ---------------------------------------------------------------- I2S
// Igual que el DMA del ESP32: i2s_channel_write bloquea mientras la cola está
// llena. La cola real es dma_desc_num × dma_frame_num frames (4 × 128 ≈ 12 ms en
// los firmwares del PercuSynth); acá se deja un poco más porque el navegador
// necesita colchón, pero el sketch igual queda amarrado al reloj del audio.
#define COLA_MIN_FRAMES 1536
struct sim_i2s_chan {
  int puerto;
  bool tx;
  bool activo;
  uint32_t tasa;
  int bits;
  int slots;
  uint32_t cola;
  double reloj_t0;       // reloj virtual para cuando la interfaz no consume audio
  uint64_t reloj_frames;
  double mic_pos = -1;   // canal de entrada: posición de lectura en el anillo del micrófono
};
static sim_i2s_chan *g_canal_dac = nullptr;

extern "C" esp_err_t i2s_new_channel(const i2s_chan_config_t *cfg, i2s_chan_handle_t *tx, i2s_chan_handle_t *rx) {
  uint32_t cola = cfg->dma_desc_num * cfg->dma_frame_num;
  if (tx) { sim_i2s_chan *c = new sim_i2s_chan(); c->puerto = cfg->id; c->tx = true; c->tasa = 44100; c->bits = 16; c->slots = 2;
            c->cola = cola; c->reloj_t0 = 0; c->reloj_frames = 0; *tx = c; }
  if (rx) { sim_i2s_chan *c = new sim_i2s_chan(); c->puerto = cfg->id; c->tx = false; c->tasa = 44100; c->bits = 16; c->slots = 2;
            c->cola = cola; c->reloj_t0 = 0; c->reloj_frames = 0; *rx = c; g_bus.i2s_rx = 1; }
  return ESP_OK;
}
extern "C" esp_err_t i2s_del_channel(i2s_chan_handle_t h) { if (h == g_canal_dac) g_canal_dac = nullptr; delete h; return ESP_OK; }
extern "C" esp_err_t i2s_channel_init_std_mode(i2s_chan_handle_t h, const i2s_std_config_t *cfg) {
  h->tasa = cfg->clk_cfg.sample_rate_hz ? cfg->clk_cfg.sample_rate_hz : 44100;
  h->bits = cfg->slot_cfg.data_bit_width;
  h->slots = cfg->slot_cfg.slot_mode == I2S_SLOT_MODE_MONO ? 1 : 2;
  if (h->tx) { g_canal_dac = h; g_bus.audio_rate = h->tasa; }
  return ESP_OK;
}
extern "C" esp_err_t i2s_channel_reconfig_std_clock(i2s_chan_handle_t h, const i2s_std_clk_config_t *clk) {
  h->tasa = clk->sample_rate_hz; if (h->tx) g_bus.audio_rate = h->tasa;
  h->reloj_t0 = 0; return ESP_OK;
}
extern "C" esp_err_t i2s_channel_reconfig_std_slot(i2s_chan_handle_t h, const i2s_std_slot_config_t *s) {
  h->bits = s->data_bit_width; h->slots = s->slot_mode == I2S_SLOT_MODE_MONO ? 1 : 2; return ESP_OK;
}
extern "C" esp_err_t i2s_channel_reconfig_std_gpio(i2s_chan_handle_t, const i2s_std_gpio_config_t *) { return ESP_OK; }
extern "C" esp_err_t i2s_channel_enable(i2s_chan_handle_t h) { h->activo = true; h->reloj_t0 = 0; return ESP_OK; }
extern "C" esp_err_t i2s_channel_disable(i2s_chan_handle_t h) { h->activo = false; return ESP_OK; }
extern "C" esp_err_t i2s_channel_preload_data(i2s_chan_handle_t, const void *, size_t size, size_t *loaded) { if (loaded) *loaded = size; return ESP_OK; }
extern "C" esp_err_t i2s_channel_get_info(i2s_chan_handle_t h, i2s_chan_info_t *info) {
  memset(info, 0, sizeof *info); info->dir = h->tx ? I2S_DIR_TX : I2S_DIR_RX; info->sample_rate_hz = h->tasa;
  info->data_bit_width = (i2s_data_bit_width_t)h->bits; info->slot_mode = (i2s_slot_mode_t)h->slots; return ESP_OK;
}

// sin audio en la interfaz, el canal avanza con un reloj virtual (mismo ritmo, sin sonido)
static void pasar_reloj_virtual(sim_i2s_chan *h, uint32_t frames) {
  double ahora = ahora_ms();
  if (h->reloj_t0 == 0) { h->reloj_t0 = ahora; h->reloj_frames = 0; }
  double objetivo = h->reloj_t0 + (h->reloj_frames * 1000.0) / h->tasa;
  if (ahora - objetivo > 250) { h->reloj_t0 = ahora; h->reloj_frames = 0; objetivo = ahora; }   // no recuperar a ráfagas
  double adelanto = (double)(h->cola ? h->cola : 512) * 1000.0 / h->tasa;
  if (objetivo - adelanto > ahora) dormir_us((objetivo - adelanto - ahora) * 1000.0);
  h->reloj_frames += frames;
}

extern "C" esp_err_t i2s_channel_write(i2s_chan_handle_t h, const void *src, size_t size, size_t *written, uint32_t) {
  if (!h || !h->tx) return ESP_ERR_INVALID_ARG;
  int bps = h->bits <= 8 ? 1 : (h->bits <= 16 ? 2 : 4);
  size_t frames = size / (bps * h->slots);
  const uint8_t *p = (const uint8_t *)src;
  if (!g_bus.audio_live) {
    pasar_reloj_virtual(h, (uint32_t)frames);
  } else {
    h->reloj_t0 = 0;
    uint32_t cola = h->cola > COLA_MIN_FRAMES ? h->cola : COLA_MIN_FRAMES;
    if (cola > SIM_AUDIO_FRAMES - 512) cola = SIM_AUDIO_FRAMES - 512;
    size_t hecho = 0;
    while (hecho < frames) {
      uint32_t w = g_bus.a_w;
      uint32_t r = __atomic_load_n(&g_bus.a_r, __ATOMIC_ACQUIRE);
      uint32_t ocupado = w - r;
      if (ocupado >= cola) {
        if (!g_bus.audio_live) { pasar_reloj_virtual(h, (uint32_t)(frames - hecho)); break; }
        dormir_us(700);
        continue;
      }
      size_t n = cola - ocupado;
      if (n > frames - hecho) n = frames - hecho;
      for (size_t i = 0; i < n; i++) {
        int16_t L, R;
        const uint8_t *f = p + (hecho + i) * bps * h->slots;
        if (bps == 2) { L = ((const int16_t *)f)[0]; R = h->slots == 2 ? ((const int16_t *)f)[1] : L; }
        else if (bps == 4) { L = (int16_t)(((const int32_t *)f)[0] >> 16); R = h->slots == 2 ? (int16_t)(((const int32_t *)f)[1] >> 16) : L; }
        else { L = (int16_t)(((int)f[0] - 128) << 8); R = h->slots == 2 ? (int16_t)(((int)f[1] - 128) << 8) : L; }
        uint32_t k = (w + (uint32_t)i) & (SIM_AUDIO_FRAMES - 1);
        g_bus.audio[k * 2] = L; g_bus.audio[k * 2 + 1] = R;
      }
      __atomic_store_n(&g_bus.a_w, w + (uint32_t)n, __ATOMIC_RELEASE);
      hecho += n;
    }
  }
  if (written) *written = size;
  return ESP_OK;
}
// Micrófono (INMP441): la interfaz escribe el micrófono del PC en un anillo mono
// (float, a la frecuencia del navegador) y acá se remuestrea a la que pidió el
// sketch y se entrega en su formato: 32 bits con el dato arriba (como el INMP441),
// 16 bits, mono o estéreo (estéreo = la misma muestra en los dos slots). Sin
// micrófono activo entrega silencio al ritmo de su reloj, como un INMP441 mudo.
extern "C" esp_err_t i2s_channel_read(i2s_chan_handle_t h, void *dest, size_t size, size_t *read, uint32_t) {
  if (!h) return ESP_ERR_INVALID_ARG;
  int bps = h->bits <= 8 ? 1 : (h->bits <= 16 ? 2 : 4);
  size_t frames = size / (bps * h->slots);
  if (!g_bus.mic_live || !g_bus.mic_rate) {
    memset(dest, 0, size);
    h->mic_pos = -1;
    pasar_reloj_virtual(h, (uint32_t)frames);
    if (read) *read = size;
    return ESP_OK;
  }
  h->reloj_t0 = 0;
  double paso = (double)g_bus.mic_rate / h->tasa;
  uint32_t w = __atomic_load_n(&g_bus.mic_w, __ATOMIC_ACQUIRE);
  // primera lectura o muy atrasado (>250 ms): saltar cerca del escritor, con ~20 ms de colchón
  if (h->mic_pos < 0 || (double)w - h->mic_pos > g_bus.mic_rate * 0.25 || h->mic_pos > (double)w)
    h->mic_pos = (double)w - g_bus.mic_rate * 0.02;
  // esperar a que el navegador haya escrito lo que hace falta (el micrófono marca el ritmo)
  double necesita = h->mic_pos + frames * paso + 2;
  double limite = ahora_ms() + 200;
  while ((double)__atomic_load_n(&g_bus.mic_w, __ATOMIC_ACQUIRE) < necesita) {
    if (!g_bus.mic_live || ahora_ms() > limite) break;
    dormir_us(700);
  }
  uint8_t *p = (uint8_t *)dest;
  for (size_t i = 0; i < frames; i++) {
    uint32_t k = (uint32_t)h->mic_pos;
    float f = (float)(h->mic_pos - k);
    float a = g_bus.mic[k & (SIM_MIC_FRAMES - 1)], b = g_bus.mic[(k + 1) & (SIM_MIC_FRAMES - 1)];
    float s = a + (b - a) * f;
    if (s > 1) s = 1; if (s < -1) s = -1;
    h->mic_pos += paso;
    for (int c = 0; c < h->slots; c++) {
      uint8_t *q = p + (i * h->slots + c) * bps;
      if (bps == 4) *(int32_t *)q = (int32_t)(s * 2147483392.0f) & ~0xFF;     // 24 bits útiles, arriba
      else if (bps == 2) *(int16_t *)q = (int16_t)(s * 32767.0f);
      else *q = (uint8_t)(128 + (int)(s * 127.0f));
    }
  }
  __atomic_store_n(&g_bus.mic_r, (uint32_t)h->mic_pos, __ATOMIC_RELEASE);
  if (read) *read = size;
  return ESP_OK;
}

// ---------------------------------------------------------------- arranque
int main() {
  g_bus.magic = 0x50534D31;
  // espera a que la interfaz ponga botones, perillas y cables
  while (!__atomic_load_n(&g_bus.arranque, __ATOMIC_ACQUIRE)) dormir_us(2000);
  g_t0 = ahora_ms();
  g_xs ^= (uint32_t)(g_t0 * 1000.0);
  tarea_init(&g_tarea_loop, "loopTask", 1, 1);
  g_tarea_loop.hilo = pthread_self();
  t_actual = &g_tarea_loop;
  setup();
  // Un loop() vacío o que sólo consulta millis() giraría a todo lo que da un
  // núcleo del PC. Si mil vueltas tardan menos de 2 ms, se descansa 1 ms: para
  // el sketch es invisible (en el S3 el loopTask también cede entre vueltas).
  double marca = ahora_ms();
  unsigned vueltas = 0;
  for (;;) {
    loop();
    if (++vueltas == 1000) {
      double t = ahora_ms();
      if (t - marca < 2.0) dormir_us(1000);
      marca = ahora_ms(); vueltas = 0;
    }
  }
}
