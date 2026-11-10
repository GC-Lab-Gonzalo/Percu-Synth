// Lectura de un DHT11 / DHT22 por su protocolo de un hilo, igual que lo hace la librería de
// Adafruit en la placa: bajar la línea, soltarla y medir 40 bits por el largo de cada pulso alto.
// La usan DHT.h y DHTesp.h de la placa virtual. El sensor del otro lado (core.cpp) sólo contesta
// si está enchufado en ese pin desde la interfaz: con el DATA en otro pin, la lectura falla (NAN).
#pragma once
#include "Arduino.h"

// cuánto dura un nivel (µs), o -1 si pasa 1 ms sin cambiar (sensor desconectado)
static inline long sim_dht_pulso(uint8_t pin, int nivel) {
  unsigned long t0 = micros();
  while (digitalRead(pin) == nivel) if (micros() - t0 > 1000) return -1;
  return (long)(micros() - t0);
}

// 0 = bien, 1 = no contestó, 2 = suma de verificación mala. d[5] = los cinco bytes de la trama.
static inline int sim_dht_leer(uint8_t pin, int tipo, uint8_t d[5]) {
  memset(d, 0, 5);
  pinMode(pin, INPUT_PULLUP);
  delay(1);
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
  if (tipo == 11) delay(20); else delayMicroseconds(1100);     // DHT11 pide 18 ms como mínimo
  pinMode(pin, INPUT_PULLUP);
  delayMicroseconds(40);
  if (sim_dht_pulso(pin, HIGH) < 0) return 1;                  // termina la espera del sensor
  if (sim_dht_pulso(pin, LOW) < 0) return 1;                   // 80 µs bajo
  if (sim_dht_pulso(pin, HIGH) < 0) return 1;                  // 80 µs alto
  for (int i = 0; i < 40; i++) {
    long bajo = sim_dht_pulso(pin, LOW), alto = sim_dht_pulso(pin, HIGH);
    if (bajo < 0 || alto < 0) return 1;
    if (alto > bajo) d[i / 8] |= 0x80 >> (i % 8);              // 70 µs alto = 1, 26 µs = 0
  }
  return d[4] == (uint8_t)(d[0] + d[1] + d[2] + d[3]) ? 0 : 2;
}
static inline float sim_dht_temp(int tipo, const uint8_t *d) {
  if (tipo == 11 || tipo == 12) { float f = d[2]; if (d[3] & 0x80) f = -1 - f; return f + (d[3] & 0x0F) * 0.1f; }
  float f = (((d[2] & 0x7F) << 8) | d[3]) * 0.1f;
  return (d[2] & 0x80) ? -f : f;
}
static inline float sim_dht_hum(int tipo, const uint8_t *d) {
  if (tipo == 11 || tipo == 12) return d[0] + d[1] * 0.1f;
  return ((d[0] << 8) | d[1]) * 0.1f;
}
