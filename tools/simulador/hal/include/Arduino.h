// =============================================================================
// Arduino.h de la placa virtual — el núcleo arduino-esp32 que ve un sketch del
// PercuSynth cuando se compila para el simulador (WebAssembly).
//
// No es un emulador del ESP32-S3: es la API de Arduino/ESP-IDF reimplementada
// sobre hilos del navegador. Lo que el sketch "toca" del hardware cae en el
// SimBus (sim_bus.h), que la interfaz dibuja.
//
// Lo que NO reproduce: el costo en ciclos del S3. Un sketch que en la placa se
// queda sin tiempo (DMA vacío = chasquido) acá puede sonar limpio.
// =============================================================================
#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <math.h>
#include <ctype.h>
#include <algorithm>
#include <string>

#include "sim_bus.h"
#include "esp_attrs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "WString.h"
#include "Stream.h"
#include "HardwareSerial.h"

// ---------------------------------------------------------------- tipos y macros
typedef bool     boolean;
typedef uint8_t  byte;
typedef unsigned int word;

#define HIGH 0x1
#define LOW  0x0
#define INPUT          0x01
#define OUTPUT         0x03
#define PULLUP         0x04
#define INPUT_PULLUP   0x05
#define PULLDOWN       0x08
#define INPUT_PULLDOWN 0x09
#define OPEN_DRAIN     0x10
#define OUTPUT_OPEN_DRAIN 0x13
#define ANALOG         0xC0

#define RISING  0x01
#define FALLING 0x02
#define CHANGE  0x03

#define LED_BUILTIN 48
#define RGB_BUILTIN 48
#define RGB_BRIGHTNESS 64

#ifndef PI
#define PI 3.1415926535897932384626433832795
#endif
#define HALF_PI 1.5707963267948966192313216916398
#define TWO_PI  6.283185307179586476925286766559
#define DEG_TO_RAD 0.017453292519943295769236907684886
#define RAD_TO_DEG 57.295779513082320876798154814105
#define EULER 2.718281828459045235360287471352

#define radians(deg) ((deg)*DEG_TO_RAD)
#define degrees(rad) ((rad)*RAD_TO_DEG)
#define sq(x) ((x)*(x))
#define constrain(amt,low,high) ((amt)<(low)?(low):((amt)>(high)?(high):(amt)))
#define _min(a,b) ((a)<(b)?(a):(b))
#define _max(a,b) ((a)>(b)?(a):(b))
using std::min;
using std::max;
using std::abs;
using std::isinf;
using std::isnan;

#define lowByte(w)  ((uint8_t) ((w) & 0xff))
#define highByte(w) ((uint8_t) ((w) >> 8))
#define bitRead(value, bit)  (((value) >> (bit)) & 0x01)
#define bitSet(value, bit)   ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitToggle(value, bit) ((value) ^= (1UL << (bit)))
#define bitWrite(value, bit, bitvalue) ((bitvalue) ? bitSet(value, bit) : bitClear(value, bit))
#ifndef bit
#define bit(b) (1UL << (b))
#endif
#define _BV(b) (1UL << (b))

#define PROGMEM
#define PGM_P const char *
#define PSTR(s) (s)
#define pgm_read_byte(addr)  (*(const uint8_t  *)(addr))
#define pgm_read_word(addr)  (*(const uint16_t *)(addr))
#define pgm_read_dword(addr) (*(const uint32_t *)(addr))
#define pgm_read_float(addr) (*(const float    *)(addr))
#define pgm_read_ptr(addr)   (*(void * const   *)(addr))
#define pgm_read_byte_near(a) pgm_read_byte(a)
#define pgm_read_word_near(a) pgm_read_word(a)
#define memcpy_P memcpy
#define strcpy_P strcpy
#define strlen_P strlen
#define sprintf_P sprintf

#define interrupts()
#define noInterrupts()
#define digitalPinToInterrupt(p) (p)

// ---------------------------------------------------------------- tiempo
unsigned long millis();
unsigned long micros();
void delay(uint32_t ms);
void delayMicroseconds(uint32_t us);
void yield();

// ---------------------------------------------------------------- GPIO y ADC
void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t val);
int  digitalRead(uint8_t pin);
uint16_t analogRead(uint8_t pin);
uint32_t analogReadMilliVolts(uint8_t pin);
void analogReadResolution(uint8_t bits);
void analogSetAttenuation(int attenuation);
void analogSetPinAttenuation(uint8_t pin, int attenuation);
void analogSetWidth(uint8_t bits);
void analogWrite(uint8_t pin, int value);
void neopixelWrite(uint8_t pin, uint8_t r, uint8_t g, uint8_t b);
void rgbLedWrite(uint8_t pin, uint8_t r, uint8_t g, uint8_t b);
typedef enum { ADC_0db, ADC_2_5db, ADC_6db, ADC_11db, ADC_ATTENDB_MAX } adc_attenuation_t;
void attachInterrupt(uint8_t pin, void (*fn)(void), int mode);
void detachInterrupt(uint8_t pin);
uint16_t touchRead(uint8_t pin);
unsigned long pulseIn(uint8_t pin, uint8_t state, unsigned long timeout = 1000000L);
unsigned long pulseInLong(uint8_t pin, uint8_t state, unsigned long timeout = 1000000L);

// ---------------------------------------------------------------- matemática
long random(long howbig);
long random(long howsmall, long howbig);
void randomSeed(unsigned long seed);
long map(long x, long in_min, long in_max, long out_min, long out_max);

// ---------------------------------------------------------------- ESP y PSRAM
class EspClass {
public:
  uint32_t getCycleCount();
  uint32_t getFreeHeap()      { return 280 * 1024; }
  uint32_t getHeapSize()      { return 320 * 1024; }
  uint32_t getMinFreeHeap()   { return 250 * 1024; }
  uint32_t getMaxAllocHeap()  { return 110 * 1024; }
  uint32_t getPsramSize()     { return 8 * 1024 * 1024; }
  uint32_t getFreePsram()     { return 8 * 1024 * 1024 - 64 * 1024; }
  uint32_t getMaxAllocPsram() { return 4 * 1024 * 1024; }
  uint32_t getCpuFreqMHz()    { return 240; }
  uint32_t getFlashChipSize() { return 16 * 1024 * 1024; }
  const char *getChipModel()  { return "ESP32-S3"; }
  uint8_t  getChipRevision()  { return 0; }
  uint8_t  getChipCores()     { return 2; }
  const char *getSdkVersion() { return "simulador"; }
  uint64_t getEfuseMac()      { return 0x0000A1B2C3D4E5F6ULL; }
  void restart();
};
extern EspClass ESP;
bool psramFound();
bool psramInit();
void *ps_malloc(size_t size);
void *ps_calloc(size_t n, size_t size);
void *ps_realloc(void *p, size_t size);

// ---------------------------------------------------------------- tareas
void setup();
void loop();
void disableCore0WDT();
void disableCore1WDT();
void enableCore0WDT();
void enableCore1WDT();
void disableLoopWDT();
void enableLoopWDT();

